#include "esp_camera.h"
#include "esp_http_server.h"
#include <WiFi.h>

// ==========================================
// --- CONFIGURACIÓN DE CREDENCIALES WI-FI ---
// ==========================================
const char *ssid = "holamundo";
const char *password = "12345678";

// ==========================================
// --- MAPEADO DE PINES (Modelo AI-Thinker) ---
// ==========================================
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Instancia del servidor HTTP
httpd_handle_t camera_httpd = NULL;

// Handler que se ejecuta al entrar a http://IP_CAMARA/capture
static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;

  // Capturar un cuadro con el sensor
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Error: Falló la captura de imagen");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  // Configurar los headers HTTP para enviar una imagen JPEG pura
  res = httpd_resp_set_type(req, "image/jpeg");
  if (res == ESP_OK) {
    res = httpd_resp_set_hdr(req, "Content-Disposition",
                             "inline; filename=capture.jpg");
  }
  if (res == ESP_OK) {
    // Forzar cierre de conexión TCP tras cada respuesta
    res = httpd_resp_set_hdr(req, "Connection", "close");
  }
  if (res == ESP_OK) {
    res = httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store");
  }

  // Enviar los bytes de la imagen
  if (res == ESP_OK) {
    res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  }

  // Liberar el buffer de la cámara para la siguiente foto
  esp_camera_fb_return(fb);
  return res;
}

// Configurar las rutas del servidor web
void startCameraServer() {
  httpd_config_t config =
      HTTPD_DEFAULT_CONFIG(); // <-- CORREGIDO: Estructura nativa correcta
  config.server_port = 80;

  httpd_uri_t capture_uri = {.uri = "/capture",
                             .method = HTTP_GET,
                             .handler = capture_handler,
                             .user_ctx = NULL};

  Serial.printf("Iniciando servidor web en el puerto: '%d'\n",
                config.server_port);
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &capture_uri);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Configuración estructural de la cámara
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // VGA (640x480): menor peso por frame, el ESP32 respira más
  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 15;  // 0-63: mayor número = menos datos, más rápido
    config.fb_count = 1;       // 1 buffer evita acumulación en cola
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }

  // Inicializar el hardware de la cámara
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error fatal al inicializar la cámara: 0x%x", err);
    return;
  }

  // Conectarse a la red Wi-Fi
  Serial.printf("Conectando a la red: %s \n", ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("¡Wi-Fi Conectado exitosamente!");

  // Arrancar el servidor web de capturas
  startCameraServer();

  // Imprimir la IP exacta en el monitor serie
  Serial.print("ESP32-CAM IP: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  // El loop se queda vacío porque el servidor HTTP corre de forma asíncrona en
  // segundo plano
  delay(1000);
}
#include <DHT.h> // Librería para el sensor de temperatura
#include <PubSubClient.h>
#include <WiFi.h>

// --- CONFIGURACIÓN DE RED ---
const char *ssid = "holamundo";    // Cambia por tu red
const char *password = "12345678"; // Cambia por tu clave
const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// --- ASIGNACIÓN DE PINES ---
#define PIN_MICRO 32 // Micrófono MAX4466 (Analógico - ADC1)
#define PIN_DHT 22   // Temperatura DHT11/21 (Digital)
#define PIN_GAS 33   // Gas MQ-6 (Analógico - ADC1)
#define PIN_AIRE 35  // Calidad de Aire MQ-135 (Analógico - ADC1)
#define PIN_LED 23   // LED de Alerta para Cámara (Digital)

// --- CONFIGURACIÓN DEL SENSOR DE TEMPERATURA ---
#define DHTTYPE                                                                \
  DHT21 // Usa DHT11 para el sensor negro. Si es blanco, cambia a DHT22
DHT dht(PIN_DHT, DHTTYPE);

// --- CONFIGURACIÓN DEL MICRÓFONO ---
const int sampleWindow = 50; // Ventana de muestreo de 50ms

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;

// --- FUNCIÓN CALLBACK (El "Oído" del ESP32) ---
// Esta función se ejecuta automáticamente cuando llega un mensaje desde
// Node-RED
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Mensaje recibido en el canal: ");
  Serial.println(topic);

  // Convertir el mensaje a un String para poder leerlo
  String mensaje = "";
  for (int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }

  Serial.print("Contenido: ");
  Serial.println(mensaje);

  // Si el mensaje dice "Desconocida", prendemos el LED. Si no, lo apagamos.
  if (String(topic) == "smartphone/equipoXX/alerta_camara") {
    if (mensaje == "Desconocido") {
      digitalWrite(PIN_LED, HIGH); // Prender LED
      Serial.println("¡ALERTA! Persona desconocida detectada. LED ENCENDIDO.");
    } else if (mensaje == "Apagar" || mensaje == "Conocida") {
      digitalWrite(PIN_LED, LOW); // Apagar LED
      Serial.println("Zona segura. LED APAGADO.");
    }
  }
}

void setup_wifi() {
  delay(10);
  Serial.println("\nConectando a Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n¡Wi-Fi conectado exitosamente!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión a HiveMQ...");
    // Creamos un ID aleatorio para evitar choques en el broker público
    String clientId = "ESP32_ProyectoIoT-";
    clientId += String(random(0, 0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("¡Conectado al Broker MQTT!");

      // ¡MUY IMPORTANTE! Aquí nos suscribimos al canal para escuchar órdenes
      client.subscribe("smartphone/equipoXX/alerta_camara");

    } else {
      Serial.print("Falló, rc=");
      Serial.print(client.state());
      Serial.println(" Reintentando en 5 segundos...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // Vincular la función de escucha al cliente MQTT
  client.setCallback(callback);

  // Configurar los pines
  pinMode(PIN_MICRO, INPUT);
  pinMode(PIN_GAS, INPUT);
  pinMode(PIN_AIRE, INPUT);
  pinMode(PIN_LED, OUTPUT);   // Configurar el LED como salida
  digitalWrite(PIN_LED, LOW); // Asegurarnos de que el LED empiece apagado

  // Inicializar el sensor de temperatura
  dht.begin();

  Serial.println("¡Sistema listo! Empezando lectura de sensores...");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Esto mantiene el "oído" encendido buscando mensajes

  // Enviar datos cada 3 segundos
  unsigned long now = millis();
  if (now - lastMsg > 3000) {
    lastMsg = now;

    // --- 1. LECTURA DE TEMPERATURA ---
    float temp = dht.readTemperature();
    if (isnan(temp)) {
      Serial.println("¡Advertencia: Error al leer el sensor de Temperatura!");
      temp = 0.0;
    }

    // --- 2. LECTURA DE GASES ---
    int valorGas = analogRead(PIN_GAS);
    int valorAire = analogRead(PIN_AIRE);

    // --- 3. LECTURA DE MICRÓFONO ---
    unsigned long startMillis = millis();
    unsigned int signalMax = 0;
    unsigned int signalMin = 4095;

    while (millis() - startMillis < sampleWindow) {
      unsigned int sample = analogRead(PIN_MICRO);
      if (sample < 4095) {
        if (sample > signalMax)
          signalMax = sample;
        else if (sample < signalMin)
          signalMin = sample;
      }
    }
    unsigned int ruido = signalMax - signalMin;

    // --- 4. PUBLICACIÓN A NODE-RED ---
    Serial.println("--- ENVIANDO DATOS A NODE-RED ---");

    client.publish("smartphone/equipoXX/temperatura", String(temp).c_str());
    Serial.print("Temperatura: ");
    Serial.print(temp);
    Serial.println(" °C");

    client.publish("smartphone/equipoXX/gas", String(valorGas).c_str());
    Serial.print("Gas (Pin 33): ");
    Serial.println(valorGas);

    client.publish("smartphone/equipoXX/aire", String(valorAire).c_str());
    Serial.print("Aire (Pin 35): ");
    Serial.println(valorAire);

    // CORRECCIÓN APLICADA: Sin tilde para que Node-RED lo entienda bien
    client.publish("smartphone/equipoXX/microfono", String(ruido).c_str());
    Serial.print("Ruido (Pin 32): ");
    Serial.println(ruido);

    Serial.println("---------------------------------");
  }
}
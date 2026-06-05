# 🎥 Sistema IoT de Reconocimiento Facial con ESP32-CAM

Sistema completo de reconocimiento facial en tiempo real usando un **ESP32-CAM** como fuente de video y **Python** como motor de procesamiento. Publica resultados vía **MQTT** a Node-RED para integrarse en un entorno domótico.

---

## 📁 Estructura del proyecto

```
reconocimiento/
├── esp32cam.ino       # Firmware: servidor HTTP de captura de imágenes (cámara)
├── esp32default.ino   # Firmware: sensores ambientales + LED de alerta
├── reconocimiento.py  # Script Python: reconocimiento facial + publicación MQTT
└── caras_conocidas/   # Carpeta con fotos de referencia (.jpeg)
    ├── Nombre1.jpeg
    └── Nombre2.jpeg
```

---

## ⚙️ ¿Cómo funciona?

### Flujo completo

```
Sensor OV2640 (ESP32-CAM)
        │  captura RAW
        ▼
  Compresión JPEG (hardware)
        │  bytes JPEG en RAM
        ▼
  Servidor HTTP  ─── GET /capture ──►  Python (urllib)
        │                                    │
        │                              bytearray (bytes crudos)
        │                                    │
        │                              np.array  →  vector uint8 1D
        │                                    │
        │                              cv2.imdecode  →  matriz (H, W, 3) BGR
        │                                    │
        │                              cv2.cvtColor  →  RGB
        │                                    │
        │                              face_recognition
        │                                    │
        └──────────────────────────    MQTT → Node-RED
```

### Formato de la imagen en cada etapa

| Etapa | Formato | Tipo Python | Forma |
|-------|---------|-------------|-------|
| Sensor → RAM del ESP32 | RAW Bayer → **JPEG** | `uint8[]` en C | 1D |
| Viaje por WiFi/HTTP | Bytes JPEG en el Body HTTP | — | — |
| `img_resp.read()` | **bytes** | `bytearray` | 1D |
| `np.array(...)` | Array NumPy | `ndarray uint8` | `(N,)` |
| `cv2.imdecode(..., -1)` | **Imagen BGR** | `ndarray uint8` | `(H, W, 3)` |
| `cvtColor BGR→RGB` | **Imagen RGB** | `ndarray uint8` | `(H, W, 3)` |

> `-1` en `cv2.imdecode` equivale a `IMREAD_UNCHANGED`: carga tal como es (3 canales color).  
> OpenCV usa **BGR** internamente; `face_recognition` (dlib) espera **RGB** — por eso se convierte.

---

## 🔌 Hardware

| Dispositivo | Firmware | Función |
|-------------|----------|---------|
| ESP32-CAM (AI-Thinker) | `esp32cam.ino` | Servidor HTTP `/capture` — entrega JPEG por petición |
| ESP32 (genérico) | `esp32default.ino` | Lee temperatura, gas, aire, micrófono; enciende LED de alerta |

### Pines ESP32-CAM (AI-Thinker)

| Señal | GPIO |
|-------|------|
| PWDN | 32 |
| XCLK | 0 |
| SIOD/SDA | 26 |
| SIOC/SCL | 27 |
| Y2–Y9 | 5,18,19,21,36,39,34,35 |
| VSYNC | 25 |
| HREF | 23 |
| PCLK | 22 |

### Pines ESP32 default

| Sensor | Pin |
|--------|-----|
| Micrófono MAX4466 | GPIO 32 (ADC1) |
| Temperatura DHT21 | GPIO 22 |
| Gas MQ-6 | GPIO 33 (ADC1) |
| Calidad aire MQ-135 | GPIO 35 (ADC1) |
| LED alerta | GPIO 23 |

---

## 📡 MQTT — Tópicos

| Tópico | Dispositivo | Payload | Descripción |
|--------|-------------|---------|-------------|
| `smarthome/equipoXX/camara` | Python | JSON | Estado del reconocimiento facial |
| `smartphone/equipoXX/temperatura` | ESP32 default | `float` | Temperatura en °C |
| `smartphone/equipoXX/gas` | ESP32 default | `int` | Lectura cruda MQ-6 |
| `smartphone/equipoXX/aire` | ESP32 default | `int` | Lectura cruda MQ-135 |
| `smartphone/equipoXX/microfono` | ESP32 default | `int` | Nivel de ruido (pico-pico) |
| `smartphone/equipoXX/alerta_camara` | Node-RED → ESP32 | `"Desconocido"` / `"Apagar"` | Enciende/apaga LED |

### Ejemplo de payload de la cámara

```json
{
  "movimiento": 1,
  "persona": "Augusto",
  "estado": "Seguro"
}
```

| Campo | Valores posibles |
|-------|-----------------|
| `movimiento` | `0` = nadie / `1` = alguien detectado |
| `persona` | `"Ninguno"`, `"Desconocido"`, o el nombre reconocido |
| `estado` | `"Monitoreando"`, `"Seguro"`, `"Alerta"` |

---

## 🛠️ Instalación

### Dependencias Python

```bash
pip install opencv-python face_recognition numpy paho-mqtt
```

> `face_recognition` requiere `dlib`, que a su vez necesita `cmake` y `build-essential`:
> ```bash
> sudo apt install cmake build-essential
> ```

### Flashear los ESP32

1. Abrir el `.ino` correspondiente en el **IDE de Arduino**.
2. Instalar las bibliotecas necesarias:
   - `esp32cam.ino`: solo el paquete de placas ESP32 (incluye `esp_camera` y `esp_http_server`).
   - `esp32default.ino`: `DHT sensor library` + `PubSubClient`.
3. Seleccionar placa **AI-Thinker ESP32-CAM** (para `esp32cam.ino`) o tu modelo de ESP32.
4. Compilar y subir.

---

## ⚙️ Configuración

### 1. Credenciales WiFi (ambos `.ino`)

```cpp
const char *ssid     = "TU_RED";
const char *password = "TU_CLAVE";
```

### 2. IP de la cámara (`reconocimiento.py`)

Obtén la IP del ESP32-CAM desde el Monitor Serie y actualiza:

```python
ESP32_URL = "http://<IP_DEL_ESP32>/capture"
```

### 3. Tópico MQTT (`reconocimiento.py` y `esp32default.ino`)

Cambia `XX` por el número de tu equipo en todos los tópicos:

```python
MQTT_TOPIC = "smarthome/equipoXX/camara"
```

### 4. Base de datos de rostros

Crea la carpeta `caras_conocidas/` y agrega una foto `.jpeg` por persona con el nombre exacto:

```
caras_conocidas/
  Augusto.jpeg
  Boris.jpeg
```

Luego regístralos en el script:

```python
archivos_caras = {
    "Augusto": "caras_conocidas/Augusto.jpeg",
    "Boris":   "caras_conocidas/Boris.jpeg",
}
```

---

## 🚀 Uso

```bash
python3 reconocimiento.py
```

- Se abre una ventana con el video en vivo.
- Las caras conocidas se encuadran en **verde**, las desconocidas en **rojo**.
- Los resultados se publican por MQTT cada vez que cambia el estado o cada 3 segundos.
- Presiona `q` para salir limpiamente.

---

## ⚡ Optimizaciones anti-saturación (ESP32-CAM)

El ESP32-CAM sólo puede atender ~3–5 peticiones HTTP por segundo. Sin control de velocidad, Python lo satura con timeouts en cascada. Las siguientes optimizaciones evitan ese problema:

### En `esp32cam.ino`

| Parámetro | Antes | Ahora | Motivo |
|-----------|-------|-------|--------|
| Resolución | SVGA (800×600) | **VGA (640×480)** | 44% menos píxeles por frame |
| `fb_count` | 2 | **1** | Elimina la cola de frames acumulados |
| `jpeg_quality` | 12 | **15** | Frames más ligeros, transmisión más rápida |
| Header TCP | — | `Connection: close` + `Cache-Control: no-cache` | Fuerza cierre de conexión tras cada request |

### En `reconocimiento.py`

| Cambio | Antes | Ahora |
|--------|-------|-------|
| Velocidad de captura | Sin límite (~50 fps) | **~3 fps** (`FRAME_INTERVAL = 0.35 s`) |
| Timeout HTTP | 2 s | **5 s** |
| Manejo de errores | `sleep(0.1)` fijo | **Backoff exponencial** (0.5 → 1 → 2 → 4 → 5 s máx.) |
| Header enviado | Ninguno | `Connection: close` |

---

## 🧰 Tecnologías

- [OpenCV](https://opencv.org/) — decodificación y visualización de imágenes
- [face_recognition](https://github.com/ageitgey/face_recognition) — detección y codificación facial (basado en dlib)
- [NumPy](https://numpy.org/) — manipulación de arrays de imagen
- [paho-mqtt](https://pypi.org/project/paho-mqtt/) — cliente MQTT en Python
- [PubSubClient](https://pubsubclient.knolleary.net/) — cliente MQTT para Arduino/ESP32
- [ESP32-CAM](https://github.com/espressif/esp32-camera) — hardware de cámara + servidor HTTP embebido
- [HiveMQ](https://www.hivemq.com/) — broker MQTT público

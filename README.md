# 🎥 Reconocimiento Facial con ESP32-CAM

Sistema de reconocimiento facial en tiempo real usando un ESP32-CAM y Python.

## ¿Qué hace?

Captura el stream de video de una cámara ESP32-CAM y detecta/reconoce rostros en tiempo real comparándolos contra una base de datos de caras conocidas.

## Requisitos

```bash
pip install opencv-python face_recognition numpy
```

> **Nota:** `face_recognition` requiere `dlib` que a su vez requiere `cmake` y `build-essential`.

## Configuración

1. Flashear el firmware **CameraWebServer** en el ESP32-CAM.
2. Conectar el ESP32 a la red WiFi local.
3. Obtener la IP asignada al ESP32 (ej: `192.168.1.X`).
4. Editar `reconocimiento.py` y cambiar la IP:

```python
ESP32_URL = "http://<IP_DE_TU_ESP32>:81/stream"
```

5. Crear la carpeta `caras_conocidas/` y agregar una foto `.jpeg` por persona:

```
caras_conocidas/
  Nombre1.jpeg
  Nombre2.jpeg
```

6. Agregar los nombres al diccionario en el script:

```python
archivos_caras = {
    "Nombre1": "caras_conocidas/Nombre1.jpeg",
    "Nombre2": "caras_conocidas/Nombre2.jpeg",
}
```

## Uso

```bash
python reconocimiento.py
```

- Se abre una ventana con el video en vivo.
- Cuando detecta una cara conocida imprime `*** Nombre verificado ***` en consola.
- Presiona `q` para salir.

## Tecnologías

- [OpenCV](https://opencv.org/) — captura y visualización de video
- [face_recognition](https://github.com/ageitgey/face_recognition) — detección y codificación facial
- [ESP32-CAM](https://github.com/espressif/esp32-camera) — hardware de cámara

import cv2
import face_recognition
import numpy as np
import os
import time
import json
import urllib.request  
import paho.mqtt.client as mqtt

# ====================================================================
# --- CONFIGURACIÓN DE PARÁMETROS ---
# ====================================================================

# IP actual de tu ESP32-CAM (Usando /capture para fotos estáticas)
ESP32_URL = "http://10.36.94.253/capture" 

# Broker público de internet
MQTT_BROKER = "broker.hivemq.com"                    
MQTT_PORT = 1883
MQTT_TOPIC = "smarthome/equipoXX/camara"  # <-- Cambia XX por tu número de equipo

# ====================================================================
# --- CONEXIÓN AL BROKER MQTT ---
# ====================================================================
print("Conectando al Broker HiveMQ...")
mqtt_client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.loop_start()  # Inicia el cliente MQTT en segundo plano

# ====================================================================
# --- CARGAR BASE DE DATOS DE ROSTROS ---
# ====================================================================
print("Cargando base de datos de rostros...")
caras_conocidas_encodings = []
nombres_conocidos = []

archivos_caras = {
    "Augusto": "caras_conocidas/Augusto.jpeg",
    "Boris": "caras_conocidas/Boris.jpeg",
    "Thomas": "caras_conocidas/Thomas.jpeg"
}

for nombre, ruta in archivos_caras.items():
    if os.path.exists(ruta):
        imagen = face_recognition.load_image_file(ruta)
        encoding = face_recognition.face_encodings(imagen)[0]
        caras_conocidas_encodings.append(encoding)
        nombres_conocidos.append(nombre)
        print(f" Rostro de [{nombre}] cargado exitosamente.")
    else:
        print(f"Advertencia: No se encontró la imagen {ruta}")

# ====================================================================
# --- BUCLE PRINCIPAL DE RECONOCIMIENTO EN VIVO ---
# ====================================================================
print("\nIniciando bucle de captura de rostros (Ametralladora HTTP)...")
ultima_notificacion = ""
tiempo_ultimo_envio = 0

while True:
    try:
        # Abrimos y cerramos la conexión HTTP de inmediato para no congelar el buffer de la cámara
        with urllib.request.urlopen(ESP32_URL, timeout=2) as img_resp:
            img_bytes = bytearray(img_resp.read())
        
        # Convertimos los bytes en una matriz que OpenCV entienda
        img_np = np.array(img_bytes, dtype=np.uint8)
        frame = cv2.imdecode(img_np, -1)
        
        if frame is None:
            print("Frame vacío o corrupto, reintentando...")
            continue
            
    except Exception as e:
        print(f"Error al obtener imagen desde la cámara: {e}")
        time.sleep(0.1)
        continue

    # Redimensionar el cuadro a 1/4 para acelerar drásticamente el procesamiento
    small_frame = cv2.resize(frame, (0, 0), fx=0.25, fy=0.25)
    rgb_small_frame = cv2.cvtColor(small_frame, cv2.COLOR_BGR2RGB)

    # Localizar y codificar rostros presentes en la imagen
    face_locations = face_recognition.face_locations(rgb_small_frame)
    face_encodings = face_recognition.face_encodings(rgb_small_frame, face_locations)

    nombre_detectado = "Ninguno"

    for (top, right, bottom, left), face_encoding in zip(face_locations, face_encodings):
        matches = face_recognition.compare_faces(caras_conocidas_encodings, face_encoding)
        nombre_detectado = "Desconocido"

        face_distances = face_recognition.face_distance(caras_conocidas_encodings, face_encoding)

        if len(face_distances) > 0:
            mejor_match_index = np.argmin(face_distances)

            if matches[mejor_match_index]:
                nombre_detectado = nombres_conocidos[mejor_match_index]
                print(f" {nombre_detectado} verificado frente a la cámara ")

        # Escalamos los recuadros de vuelta al tamaño original (1/4 * 4)
        top *= 4
        right *= 4
        bottom *= 4
        left *= 4

        # Cambiar color según reconocimiento: Verde para conocidos, Rojo para desconocidos
        color_recuadro = (0, 255, 0) if nombre_detectado != "Desconocido" else (0, 0, 255)

        # Dibujar rectángulos y nombres en la ventana local de la PC
        cv2.rectangle(frame, (left, top), (right, bottom), color_recuadro, 2)
        cv2.rectangle(frame, (left, bottom - 35), (right, bottom), color_recuadro, cv2.FILLED)
        cv2.putText(frame, nombre_detectado, (left + 6, bottom - 6), cv2.FONT_HERSHEY_DUPLEX, 0.8, (255, 255, 255), 1)

    # Lógica de notificación MQTT (Envía si cambia de estado o cada 3 segundos si se mantiene fijo)
    ahora = time.time()
    if nombre_detectado != ultima_notificacion or (ahora - tiempo_ultimo_envio) > 3:

        datos_camara = {
            "movimiento": 1 if nombre_detectado != "Ninguno" else 0,
            "persona": nombre_detectado,
            "estado": "Alerta" if nombre_detectado == "Desconocido" else "Seguro" if nombre_detectado != "Ninguno" else "Monitoreando"
        }

        # Publicar el JSON hacia el broker para Node-RED
        mqtt_client.publish(MQTT_TOPIC, json.dumps(datos_camara))
        print(f"[MQTT] Publicado en {MQTT_TOPIC}: {datos_camara}")

        ultima_notificacion = nombre_detectado
        tiempo_ultimo_envio = ahora

    # Dibujar la ventana con el video en vivo localmente en tu computadora
    cv2.imshow('Video ESP32-CAM', frame)

    # Requisito de OpenCV para procesar la ventana y permitir salir con la tecla 'q'
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# Limpieza final al cerrar
cv2.destroyAllWindows()
mqtt_client.loop_stop()
mqtt_client.disconnect()
print("Proceso finalizado con éxito.")
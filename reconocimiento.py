import cv2
import face_recognition
import numpy as np
import os
import time

# 1. Configuración de la conexión al ESP32-CAM
# NOTA: El puerto 81 es el estándar para el streaming de video en el CameraWebServer
ESP32_URL = "http://172.26.75.77:81/stream"  # <--- CAMBIA ESTO POR TU IP

# 2. Cargar las caras conocidas
print("Cargando base de datos de rostros...")
caras_conocidas_encodings = []
nombres_conocidos = []

# Nombres y rutas de las imágenes que guardaste
archivos_caras = {
    "Augusto": "caras_conocidas/Augusto.jpeg",
    "Boris": "caras_conocidas/Boris.jpeg",
    "Thomas": "caras_conocidas/Thomas.jpeg"
}

for nombre, ruta in archivos_caras.items():
    if os.path.exists(ruta):
        imagen = face_recognition.load_image_file(ruta)
        # Obtenemos la codificación facial de la imagen
        encoding = face_recognition.face_encodings(imagen)[0]
        caras_conocidas_encodings.append(encoding)
        nombres_conocidos.append(nombre)
    else:
        print(f"Advertencia: No se encontró la imagen {ruta}")

print("Iniciando conexión con la cámara...")
video_capture = cv2.VideoCapture(ESP32_URL)
video_capture.set(cv2.CAP_PROP_OPEN_TIMEOUT_MSEC, 5000)
video_capture.set(cv2.CAP_PROP_READ_TIMEOUT_MSEC, 5000)

if not video_capture.isOpened():
    print("Error: No se pudo conectar al stream del ESP32. Revisa la IP y la red.")
    exit()

# 3. Bucle principal de lectura y reconocimiento
while True:
    # Leer el frame actual del stream
    ret, frame = video_capture.read()
    if not ret:
        print("Frame perdido, reintentando...")
        time.sleep(0.1)
        continue

    # Reducir el tamaño del frame a 1/4 para que el procesamiento sea mucho más rápido
    small_frame = cv2.resize(frame, (0, 0), fx=0.25, fy=0.25)

    # Convertir la imagen de BGR (que usa OpenCV) a RGB (que usa face_recognition)
    rgb_small_frame = cv2.cvtColor(small_frame, cv2.COLOR_BGR2RGB)

    # Encontrar todas las caras y sus codificaciones en el frame actual
    face_locations = face_recognition.face_locations(rgb_small_frame)
    face_encodings = face_recognition.face_encodings(rgb_small_frame, face_locations)

    # Iterar sobre cada cara detectada
    for face_encoding in face_encodings:
        # Ver si la cara coincide con alguna de las conocidas
        matches = face_recognition.compare_faces(caras_conocidas_encodings, face_encoding)
        nombre_detectado = "Desconocido"

        # Calcular la distancia euclidiana para encontrar el mejor match (el más parecido)
        face_distances = face_recognition.face_distance(caras_conocidas_encodings, face_encoding)
        
        if len(face_distances) > 0:
            mejor_match_index = np.argmin(face_distances)
            
            # Si hay un match suficientemente cercano, verificamos la identidad
            if matches[mejor_match_index]:
                nombre_detectado = nombres_conocidos[mejor_match_index]
                
                # ¡AQUÍ ESTÁ EL MENSAJE DE VERIFICACIÓN!
                # Puedes reemplazar este print por un insert a una base de datos SQL o un log
                print(f"*** {nombre_detectado} verificado ***")

    # (Opcional) Mostrar el video en una ventana en tu monitor para ver qué está pasando
    cv2.imshow('Video ESP32-CAM', frame)

    # Presionar 'q' para salir del bucle
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# Limpiar al salir
video_capture.release()
cv2.destroyAllWindows()
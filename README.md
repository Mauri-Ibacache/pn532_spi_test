Funcionalidades principales
Lectura de tarjetas NFC

Se utiliza un lector PN532 en modo SPI.

Se obtiene el UID en formato HEX (ej: A3D7F705).

Envío de asistencia a EMQX

El Arduino publica un JSON con:

{
  "uid": "A3D7F705",
  "mac": "AA:BB:CC:DD:EE:FF",
  "sala": "B-201",
  "hora": "14:22:18",
  "epoch": 1731500000
}


Se envía al tópico:

pandaCheck/prod/asistencia

Recepción de sala asignada

El dispositivo se suscribe a:

Broadcast general
pandaCheck/rooms/assign

Asignación específica del dispositivo
pandaCheck/device/<MAC>/assign


Cuando llega un mensaje con la MAC del Arduino, este guarda la sala asignada para enviarla junto a la asistencia.

Sincronización de hora

Se usa NTP (pool.ntp.org)

Offset configurado para Chile (UTC-3)

Se agrega hora local + epoch al JSON

Tópicos usados
Tipo	Tópico
Publicación de asistencia	pandaCheck/prod/asistencia
Asignación broadcast	pandaCheck/rooms/assign
Asignación individual	pandaCheck/device/<MAC>/assign
Hardware necesario

Arduino UNO R4 WiFi

Lector NFC PN532 (modo SPI)

Tarjetas NFC MIFARE

Red WiFi 2.4 GHz

Seguridad

La conexión MQTT usa TLS (puerto 8883) con autenticación por:

usuario

contraseña

No requiere cargar certificados extra al Arduino, ya que EMQX maneja el TLS.

Código principal

El archivo principal del proyecto es:

/src/main.cpp


Contiene:

configuración WiFi

conexión MQTT (TLS)

suscripciones a tópicos

lectura del PN532

envío de JSON

recepción de sala

Flujo general de operación

Arduino se conecta a WiFi

Se conecta a EMQX por MQTT (TLS)

Se suscribe a los tópicos de asignación

Lee tarjetas NFC

Genera JSON con UID + MAC + sala + hora

Publica la asistencia

EMQX reenvía a AWS (Lambda → DynamoDB)
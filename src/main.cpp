#include <SPI.h>
#include <WiFiS3.h>              // WiFi Arduino UNO R4 WiFi
#include <ArduinoMqttClient.h>   // MQTT (TLS)
#include <Adafruit_PN532.h>      // PN532 NFC
#include <WiFiUdp.h>             // NTP
#include <NTPClient.h>

// ======== PN532 (SPI) ========
#define PN532_SS 10
Adafruit_PN532 nfc(PN532_SS);

// ======== WiFi ========
const char* ssid     = "S24";
const char* password = "mauriloco007";

// ======== MQTT (EMQX CLOUD) ========
const char* broker    = "q1d1111e.ala.us-east-1.emqxsl.com";
const int   mqttPort  = 8883;          // TLS
const char* mqttUser  = "PandaCheck";
const char* mqttPass  = "panda1234";

// Topics
//const char* TOPIC_ASISTENCIA = "pandaCheck/dev/asistencia";    
const char* TOPIC_ASISTENCIA = "pandaCheck/prod/asistencia";            // publica asistencias
const char* TOPIC_ASSIGN_BC  = "pandaCheck/rooms/assign";            // asignación broadcast
// También usaremos uno por-dispositivo: pandaCheck/device/<MAC>/assign (se arma en runtime)

// ======== MQTT/TLS Client ========
WiFiSSLClient wifiClient;
MqttClient    mqttClient(wifiClient);

// ======== NTP (hora local Chile) ========
WiFiUDP ntpUDP;
// Nota: En noviembre Chile está en UTC-3. Si cambia el horario, ajusta este offset.
const long TZ_OFFSET_SECONDS = -3 * 3600;   // UTC-3
NTPClient timeClient(ntpUDP, "pool.ntp.org", TZ_OFFSET_SECONDS, 60 * 1000); // actualiza cada 60s

// ======== Estado global ========
String deviceMAC = "";
String deviceMAC_noColon = "";   // para usar en tópicos
String assignedRoom = "";        // se completa por MQTT (o puedes setear un default)

// ======== Utils ========
String macToString(const byte mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

String macWithoutColons(const String& mac) {
  String s;
  for (size_t i = 0; i < mac.length(); i++) {
    if (mac[i] != ':') s += mac[i];
  }
  return s;
}

String uidToHex(const uint8_t* uid, uint8_t uidLength) {
  String uidHex;
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) uidHex += "0";
    uidHex += String(uid[i], HEX);
  }
  uidHex.toUpperCase();
  return uidHex;
}

// ======== Conexiones ========
void conectarWiFi() {
  Serial.print("Conectando a WiFi: ");
  Serial.println(ssid);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Ya conectado. IP: ");
    Serial.println(WiFi.localIP());
    return;
  }

  WiFi.begin(ssid, password);

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 40) {
    delay(250);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi CONECTADO");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nNo se pudo conectar a WiFi.");
  }
}

String deviceAssignTopic() {
  // pandaCheck/device/<MAC_sin_>:<>/assign (usamos sin ':')
  return "pandaCheck/device/" + deviceMAC_noColon + "/assign";
}

void onMqttMessage(int messageSize) {
  Serial.println("\n[MQTT] CALLBACK DISPARADO");

  String topic = mqttClient.messageTopic();
  Serial.print("Topic recibido: ");
  Serial.println(topic);

  String payload;
  payload.reserve(messageSize + 8);
  while (mqttClient.available()) {
    payload += (char)mqttClient.read();
  }

  Serial.print("Payload recibido: ");
  Serial.println(payload);

  // --- Lógica de asignación igual que antes ---

  if (topic == TOPIC_ASSIGN_BC) {
    String needle = "\"mac\":\"" + deviceMAC + "\"";
    int idx = payload.indexOf(needle);
    if (idx >= 0) {
      int rKey = payload.indexOf("\"room\"");
      if (rKey >= 0) {
        int colon = payload.indexOf(":", rKey);
        int q1 = payload.indexOf("\"", colon + 1);
        int q2 = payload.indexOf("\"", q1 + 1);
        if (q1 >= 0 && q2 > q1) {
          assignedRoom = payload.substring(q1 + 1, q2);
          Serial.print("[MQTT] Sala asignada (BC): ");
          Serial.println(assignedRoom);
        }
      }
    }
  } else if (topic == deviceAssignTopic()) {
    int rKey = payload.indexOf("\"room\"");
    if (rKey >= 0) {
      int colon = payload.indexOf(":", rKey);
      int q1 = payload.indexOf("\"", colon + 1);
      int q2 = payload.indexOf("\"", q1 + 1);
      if (q1 >= 0 && q2 > q1) {
        assignedRoom = payload.substring(q1 + 1, q2);
        Serial.print("[MQTT] Sala asignada (directo): ");
        Serial.println(assignedRoom);
      }
    }
  }
}

void conectarMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No hay WiFi, no puedo conectar a MQTT.");
    return;
  }

  Serial.print("Conectando al broker MQTT... ");
  mqttClient.setUsernamePassword(mqttUser, mqttPass);

  if (!mqttClient.connect(broker, mqttPort)) {
    Serial.print(" Error MQTT: ");
    Serial.println(mqttClient.connectError());
    return;
  }

  Serial.println("MQTT conectado.");

  // Suscripciones con debug
  int res1 = mqttClient.subscribe(TOPIC_ASSIGN_BC);  // pandaCheck/rooms/assign
  String t = deviceAssignTopic();                    // pandaCheck/device/<MAC>/assign
  int res2 = mqttClient.subscribe(t.c_str());

  Serial.print("Suscrito a: ");
  Serial.print(TOPIC_ASSIGN_BC);
  Serial.print(" -> resultado = ");
  Serial.println(res1);        // 1 = OK, 0 = FAIL

  Serial.print("Suscrito a: ");
  Serial.print(t);
  Serial.print(" -> resultado = ");
  Serial.println(res2);        // 1 = OK, 0 = FAIL
}


// ======== Publicación de asistencia ========
void publicarAsistencia(const String& uidHex) {
  // Hora local (HH:MM:SS) y epoch local (según TZ_OFFSET_SECONDS)
  timeClient.update();
  unsigned long epochLocal = timeClient.getEpochTime();  // ya con offset aplicado
  String hhmmss = timeClient.getFormattedTime();         // HH:MM:SS

  // JSON:
  // {
  //   "uid": "...",
  //   "mac": "AA:BB:CC:DD:EE:FF",
  //   "sala": "B-201",
  //   "hora": "HH:MM:SS",
  //   "epoch": 1731500000
  // }
  String payload = String("{\"uid\":\"") + uidHex +
                   "\",\"mac\":\"" + deviceMAC +
                   "\",\"sala\":\"" + (assignedRoom.length() ? assignedRoom : String("SIN_SALA")) +
                   "\",\"hora\":\"" + hhmmss +
                   "\",\"epoch\":" + String(epochLocal) +
                   "}";

  Serial.print("Publicando -> "); Serial.println(payload);
  mqttClient.beginMessage(TOPIC_ASISTENCIA);
  mqttClient.print(payload);
  mqttClient.endMessage();
}

// ======== Setup ========
void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  // PN532
  Serial.println("Iniciando PN532 (SPI)...");
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("No se detectó PN532. Revisa conexiones y modo SPI.");
    while (true) delay(1000);
  }
  Serial.print("PN532 OK. FW: 0x");
  Serial.println(versiondata, HEX);
  nfc.SAMConfig();
  Serial.println("Lector NFC listo. Acerca una tarjeta...");

  // WiFi
  conectarWiFi();

  // MAC
  byte macRaw[6];
  WiFi.macAddress(macRaw);
  deviceMAC = macToString(macRaw);
  deviceMAC_noColon = macWithoutColons(deviceMAC);
  Serial.print("MAC del dispositivo: ");
  Serial.println(deviceMAC);

  // NTP
  timeClient.begin();

  // MQTT
  mqttClient.onMessage(onMqttMessage);   // callback
  conectarMQTT();
}

// ======== Loop ========
void loop() {
  // Mantener conexiones
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado, reintentando...");
    conectarWiFi();
  }
  if (!mqttClient.connected()) {
    Serial.println("MQTT desconectado, reintentando...");
    conectarMQTT();
  }
  mqttClient.poll();  // keep-alive

  // Lectura NFC (timeout 1000 ms)
  uint8_t uid[7];
  uint8_t uidLength;
  bool success = nfc.readPassiveTargetID(
    PN532_MIFARE_ISO14443A, uid, &uidLength, 1000
  );

  if (success) {
    String uidHex = uidToHex(uid, uidLength);
    Serial.print("Tarjeta detectada UID: ");
    Serial.println(uidHex);

    if (mqttClient.connected()) {
      publicarAsistencia(uidHex);
    } else {
      Serial.println("No se pudo publicar: MQTT no conectado.");
    }
    delay(1500);  // anti-rebote
  }

  delay(50);
}

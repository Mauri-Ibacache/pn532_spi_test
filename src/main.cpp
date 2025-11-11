#include <SPI.h>
#include <WiFiS3.h>              // WiFi Arduino UNO R4 WiFi
#include <ArduinoMqttClient.h>   // MQTT seguro (TLS)
#include <Adafruit_PN532.h>      // Lector NFC PN532

// ===== PN532 (SPI) =====
#define PN532_SS 10
Adafruit_PN532 nfc(PN532_SS);

// ===== WiFi =====
const char* ssid     = "S24";
const char* password = "mauriloco007";

// ===== MQTT (EMQX CLOUD) =====
const char* broker    = "k802d52a.ala.us-east-1.emqxsl.com";
const int   mqttPort  = 8883;             // TLS
const char* mqttUser  = "arduino1";
const char* mqttPass  = "panda1234";
const char* mqttTopic = "pandaCheck/asistencia";

WiFiSSLClient wifiClient;       // Cliente TLS
MqttClient    mqttClient(wifiClient);

// ===== FUNCIONES =====

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
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n WiFi CONECTADO");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n No se pudo conectar a WiFi.");
  }
}

void conectarMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" No hay WiFi, no puedo conectar a MQTT.");
    return;
  }

  Serial.print("Conectando al broker MQTT... ");
  mqttClient.setUsernamePassword(mqttUser, mqttPass);

  if (!mqttClient.connect(broker, mqttPort)) {
    Serial.print(" Error MQTT: ");
    Serial.println(mqttClient.connectError());
  } else {
    Serial.println(" MQTT conectado correctamente");
  }
}

// Convierte UID en HEX mayúsculas
String uidToHex(const uint8_t* uid, uint8_t uidLength) {
  String uidHex;
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) uidHex += "0";
    uidHex += String(uid[i], HEX);
  }
  uidHex.toUpperCase();
  return uidHex;
}

// ===== SETUP =====

void setup() {
  Serial.begin(115200);
  while (!Serial) { /* espera puerto serie */ }

  Serial.println("Iniciando PN532 (SPI)...");

  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println(" No se detectó PN532. Revisa conexiones y modo SPI.");
    while (true) delay(1000);
  }

  Serial.print("PN532 detectado. Firmware: 0x");
  Serial.println(versiondata, HEX);

  nfc.SAMConfig();
  Serial.println(" Lector NFC listo. Esperando tarjetas...");

  conectarWiFi();
  conectarMQTT();
}

// ===== LOOP =====

void loop() {
  // Verificar WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado, reintentando...");
    conectarWiFi();
  }

  // Verificar MQTT
  if (!mqttClient.connected()) {
    Serial.println("MQTT desconectado, reintentando...");
    conectarMQTT();
  }

  mqttClient.poll();  // keep-alive MQTT

  // Lectura NFC
  uint8_t uid[7];
  uint8_t uidLength;

  bool success = nfc.readPassiveTargetID(
    PN532_MIFARE_ISO14443A,
    uid,
    &uidLength,
    1000
  );

  if (success) {
    String uidHex = uidToHex(uid, uidLength);

    Serial.print("Tarjeta detectada! UID: ");
    Serial.println(uidHex);

    // JSON: {"uid":"A3D7F705"}
    String payload = String("{\"uid\":\"") + uidHex + "\"}";

    if (mqttClient.connected()) {
      Serial.print(" Publicando en MQTT -> ");
      Serial.println(payload);

      mqttClient.beginMessage(mqttTopic);
      mqttClient.print(payload);
      mqttClient.endMessage();
    } else {
      Serial.println(" No se pudo publicar, MQTT no conectado.");
    }

    delay(1500);  // antirrebote de lecturas
  }

  delay(100);
}

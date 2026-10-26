/*
 * ==============================================================================
 * PRAKTIKUM 7 (TUGAS): SIMPUL SENSOR-AKTUATOR DENGAN AUTENTIKASI MQTT RESMI
 * ==============================================================================
 * Program ini membaca sensor DHT22, menyusun paket telemetri JSON, mengirimkannya
 * ke Broker Mosquitto Lokal dengan autentikasi akun, serta menerima instruksi kendali.
 * ==============================================================================
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// 1. Kredensial Wi-Fi
const char* ssid     = "POCO M3";
const char* password = "123456789";

// 2. Konfigurasi Broker MQTT Lokal & Kredensial Akun (TUGAS 5)
const char* mqttServer   = "10.202.167.71"; 
const int   mqttPort     = 1883;
const char* mqttUser     = "admin";       // Username MQTT resmi
const char* mqttPassword = "rahasia123";  // Password MQTT resmi

// 3. Konfigurasi Identitas Simpul & Topik
const char* clientId      = "ESP8266_Local_Node01";
const char* topicPubTele  = "kampus/labiot/telemetri";
const char* topicSubCmd   = "kampus/labiot/kendali";

// 4. Alokasi Pin Perangkat Keras
const byte dhtPin = 2; // Pin D4 (GPIO 2)
const byte ledPin = 5; // Pin D1 (GPIO 5)

#define DHTTYPE DHT22 // Sensor DHT22 fisik warna putih

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTeleTime = 0;
const unsigned long teleInterval = 4000; // Siklus kirim data setiap 4 detik

// Penanganan Paket Masuk dari Node-RED melalui Broker Lokal
void onMessageCallback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("\n----------------------------------------");
  Serial.printf("[INSTRUKSI MASUK] Topik: %s\n", topic);
  Serial.printf("[PAYLOAD]         : %s\n", incomingBuffer);

  // Parsing JSON Instruksi Kendali: {"led": 1} atau {"led": 0}
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);

  if (err) {
    Serial.printf("[ERROR] Gagal parsing JSON instruksi: %s\n", err.c_str());
    return;
  }

  // Evaluasi Kendali Indikator LED
  if (doc.containsKey("led")) {
    int stateLED = doc["led"];
    digitalWrite(ledPin, stateLED == 1 ? HIGH : LOW);
    Serial.printf("--> [AKTUATOR] Status LED diubah: %s\n", stateLED == 1 ? "MENYALA (ON)" : "PADAM (OFF)");
  } else {
    Serial.println("[WARNING] Kunci 'led' tidak ditemukan dalam payload!");
  }
  Serial.println("----------------------------------------");
}

void connectToWiFi() {
  Serial.println();
  Serial.print("[Wi-Fi] Menghubungkan ke ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[Wi-Fi] Berhasil Terhubung!");
  Serial.print("[Wi-Fi] IP NodeMCU: ");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("[MQTT] Menghubungkan ke Broker Lokal dengan Akun...");

    // TUGAS 5: Menghubungkan ke broker dengan Username dan Password
    if (client.connect(clientId, mqttUser, mqttPassword)) {
      Serial.println(" Terhubung dengan Autentikasi Sah!");
      // Segera daftarkan subskripsi topik kendali
      client.subscribe(topicSubCmd);
      Serial.printf("[MQTT] Berlangganan ke topik: %s\n", topicSubCmd);
    } else {
      Serial.printf(" Gagal (rc=%d). Coba lagi dalam 4 detik...\n", client.state());
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Awal sistem: LED padam

  dht.begin();
  connectToWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(onMessageCallback);
  client.setBufferSize(512);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop(); // Mendengarkan instruksi masuk

  // Pengiriman Telemetri Periodik Non-Blocking
  unsigned long now = millis();
  if (now - lastTeleTime >= teleInterval) {
    lastTeleTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      StaticJsonDocument<256> docOut;
      docOut["node_id"]    = clientId;
      docOut["suhu"]       = serialized(String(t, 2));
      docOut["kelembapan"] = serialized(String(h, 2));
      docOut["status_led"] = digitalRead(ledPin);

      char teleBuffer[256];
      serializeJson(docOut, teleBuffer);

      bool success = client.publish(topicPubTele, teleBuffer);
      if (success) {
        Serial.printf("[TELEMETRI TERKIRIM] %s\n", teleBuffer);
      } else {
        Serial.println("[ERROR] Gagal mengirim paket telemetri ke broker lokal!");
      }
    } else {
      Serial.println("[ERROR] Gagal membaca data dari sensor DHT fisik!");
    }
  }
}
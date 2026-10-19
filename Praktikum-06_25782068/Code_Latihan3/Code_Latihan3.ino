#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Kredensial Jaringan Wi-Fi Anda
const char* ssid     = "Aku suka kamu";
const char* password = "Kamusukasamayanglain";

// Konfigurasi Broker MQTT Publik
const char* mqttServer = "broker.hivemq.com";
const int mqttPort     = 1883;

// IDENTITAS KLIEN & TOPIK
const char* clientId          = "ESP8266_LabIoT_068_Lat3";
const char* topicPubTelemetry = "kampus/labiot/068/telemetri";

// Konfigurasi Pin Sensor DHT22
const byte dhtPin = 2; // Pin D4 (GPIO 2)
#define DHTTYPE DHT22

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long previousPublishTime = 0;
const unsigned long publishInterval = 5000;

void connectToWiFi() {
  Serial.print("Menghubungkan ke Wi-Fi: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Terkoneksi!");
  Serial.print("Alamat IP Lokal: ");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT Broker...");
    if (client.connect(clientId)) {
      Serial.println(" Sukses Terhubung!");
    } else {
      Serial.printf(" Gagal (rc=%d). Coba lagi dalam 5 detik...\n", client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  connectToWiFi();
  client.setServer(mqttServer, mqttPort);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - previousPublishTime >= publishInterval) {
    previousPublishTime = currentMillis;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
      Serial.println("Peringatan: Gagal membaca data DHT!");
      return;
    }

    // --- MODIFIKASI LATIHAN 3 (Menambah Uptime & IP Lokal) ---
    StaticJsonDocument<256> doc;
    doc["node_id"]      = clientId;
    doc["suhu"]         = serialized(String(t, 2));
    doc["kelembapan"]   = serialized(String(h, 2));
    doc["uptime_detik"] = millis() / 1000;              // Parameter baru 1
    doc["ip_lokal"]     = WiFi.localIP().toString();    // Parameter baru 2

    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer);

    bool isSuccess = client.publish(topicPubTelemetry, jsonBuffer);
    if (isSuccess) {
      Serial.print("Telemetri Terkirim [");
      Serial.print(topicPubTelemetry);
      Serial.print("]: ");
      Serial.println(jsonBuffer);
    } else {
      Serial.println("Peringatan: Gagal memublikasikan paket!");
    }
  }
}
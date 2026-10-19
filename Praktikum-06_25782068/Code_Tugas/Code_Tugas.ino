#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ================= PENGATURAN KREDENSIAL =================
const char* ssid     = "Aku suka kamu";
const char* password = "Kamusukasamayanglain";

const char* mqttServer = "broker.hivemq.com";
const int mqttPort     = 1883;

// IDENTITAS KLIEN & HIERARKI TOPIK (NIM 068)
const char* clientId       = "ESP8266_LabIoT_068_Safety";
const char* topicTelemetry  = "kampus/labiot/068/telemetri";
const char* topicCommand    = "kampus/labiot/068/kendali";
const char* topicAlert      = "kampus/labiot/068/alert";
const char* topicStatus     = "kampus/labiot/068/status";

// ================= HARDWARE & VARIABEL ===================
const byte dhtPin = 2; // Pin D4 (GPIO 2)
const byte ledPin = 5; // Pin D1 (GPIO 5)
#define DHTTYPE DHT22

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// Batas Ambang Suhu Kritis (Sesuai Modul: 31.0 C)
// Catatan: Jika suhu kamarmu saat ini sudah 31.5 C, kamu bisa naikkan batas ini 
// sedikit (misal jadi 33.0 atau 34.0) agar bisa menguji kondisi normal vs panas.
const float TEMP_THRESHOLD = 31.0;

unsigned long lastTelemetryTime = 0;
const unsigned long telemetryInterval = 4000; // Siklus telemetri 4 detik

// State Latch Flags
bool alertTriggered = false;
float latestTemperature = 0.0;

// ================= CALLBACK KENDALI MANUAL ================
void callback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("----------------------------------------");
  Serial.printf("[PERINTAH MASUK] %s\n", incomingBuffer);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);
  if (err) {
    Serial.println("Gagal membedah JSON!");
    return;
  }

  if (doc.containsKey("led")) {
    int val = doc["led"];
    
    // ATURAN 4: Kendali manual hanya diizinkan saat suhu NORMAL
    if (latestTemperature <= TEMP_THRESHOLD) {
      digitalWrite(ledPin, val == 1 ? HIGH : LOW);
      Serial.printf("--> [MANUAL OK] LED disetel: %s\n", val == 1 ? "MENYALA" : "PADAM");
    } else {
      // Jika OVERHEAT, tolak perintah override demi keselamatan
      digitalWrite(ledPin, HIGH); // Paksa tetap menyala
      Serial.println("--> [DITOLAK] Kondisi BAHAYA OVERHEAT! LED dikunci MENYALA.");
    }
  }
  Serial.println("----------------------------------------");
}

// ================= RECONNECT DENGAN LWT ===================
void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke Broker dengan LWT...");

    // ATURAN 3: Payload Pesan Wasiat (Last Will Message)
    const char* willMessage = "{\"status\":\"OFFLINE\",\"keterangan\":\"Perangkat Terputus Tak Terduga\"}";

    // Mendaftar ke broker dengan LWT: QoS=1, Retain=true
    if (client.connect(clientId, topicStatus, 1, true, willMessage)) {
      Serial.println(" Sukses Terhubung!");

      // 1. Publikasikan status ONLINE seketika (Retain=true)
      const char* onlineMessage = "{\"status\":\"ONLINE\",\"keterangan\":\"Perangkat Beroperasi Normal\"}";
      client.publish(topicStatus, onlineMessage, true);
      Serial.printf("[LWT STATUS] Terdaftar ONLINE di %s\n", topicStatus);

      // 2. Subskripsi topik kendali
      client.subscribe(topicCommand);
      Serial.printf("[SUBSKRIPSI] %s\n", topicCommand);
    } else {
      Serial.printf("Gagal (rc=%d). Coba lagi dalam 4 detik...\n", client.state());
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Terkoneksi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastTelemetryTime >= telemetryInterval) {
    lastTelemetryTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      latestTemperature = t;

      // ================= ATURAN 2: SAFETY RULE ENGINE =================
      if (t > TEMP_THRESHOLD) {
        // Kondisi Overheat: Otomatis nyalakan indikator bahaya
        digitalWrite(ledPin, HIGH);

        // Latch Flag: Kirim notifikasi bahaya HANYA saat pertama kali terpicu
        if (!alertTriggered) {
          StaticJsonDocument<256> alertDoc;
          alertDoc["status"]     = "BAHAYA";
          alertDoc["parameter"]  = "OVERHEAT";
          alertDoc["nilai_suhu"] = serialized(String(t, 2));

          char alertBuf[256];
          serializeJson(alertDoc, alertBuf);
          client.publish(topicAlert, alertBuf);

          Serial.printf("\n [ALERT TERKIRIM] %s -> %s\n\n", topicAlert, alertBuf);
          alertTriggered = true; // Kunci latch agar tidak membanjiri broker
        }
      } else {
        // Kondisi Normal: Reset latch agar siap mendeteksi bahaya berikutnya
        if (alertTriggered) {
          Serial.println("\n [STATUS] Suhu kembali NORMAL.\n");
          alertTriggered = false;
        }
      }

      // ================= ATURAN 1: TELEMETRI RUTIN ====================
      StaticJsonDocument<256> docOut;
      docOut["node_id"]      = clientId;
      docOut["suhu"]         = serialized(String(t, 2));
      docOut["kelembapan"]   = serialized(String(h, 2));
      docOut["status_led"]   = digitalRead(ledPin);
      docOut["kondisi_suhu"] = (t > TEMP_THRESHOLD) ? "PANAS" : "NORMAL";

      char buffer[256];
      serializeJson(docOut, buffer);
      client.publish(topicTelemetry, buffer);

      Serial.printf("[TELEMETRI] %s\n", buffer);
    } else {
      Serial.println("[ERROR] Gagal membaca sensor DHT!");
    }
  }
}
#include <DHT.h>

const byte ldrPin   = 34;  // Pin D34
const byte dhtPin   = 13;  // Pin D13
const byte relayPin = 12;  // Pin D12
const byte ledPin   = 5;   // Pin D5

#define DHTTYPE DHT11
DHT dht(dhtPin, DHTTYPE);

void setup() {
  Serial.begin(115200);
  delay(1000); // Beri waktu serial dan sensor stabil

  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Kondisi awal mati aman
  digitalWrite(relayPin, LOW);
  digitalWrite(ledPin, LOW);

  analogReadResolution(10); // Skala 0 - 1023
  dht.begin();

  Serial.println("\n==========================================");
  Serial.println("  SISTEM SMART WAREHOUSE SIAP DIGUNAKAN  ");
  Serial.println("==========================================");
}

void loop() {
  delay(2000); 

  int ldrValue = analogRead(ldrPin);
  float temp   = dht.readTemperature();
  float hum    = dht.readHumidity();

  // Jika DHT masih loading pada detik-detik awal, gunakan nilai default sementara
  if (isnan(temp) || isnan(hum)) {
    Serial.println("[INFO] Menunggu sensor DHT siap... (Menggunakan data sementara)");
    temp = 28.0;
    hum  = 60.0;
  }

  // Tampilkan data sensor
  Serial.println("------------------------------------------");
  Serial.print("Suhu: ");
  Serial.print(temp, 1);
  Serial.print(" °C | Kelembapan: ");
  Serial.print(hum, 1);
  Serial.print(" % | Cahaya: ");
  Serial.println(ldrValue);

  // ATURAN/RULE ENGINE (Suhu > 34 ATAU Cahaya < 300)
  if (temp > 34.0 || ldrValue < 300) {
    digitalWrite(relayPin, HIGH); // Relay Aktif
    digitalWrite(ledPin, HIGH);   // LED Indikator Nyala
    
    Serial.print(">>> [STATUS] PERINGATAN: AKTUATOR AKTIF! ");
    if (temp > 34.0 && ldrValue < 300) {
      Serial.println("(Suhu Panas & Ruangan Gelap)");
    } else if (temp > 34.0) {
      Serial.println("(Suhu Panas > 34°C)");
    } else {
      Serial.println("(Ruangan Terlalu Gelap < 300)");
    }
  } else {
    digitalWrite(relayPin, LOW);  // Relay Mati
    digitalWrite(ledPin, LOW);    // LED Mati
    Serial.println(">>> [STATUS] Kondisi Aman (Suhu Normal & Terang)");
  }

  Serial.flush();
}
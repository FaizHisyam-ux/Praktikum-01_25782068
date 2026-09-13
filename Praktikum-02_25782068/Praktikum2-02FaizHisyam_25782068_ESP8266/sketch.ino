#include <DHT.h> 

// Pin data DHT terhubung ke pin D13 pada ESP32
const byte dhtPin = 13;       

// Gunakan DHT11 atau DHT22 sesuai komponen yang kamu pasang di Wokwi
#define DHTTYPE DHT22      

DHT dht(dhtPin, DHTTYPE); 

void setup() {  
  Serial.begin(115200);  
  dht.begin();   
  Serial.println("Memulai Sensor DHT (Suhu & Kelembapan)...");  
}

void loop() {  
  // Sensor DHT butuh jeda waktu sekitar 2 detik untuk membaca data baru
  delay(2000); 

  float temp = dht.readTemperature(); // Membaca suhu dalam Celcius
  float hum  = dht.readHumidity();    // Membaca kelembapan (%)

  // Cek apakah sensor berhasil dibaca
  if (isnan(temp) || isnan(hum)) {  
    Serial.println("[ERROR] Gagal membaca data dari sensor DHT!");  
    return; 
  }

  // Tampilkan hasilnya ke Serial Monitor
  Serial.print("Suhu: ");  
  Serial.print(temp);  
  Serial.print(" °C | Kelembapan: ");  
  Serial.print(hum);  
  Serial.println(" %");  
}
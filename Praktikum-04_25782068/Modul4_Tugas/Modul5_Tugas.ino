#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

const char* ssid = "Aku suka kamu"; 
const char* password = "Kamusukasamayanglain";

// Konfigurasi Pin
const byte dhtPin = 2;        // D4 (GPIO 2)
const byte buttonPin = 4;     // D2 (GPIO 4)
const byte ledPin = 12;       // D6 (GPIO 12)
const byte pwmLedPin = 5;  // D1 (GPIO5)
int pwmValue = 0;

DHT dht(dhtPin, DHT22);

// Variabel Pelacak Status (State & Cache)
bool ledState = false;
String currentTemp = "--";
int buttonState;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long lastTime = 0;

// Inisialisasi Async Web Server (port 80) & WebSocket (rute /ws)
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ---------------- HTML & JAVASCRIPT (FRONT-END) ----------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Real-Time IoT Web</title>
  <style>
    body { font-family: Arial; text-align: center; }
    .card { background: #f0f0f0; margin: 20px auto; padding: 20px; max-width: 300px; border-radius: 10px; }
    button { padding: 15px 30px; font-size: 20px; border-radius: 5px; cursor: pointer; color: white;}  
    .btn-on { background-color: #4CAF50; }  
    .btn-off { background-color: #f44336; }  
  </style>
</head>
<body>
  <h1>Smart Room</h1>
  <div class="card">
    <h2>Suhu: <span id="tempValue">--</span> Celcius</h2>
  </div>
  <div class="card">
    <h2>LED: <span id="ledStatus">OFF</span></h2>
    <button id="toggleBtn" class="btn-off"
      onclick="toggleLed()">Turn ON</button>
  </div>
  <div class="card">
    <h2>Intensitas LED PWM</h2>
    <p>Nilai PWM: <span id="pwmValue">0</span></p>

    <input type="range"
      min="0"
      max="1023"
      value="0"
      id="pwmSlider"
      oninput="sendPWM(this.value)">
  </div>

  <script>
    // Membuka terowongan koneksi WebSocket ke alamat IP ESP8266
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    // Menjalankan inisiasi saat halaman pertama kali dimuat
    window.addEventListener('load', onLoad);
    function onLoad(event) { initWebSocket(); }

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen    = onOpen;
      websocket.onclose   = onClose;
      websocket.onmessage = onMessage;
    }

    function onOpen(event) { console.log('WebSocket Terkoneksi'); }
    function onClose(event) { setTimeout(initWebSocket, 2000); }

    // Dipanggil saat tombol di layar web ditekan
    function toggleLed(){  
      websocket.send('toggle');
    }

    function sendPWM(value) {
      document.getElementById('pwmValue').textContent = value;

      if (websocket &&
        websocket.readyState === WebSocket.OPEN) {
      websocket.send('pwm,' + value);
      }
    }

    // Menangkap paket data JSON yang "didorong" (PUSH) oleh C++ NodeMCU
    function onMessage(event) {
      var dataObj = JSON.parse(event.data);  
        
      // Menyuntikkan teks angka Suhu ke dalam id "tempValue" HTML
      if(dataObj.suhu !== undefined) {  
         document.getElementById('tempValue').innerHTML = dataObj.suhu;  
      }  
        
      // Merombak UI Tombol & Teks sesuai status hardware  
      if(dataObj.led !== undefined) {  
         var btn = document.getElementById('toggleBtn');  
         var status = document.getElementById('ledStatus');  
         if(dataObj.led == "1"){  
           status.innerHTML = "ON";  
           btn.innerHTML = "Turn OFF";  
           btn.className = "btn-on";  
         } else {  
           status.innerHTML = "OFF";  
           btn.innerHTML = "Turn ON";  
           btn.className = "btn-off";  
         }  
      }
      // TAMBAHAN: Memperbarui nilai slider PWM
      if (dataObj.pwm !== undefined) {
        document.getElementById('pwmValue').textContent =
          dataObj.pwm;

        document.getElementById('pwmSlider').value =
          dataObj.pwm;
  }  
    }  
  </script>  
</body>  
</html>  
)rawliteral";

// ---------------- BACK-END & WEBSOCKET LOGIC ----------------

// Fungsi menyebarkan paket data JSON ke SELURUH browser pengunjung
void notifyClients() {
  String jsonString = "{\"led\":\"";
  jsonString += String(ledState ? 1 : 0);
  jsonString += "\",\"suhu\":\"";
  jsonString += currentTemp;
  jsonString += "\",\"pwm\":\"";
  jsonString += String(pwmValue);
  jsonString += "\"}";

  ws.textAll(jsonString);
}

// Handler pesan masuk (dari klik browser)

void handleWebSocketMessage(
  void *arg, uint8_t *data, size_t len
) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;

  if (!(info->final && info->index == 0 &&
        info->len == len && info->opcode == WS_TEXT)) {
    return;
  }

  // Menyalin pesan ke buffer yang aman
  char message[32];

  if (len == 0 || len >= sizeof(message)) {
    return;
  }

  memcpy(message, data, len);
  message[len] = '\0';

  // ===== KONTROL LED UTAMA =====
  if (strcmp(message, "toggle") == 0) {
    ledState = !ledState;
    digitalWrite(ledPin, ledState ? HIGH : LOW);
    notifyClients();
    return;
  }

  // ===== KONTROL LED PWM =====
  if (strncmp(message, "pwm,", 4) == 0) {
    char *endPtr;
    long value = strtol(message + 4, &endPtr, 10);

    if (*endPtr == '\0' &&
        endPtr != message + 4 &&
        value >= 0 && value <= 1023) {

      pwmValue = (int)value;

      analogWrite(pwmLedPin, pwmValue);

      notifyClients();
    }
  }
}


// Event handler WebSocket bawaan AsyncWebServer
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Client WebSocket #%u terhubung\n", client->id());
      notifyClients(); // Kirim status terkini kepada klien yang baru buka web
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Client WebSocket #%u terputus\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
  }
}

void setup() {
  Serial.begin(115200);

  // Mengatur pin tombol sebagai input (dengan resistor pull-down eksternal)
  pinMode(buttonPin, INPUT);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  pinMode(pwmLedPin, OUTPUT);
  analogWriteRange(1023);
  analogWrite(pwmLedPin, 0);

dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nIP Address: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Ikat handler WebSockets ke server utama
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  ws.cleanupClients();

  // 1. Eksekusi perangkat keras LED
  digitalWrite(ledPin, ledState ? HIGH : LOW);

  // 2. Baca Tombol Fisik (Debounce) tanpa nge-freeze program
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      
      // Karena menggunakan resistor PULL-DOWN eksternal, tombol ditekan = HIGH
      if (buttonState == HIGH) {
        ledState = !ledState;
        notifyClients(); // PUSH DATA KE BROWSER SEKETIKA
      }
    }
  }
  lastButtonState = reading;

  // 3. Baca DHT setiap 3 detik di memori cache, tanpa memblokir CPU
  if ((millis() - lastTime) > 3000) {
    float t = dht.readTemperature();
    if(!isnan(t)) {
      currentTemp = String(t);
      notifyClients(); // Push pembaruan suhu otomatis
    }
    lastTime = millis();
  }
}
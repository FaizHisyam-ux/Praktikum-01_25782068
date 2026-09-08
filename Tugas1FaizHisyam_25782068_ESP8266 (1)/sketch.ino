// Definisi Pin ESP32
const int buttonPin = 4; // Pin D4 terhubung ke Push Button (Pull-Down eksternal)
const int ledPin = 5;    // Pin D5 terhubung ke Anoda LED

// Variabel pelacakan status (state tracking)
int buttonState = 0;         // Pembacaan status tombol saat ini
int lastButtonState = LOW;   // Menyimpan status tombol pada iterasi sebelumnya
bool ledState = false;       // Melacak kondisi lampu (false = MATI, true = MENYALA)

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, LOW);
  Serial.println("Sistem ESP32 Siap! Status Awal: LED MATI");
}

void loop() {

  buttonState = digitalRead(buttonPin);

  if (buttonState == HIGH && lastButtonState == LOW) {


    ledState = !ledState;


    if (ledState == true) {
      digitalWrite(ledPin, HIGH);
      Serial.println("Tombol ditekan 1x -> LED MENYALA (Terkunci ON)");
    } else {
      digitalWrite(ledPin, LOW);
      Serial.println("Tombol ditekan 1x -> LED MATI (Terkunci OFF)");
    }

    delay(200);
  }

  lastButtonState = buttonState;
}
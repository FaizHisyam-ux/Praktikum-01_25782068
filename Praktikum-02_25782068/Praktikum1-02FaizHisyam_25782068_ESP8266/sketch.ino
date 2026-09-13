const byte ldrPin = 34; // Pin analog LDR
const byte ledPin = 5;  // Pin LED (D1)

void setup() {
  Serial.begin(115200);
  
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Kondisi awal LED mati
  
  analogReadResolution(10);  // Skala ADC 10-bit (0 - 1023)
}

void loop() {
  int ldrValue = analogRead(ldrPin);

  Serial.print("Nilai Cahaya (ADC): ");
  Serial.print(ldrValue);

  // Nyala HANYA JIKA gelap gulita (< 200)
  if (ldrValue < 200) {
    digitalWrite(ledPin, HIGH);
    Serial.println(" -> Gelap Gulita [LED ON]");
  } else {
    digitalWrite(ledPin, LOW);
    Serial.println(" -> Terang [LED OFF]");
  }

  delay(500);
}
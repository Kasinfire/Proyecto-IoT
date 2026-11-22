// Prueba del circuito de la ranita

const int PIN_LDR = 34;
const int PIN_MEJILLA_IZQ = 25;
const int PIN_MEJILLA_DER = 26;

// Si la lectura baja de este valor, hay sombra. Ajústalo con tus números.
const int UMBRAL_SOMBRA = 1500;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_MEJILLA_IZQ, OUTPUT);
  pinMode(PIN_MEJILLA_DER, OUTPUT);

  // Prueba de LEDs: parpadea cada mejilla por separado.
  Serial.println("Probando mejilla izquierda (25)...");
  digitalWrite(PIN_MEJILLA_IZQ, HIGH);
  delay(1000);
  digitalWrite(PIN_MEJILLA_IZQ, LOW);

  Serial.println("Probando mejilla derecha (26)...");
  digitalWrite(PIN_MEJILLA_DER, HIGH);
  delay(1000);
  digitalWrite(PIN_MEJILLA_DER, LOW);

  Serial.println("Ahora tapa y destapa la LDR con la mano.");
}

void loop() {
  int luz = analogRead(PIN_LDR);  // 0 = oscuro, 4095 = mucha luz
  bool sombra = luz < UMBRAL_SOMBRA;

  Serial.print("luz: ");
  Serial.print(luz);
  Serial.println(sombra ? "  -> SOMBRA" : "");

  digitalWrite(PIN_MEJILLA_IZQ, sombra);
  digitalWrite(PIN_MEJILLA_DER, sombra);
  delay(100);
}

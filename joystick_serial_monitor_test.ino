// Joystick 1 (Left) Pins
const int J1_VRX = 32;
const int J1_VRY = 33;
const int J1_SW  = 25;

// Joystick 2 (Right) Pins
const int J2_VRX = 34;
const int J2_VRY = 35;
const int J2_SW  = 26;

void setup() {
  Serial.begin(115200);
  
  // Configure switch pins with internal pull-ups
  pinMode(J1_SW, INPUT_PULLUP);
  pinMode(J2_SW, INPUT_PULLUP);
}

void loop() {
  // Read analog axes (0 to 4095 on ESP32 12-bit ADC)
  int j1_x = analogRead(J1_VRX);
  int j1_y = analogRead(J1_VRY);
  int j2_x = analogRead(J2_VRX);
  int j2_y = analogRead(J2_VRY);

  // Read digital button states (LOW when pressed, HIGH when released)
  bool j1_pressed = digitalRead(J1_SW) == LOW;
  bool j2_pressed = digitalRead(J2_SW) == LOW;

  // Print results
  Serial.print("LEFT  [X: "); Serial.print(j1_x);
  Serial.print(", Y: "); Serial.print(j1_y);
  Serial.print(", Btn: "); Serial.print(j1_pressed ? "PRESSED" : "OPEN");
  
  Serial.print("] | RIGHT [X: "); Serial.print(j2_x);
  Serial.print(", Y: "); Serial.print(j2_y);
  Serial.print(", Btn: "); Serial.print(j2_pressed ? "PRESSED" : "OPEN");
  Serial.println("]");

  delay(100);
}

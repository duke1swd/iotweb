
void setup() {
  // put your setup code here, to run once:
  pinMode(D4, OUTPUT);
  pinMode(D8, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D0, INPUT_PULLUP);
  pinMode(D1, INPUT_PULLUP);
}

int v;

void loop() {
  // put your main code here, to run repeatedly:
  v = (millis()/100) & 1;
  digitalWrite(D4, v); // on board LED
  v = (millis()/200) & 1;
  digitalWrite(D8, v);
  v = (millis()/400) & 1;
  digitalWrite(D3, v);
}

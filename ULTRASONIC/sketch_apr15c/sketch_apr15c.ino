// -------- PIN CONFIG --------
const int trigPin = 9;
const int echoPin = 10;

const int IN1 = 12;
const int IN2 = 13;
const int ENA = 11;  // PWM pin

// -------- SETTINGS --------
const int thresholdDistance = 20; // cm
const int motorSpeed = 200;       // 0–255

// -------- VARIABLES --------
long duration;
int distance;

// -------- FUNCTION: Measure Distance --------
int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH, 30000); // timeout 30ms

  int dist = duration * 0.034 / 2;

  return dist;
}

// -------- MOTOR CONTROL --------
void motorRun() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, motorSpeed);
}

void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 0);
}

// -------- SETUP --------
void setup() {
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  Serial.println("System started. Motor running...");
}

// -------- LOOP --------
void loop() {

  distance = getDistance();

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance > 0 && distance < thresholdDistance) {
    Serial.println("Obstacle detected! Stopping motor...");

    motorStop();

    delay(5000);  // wait 5 seconds

    Serial.println("Resuming motor...");
  }

  motorRun();

  delay(100); // small stability delay
}
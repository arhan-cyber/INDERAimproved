#include <Servo.h>

// -------- ULTRASONIC --------
const int trigPin = 9;
const int echoPin = 10;

// -------- L298N --------
const int IN1 = 12;
const int IN2 = 13;
const int ENA = 11;

// -------- SETTINGS --------
const int thresholdDistance = 20; // cm
const int motorSpeed = 255;

// -------- SERVO SETUP --------
Servo baseServo, shoulderServo, elbowServo, 
      wristPitch, wristRoll, gripper;

int curpos[6] = {90, 90, 90, 90, 90, 150};
int tgtpos[6] = {90, 90, 90, 90, 90, 180};

const int stepSize = 1;
const int stepDelay = 15;

// -------- VARIABLES --------
long duration;
int distance;
bool actionInProgress = false;

// -------- ULTRASONIC FUNCTION --------
int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH, 30000);
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

// -------- SERVO SMOOTH MOVE --------
void smoothMove(Servo &s, int i) {
  while (curpos[i] < tgtpos[i]) {
    curpos[i] += stepSize;
    s.write(curpos[i]);
    delay(stepDelay);
  }

  while (curpos[i] > tgtpos[i]) {
    curpos[i] -= stepSize;
    s.write(curpos[i]);
    delay(stepDelay);
  }
}

// -------- BASE ROTATION ACTION --------
void performArmAction() {
  Serial.println("Performing arm action...");

  // Example: rotate base from 90 → 140 → back to 90
  tgtpos[0] = 140;
  smoothMove(baseServo, 0);

  delay(500);

  tgtpos[0] = 90;
  smoothMove(baseServo, 0);

  Serial.println("Arm action complete.");
}

// -------- SETUP --------
void setup() {
  Serial.begin(9600);

  // Ultrasonic
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Motor
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  // Servos
  baseServo.attach(2);
  shoulderServo.attach(3);
  elbowServo.attach(4);
  wristPitch.attach(5);
  wristRoll.attach(6);
  gripper.attach(7);

  Serial.println("System Ready.");
}

// -------- LOOP --------
void loop() {

  distance = getDistance();

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // -------- TRIGGER CONDITION --------
  if (distance > 0 && distance < thresholdDistance && !actionInProgress) {
    
    actionInProgress = true;

    Serial.println("Obstacle detected!");

    // Stop motor
    motorStop();

    // Perform arm movement
    performArmAction();

    delay(500);  // wait after action

    Serial.println("Resuming motor...");

    actionInProgress = false;
  }

  // Default: motor runs
  if (!actionInProgress) {
    motorRun();
  }

  delay(100);
}
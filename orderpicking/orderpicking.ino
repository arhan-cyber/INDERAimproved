#include <Servo.h>

// -------- L298N --------
const int IN1 = 12;
const int IN2 = 13;
const int ENA = 11;

// -------- SETTINGS --------
const int motorSpeed = 255;
const int stepDelay = 15;

// -------- SERVOS --------
Servo baseServo, shoulderServo, elbowServo,
      wristPitch, wristRoll, gripper;

// -------- POSITIONS --------
int curpos[6]    = {90, 90, 90, 90, 90, 180};
int pickupPos[6] = {80, 85, 175, 90, 90, 110};
int dropPos[6]   = {0, 55, 180, 130, 90, 180}; // Single, universal drop location

// -------- STATE --------
bool busy = false;

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

// -------- SMOOTH SERVO MOVE --------
void moveServo(Servo &s, int i, int target) {
  while (curpos[i] < target) {
    curpos[i]++;
    s.write(curpos[i]);
    delay(stepDelay);
  }
  while (curpos[i] > target) {
    curpos[i]--;
    s.write(curpos[i]);
    delay(stepDelay);
  }
}

// -------- PICK + PLACE SEQUENCE --------
void performPickAndPlace() {

  Serial.println("ACTION: START");

  // Stop conveyor
  motorStop();

  // ---- MOVE TO PICK ----
  moveServo(shoulderServo, 1, pickupPos[1]);
  moveServo(elbowServo,    2, pickupPos[2]);
  moveServo(wristPitch,    3, pickupPos[3]);

  // ---- GRIP ----
  moveServo(gripper, 5, pickupPos[5]);
  delay(300);

  // ---- LIFT ----
  moveServo(elbowServo, 2, 150);
  delay(300);

  // ---- MOVE TO DROP ----
  moveServo(baseServo,     0, dropPos[0]);
  moveServo(shoulderServo, 1, dropPos[1]);
  moveServo(elbowServo,    2, dropPos[2]);
  moveServo(wristPitch,    3, dropPos[3]);

  // ---- RELEASE ----
  moveServo(gripper, 5, dropPos[5]);
  delay(500);

  // ---- RETURN HOME (SAFE) ----
  moveServo(shoulderServo, 1, 90);
  moveServo(elbowServo,    2, 90);
  delay(300);

  moveServo(baseServo, 0, 90);
  delay(300);

  moveServo(wristPitch, 3, 90);
  moveServo(wristRoll,  4, 90);
  delay(300);

  moveServo(gripper, 5, 180);

  Serial.println("ACTION: DONE");

  // Resume conveyor
  motorRun();
}

// -------- COMMAND HANDLER --------
void handleCommand(String cmd) {

  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "RUN") {
    motorRun();
    Serial.println("ACK: RUN");

  } else if (cmd == "STOP") {
    motorStop();
    Serial.println("ACK: STOP");

  } else if (cmd == "PICK") {

    if (!busy) {
      busy = true;
      Serial.println("ACK: PICK");
      performPickAndPlace();
      busy = false;
    } else {
      Serial.println("BUSY");
    }
  }
}

// -------- SETUP --------
void setup() {
  Serial.begin(9600);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  // Attach servos
  baseServo.attach(2);
  shoulderServo.attach(3);
  elbowServo.attach(4);
  wristPitch.attach(5);
  wristRoll.attach(6);
  gripper.attach(7);

  // Initialize positions BEFORE movement
  baseServo.write(curpos[0]);
  shoulderServo.write(curpos[1]);
  elbowServo.write(curpos[2]);
  wristPitch.write(curpos[3]);
  wristRoll.write(curpos[4]);
  gripper.write(curpos[5]);

  delay(500);

  Serial.println("SYSTEM READY");

  // Start conveyor by default
  motorRun();
}

// -------- LOOP --------
void loop() {

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    handleCommand(cmd);
  }
}
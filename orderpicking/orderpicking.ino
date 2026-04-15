#include <Servo.h>

int blockCount = 0;

// -------- ULTRASONIC --------
const int trigPin = 9;
const int echoPin = 10;

// -------- L298N --------
const int IN1 = 12;
const int IN2 = 13;
const int ENA = 11;

// -------- SETTINGS --------
const int thresholdDistance = 20;
const int motorSpeed = 255;

// -------- SERVOS --------
Servo baseServo, shoulderServo, elbowServo, 
      wristPitch, wristRoll, gripper;

// -------- POSITIONS --------
int curpos[6]   = {90, 65, 90, 90, 90, 180};
int pickupPos[6] = {80, 65, 175, 90, 90, 100};
int dropPos[6]   = {0, 35, 180, 130, 90, 180};
int dropPos2[6] = {0, 23, 165, 130, 90, 180};  // layer 2 (on top)

// -------- SETTINGS --------
const int stepDelay = 15;

// -------- VARIABLES --------
bool actionInProgress = false;
int targetLevel = 1;  // NEW: Target stacking level from Python

// -------- MOVING AVERAGE --------
const int NUM_SAMPLES = 3;
int readings[NUM_SAMPLES] = {0};
int index = 0;
long total = 0;
int lastValid = 0;

int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 10000);
  if (duration == 0) return lastValid;

  int dist = duration * 0.034 / 2;

  if (dist > 2 && dist < 400) {
    total -= readings[index];
    readings[index] = dist;
    total += readings[index];
    index++;
    if (index >= NUM_SAMPLES) index = 0;
    int avg = total / NUM_SAMPLES;
    lastValid = avg;
  }

  return lastValid;
}

// -------- MOTOR --------
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

// -------- MOVE ONE SERVO --------
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

// -------- ARM SEQUENCE --------
void performArmAction() {
  Serial.println("ACTION: Picking object...");

  // ---- MOVE ARM (NO GRIPPER YET) ----
  moveServo(shoulderServo, 1, pickupPos[1]);
  moveServo(elbowServo,    2, pickupPos[2]);
  moveServo(wristPitch,    3, pickupPos[3]);

  // ---- CLOSE GRIPPER LAST ----
  moveServo(gripper, 5, pickupPos[5]);
  delay(300);

  // ---- LIFT (ELBOW ONLY) ----
  Serial.println("ACTION: Lifting...");
  moveServo(elbowServo, 2, 150);
  delay(300);

  Serial.print("ACTION: Moving to drop (Level ");
  Serial.print(targetLevel);
  Serial.println(")");

  // ---- SELECT DROP POSITION BASED ON TARGET LEVEL ----
  int *targetDrop;
  if (targetLevel == 1) {
    targetDrop = dropPos;     // Level 1 (ground)
  } else {
    targetDrop = dropPos2;    // Level 2 (stacked)
  }

  // move wrist first (prevents last-moment jerk)
  moveServo(wristPitch,    3, targetDrop[3]);
  moveServo(baseServo,     0, targetDrop[0]);
  moveServo(shoulderServo, 1, targetDrop[1]);
  moveServo(elbowServo,    2, targetDrop[2]);

  // ---- OPEN GRIPPER LAST ----
  Serial.println("ACTION: Dropping...");
  moveServo(elbowServo, 2, targetDrop[2] - 10);
  delay(200);
  moveServo(gripper, 5, targetDrop[5]);
  delay(500);

  Serial.println("ACTION: Returning home...");

  // ---- RETURN TO START (SAFE SEQUENCE) ----
  moveServo(shoulderServo, 1, 5);
  moveServo(elbowServo,    2, 90);
  delay(300);

  moveServo(baseServo, 0, 90);
  delay(300);

  moveServo(wristPitch, 3, 90);
  moveServo(wristRoll,  4, 90);
  delay(300);

  moveServo(gripper, 5, 180);

  Serial.println("ACTION: DONE");
}

// -------- SERIAL COMMAND PARSER --------
void processSerialCommand() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.startsWith("PICK")) {
      // Format: "PICK:1" or "PICK:2"
      int colonIndex = command.indexOf(':');
      if (colonIndex != -1) {
        targetLevel = command.substring(colonIndex + 1).toInt();
        if (targetLevel < 1) targetLevel = 1;
        if (targetLevel > 2) targetLevel = 2;
      } else {
        targetLevel = 1; // Default to level 1
      }

      if (!actionInProgress) {
        actionInProgress = true;
        Serial.print("COMMAND: Received PICK for Level ");
        Serial.println(targetLevel);
        motorStop();
        performArmAction();
        delay(500);
        Serial.println("COMMAND: Resuming conveyor...");
        actionInProgress = false;
      }
    }
    else if (command == "RESET") {
      blockCount = 0;
      Serial.println("COMMAND: Block count reset");
    }
  }
}

// -------- SETUP --------
void setup() {
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  // Prevent jump
  baseServo.write(curpos[0]);
  shoulderServo.write(curpos[1]);
  elbowServo.write(curpos[2]);
  wristPitch.write(curpos[3]);
  wristRoll.write(curpos[4]);
  gripper.write(curpos[5]);

  baseServo.attach(2);
  shoulderServo.attach(3);
  elbowServo.attach(4);
  wristPitch.attach(5);
  wristRoll.attach(6);
  gripper.attach(7);

  Serial.println("SYSTEM READY");
}

// -------- LOOP --------
void loop() {
  // Check for serial commands from Python
  processSerialCommand();

  int distance = getDistance();

  // Optionally keep ultrasonic detection running
  // (You can disable this if using only Python vision)
  /*
  if (distance > 0 && distance < thresholdDistance && !actionInProgress) {
    actionInProgress = true;
    Serial.println("SENSOR: Object detected!");
    motorStop();
    performArmAction();
    delay(500);
    Serial.println("SENSOR: Resuming conveyor...");
    actionInProgress = false;
  }
  */

  if (!actionInProgress) {
    motorRun();
  }

  delay(50);
}
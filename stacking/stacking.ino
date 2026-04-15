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

// -------- MOVING AVERAGE --------
const int NUM_SAMPLES = 3;   // was 5 → faster response

int readings[NUM_SAMPLES] = {0};
int index = 0;
long total = 0;

int lastValid = 0;

int getDistance() {

  // --- TRIGGER PULSE ---
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // --- READ ECHO ---
  long duration = pulseIn(echoPin, HIGH, 10000);

  if (duration == 0) return lastValid;

  int dist = duration * 0.034 / 2;

  // --- VALIDITY CHECK ---
  if (dist > 2 && dist < 400) {

    // Remove oldest value from total
    total -= readings[index];

    // Store new reading
    readings[index] = dist;

    // Add new value
    total += readings[index];

    // Move index forward
    index++;
    if (index >= NUM_SAMPLES) index = 0;

    // Calculate average
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

  Serial.println("Picking object...");

  // ---- MOVE ARM (NO GRIPPER YET) ----
  moveServo(shoulderServo, 1, pickupPos[1]);
  moveServo(elbowServo,    2, pickupPos[2]);
  moveServo(wristPitch,    3, pickupPos[3]);

  // ---- CLOSE GRIPPER LAST ----
  moveServo(gripper, 5, pickupPos[5]);

  delay(300);

  // ---- LIFT (ELBOW ONLY) ----
  Serial.println("Lifting...");
  moveServo(elbowServo, 2, 150);

  delay(300);

  Serial.println("Moving to drop...");

  // ---- MOVE TO DROP (NO GRIPPER YET) ----
  int *targetDrop;

  if (blockCount == 0) {
    targetDrop = dropPos;     // first block
  } else {
    targetDrop = dropPos2;    // second block (stacked)
  }

  // move wrist first (prevents last-moment jerk)
  moveServo(wristPitch,    3, targetDrop[3]);

  moveServo(baseServo,     0, targetDrop[0]);
  moveServo(shoulderServo, 1, targetDrop[1]);
  moveServo(elbowServo,    2, targetDrop[2]);
  

  // ---- OPEN GRIPPER LAST ----
  Serial.println("Dropping...");

  // slight lift to avoid collision with lower block
  moveServo(elbowServo, 2, targetDrop[2] - 10);

  delay(200);

  // open gripper
  moveServo(gripper, 5, targetDrop[5]);

  delay(500);
  blockCount++;
  if (blockCount >= 2) blockCount = 0;

  Serial.println("Returning home...");

  // ---- RETURN TO START ----
  // ---- RETURN TO START (SAFE SEQUENCE) ----

  // 1. FIRST: Lift arm vertically (shoulder + elbow)
  moveServo(shoulderServo, 1, 5);
  moveServo(elbowServo,    2, 90);

  delay(300);

  // 2. THEN: Rotate base
  moveServo(baseServo, 0, 90);

  delay(300);

  // 3. THEN: Fix wrist + roll
  moveServo(wristPitch, 3, 90);
  moveServo(wristRoll,  4, 90);

  delay(300);

  // 4. FINALLY: ensure gripper open
  moveServo(gripper, 5, 180);

  Serial.println("Done.");
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

  Serial.println("System Ready.");
}

// -------- LOOP --------
void loop() {

  int distance = getDistance();

  Serial.print("Distance: ");
  Serial.println(distance);

  if (distance > 0 && distance < thresholdDistance && !actionInProgress) {

    actionInProgress = true;

    Serial.println("Object detected!");

    motorStop();

    performArmAction();

    delay(500);

    Serial.println("Resuming conveyor...");
    actionInProgress = false;
  }

  if (!actionInProgress) {
    motorRun();
  }

  delay(100);
}
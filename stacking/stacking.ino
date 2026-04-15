#include <Servo.h>

// ======================================================
//   CONVEYOR + ROBOTIC ARM — STACKING VERSION
//   Flow: detect obj1 → pick → place at dropPos
//         detect obj2 → pick → place at dropPosStacked (higher)
//         reset to obj1 mode, repeat
// ======================================================

// -------- ULTRASONIC --------
const int TRIG_PIN = 9;
const int ECHO_PIN = 10;

// -------- L298N MOTOR --------
const int IN1 = 12;
const int IN2 = 13;
const int ENA = 11;

// -------- SETTINGS --------
const int   THRESHOLD_DIST   = 10;    // cm — trigger distance
const int   MOTOR_SPEED      = 255;
const int   STEP_DELAY       = 15;    // ms per servo step
const int   COOLDOWN_MS      = 2000;  // ms before re-triggering
const int   REQUIRED_DETECTS = 3;     // hits needed to confirm object
const int   WARMUP_CYCLES    = 20;

// -------- MOVING AVERAGE --------
const int   NUM_SAMPLES      = 3;

// -------- SERVOS --------
Servo baseServo, shoulderServo, elbowServo,
      wristPitch, wristRoll, gripper;

// -------- POSITIONS [base, shoulder, elbow, wristPitch, wristRoll, gripper] --------
int curpos[6]          = { 90,  90,  90,  90,  90, 180 };

// Pickup position — same for both objects (they arrive at same spot)
int pickupPos[6]       = { 80, 105, 180,  90,  90, 115 };

// Drop position for object 1 — base level
int dropPos[6]         = {  0,  65, 180, 130,  90, 180 };

// Drop position for object 2 — higher (shoulder up, elbow less bent, wrist adjusted)
// TUNE THESE VALUES to match how high your stacked object needs to be placed
int dropPosStacked[6]  = {  0,  50, 155, 110,  90, 180 };

// -------- STATE --------
bool          actionInProgress = false;
unsigned long lastActionTime   = 0;
int           warmupCycles     = WARMUP_CYCLES;
int           detectionCount   = 0;

// -------- STACKING STATE --------
// 0 = waiting for object 1, 1 = waiting for object 2
int stackLayer = 0;

// -------- MOVING AVERAGE STATE --------
int  maReadings[NUM_SAMPLES];
int  maIndex  = 0;
long maTotal  = 0;
int  maFilled = 0;

// ======================================================
//   DISTANCE
// ======================================================
int getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(4);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 6000UL);

  if (duration == 0) return -1;

  int dist = (int)(duration * 0.034f / 2.0f);

  if (dist < 2 || dist > 400) return -1;

  maTotal -= maReadings[maIndex];
  maReadings[maIndex] = dist;
  maTotal += dist;
  maIndex = (maIndex + 1) % NUM_SAMPLES;

  if (maFilled < NUM_SAMPLES) maFilled++;

  return (maFilled >= NUM_SAMPLES) ? (int)(maTotal / NUM_SAMPLES) : dist;
}

// ======================================================
//   MOTOR
// ======================================================
void motorRun() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, MOTOR_SPEED);
}

void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 0);
}

// ======================================================
//   SERVO — standard linear move
// ======================================================
void moveServo(Servo &s, int idx, int target) {
  target = constrain(target, 0, 180);
  while (curpos[idx] < target) {
    curpos[idx]++;
    s.write(curpos[idx]);
    delay(STEP_DELAY);
  }
  while (curpos[idx] > target) {
    curpos[idx]--;
    s.write(curpos[idx]);
    delay(STEP_DELAY);
  }
}

// ======================================================
//   SERVO — smooth eased move (for shoulder)
// ======================================================
void moveServoSmooth(Servo &s, int idx, int target) {
  target = constrain(target, 0, 180);
  int diff = abs(target - curpos[idx]);
  if (diff == 0) return;

  for (int step = 0; step < diff; step++) {
    if (curpos[idx] < target) curpos[idx]++;
    else                       curpos[idx]--;

    s.write(curpos[idx]);

    float progress = (float)step / (float)diff;
    int d = (progress < 0.2f || progress > 0.8f) ? 30 : 10;
    delay(d);
  }
}

// ======================================================
//   PICK — shared pickup sequence (same for both objects)
// ======================================================
void pickObject() {
  Serial.println("[ARM] Moving to pickup...");

  moveServo(shoulderServo, 1, pickupPos[1]);
  moveServo(elbowServo,    2, pickupPos[2]);
  moveServo(wristPitch,    3, pickupPos[3]);
  delay(300);

  Serial.println("[ARM] Closing gripper...");
  moveServo(gripper, 5, pickupPos[5]);
  delay(300);

  Serial.println("[ARM] Lifting...");
  moveServo(elbowServo, 2, 150);
  delay(300);
}

// ======================================================
//   DROP — places at given position array
// ======================================================
void dropObject(int pos[6]) {
  Serial.println("[ARM] Moving to drop...");

  moveServo(wristPitch, 3, pos[3]);               // wrist first — less jerk
  moveServo(baseServo,  0, pos[0]);
  moveServoSmooth(shoulderServo, 1, pos[1]);
  moveServo(elbowServo, 2, pos[2]);
  delay(300);

  Serial.println("[ARM] Releasing...");
  moveServo(gripper, 5, pos[5]);
  delay(500);
}

// ======================================================
//   RETURN HOME
// ======================================================
void returnHome() {
  Serial.println("[ARM] Returning home...");

  moveServo(shoulderServo, 1, 90);
  moveServo(elbowServo,    2, 90);
  delay(300);

  moveServo(baseServo, 0, 90);
  delay(300);

  moveServo(wristPitch, 3, 90);
  moveServo(wristRoll,  4, 90);
  delay(300);

  moveServo(gripper, 5, 180);

  Serial.println("[ARM] Home.");
}

// ======================================================
//   ARM ACTION — picks and drops based on current layer
// ======================================================
void performArmAction() {
  pickObject();

  if (stackLayer == 0) {
    Serial.println("[STACK] Placing object 1 (base layer)...");
    dropObject(dropPos);
  } else {
    Serial.println("[STACK] Placing object 2 (stacked on top)...");
    dropObject(dropPosStacked);
  }

  returnHome();

  // Advance layer: 0 → 1 → back to 0
  stackLayer = (stackLayer + 1) % 2;

  Serial.print("[STACK] Next layer: ");
  Serial.println(stackLayer);
}

// ======================================================
//   SETUP
// ======================================================
void setup() {
  Serial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  for (int i = 0; i < NUM_SAMPLES; i++) maReadings[i] = 0;

  // Write before attach — prevents startup jerk
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

  Serial.println("[SYS] System Ready. Waiting for object 1...");
}

// ======================================================
//   LOOP
// ======================================================
void loop() {

  if (warmupCycles > 0) {
    warmupCycles--;
    delay(100);
    return;
  }

  int distance = getDistance();

  // Debug — comment out after tuning
  Serial.print("[SENSOR] dist=");
  Serial.print(distance);
  Serial.print("  count=");
  Serial.print(detectionCount);
  Serial.print("  layer=");
  Serial.println(stackLayer);

  if (distance != -1 && distance < THRESHOLD_DIST) {
    detectionCount++;
  } else {
    detectionCount = 0;
  }

  if (detectionCount >= REQUIRED_DETECTS
      && !actionInProgress
      && (millis() - lastActionTime) > COOLDOWN_MS) {

    actionInProgress = true;
    detectionCount   = 0;

    Serial.print("[SYS] Object detected! Layer=");
    Serial.println(stackLayer);

    motorStop();

    performArmAction();

    lastActionTime = millis();
    delay(500);

    Serial.println("[SYS] Resuming conveyor...");
    actionInProgress = false;
  }

  if (!actionInProgress) {
    motorRun();
  }

  // No delay — fast loop for quick detection
}

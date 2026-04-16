#include <Servo.h>

// -------- ULTRASONIC --------
const int trigPin = 9;
const int echoPin = 10;

// -------- L298N --------
const int IN1 = 12;
const int IN2 = 13;
const int ENA = 11;

// -------- HARDWARE SETTINGS --------
const int thresholdDistance = 10;      // Trigger distance in cm
const int motorSpeed        = 255;
const int stepDelay         = 15;      // ms per standard servo step
const int COOLDOWN_MS       = 2000;    // ms before re-triggering
const int REQUIRED_DETECTS  = 3;       // Hits needed to confirm object
const int WARMUP_CYCLES     = 20;

// -------- SERVOS --------
Servo baseServo, shoulderServo, elbowServo, 
    wristPitch, wristRoll, gripper;

// -------- POSITIONS --------
int curpos[6]    = {90,  90,  90,  90,  90, 180};
int pickupPos[6] = {80, 105, 180,  90,  90, 115};
int dropPos[6]   = { 0,  65, 180, 130,  90, 180}; // Base layer
int dropPos2[6]  = { 0,  68, 175, 119,  90, 180}; // Layer 2 (Stacked)

// -------- SYSTEM STATE --------
bool actionInProgress = false;
bool waitingForVision = false; 
int targetLevel       = 1;  
int warmupCycles      = WARMUP_CYCLES;
int detectionCount    = 0;
unsigned long lastActionTime = 0;

// -------- MOVING AVERAGE --------
const int NUM_SAMPLES = 3;
int maReadings[NUM_SAMPLES] = {0};
int maIndex  = 0;
long maTotal = 0;
int maFilled = 0;

// ======================================================
//   DISTANCE SENSOR
// ======================================================
int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(4);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 6000UL); // Timeout faster

  if (duration == 0) return -1;

  int dist = (int)(duration * 0.034f / 2.0f);
  if (dist < 2 || dist > 400) return -1;

  // Moving average calculation
  maTotal -= maReadings[maIndex];
  maReadings[maIndex] = dist;
  maTotal += dist;
  maIndex = (maIndex + 1) % NUM_SAMPLES;

  if (maFilled < NUM_SAMPLES) maFilled++;

  return (maFilled >= NUM_SAMPLES) ? (int)(maTotal / NUM_SAMPLES) : dist;
}

// ======================================================
//   MOTOR CONTROL
// ======================================================
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

// ======================================================
//   SERVO KINEMATICS
// ======================================================
void moveServo(Servo &s, int i, int target) {
  target = constrain(target, 0, 180);
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

// Smooth eased move (prevents jerking stacked items)
void moveServoSmooth(Servo &s, int idx, int target) {
  target = constrain(target, 0, 180);
  int diff = abs(target - curpos[idx]);
  if (diff == 0) return;

  for (int step = 0; step < diff; step++) {
    if (curpos[idx] < target) curpos[idx]++;
    else                      curpos[idx]--;

    s.write(curpos[idx]);

    float progress = (float)step / (float)diff;
    int d = (progress < 0.2f || progress > 0.8f) ? 30 : 10;
    delay(d);
  }
}

// ======================================================
//   ARM ACTION SEQUENCE
// ======================================================
void performArmAction() {
  Serial.println("ACTION: Picking object...");

  moveServo(shoulderServo, 1, pickupPos[1]);
  moveServo(elbowServo,    2, pickupPos[2]);
  moveServo(wristPitch,    3, pickupPos[3]);
  delay(300);

  Serial.println("ACTION: Closing gripper...");
  moveServo(gripper, 5, pickupPos[5]);
  delay(300);

  Serial.println("ACTION: Lifting...");
  moveServo(elbowServo, 2, 150);
  delay(300);

  Serial.print("ACTION: Moving to drop (Level ");
  Serial.print(targetLevel);
  Serial.println(")");

  int *targetDrop;
  if (targetLevel == 1) {
    targetDrop = dropPos;
  } else {
    targetDrop = dropPos2;
  }

  // Drop sequence with smooth shoulder motion
  moveServo(wristPitch,    3, targetDrop[3]);
  moveServo(baseServo,     0, targetDrop[0]);
  moveServoSmooth(shoulderServo, 1, targetDrop[1]); 
  moveServo(elbowServo,    2, targetDrop[2]);
  delay(300);

  Serial.println("ACTION: Dropping...");
  moveServo(gripper, 5, targetDrop[5]);
  delay(500);

  Serial.println("ACTION: Returning home...");

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
}

// ======================================================
//   PYTHON SERIAL PARSER
// ======================================================
void processSerialCommand() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.startsWith("PICK")) {
      int colonIndex = command.indexOf(':');
      if (colonIndex != -1) {
        targetLevel = command.substring(colonIndex + 1).toInt();
        if (targetLevel < 1) targetLevel = 1;
        if (targetLevel > 2) targetLevel = 2;
      } else {
        targetLevel = 1; 
      }

      actionInProgress = true;
      Serial.print("COMMAND: Received PICK for Level ");
      Serial.println(targetLevel);
      
      performArmAction();
      
      Serial.println("COMMAND: Resuming conveyor...");
      waitingForVision = false; 
      actionInProgress = false;
      lastActionTime = millis(); // Reset cooldown
      
      motorRun();
    }
    else if (command == "RESUME") {
      Serial.println("COMMAND: Ignoring block, resuming conveyor...");
      waitingForVision = false; 
      lastActionTime = millis(); // Reset cooldown
      
      motorRun();
    }
  }
}

// ======================================================
//   SETUP
// ======================================================
void setup() {
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  // Pre-write to prevent snap
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

// ======================================================
//   LOOP
// ======================================================
void loop() {
  // 1. Boot Stabilization
  if (warmupCycles > 0) {
    warmupCycles--;
    delay(100);
    return;
  }

  // 2. Handle Python Commands
  processSerialCommand();

  // 3. Hardware Polling (Only if idle and not cooling down)
  if (!actionInProgress && !waitingForVision) {
    motorRun(); // Default state
    
    int distance = getDistance();
    
    // Check if object is within threshold
    if (distance != -1 && distance < thresholdDistance) {
      detectionCount++;
    } else {
      detectionCount = 0;
    }

    // Trigger vision check if confirmed and cooldown elapsed
    if (detectionCount >= REQUIRED_DETECTS && (millis() - lastActionTime) > COOLDOWN_MS) {
      motorStop(); // Tripwire hit!
      waitingForVision = true;
      detectionCount = 0; // Reset for next time
      
      delay(300); // Wait for the block to physically settle
      Serial.println("STATUS: WAITING"); // Hand control to Python
    }
  }

  // Fast loop for sensor polling
  delay(20);
}
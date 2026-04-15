#include <Servo.h>

Servo servos[6];

// current and target positions
int curpos[6] = {90, 90, 90, 90, 90, 180};
int tgtpos[6] = {90, 90, 90, 90, 90, 180};

// saved poses
int home[6], pick[6], dropPos[6];

// servo safety limits (adjust for your arm)
int minLimit[6] = {0, 20, 10, 0, 0, 60};
int maxLimit[6] = {180, 160, 170, 180, 180, 180};

int stepDelay = 15;
bool emergencyStop = false;

int servoAngle;

void moveAllSync() {

  bool moving = true;

  while (moving) {

    // emergency stop check
    if (Serial.available()) {
      String s = Serial.readStringUntil('\n');
      s.trim();
      if (s == "STOP") {
        Serial.println("EMERGENCY STOP");
        return;
      }
    }

    moving = false;

    for (int i = 0; i < 6; i++) {

      int diff = tgtpos[i] - curpos[i];
      int step = max(1, abs(diff) / 20);   // smooth acceleration

      if (curpos[i] < tgtpos[i]) {
        curpos[i] += step;
        moving = true;
      }
      else if (curpos[i] > tgtpos[i]) {
        curpos[i] -= step;
        moving = true;
      }

      // safety clamp
      curpos[i] = constrain(curpos[i], minLimit[i], maxLimit[i]);
    }

    // write all servos
    for (int i = 0; i < 6; i++) {
      servos[i].write(curpos[i]);
    }

    delay(stepDelay);
  }
}

void savePose(int pose[]) {
  for (int i = 0; i < 6; i++)
    pose[i] = curpos[i];
}

void loadPose(int pose[]) {
  for (int i = 0; i < 6; i++)
    tgtpos[i] = pose[i];

  moveAllSync();
}

void printCurrent() {
  Serial.print("Current: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(curpos[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void pickAndPlace() {

  Serial.println("Running Pick & Place...");

  loadPose(home);
  delay(500);

  loadPose(pick);
  delay(500);

  // close gripper
  tgtpos[5] = 60;
  moveAllSync();
  delay(500);

  loadPose(home);
  delay(500);

  loadPose(dropPos);
  delay(500);

  // open gripper
  tgtpos[5] = 180;
  moveAllSync();
  delay(500);

  loadPose(home);

  Serial.println("Done.");
}

void setup() {

  Serial.begin(9600);

  servos[0].attach(2); // base
  servos[1].attach(3); // shoulder
  servos[2].attach(4); // elbow
  servos[3].attach(5); // wrist pitch
  servos[4].attach(6); // wrist roll
  servos[5].attach(7); // gripper

  // move to startup position
  moveAllSync();

  Serial.println("=== 5DOF ARM CONTROL ===");
  Serial.println("Commands:");
  Serial.println("B90  -> Base");
  Serial.println("S90  -> Shoulder");
  Serial.println("E90  -> Elbow");
  Serial.println("W90  -> Wrist pitch");
  Serial.println("R90  -> Wrist roll");
  Serial.println("G90  -> Gripper");
  Serial.println("H -> save HOME");
  Serial.println("P -> save PICK");
  Serial.println("D -> save DROP");
  Serial.println("X -> RUN pick & place");
  Serial.println("C -> print current");
  Serial.println("T10 -> speed");
  Serial.println("STOP -> emergency stop");
}

void loop() {

  if (Serial.available() == 0) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  // ===== SPEED CONTROL =====
  if (cmd.startsWith("T")) {
    stepDelay = cmd.substring(1).toInt();
    Serial.print("Speed set: ");
    Serial.println(stepDelay);
    return;
  }

  // ===== SAVE POSES =====
  if (cmd == "H") {
    savePose(home);
    Serial.println("HOME saved");
    return;
  }

  if (cmd == "P") {
    savePose(pick);
    Serial.println("PICK saved");
    return;
  }

  if (cmd == "D") {
    savePose(dropPos);
    Serial.println("DROP saved");
    return;
  }

  // ===== RUN =====
  if (cmd == "X") {
    pickAndPlace();
    return;
  }

  // ===== PRINT =====
  if (cmd == "C") {
    printCurrent();
    return;
  }

  // ===== MANUAL CONTROL =====
  char motorID = cmd.charAt(0);
  servoAngle = cmd.substring(1).toInt();

  if (servoAngle >= 0 && servoAngle <= 180) {

    switch (motorID) {

      case 'B': case 'b':
        tgtpos[0] = servoAngle;
        break;

      case 'S': case 's':
        tgtpos[1] = servoAngle;
        break;

      case 'E': case 'e':
        tgtpos[2] = servoAngle;
        break;

      case 'W': case 'w':
        tgtpos[3] = servoAngle;
        break;

      case 'R': case 'r':
        tgtpos[4] = servoAngle;
        break;

      case 'G': case 'g':
        tgtpos[5] = servoAngle;
        break;

      default:
        Serial.println("Invalid motor");
        return;
    }

    moveAllSync();
    printCurrent();
  }
  else {
    Serial.println("Angle must be 0-180");
  }
}
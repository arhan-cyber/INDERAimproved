#include <Servo.h>

Servo baseServo, shoulderServo, elbowServo, 
      wristPitch, wristRoll, gripper;

int curpos[6] = {90,180 , 90, 90, 90, 100};
int tgtpos[6] = {90, 180, 90, 90, 90, 100};

const int stepSize = 1;
const int stepDelay = 15;

int servoAngle;   // ✅ DECLARED

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

void setup() {
  Serial.begin(9600);

  baseServo.attach(2);
  shoulderServo.attach(3);
  elbowServo.attach(4);
  wristPitch.attach(5);
  wristRoll.attach(6);
  gripper.attach(7);

  Serial.println("Homing in progress...");

  /*for (int i = 0; i < 6; i++) {
    smoothMove(
      (i==0)?baseServo:
      (i==1)?shoulderServo:
      (i==2)?elbowServo:
      (i==3)?wristPitch:
      (i==4)?wristRoll:
              gripper, i);
    delay(200);
  }*/

  Serial.println("Homing Complete");
}

void loop() {

  Serial.println("Enter MotorID and angle (e.g., B 120):");

  while (Serial.available() == 0) {}

  String cmd = Serial.readStringUntil('\n');

  char motorID = cmd.charAt(0);
  servoAngle = cmd.substring(2).toInt();

  Serial.print("Motor: ");
  Serial.print(motorID);
  Serial.print(" Angle: ");
  Serial.println(servoAngle);

  if (servoAngle >= 0 && servoAngle <= 180) {

    switch(motorID){

      case 'b':
      case 'B':
        tgtpos[0] = servoAngle;
        smoothMove(baseServo, 0);
        break;

      case 's':
      case 'S':
        tgtpos[1] = servoAngle;
        smoothMove(shoulderServo, 1);
        break;

      case 'e':
      case 'E':
        tgtpos[2] = servoAngle;
        smoothMove(elbowServo, 2);
        break;

      case 'w':
      case 'W':
        tgtpos[3] = servoAngle;
        smoothMove(wristPitch, 3);
        break;

      case 'r':
      case 'R':
        tgtpos[4] = servoAngle;
        smoothMove(wristRoll, 4);
        break;

      case 'g':
      case 'G':
        tgtpos[5] = servoAngle;
        smoothMove(gripper, 5);
        break;

      default:
        Serial.println("Invalid Motor ID.");
        return;
    }

    Serial.println("Position updated.");
  } 
  else {
    Serial.println("Error: Angle must be 0-180.");
  }
}
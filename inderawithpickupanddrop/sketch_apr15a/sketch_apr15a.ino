#include <Servo.h>

Servo servos[6];
int pins[6] = {2, 3, 4, 5, 6, 7};
String servoNames[6] = {"Base", "Shoulder", "Elbow", "WristPitch", "WristRoll", "Gripper"};
int currentAngles[6] = {90, 90, 90, 90, 90, 140}; // Safe starting positions

// -------- SPEED SETTINGS --------
const int stepSize = 1;
const int stepDelay = 20; // Increase this number to make the arm move SLOWER

// -------- SMOOTH MOVE FUNCTION --------
void smoothMove(int index, int targetAngle) {
  while (currentAngles[index] < targetAngle) {
    currentAngles[index] += stepSize;
    servos[index].write(currentAngles[index]);
    delay(stepDelay);
  }
  while (currentAngles[index] > targetAngle) {
    currentAngles[index] -= stepSize;
    servos[index].write(currentAngles[index]);
    delay(stepDelay);
  }
}

void setup() {
  Serial.begin(9600);
  
  // Initialize perfectly to prevent startup snap
  for (int i = 0; i < 6; i++) {
    servos[i].write(currentAngles[i]);
    servos[i].attach(pins[i]);
  }
  
  Serial.println("Smooth Calibration Ready.");
  Serial.println("Format: [Letter] [Angle]");
  Serial.println("Example: G 150 (Moves Gripper to 150)");
  Serial.println("B:Base, S:Shoulder, E:Elbow, P:Pitch, R:Roll, G:Gripper");
}

void loop() {
  if (Serial.available() > 0) {
    
    // 1. Read the first character (The Letter)
    char cmd = Serial.read();
    
    // Ignore stray spaces or newlines in the serial buffer
    if (cmd == ' ' || cmd == '\n' || cmd == '\r') {
      return; 
    }

    // 2. Read the number that comes after the letter
    int targetAngle = Serial.parseInt();

    // 3. Convert the letter to uppercase (so 'g' and 'G' both work)
    cmd = toupper(cmd);

    // 4. Map the letter back to your original 0-5 index
    int servoIndex = -1;
    if (cmd == 'B') servoIndex = 0;      // Base
    else if (cmd == 'S') servoIndex = 1; // Shoulder
    else if (cmd == 'E') servoIndex = 2; // Elbow
    else if (cmd == 'P') servoIndex = 3; // Pitch
    else if (cmd == 'R') servoIndex = 4; // Roll
    else if (cmd == 'G') servoIndex = 5; // Gripper

    // 5. Execute if the inputs are valid
    if (servoIndex != -1 && targetAngle >= 0 && targetAngle <= 180) {
      
      Serial.print("Moving ");
      Serial.print(servoNames[servoIndex]);
      Serial.print(" to ");
      Serial.print(targetAngle);
      Serial.println("...");

      // Call the smooth move function
      smoothMove(servoIndex, targetAngle); 
      
      Serial.println("Done.\n");
      
    } else if (servoIndex == -1) {
      Serial.println("Invalid letter! Use B, S, E, P, R, or G.");
    } else {
      Serial.println("Invalid angle! Must be between 0 and 180.");
    }
  }
}
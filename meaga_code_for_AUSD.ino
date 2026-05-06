/*
 * PROJECT NEPTUNE-AI: Control Layer
 * Board: Arduino Mega 2560
 * Features: Single Thruster ESC, Ultrasonic Safety, Hybrid RC/AI Control
 */

#include <Servo.h>

// ==========================================
// 1. PIN DEFINITIONS
// ==========================================
const int escMainPin = 2;       // Single ESC (PWM)
const int trigPin = 7;          // Ultrasonic Trigger
const int echoPin = 8;          // Ultrasonic Echo
const int rcThrottlePin = 10;   // RC Receiver CH1 (Joystick Forward/Reverse)
const int rcModePin = 11;       // RC Receiver CH5 (Mode Toggle Switch)

// ==========================================
// 2. OBJECTS & VARIABLES
// ==========================================
Servo escMain;

long duration;
int distance_cm;
char aiCommand = 'S';           // Default to Stop
int rcThrottlePWM = 1500;       // Default neutral
int rcModePWM = 1500;           // Default neutral
bool isManualMode = false;      // Default to Auto

void setup() {
  // --- Initialize Serial Communication ---
  Serial.begin(115200);   // To PC Monitor (for debugging)
  Serial1.begin(115200);  // RX1/TX1 to Raspberry Pi 5
  Serial.println("NEPTUNE-AI: Control Layer Initializing...");

  // --- Initialize Pins ---
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(rcThrottlePin, INPUT);
  pinMode(rcModePin, INPUT);

  // --- Initialize ESC ---
  escMain.attach(escMainPin, 1000, 2000);
  escMain.writeMicroseconds(1500); // Send arming signal (Neutral)
  
  Serial.println("Arming ESC... Please wait 3 seconds.");
  delay(3000); // Wait for the ESC initialization beeps
  Serial.println("System Armed and Ready!");
}

void loop() {
  // ==========================================
  // A. READ SENSORS (Ultrasonic)
  // ==========================================
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout
  distance_cm = (duration == 0) ? 999 : (duration * 0.034 / 2);

  // ==========================================
  // B. READ RC RECEIVER
  // ==========================================
  rcModePWM = pulseIn(rcModePin, HIGH, 25000);
  rcThrottlePWM = pulseIn(rcThrottlePin, HIGH, 25000);

  // Determine Mode: Switch > 1500 is Manual, otherwise Auto
  isManualMode = (rcModePWM > 1500);

  // ==========================================
  // C. READ AI COMMANDS (From Raspberry Pi)
  // ==========================================
  if (Serial1.available() > 0) {
    aiCommand = Serial1.read();
  }

  // ==========================================
  // D. SEND TELEMETRY (To Raspberry Pi)
  // ==========================================
  // Format sent to Pi: "D:45,M:AUTO"
  Serial1.print("D:");
  Serial1.print(distance_cm);
  Serial1.print(",M:");
  Serial1.println(isManualMode ? "MANUAL" : "AUTO");

  // ==========================================
  // E. DETERMINE TARGET SPEED
  // ==========================================
  int targetPWM = 1500; // Default is absolute stop

  if (isManualMode) {
    // --- MANUAL MODE (RC Joystick) ---
    if (rcThrottlePWM > 900) { // Check if remote is actually turned on
      // Apply Deadband: Ignore small stick drifts between 1470 and 1510
      if (rcThrottlePWM > 1470 && rcThrottlePWM < 1510) {
        targetPWM = 1500; // Force exact stop
      } else {
        targetPWM = rcThrottlePWM; // Pass joystick directly to ESC
      }
    }
  } else {
    // --- AUTO MODE (AI Commands) ---
    switch (aiCommand) {
      case 'F': targetPWM = 1600; break; // Forward speed
      case 'B': targetPWM = 1400; break; // Reverse speed
      case 'S': 
      default: targetPWM = 1500; break;  // Stop
    }
  }

  // ==========================================
  // F. HARDWARE SAFETY OVERRIDE
  // ==========================================
  // If an object is closer than 20cm AND we are trying to drive Forward (> 1500)
  if (distance_cm < 20 && targetPWM > 1500) {
    targetPWM = 1500; // FORCE STOP
    Serial.println("COLLISION ALERT: Forward Motion Blocked!");
  }

  // ==========================================
  // G. EXECUTE MOVEMENT
  // ==========================================
  escMain.writeMicroseconds(targetPWM);
  
  delay(30); // Loop stability delay
}
#include <Alfredo_NoU2.h>
#include <AlfredoConnect.h>
#include <BluetoothSerial.h>
#include <FastLED.h>

#define FASTLED_ALLOW_INTERRUPTS 0 //FASTLED SERIAL INTERRUPTS ALLOWED TO ZERO
#define NUM_LEDS 16 //NUM OF LEDS IN CHAIN
#define DATA_PIN 5 //DATA PIN 5 (GPIO D5)
CRGB leds[NUM_LEDS]; 

//GAME STATE ENUM
  enum GameState {
    NONE, 
    CUBE, 
    CONE
  } gamePiece;

//ARM SETPOINTS ENUM
 enum ArmSetpoints {  //(JOINT SERVO, WRIST SERVO)
  HIGH_JOINT = 60,
  HIGH_WRIST = 180,
  MID_JOINT = 55,
  MID_WRIST = 150,
  SUBSTATION_JOINT = 50,
  SUBSTATION_WRIST = 120,
  STOW_JOINT = 140,
  STOW_WRIST = 0,
  GROUND_INTAKE_JOINT = 5,
  GROUND_INTAKE_WRIST = 150,
  UNFLIP_ROBOT_JOINT = 180,
  UNFLIP_ROBOT_WRIST = 0
} jointSetpoint,
  wristSetpoint;

//INTAKE SETPOINTS ENUM
 enum IntakeSetpoints {  // (CLOSED POSITION, OPEN POSITION)
  CONE_CLOSED = 0,
  CONE_OPEN = 30,
  CUBE_CLOSED = 20,
  CUBE_OPEN = 60
} intakeSetpoint;

//MOTORS AND SERVOS
NoU_Motor DriveFR(6);
NoU_Motor DriveFL(5);
NoU_Motor DriveRR(1);
NoU_Motor DriveRL(2);
NoU_Motor ArmPinion(4);
NoU_Servo JointServo(1);
NoU_Servo WristServo(2);
NoU_Servo GrabberServo(3);

//BLUETOOTH
BluetoothSerial bluetooth;

//BUTTON TOGGLES
boolean intakeToggle = false;
boolean intakeButton = false;
boolean gamePieceToggle = false;
boolean coneModeButton = false;
boolean auto1Button = false;
boolean auto2Button = false;
boolean auto3Button = false;
unsigned long previousMillis = 0;
int LEDCounter = 0;

//DEADBAND AND INPUT CURVE
//double controllerDeadband = -0.0650; 

void setup() {
  //GAME PIECE INIT TO NONE
  gamePiece = NONE;
  //LEDs
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.setBrightness(30);
  FastLED.clear();
  FastLED.show();
  //Bluetooth serial opening
  bluetooth.begin("MidTown Robotics");
  AlfredoConnect.begin(bluetooth);
  bluetooth.println("MIDTOWN ROBOTICS #40");
  bluetooth.println("BLUETOOTH CONNECTED");
}

//SET LEDs BASED ON GAME STATE (CONE / CUBE)
//Drives LEDS outside of loop due to incorrect FastLED interrupt timing
void setLEDS() {
    if(LEDCounter > 7) {
      LEDCounter = 0;
      for(int i = 0; i < 16; i++) {
        leds[i] = CRGB::Black;
      }
    }
    if(gamePiece == CONE) {
      leds[LEDCounter] = CRGB::Yellow;
      leds[LEDCounter + 8] = CRGB::Yellow;
    }
    else if(gamePiece == CUBE) {
      leds[LEDCounter] = CRGB::Purple;
      leds[LEDCounter + 8] = CRGB::Purple;
    }
    else if(gamePiece == NONE) {
      leds[LEDCounter] = CRGB::Blue;
      leds[LEDCounter + 8] = CRGB::Blue;
    }
    if(LEDCounter > 0 && (LEDCounter + 8) > 8) {
      leds[LEDCounter -1] = CRGB::Black;
      leds[LEDCounter + 7] = CRGB::Black;
    }
  LEDCounter++;
  FastLED.show();
}

//Drives LEDS outside of loop() due to incorrect FastLED interrupt timing
void setLEDSColor(CRGB color) {
  for (int i = 0; i < 16; i++) {
    leds[i] = color;
  }
  FastLED.show();
}
//Sets drive motor speeds based on axes
void setDrivetrainMotors(float speed, float turn, float strafe) {
  float frontRight = (speed - turn - strafe);
  float frontLeft = (speed + turn + strafe);
  float rearRight = (speed - turn + strafe);
  float rearLeft = (speed + turn - strafe);

  DriveFR.set(frontRight);
  DriveFL.set(-frontLeft);
  DriveRR.set(rearRight);
  DriveRL.set(-rearLeft);
  bluetooth.println("#############################################");
  bluetooth.println("DRIVETRAIN");
  bluetooth.print("FRONT RIGHT DRIVE: "); bluetooth.println(frontRight);
  bluetooth.print("FRONT LEFT DRIVE: ");  bluetooth.println(frontLeft); 
  bluetooth.print("REAR RIGHT DRIVE: ");  bluetooth.println(rearRight);
  bluetooth.print("REAR LEFT DRIVE: ");   bluetooth.println(rearLeft);
  bluetooth.println("#############################################");
}

void setRackAndPinion(float rackPinionSpeed) {
  ArmPinion.set(rackPinionSpeed);
  bluetooth.println("#############################################");
  bluetooth.print("RACK AND PINION: "); bluetooth.println(rackPinionSpeed);
  bluetooth.println("#############################################");
}

void setIntakePosition(IntakeSetpoints intakeSetpoint) {
  GrabberServo.write(intakeSetpoint);
  bluetooth.println("#############################################");
  bluetooth.print("INTAKE POSITION: "); bluetooth.println(intakeSetpoint);
  bluetooth.println("#############################################");
}

void setArmPosition(ArmSetpoints jointSetpoint, ArmSetpoints wristSetpoint) {
    JointServo.write(jointSetpoint);
    WristServo.write(wristSetpoint);
    bluetooth.println("#############################################");
    bluetooth.println("ARM");
    bluetooth.print("JOINT POSITION: "); bluetooth.println(jointSetpoint);
    bluetooth.print("WRIST POSITION: "); bluetooth.println(wristSetpoint);
    bluetooth.println("#############################################");
}

// | L2 | - Switch Game Piece Mode
void setGameState(boolean isSwitchButtonPressed) {
  if (isSwitchButtonPressed && !coneModeButton) {
    gamePieceToggle = !gamePieceToggle;
    if(gamePiece == CUBE || gamePiece == NONE) {
      gamePiece = CONE;
    }
    else {
      gamePiece = CUBE;
    }
  }
  
  coneModeButton = isSwitchButtonPressed;
}

void autonomousSequence() {
    // | NUMPAD1 | - Score High Cube, Leave Comm Auto
  boolean auto1KeyPressed = AlfredoConnect.keyHeld(Key::Numpad1);
  if(auto1KeyPressed && !auto1Button) {
        setArmPosition(HIGH_JOINT, HIGH_WRIST);
        delay(800);
        setLEDSColor(CRGB::Green);
        setRackAndPinion(-1);
        delay(1000);
        setRackAndPinion(0);
        setDrivetrainMotors(1, 0, 0);
        setLEDSColor(CRGB::White);
        delay(200);
        setIntakePosition(CUBE_OPEN);
        setDrivetrainMotors(0, 0, 0);
        delay(400);
        setLEDSColor(CRGB::Green);
        setRackAndPinion(1);
        delay(600);
        setRackAndPinion(0);
        setArmPosition(STOW_JOINT, STOW_WRIST);
        delay(300);
        setLEDSColor(CRGB::White);
        setDrivetrainMotors(-1, 0, 0);
        delay(1080);
        setDrivetrainMotors(0, 0, 0);
  }
  auto1Button = auto1KeyPressed;

// | NUMPAD2 | - Score Cone, Balance Charge Station
  boolean auto2KeyPressed = AlfredoConnect.keyHeld(Key::Numpad2);
  if(auto2KeyPressed && !auto2Button) {
        setArmPosition(HIGH_JOINT, HIGH_WRIST);
        delay(800);
        setLEDSColor(CRGB::Green);
        setRackAndPinion(-1);
        delay(1000);
        setRackAndPinion(0);
        setDrivetrainMotors(1, 0, 0);
        setLEDSColor(CRGB::White);
        delay(200);
        setIntakePosition(CUBE_OPEN);
        setDrivetrainMotors(0, 0, 0);
        delay(400);
        setLEDSColor(CRGB::Green);
        setRackAndPinion(1);
        delay(600);
        setRackAndPinion(0);
        setArmPosition(STOW_JOINT, STOW_WRIST);
        delay(300);
        setLEDSColor(CRGB::White);
        setDrivetrainMotors(-1, 0, 0);
        delay(700);
        setDrivetrainMotors(0, 0, 0);
  }
  auto2Button = auto2KeyPressed;

// | NUMPAD3 | - Score High Cube, Leave Comm Auto
  boolean auto3KeyPressed = AlfredoConnect.keyHeld(Key::Numpad3);
  if(auto3KeyPressed && !auto3Button) {
        setArmPosition(HIGH_JOINT, HIGH_WRIST);
        delay(800);
        setLEDSColor(CRGB::Green);
        setRackAndPinion(-1);
        delay(1000);
        ArmPinion.set(0);
        setRackAndPinion(0);
        setDrivetrainMotors(1, 0, 0);
        setLEDSColor(CRGB::White);
        delay(200);
        setIntakePosition(CONE_OPEN);
        setDrivetrainMotors(0, 0, 0);
        delay(400);
        setLEDSColor(CRGB::Green);
        setRackAndPinion(1);
        delay(600);
        setRackAndPinion(0);
        setArmPosition(STOW_JOINT, STOW_WRIST);
        delay(300);
        setLEDSColor(CRGB::White);
        setDrivetrainMotors(-1, 0, 0);
        delay(950);
        setDrivetrainMotors(0, 0, 0);
  }
  auto3Button = auto3KeyPressed;
}

void loop() {
  autonomousSequence();

  setGameState(AlfredoConnect.buttonHeld(0, 6));

  unsigned long currentMillis = millis();

  if(currentMillis - previousMillis >= 100) {
    previousMillis = currentMillis;
    setLEDS();
  }

//DRIVING
  float driveSpeed = -(AlfredoConnect.getAxis(0, 1));
  float driveTurn = (AlfredoConnect.getAxis(0, 2));
  float driveStrafe = (AlfredoConnect.getAxis(0, 0));
  //if(driveSpeed > -1*(controllerDeadband) && driveTurn > controllerDeadband && driveStrafe > controllerDeadband)
    setDrivetrainMotors(driveSpeed, driveTurn, driveStrafe);
  //else
   // setDrivetrainMotors(0, 0, 0);

  //RACK AND PINION
  if(AlfredoConnect.buttonHeld(0, 5)) // | R1 | - EXTEND
    setRackAndPinion(-1);
  else if(AlfredoConnect.buttonHeld(0, 4)) // | L1 | - RETRACT
    setRackAndPinion(1);
  else
    setRackAndPinion(0);
//ARM
  //| TRIANGLE | - High Arm Position
  if (AlfredoConnect.buttonHeld(0, 3)) {
    setArmPosition(HIGH_JOINT, HIGH_WRIST);
  }
  // | SQUARE | - Mid Arm Position
  else if (AlfredoConnect.buttonHeld(0, 2)) {
    setArmPosition(MID_JOINT, MID_WRIST);
  }
  // | X | - Ground Intake Arm Position
  else if (AlfredoConnect.buttonHeld(0, 0)) {
    setArmPosition(GROUND_INTAKE_JOINT, GROUND_INTAKE_WRIST);
  }
    // | CIRCLE | - Substation Arm Position
    else if (AlfredoConnect.buttonHeld(0, 1)) {
      setArmPosition(SUBSTATION_JOINT, SUBSTATION_WRIST);
    }
    // | SHARE | - Unflip Robot
    else if (AlfredoConnect.buttonHeld(0, 8)) {
      setArmPosition(UNFLIP_ROBOT_JOINT, UNFLIP_ROBOT_WRIST);
    }
    // | NO KEY | - STOW
    else {
      setArmPosition(STOW_JOINT, STOW_WRIST);
    }

    // Intake Controls
    // | R2 | - Intake Toggle
    boolean isIntakeKeyPressed = AlfredoConnect.buttonHeld(0, 7);
    if (isIntakeKeyPressed && !intakeButton) {
      intakeToggle = !intakeToggle;
      if (intakeToggle) { // OPEN INTAKE
        if (gamePieceToggle) { // CONE OPEN
          setIntakePosition(CONE_OPEN); 
        } else // CUBE OPEN
          setIntakePosition(CUBE_OPEN);
      }
      else { //CLOSED INTAKE
        if (gamePieceToggle) { // CONE CLOSED
          setIntakePosition(CONE_CLOSED);
        } else //CUBE CLOSED
         setIntakePosition(CUBE_CLOSED);
      }
    }
    intakeButton = isIntakeKeyPressed;

    AlfredoConnect.update();
  }
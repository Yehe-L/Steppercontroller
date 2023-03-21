#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AccelStepper.h>
#include <EEPROM.h>
#include <limits.h>

#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))
#define CPU_16MHz 0x00

// OLED display parameters
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

// Stepper motor control pins
#define ENABLE_PIN 24
#define STEP_PIN 12
#define DIR_PIN 13
#define BUTTON_PIN_1 19
#define BUTTON_PIN_2 20
#define BUTTON_PIN_3 21

// Stepper motor parameters
#define STEPS_PER_REVOLUTION 200

// EEPROM addresses
#define EEPROM_ABS_LOC_ADDR 0
#define EEPROM_HOME_LOC_ADDR 4
#define EEPROM_MIN_ADDR 8
#define EEPROM_MAX_ADDR 12
#define EEPROM_SPD_ADDR 16
#define EEPROM_ACC_ADDR 20

class Timer
{
public:
  Timer(unsigned long interval) : interval(interval), previousMillis(0) {}

  bool elapsed()
  {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
      previousMillis += interval;
      return true;
    }
    return false;
  }

  void reset()
  {
    previousMillis = millis();
  }

  void setInterval(unsigned long interval)
  {
    this->interval = interval;
  }

private:
  unsigned long interval;
  unsigned long previousMillis;
};

// Initialize stepper motor
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
Timer timer1(500), timer3(3000);

// Positions
long absMinPosition = -9999999;
long absMaxPosition = 9999999;
long distancePerStep = 1;
long homePosition = 0;
long refPosition = 0;
long speed = 32000000;
long acceleration = 32000000;

// Button states
bool button1State, button2State, button3State, OLEDState;

// Function prototypes
void updateButtons();
void blinkOnboardLED();
void processSerialCommands();
void displayData();
void setHome();
void setAbsMs();
bool inRange();
void eepWrite();
void eepRead();

void setup()
{
  CPU_PRESCALE(0x00);

  // Initialize buttons
  pinMode(BUTTON_PIN_1, INPUT_PULLUP);
  pinMode(BUTTON_PIN_2, INPUT_PULLUP);
  pinMode(BUTTON_PIN_3, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  eepRead();

  // Initialize serial communication
  Serial.begin(9600);

  // Initialize OLED display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  // Configure pins
  pinMode(ENABLE_PIN, OUTPUT);

  // Initialize stepper motor
  stepper.setSpeed(speed);
  stepper.setMaxSpeed(speed);
  stepper.setAcceleration(acceleration);
  stepper.setEnablePin(ENABLE_PIN);
  stepper.setPinsInverted(false, false, true); // Invert enable pin
  stepper.enableOutputs();

  // Set home position
  setHome();
}

void loop()
{
  processSerialCommands();
  if (timer1.elapsed())
  {
    displayData();
  }
  updateButtons();
  blinkOnboardLED();
}

void processSerialCommands()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    int valueIndex = command.indexOf(' ');

    String cmd = command;
    String valueStr = "";
    long value = 0;

    if (valueIndex != -1)
    {
      cmd = command.substring(0, valueIndex);
      valueStr = command.substring(valueIndex + 1);
      value = valueStr.toInt();
    }

    switch (cmd[0])
    {
    case 'F':
      stepper.move(value);
      if (inRange())
      {
        stepper.runToPosition();
      }
      break;
    case 'B':
      stepper.move(-value);
      if (inRange())
      {
        stepper.runToPosition();
      }
      break;
    case 'S':
      speed = value;
      stepper.setSpeed(value);
      break;
    case 'A':
      Serial.println(stepper.currentPosition());
      break;
    case 'R':
      Serial.println(stepper.currentPosition() * distancePerStep);
      break;
    case 'P':
      stepper.moveTo(value);
      if (inRange())
      {
        stepper.runToPosition();
      }
      break;
    case 'H':
      stepper.moveTo(homePosition);
      break;
    case 'C':
      setHome();
      break;
    case 'Z':
      stepper.setCurrentPosition(0);
      break;
    case 'M':
      speed = value;
      stepper.setMaxSpeed(value);
      break;
    case 'L':
      acceleration = value;
      stepper.setAcceleration(value);
      break;
    case 'D':
      distancePerStep = value;
      break;
    case 'X':
      absMaxPosition = value;
      break;
    case 'Y':
      absMinPosition = value;
      break;
    case 'E':
      eepWrite();
      break;
    case 'G':
      eepRead();
      break;
    case 'T':
      timer1.setInterval(value);
      break;
    default:
      Serial.println("Invalid command.");
      break;
    }
  }

  if (stepper.currentPosition() < absMinPosition)
  {
    stepper.moveTo(absMinPosition);
  }
  else if (stepper.currentPosition() > absMaxPosition)
  {
    stepper.moveTo(absMaxPosition);
  }

  stepper.run();
  refPosition = stepper.currentPosition() - homePosition;
}

void displayData()
{
  if (!stepper.isRunning())
  {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);

    display.print("Ref:");
    display.println((stepper.currentPosition() - homePosition) * distancePerStep);

    display.print("Abs:");
    display.println(stepper.currentPosition() * distancePerStep);

    display.print("Tar:");
    display.println((stepper.targetPosition() - homePosition) * distancePerStep);

    display.print("-:");
    display.print(absMinPosition * distancePerStep);
    display.print(", +:");
    display.println(absMaxPosition * distancePerStep);

    display.display();
  }
}

void setHome()
{
  homePosition = stepper.currentPosition();
}

void updateButtons()
{
  static Timer timerB(3000);
  button1State = !digitalRead(BUTTON_PIN_1);
  button2State = !digitalRead(BUTTON_PIN_2);
  button3State = !digitalRead(BUTTON_PIN_3);

  if (button1State && button3State)
  {
    // If both buttons are pressed together for 3 seconds
    if (timerB.elapsed())
    {
      eepWrite();
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println("Data saved");
      display.display();
      delay(3000);
    }
  }
  else if (button2State)
  {
    // If button 2 is pressed for 3 seconds
    if (timerB.elapsed())
    {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println("Set min position");
      display.display();
      delay(3000);
      setAbsMs();
    }
  }
  else if (button1State && inRange())
  {
    timerB.reset();
    stepper.moveTo(absMinPosition);
    stepper.runSpeed();
  }
  else if (button3State && inRange())
  {
    timerB.reset();
    stepper.moveTo(absMaxPosition);
    stepper.runSpeed();
  }
  else
  {
    timerB.reset();
    stepper.moveTo(stepper.currentPosition());
    refPosition = stepper.currentPosition() - homePosition;
  }

  if (button2State)
  {
    if (stepper.isRunning())
    {
      stepper.stop();
    }
    setHome();
  }
}

void blinkOnboardLED()
{
  static Timer timerLED(500);
  static bool ledState = LOW;
  if (!button1State && !button2State && !button3State)
  {
    if (timerLED.elapsed())
    {
      ledState = !ledState;
      digitalWrite(LED_BUILTIN, ledState);
    }
  }
  else
  {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void setAbsMs()
{
  static int counter = 0;
  const char *setState[3] = {"Hold to set min!", "Hold to set max!", "Hold to exit!"};
  static Timer timerS(3000);

  while (counter < 3)
  {
    button1State = !digitalRead(BUTTON_PIN_1);
    button2State = !digitalRead(BUTTON_PIN_2);
    button3State = !digitalRead(BUTTON_PIN_3);
    if (button2State)
    {
      // If both buttons are pressed together for 3 seconds
      if (timerS.elapsed())
      {
        if (counter == 0)
        {
          absMinPosition = stepper.currentPosition();
        }
        else if (counter == 1)
        {
          absMaxPosition = stepper.currentPosition();
        }
        else if (counter == 2)
        {
          long temp = absMinPosition;
          if (absMinPosition > absMaxPosition)
          {
            absMinPosition = absMaxPosition;
            absMaxPosition = temp;
          }
          homePosition = absMinPosition / 2 + absMaxPosition / 2;
        }
        counter++;

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println(setState[counter]);
        display.display();
        delay(1000);    

      }
    }
    else if (button1State)
    {
      timerS.reset();
      stepper.moveTo(-1000000000);
      stepper.runSpeed();
    }
    else if (button3State)
    {
      timerS.reset();
      stepper.moveTo(1000000000);
      stepper.runSpeed();
    }
    else
    {
      timerS.reset();
      stepper.moveTo(stepper.currentPosition());
      refPosition = stepper.currentPosition() - homePosition;

      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print("Abs:");
      display.println(stepper.currentPosition() * distancePerStep);
      display.display();
    }
  }
}

bool inRange()
{
  return stepper.currentPosition() >= absMinPosition && stepper.currentPosition() <= absMaxPosition;
}

void eepRead()
{
  EEPROM.get(EEPROM_ABS_LOC_ADDR, refPosition);
  EEPROM.get(EEPROM_HOME_LOC_ADDR, homePosition);
  EEPROM.get(EEPROM_MIN_ADDR, absMinPosition);
  EEPROM.get(EEPROM_MAX_ADDR, absMaxPosition);
  EEPROM.get(EEPROM_SPD_ADDR, speed);
  EEPROM.get(EEPROM_ACC_ADDR, acceleration);
  stepper.setCurrentPosition(refPosition);
}

void eepWrite()
{
  EEPROM.put(EEPROM_ABS_LOC_ADDR, stepper.currentPosition());
  EEPROM.put(EEPROM_HOME_LOC_ADDR, homePosition);
  EEPROM.put(EEPROM_MIN_ADDR, absMinPosition);
  EEPROM.put(EEPROM_MAX_ADDR, absMaxPosition);
  EEPROM.put(EEPROM_SPD_ADDR, speed);
  EEPROM.put(EEPROM_ACC_ADDR, acceleration);
}

/*
Instrunction set:
F 100 - Move the stepper motor forward by 100 steps.
B 100 - Move the stepper motor backward by 100 steps.
S 300 - Set the stepper motor speed to 300 steps per second.
A - Read the absolute position of the stepper motor in steps and print it to the serial monitor.
R - Read the reference position of the stepper motor, which is the absolute position normalized to DISTANCE_PER_STEP, and print it to the serial monitor.
P 500 - Move the stepper motor to the target position (500 steps) within the defined min and max position limits.
H - Move the stepper motor to the home position (0 steps).
C - Set the current position of the stepper motor as the home position.
Z - Set the stepper motor's absolute position to 0 steps.
M 1000 - Set the stepper motor's maximum speed to 1000 steps per second.
L 2000 - Set the stepper motor's acceleration to 2000 steps per second squared.
D 5 - Set the distance per step to 5 units (e.g., 5 um).
X 2000 - Set the maximum absolute position to 2000 steps.
Y -500 - Set the minimum absolute position to -500 steps.
E - Write the current absolute location, current location, home location, min, and max values to EEPROM.
G - Read the saved absolute location, current location, home location, min, and max values from EEPROM.
*/
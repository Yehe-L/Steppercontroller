# Steppercontroller

This is a simple firmware for controll a stepper motor via Teensy 2.0. It has a serial interface and basic button controls

Example Instrunction set:
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


/* Warren Fox, Sep 2015 */

/* the valid door commands sent via http PUT */
const String OpenCommand = "OPEN";
const String CloseCommand = "CLOSE";
const String CheckSingleCommand = "CHECKSINGLE";
const String CheckDoubleCommand = "CHECKDOUBLE";

/* door state string values to export to the server.
 * An http GET directed to the server (e.g. from an app)
 * can directly display these strings to the user.
 */
const char* ClosedState = "Down";
const char* OpenedState = "Up";

/* offset for the CHECKxxx command response, see doorCommand() */
const int StateOffset = 10;
const int StateStringMaxLen = 50;

/* door states */
typedef enum {
  DoorUp,
  DoorDown,
  StateUnknown
} DoorState;

/* door state and control/monitor access for the door */
typedef struct {
  int controlPin;   // raise high briefly to signal the door controller
  int senseOutPin;  // briefly bring high ...
  int senseInPin;   // ... and sample this pin for the door sensor
                    // 1 = door down, 0 = up; either full or partway
  DoorState state;  // sensed state of door
  char stateString[StateStringMaxLen];  // door state in string form
} Door;

// prototypes for local functions
void setDoorPins(Door *door);
int engageDoor(Door *door);
DoorState readDoorState(Door *door);
void setLed(DoorState doorState);
DoorState updateDoorState(Door *door);


// initial control settings for the doors
// D0, D1 etc refer to the pins on the Particle Core.
Door singleDoor = {D0, D1, D2, StateUnknown, ""};
Door doubleDoor = {D4, D5, D6, StateUnknown, ""};
const int OnBoardLedPin = D7;           // on board LED

/**
 * setup(), loop(), and doorCommand() are called by the Particle framework.
 * The signatures for these functions are defined by the framework.
 * Global variables are required in order to share data between these functions.
 *
 * doorCommand() is called in response to an http PUT to the server referencing
 * this function by name.  This is the entry point to this app to command a door
 * change or query door state.
 *
 * The other functions in this file are called by setup(), loop() and
 * doorCommand().
 */

void setup() {
  // register server functions and variables
  Spark.function("action", doorCommand);
  // these variables (2nd argument) are polled by the server every 30 seconds
  Spark.variable("singleDoor", singleDoor.stateString, STRING);
  Spark.variable("doubleDoor", doubleDoor.stateString, STRING);

  // set the pins that control/monitor the doors
  setDoorPins(&singleDoor);
  setDoorPins(&doubleDoor);

  // configure the on board LED
  pinMode(OnBoardLedPin, OUTPUT);
}

// continually monitor both doors for state changes
void loop(void) {
  delay(1000);
  updateDoorState(&doubleDoor);
  setLed(updateDoorState(&singleDoor)); // use LED for state of single door
}

// Parse the command string from the server, act upon the command.
// Currently only the single door may be opened or closed.
// This function returns a single int to the server, which is then
// provided to the original requestor as a nominal status code.
// The return code is overloaded:
//    -1 on bad input
//    0 on no action
//    1 on door toggle initiated (OPEN or CLOSE command)
//    StateOffset + DoorState on status request (CHECKSINGLE or CHECKDOUBLE)
//      (currently, 10 = DoorUp, 11 = DoorDown)
int doorCommand(String command)
{
  int result = 0;

  if(command.startsWith(OpenCommand)) {
    // if commanded to open the closed (single) door
    if (singleDoor.state == DoorDown) {
      result = engageDoor(&singleDoor);
    }
  } else if (command.startsWith(CloseCommand)) {
    // if commanded to close the open (single) door
    if (singleDoor.state == DoorUp) {
      result = engageDoor(&singleDoor);
    }
  } else if (command.startsWith(CheckSingleCommand)) {
    // respond with single door state
    result = (int)StateOffset + (int)singleDoor.state;
  } else if (command.startsWith(CheckDoubleCommand)) {
    // respond with double door state
    result = (int)StateOffset + (int)doubleDoor.state;
  } else {
    // command not recognized
    result = -1;
  }

  return result;
}

// Set the pins for sensing and opening/closing the door.
void setDoorPins(Door *door) {
  pinMode(door->controlPin, OUTPUT);
  digitalWrite(door->controlPin, LOW);

  pinMode(door->senseOutPin, OUTPUT);
  pinMode(door->senseInPin, INPUT_PULLDOWN);
}

// Engage the garage door controller to change the door state.
int engageDoor(Door *door) {
  digitalWrite(door->controlPin, HIGH);
  delay(300);
  digitalWrite(door->controlPin, LOW);
  return 1;
}

// Read the state of the specified door according to the attached sensor.
// Return the sensed state.
// Note: this function assumes the respective senseOutPin has been set HIGH.
DoorState readDoorState(Door *door) {
  // the Reed magnetic sensor presents a closed circuit (=1) when the door is down
  if (digitalRead(door->senseInPin) == 1) {
    return DoorDown;
  }
  return DoorUp;
}

// turn on LED on DoorUp, turn off otherwise.
void setLed(DoorState doorState) {
  if (doorState == DoorUp) {
    digitalWrite(OnBoardLedPin, HIGH);
  } else {
    digitalWrite(OnBoardLedPin, LOW);
  }
}

// Update the door state variables based on the door sensor reading.
// Return the door state.
DoorState updateDoorState(Door *door) {
  // raise the sense line and allow time for propagation
  digitalWrite(door->senseOutPin, HIGH);
  delay(5);

  // acquire door state from the sensor
  DoorState sensedState = readDoorState(door);

  // update the state variables, local and server
  if (sensedState == DoorDown) {
    door->state = DoorDown;
    strlcpy(door->stateString, ClosedState, StateStringMaxLen);
  } else {
    door->state = DoorUp;
    strlcpy(door->stateString, OpenedState, StateStringMaxLen);
  }
  // lower the sense line
  digitalWrite(door->senseOutPin, LOW);

  return sensedState;
}

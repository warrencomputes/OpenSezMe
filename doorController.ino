
/* Warren Fox, Sep 2015 */

/* the valid doors to be commanded */
const String SingleDoor = "SINGLE";
const String DoubleDoor = "DOUBLE";

/* the source of the door movement command, published as a String to the server */
const String WallSwitch = "WallSwitch";
const String ParticleCommand = "ParticleCommand";
String commandSource = WallSwitch;

/* door state string values to export to the server.
 * An http GET directed to the server (e.g. from an app)
 * can directly display these strings to the user.
 */
const char* ClosedState = "Down";
const char* OpenedState = "Up";

/* door states */
typedef enum {
  DoorUp,
  DoorDown,
  StateUnknown
} DoorState;

/* keep track of state changes, report changes to the server */
DoorState lastSingleDoorState = StateUnknown;
DoorState lastDoubleDoorState = StateUnknown;

/* door state and control/monitor access for the door */
const int StateStringMaxLen = 50;
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
DoorState updateDoorState(Door *door);


// initial control settings for the doors
// D0, D1 etc refer to the pins on the Particle Core.
Door singleDoor = {D0, D1, D2, StateUnknown, ""};
Door doubleDoor = {D4, D5, D6, StateUnknown, ""};

/**
 * setup(), loop(), openCommand(), closeCommand() and checkCommand()
 * are called by the Particle framework.
 * The signatures for these functions are defined by the framework.
 * Global variables are required in order to share data between these functions.
 *
 * openCommand(), closeCommand() and checkCommand() are called in response to
 * an http PUT to the server referencing the function by it's registered name.
 * These are the entry points to this app.
 *
 * The other functions in this file are called by setup(), loop() and
 * openCommand(), closeCommand() and checkCommand().
 */

void setup() {
  // register server functions and variables
  Spark.function("open", openCommand);
  Spark.function("close", closeCommand);
  Spark.function("check", checkCommand);
  // these variables (2nd argument) are polled by the server every 30 seconds
  Spark.variable("singleDoor", singleDoor.stateString, STRING);
  Spark.variable("doubleDoor", doubleDoor.stateString, STRING);

  // set the pins that control/monitor the doors
  setDoorPins(&singleDoor);
  setDoorPins(&doubleDoor);
}

// continually monitor both doors for state changes
void loop(void) {
  delay(1000);

  // check for state changes
  DoorState currentState;
  if ((currentState = updateDoorState(&doubleDoor)) != lastDoubleDoorState) {
    // publish and record locally
    char event[StateStringMaxLen] = "double door ";
    strcat(event, currentState==DoorDown ? ClosedState : OpenedState);
    Spark.publish(event, commandSource);
    lastDoubleDoorState = currentState;
    commandSource = WallSwitch;   // the default
  }
  if ((currentState = updateDoorState(&singleDoor)) != lastSingleDoorState) {
    // publish and record locally
    char event[StateStringMaxLen] = "single door ";
    strcat(event, currentState==DoorDown ? ClosedState : OpenedState);
    Spark.publish(event, commandSource);
    lastSingleDoorState = currentState;
    commandSource = WallSwitch;   // the default
  }
}

// Open the specified door if it is currently closed.
// This function returns a single int to the server, which is then
// provided to the original requestor as a nominal status code.
// The return code is:
//    -1 on bad input
//    0 on no action
//    1 on door open initiated
int openCommand(String command) {
  int result = 0;

  if(command.startsWith(SingleDoor)) {
    // if commanded to open the closed single door
    if (singleDoor.state == DoorDown) {
      result = engageDoor(&singleDoor);
      commandSource = ParticleCommand;
    }
  } else if (command.startsWith(DoubleDoor)) {
    // if commanded to open the closed double door
    if (doubleDoor.state == DoorDown) {
      result = engageDoor(&doubleDoor);
      commandSource = ParticleCommand;
    }
  } else {
    // door not recognized
    result = -1;
  }

  return result;
}

// Close the specified door if it is currently open.
// This function returns a single int to the server, which is then
// provided to the original requestor as a nominal status code.
// The return code is:
//    -1 on bad input
//    0 on no action
//    1 on door close initiated
int closeCommand(String command) {
  int result = 0;

  if(command.startsWith(SingleDoor)) {
    // if commanded to close the open single door
    if (singleDoor.state == DoorUp) {
      result = engageDoor(&singleDoor);
      commandSource = ParticleCommand;
    }
  } else if (command.startsWith(DoubleDoor)) {
    // if commanded to close the open double door
    if (doubleDoor.state == DoorUp) {
      result = engageDoor(&doubleDoor);
      commandSource = ParticleCommand;
    }
  } else {
    // door not recognized
    result = -1;
  }

  return result;
}

// Check the specified door, return it's status.
// This function returns a single int to the server, which is then
// provided to the original requestor as a nominal status code.
// The return code is:
//    -1 on bad input
//    0 on door up
//    1 on door down
int checkCommand(String command) {
  int result = 0;

  if (command.startsWith(SingleDoor)) {
    // respond with single door state
    result = (int)singleDoor.state;
  } else if (command.startsWith(DoubleDoor)) {
    // respond with double door state
    result = (int)doubleDoor.state;
  } else {
    // door not recognized
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

#include <TimerOne.h>

// motor driver pins
#define PIN_OPEN  3
#define PIN_CLOSE 2
#define PIN_PWM   5

// photo sensor pin
#define PIN_PHOTO  8

// motor speed 0-255 (PIN_PWM)
#define SPEED_FAST 255
#define SPEED_SLOW  80
#define SPEED_REF   70

// lock positions (rotary encoder steps)
#define LOCK_CLOSE  0
#define LOCK_OPEN   9
#define DOOR_OPEN  10

// delay to use after a field change in rotary encoder
// this gives the disc some time to move further
// and avoids counting the same field again
#define PS_DELAY 10

// LEDs
#define LED_R 11
#define LED_Y 12
#define LED_G 13

// Buttons
#define BUTTON_CLOSE 6
#define BUTTON_OPEN  7

// ToDo:
// Timeout for a field change in rotary encoder
// = maximum time between two fields
// needs to be speed independent
//#define CH_TIMEOUT

int position;


void toggleLEDs(bool r, bool y, bool g) {

    digitalWrite(LED_R, r ? HIGH : LOW);
    digitalWrite(LED_Y, y ? HIGH : LOW);
    digitalWrite(LED_G, g ? HIGH : LOW);

}


void stateChanged() {

    // the state of sphincter has changed. Update LEDs
    // and submit state over serial connection

    switch(position) {

        case LOCK_CLOSE:
            toggleLEDs(true, false, false);
            Serial.println("LOCKED");
            break;

        case LOCK_OPEN:
            toggleLEDs(false, true, false);
            Serial.println("UNLOCKED");
            break;

        case DOOR_OPEN:
            toggleLEDs(false, false, true);
            Serial.println("OPEN");
            break;

        default:
            Serial.println("NO KNOWN STATE");
            break;
    }

}


void referenceRun() {

    // turns the lock in closing direction until it blocks
    // to figure out its minimum position

    int counter = 0;
    boolean was_interrupted = false;

    toggleLEDs(true, true, true);

    analogWrite(PIN_PWM, SPEED_REF); // speed (PWM)
    digitalWrite(PIN_CLOSE, HIGH);   // start motor

    do {

        delay(15); // don´t count at "cpu speed"

        // if nothing changes disc got stuck
        // means that the lock is at its minimum position
        if( (!digitalRead(PIN_PHOTO) && !was_interrupted)
         || (digitalRead(PIN_PHOTO) && was_interrupted) ) {

            counter = 0;
            was_interrupted = !was_interrupted;

        }

        counter ++;

    } while( counter < 50 );

    digitalWrite(PIN_CLOSE, LOW);   // stop motor

    delay(PS_DELAY);

    // if the rotary encoder is interrupted
    // turn back until there is no field in between and than
    // turn one field further (= position 0)
    digitalWrite(PIN_PWM, SPEED_FAST);
    digitalWrite(PIN_OPEN, HIGH);
    while( !digitalRead(PIN_PHOTO) );
    delay(PS_DELAY);
    while( digitalRead(PIN_PHOTO) );
    digitalWrite(PIN_OPEN, LOW);

    position = 0;
    stateChanged();

}


void turnLock(int new_position) {

    if( new_position == position
       || new_position < LOCK_CLOSE
       || new_position > DOOR_OPEN ) return;

    int step;
    int direction;
    boolean was_interrupted = false;

    analogWrite(PIN_PWM, SPEED_FAST);  // set speed

    // open lock
    if( new_position > position ) {

        step =  1;           // increment position
        direction = PIN_OPEN;

    }
    // close lock
    else if( new_position < position ) {

        step = -1;           // decrement position
        direction = PIN_CLOSE;

    }


    digitalWrite(direction, HIGH);  // motor power on

    // wait for photo sensor to become free
    while( !digitalRead(PIN_PHOTO) );

    delay(PS_DELAY);

    while(true) {

        // photo sensor becomes interrupted
        if( !digitalRead(PIN_PHOTO) && !was_interrupted ) {

            position += step;
            was_interrupted = true;

        }
        // photo sensor becomes free
        else if( digitalRead(PIN_PHOTO) && was_interrupted ) {

            was_interrupted = false;

        }

        if( position != new_position ) {
            delay(PS_DELAY);
        }
        else {
            break;
        }

        if( (new_position == LOCK_CLOSE) && position > LOCK_CLOSE + 3) {
            analogWrite(PIN_PWM, SPEED_SLOW);
        }
        else {
            analogWrite(PIN_PWM, SPEED_FAST);
        }

    }

    digitalWrite(direction, LOW); // motor power off

    delay(PS_DELAY);

    // if necessary turn back to correct position
    if( direction == PIN_OPEN ) {

        digitalWrite(PIN_CLOSE, HIGH);
        while( digitalRead(PIN_PHOTO) );
        digitalWrite(PIN_CLOSE, LOW);

    }
    else if( direction == PIN_CLOSE ) {

        digitalWrite(PIN_OPEN, HIGH);
        while( digitalRead(PIN_PHOTO) );
        digitalWrite(PIN_OPEN, LOW);

    }


    stateChanged();

    // turn back after opened the door
    if( new_position == DOOR_OPEN ) {
        delay(300);
        turnLock(LOCK_OPEN);
    }

}



void processButtonEvents() {

    static boolean open_was_pressed = false;
    static boolean close_was_pressed = false;

    if( digitalRead(BUTTON_OPEN) && digitalRead(BUTTON_CLOSE) ) {

        referenceRun();
        // as in most cases one button gets pressed first,
        // one of the variables is set to true
        open_was_pressed = false;
        close_was_pressed = false;

    }
    else if( digitalRead(BUTTON_OPEN ) ) {
        open_was_pressed = true;
    }
    else if( digitalRead(BUTTON_CLOSE) ) {
        close_was_pressed = true;
    }
    else if( !digitalRead(BUTTON_OPEN) && open_was_pressed ) {

        open_was_pressed = false;
        turnLock(DOOR_OPEN);

    }
    else if( !digitalRead(BUTTON_CLOSE) && close_was_pressed ) {

        close_was_pressed = false;
        turnLock(LOCK_CLOSE);

    }

}


void processSerialEvents() {

    char incomingByte;

    // check if there was data sent
    if (Serial.available() > 0) {

        incomingByte = Serial.read();

        switch(incomingByte) {

            case 'o':
              turnLock(DOOR_OPEN);
              break;

            case 'c':
              turnLock(LOCK_CLOSE);
              break;

            case 'r':
              referenceRun();
              break;

            case 's':
              stateChanged();

            default:
              break;
        }
    }

}



void setup()  {

    // initialize timer interrupt with 10ms period
    //Timer1.initialize(10000);
    //Timer1.attachInterrupt(processTimerInterrup);

    // initialize pins
    pinMode(LED_R,     OUTPUT);
    pinMode(LED_Y,     OUTPUT);
    pinMode(LED_G,     OUTPUT);
    pinMode(PIN_OPEN,  OUTPUT);
    pinMode(PIN_CLOSE, OUTPUT);
    pinMode(PIN_PHOTO, INPUT);

    // initialize serial
    Serial.begin(9600);

    referenceRun();

}


void loop()  {

    processButtonEvents();
    processSerialEvents();

}

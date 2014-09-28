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
#define POSITION_LOCKED    0
#define POSITION_UNLOCKED  9
#define POSITION_OPEN     10

// delay to use after a field change in rotary encoder.
// This gives the disc some time to move further
// and avoids counting the same field again
#define PS_DELAY 10

// LEDs
#define LED_R 11
#define LED_Y 12
#define LED_G 13

// Buttons
#define BUTTON_CLOSE 6
#define BUTTON_OPEN  7

// "Bitmasks" for the different button combinations.
#define BUTTONS_PRESSED_NONE  0
#define BUTTONS_PRESSED_OPEN  1
#define BUTTONS_PRESSED_CLOSE 2
#define BUTTONS_PRESSED_BOTH  3

// Serial responses
#define RESPONSE_LOCKED   "LOCKED"
#define RESPONSE_UNLOCKED "UNLOCKED"
#define RESPONSE_OPEN     "OPEN"
#define RESPONSE_UNKNOWN  "UNKNOWN"
#define RESPONSE_BUSY     "BUSY"

// TODO:
// Timeout for a field change in rotary encoder
// = maximum time between two fields
// needs to be speed independent
//#define CH_TIMEOUT


// position holds the current position of sphincter.
int position;


// toggleLEDs is a helper func to change the LEDs of sphincter.
void toggleLEDs(bool r, bool y, bool g) {

    digitalWrite(LED_R, r ? HIGH : LOW);
    digitalWrite(LED_Y, y ? HIGH : LOW);
    digitalWrite(LED_G, g ? HIGH : LOW);

}


// stateChanged is called whenever the state of sphincter has changed.
// It updates the LEDs and submits the state to sphincterd.
void stateChanged() {

    switch(position) {

        case POSITION_LOCKED:
            toggleLEDs(true, false, false);
            Serial.println(RESPONSE_LOCKED);
            break;

        case POSITION_UNLOCKED:
            toggleLEDs(false, true, false);
            Serial.println(RESPONSE_UNLOCKED);
            break;

        case POSITION_OPEN:
            toggleLEDs(false, false, true);
            Serial.println(RESPONSE_OPEN);
            break;

        default:
            Serial.println(RESPONSE_UNKNOWN);
            break;
    }

}


// referenceRun turns the lock in closing direction until it get stuck to
// figure out the minimum position.
void referenceRun() {

    Serial.println(RESPONSE_BUSY);

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


// turnLock turns the lock to new_position. If new_position == POSITION_OPEN
// the lock will turn back to POSITION_UNLOCKED after a short delay.
void turnLock(int new_position) {

    if( new_position == position
       || new_position < POSITION_LOCKED
       || new_position > POSITION_OPEN ) {
        stateChanged();
        return;
    }

    Serial.println(RESPONSE_BUSY);

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

        if( (new_position == POSITION_LOCKED) && position > POSITION_LOCKED + 3) {
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
    if( new_position == POSITION_OPEN ) {
        delay(300);
        turnLock(POSITION_UNLOCKED);
    }

}


// processButtonEvents handles sphincter´s onboard buttons.
void processButtonEvents() {

    static unsigned int lp_count_open;
    static unsigned int lp_count_close;
    static unsigned int lp_count_both;

    // 00 = 0: no button pressed
    // 01 = 1: open button pressed
    // 10 = 2: close button pressed
    // 11 = 3: both buttons are pressed
    byte button_bitmask;

    if( digitalRead(BUTTON_OPEN) || digitalRead(BUTTON_CLOSE) ) {

        do {

            // generate bitmask
            button_bitmask = (digitalRead(BUTTON_OPEN)  ? 1 : 0)
                           | (digitalRead(BUTTON_CLOSE) ? 2 : 0);

            switch( button_bitmask ) {
                case BUTTONS_PRESSED_OPEN:
                    lp_count_open ++;
                    lp_count_close = lp_count_both = 0;
                    break;

                case BUTTONS_PRESSED_CLOSE:
                    lp_count_close ++;
                    lp_count_open = lp_count_both = 0;

                    switch( lp_count_close ) {
                        case 1000:
                            toggleLEDs(true, false, false);
                            break;

                        case 2000:
                            toggleLEDs(true, true, false);
                            break;

                        case 3000:
                            toggleLEDs(true, true, true);
                            break;

                        case 4000:
                            for(int i=0; i<30; i++) {
                                toggleLEDs(false, false, false);
                                delay(500 - pow(2, (8.8/30)*i));
                                toggleLEDs(true, true, true);
                                delay(50);
                            }
                            for(int i=0; i<10; i++) {
                                toggleLEDs(false, false, false);
                                delay(50);
                                toggleLEDs(true, true, true);
                                delay(50);
                            }
                            toggleLEDs(false, false, false);
                            turnLock(POSITION_LOCKED);
                            lp_count_close = 0;
                            break;
                    }
                    break;

                case BUTTONS_PRESSED_BOTH:
                    lp_count_both ++;
                    lp_count_open = lp_count_close = 0;

                    if( lp_count_both == 1000 ) {
                        referenceRun();
                    }
                    break;
            }

            delay(1);
        } while( button_bitmask != BUTTONS_PRESSED_NONE );

        // here comes the "on button up" stuff
        if( lp_count_open > 10 ) {
            turnLock(POSITION_OPEN);
        }
        if( lp_count_close > 10 ) {
            if(lp_count_close < 1000) {
                turnLock(POSITION_LOCKED);
            }
            else {
                // reset LEDs
                stateChanged();
            }
        }

        lp_count_both = lp_count_open = lp_count_close = 0;

    }
}


// processSerialEvents handles incoming requests over serial.
void processSerialEvents() {

    char incomingByte;

    // check if there was data sent
    if (Serial.available() > 0) {

        incomingByte = Serial.read();

        switch(incomingByte) {

            case 'o':
              turnLock(POSITION_OPEN);
              break;

            case 'c':
              turnLock(POSITION_LOCKED);
              break;

            case 'r':
              referenceRun();
              break;

            case 's':
              stateChanged();
              break;

        }
    }

}


void setup()  {

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

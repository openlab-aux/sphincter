// motor driver pins
#define OPEN  3
#define CLOSE 2
#define PWM   5

// photo sensor pin
#define PHOTOSENS  8

// motor speed 0-255 (PWM)
#define FAST 255
#define SLOW  80
#define REF   70

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


void stateChanged() {

    // the state of sphincter has changed. Update LEDs
    // and submit state over serial connection

    digitalWrite(LED_R, LOW);
    digitalWrite(LED_Y, LOW);
    digitalWrite(LED_G, LOW);

    switch(position) {

        case LOCK_CLOSE:
            digitalWrite(LED_R, HIGH);
            Serial.println("LOCKED");
            break;

        case LOCK_OPEN:
            digitalWrite(LED_Y, HIGH);
            Serial.println("UNLOCKED");
            break;

        case DOOR_OPEN:
            digitalWrite(LED_G, HIGH);
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

    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_Y, HIGH);
    digitalWrite(LED_G, HIGH);

    analogWrite(PWM, REF);     // speed (PWM)
    digitalWrite(CLOSE, HIGH);  // start motor

    do {

        delay(15); // don´t count at "cpu speed"

        // if nothing changes disc got stuck
        // means that the lock is at its minimum position
        if( (!digitalRead(PHOTOSENS) && !was_interrupted)
         || (digitalRead(PHOTOSENS) && was_interrupted) ) {

            counter = 0;
            was_interrupted = !was_interrupted;

        }

        counter ++;

    } while( counter < 50 );

    digitalWrite(CLOSE, LOW);   // stop motor

    delay(PS_DELAY);

    // if the rotary encoder is interrupted
    // turn back until there is no field in between and than
    // turn one field further (= position 0)
    digitalWrite(PWM, FAST);
    digitalWrite(OPEN, HIGH);
    while( !digitalRead(PHOTOSENS) );
    delay(PS_DELAY);
    while( digitalRead(PHOTOSENS) );
    digitalWrite(OPEN, LOW);

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

    analogWrite(PWM, FAST);  // set speed

    // open lock
    if( new_position > position ) {

        step =  1;           // increment position
        direction = OPEN;

    }
    // close lock
    else if( new_position < position ) {

        step = -1;           // decrement position
        direction = CLOSE;

    }


    digitalWrite(direction, HIGH);  // motor power on

    // wait for photo sensor to become free
    while( !digitalRead(PHOTOSENS) );

    delay(PS_DELAY);

    while(true) {

        // photo sensor becomes interrupted
        if( !digitalRead(PHOTOSENS) && !was_interrupted ) {

            position += step;
            was_interrupted = true;

        }
        // photo sensor becomes free
        else if( digitalRead(PHOTOSENS) && was_interrupted ) {

            was_interrupted = false;

        }

        if( position != new_position ) {
            delay(PS_DELAY);
        }
        else {
            break;
        }

        if( (new_position == LOCK_CLOSE) && position > LOCK_CLOSE + 3) {
            analogWrite(PWM, SLOW);
        }
        else {
            analogWrite(PWM, FAST);
        }

    }

    digitalWrite(direction, LOW); // motor power off

    delay(PS_DELAY);

    // if necessary turn back to correct position
    if( direction == OPEN ) {

        digitalWrite(CLOSE, HIGH);
        while( digitalRead(PHOTOSENS) );
        digitalWrite(CLOSE, LOW);

    }
    else if( direction == CLOSE ) {

        digitalWrite(OPEN, HIGH);
        while( digitalRead(PHOTOSENS) );
        digitalWrite(OPEN, LOW);

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
        delay(1000*20) //20-second delay for leaving lab
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

    // initialize pins
    pinMode(LED_R, OUTPUT);
    pinMode(LED_Y, OUTPUT);
    pinMode(LED_G, OUTPUT);
    pinMode(OPEN,  OUTPUT);
    pinMode(CLOSE, OUTPUT);
    pinMode(PHOTOSENS, INPUT);

    // initialize serial
    Serial.begin(9600);

    referenceRun();

}


void loop()  {

    processButtonEvents();
    processSerialEvents();

}

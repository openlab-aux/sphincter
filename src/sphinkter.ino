// arduino pins
#define OPEN  4
#define CLOSE 2
#define PWM   3
#define PHOTOSENS  8

// motor speed
#define FAST 255 
#define SLOW 100

// lock positions
#define LOCK_CLOSE  0
#define LOCK_OPEN   9
#define DOOR_OPEN   10

int position;
boolean was_interrupted;


void turnLock(int new_position) {

    if( new_position == position || new_position < LOCK_CLOSE || new_position > DOOR_OPEN ) return;

    int step;
    int direction;
    

    if( new_position > position ) {
        // open lock

        step = 1; // increment position
        direction = OPEN;
        
    }
    else if( new_position < position ) {
        // close lock

        step = -1; // decrement position
        direction = CLOSE;

    }


    digitalWrite(direction, HIGH);

    do {
   
        // Photo sensor becomes interrupted
        if( !digitalRead(PHOTOSENS) && !was_interrupted ) {
            position += step;
            digitalWrite(13,HIGH); // Debug LED
            was_interrupted = true;
        }
        // Photo sensor becomes free
        else if( digitalRead(PHOTOSENS) && was_interrupted ) {
            was_interrupted = false;
            digitalWrite(13,LOW); // Debug LED
        }
        
    } while( position != new_position );

    digitalWrite(direction, LOW);

    delay(40);
    analogWrite(PWM, SLOW);
    
    // turn back to correct position
    if( direction == OPEN ) {
        Serial.println("zurueck fahren, CLOSE");
       
        while( digitalRead(PHOTOSENS) ) { digitalWrite(CLOSE, HIGH); Serial.println("loop");}
        digitalWrite(CLOSE, LOW);
    }
    else if( direction == CLOSE ) {
        Serial.println("zureuck fahren, OPEN");
        while( digitalRead(PHOTOSENS) ) { digitalWrite(OPEN, HIGH); }
        digitalWrite(OPEN, LOW);
    }

    analogWrite(PWM, FAST);

}




void setup()  { 

    // serial debuggin
    Serial.begin(9600);
    Serial.println("Hello world");

    pinMode(OPEN, OUTPUT); // Richtung 1
    pinMode(CLOSE, OUTPUT); // Richtung 2
    
    pinMode(13, OUTPUT); // Debug LED

    pinMode(PHOTOSENS, INPUT);  // Lichtschranke
    
    position = 0;
    was_interrupted = false;

    analogWrite(PWM, FAST); // Geschwindigkeit (PWM)
    
    turnLock(LOCK_OPEN);
    delay(2000);
    turnLock(DOOR_OPEN);
    delay(2000);
    turnLock(LOCK_CLOSE);
}


void loop()  { 
    if(digitalRead(PHOTOSENS)) {
        digitalWrite(13,LOW);
    }
    else {
        digitalWrite(13,HIGH);
    }
    
}

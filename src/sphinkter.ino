// arduino pins
#define OPEN  3
#define CLOSE 2
#define PWM   5
#define PHOTOSENS  8

// motor speed
#define FAST 255 
#define SLOW 20

// lock positions
#define LOCK_CLOSE  0
#define LOCK_OPEN   9
#define DOOR_OPEN   10

// Delay for photo sensor to avoid flickering
#define PS_DELAY 50

//LEDs
#define R_LED 11
#define Y_LED 12
#define G_LED 13

//Buttons
#define BUTTON_CLOSE 6
#define BUTTON_OPEN 7

int position;


void stateChanged() {
    
    digitalWrite(R_LED, LOW);
    digitalWrite(Y_LED, LOW);  
    digitalWrite(G_LED, LOW);
    
    switch(position) {
    
    case LOCK_CLOSE: {
                         digitalWrite(R_LED, HIGH);
                         Serial.println("Door locked");
                         break;
                     }
    case LOCK_OPEN:  {
                         digitalWrite(Y_LED, HIGH); 
                         Serial.println("Door unlocked");
                         break;
                     }
    case DOOR_OPEN:  {
                         digitalWrite(G_LED, HIGH); 
                         Serial.println("Door open");
                         break;
                     }
  
  }
  
}


void searchRef() {

  int counter = 0;
  boolean was_interrupted = false;
  
  analogWrite(PWM, SLOW); // speed (PWM)
  digitalWrite(CLOSE,HIGH); // motor power on
  
  
    do {
      
      delay(15);
   
        // photo sensor becomes interrupted
        if( !digitalRead(PHOTOSENS) && !was_interrupted ) {

            counter = 0;
            was_interrupted = true;

        }
        // photo sensor becomes free
        else if( digitalRead(PHOTOSENS) && was_interrupted ) {
            
            counter = 0;
            was_interrupted = false;

        }
        
        counter ++;
        
    } while( counter < 50 );
      
 
    digitalWrite(CLOSE,LOW); // motor power off
    
    delay(PS_DELAY);
    
    // turn back to first pad (= position 0)
    while( digitalRead(PHOTOSENS) ) { digitalWrite(OPEN, HIGH); }
    digitalWrite(OPEN, LOW);
    
    position = 0;
    stateChanged();

}


void turnLock(int new_position) {

    if( new_position == position || new_position < LOCK_CLOSE || new_position > DOOR_OPEN ) return;

    int step;
    int direction;
    boolean was_interrupted = false;
    
    // open lock
    if( new_position > position ) {        
        
        step =  1; // increment position
        direction = OPEN;
        
    }
    // close lock 
    else if( new_position < position ) {       

        step = -1; // decrement position
        direction = CLOSE;

    }

    analogWrite(PWM, FAST); // speed (PWM)
    digitalWrite(direction, HIGH); // motor power on

    // wait for photo sensor to become free
    while( !digitalRead(PHOTOSENS) );
    delay(PS_DELAY);

    do {
   
        // photo sensor becomes interrupted
        if( !digitalRead(PHOTOSENS) && !was_interrupted ) {

            position += step;
            was_interrupted = true;

        }
        // photo sensor becomes free
        else if( digitalRead(PHOTOSENS) && was_interrupted ) {
            
            was_interrupted = false;

        }
        
    } while( position != new_position );

    digitalWrite(direction, LOW); // motor power off

    delay(PS_DELAY);
    
    // if necessary turn back to correct position
    if( direction == OPEN ) {

        while( digitalRead(PHOTOSENS) ) { digitalWrite(CLOSE, HIGH); } 
        digitalWrite(CLOSE, LOW);

    }
    else if( direction == CLOSE ) {
        
        while( digitalRead(PHOTOSENS) ) { digitalWrite(OPEN, HIGH); }
        digitalWrite(OPEN, LOW);

    }


    stateChanged();

    // turn back after opened the door
    if( new_position == DOOR_OPEN ) {
        delay(500); 
        turnLock(LOCK_OPEN);
    }

}



void processButtonEvents() {
  
    static boolean open_was_pressed = false;
    static boolean close_was_pressed = false;

    if( digitalRead(BUTTON_OPEN) && digitalRead(BUTTON_CLOSE) ) {

        searchRef();
        open_was_pressed = false;
        close_was_pressed = false;

    }
    else if( digitalRead(BUTTON_OPEN ))  open_was_pressed = true; 
    else if( digitalRead(BUTTON_CLOSE))  close_was_pressed = true; 

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
            // read the incoming byte:
            incomingByte = Serial.read();

            switch(incomingByte) {
                case 'o': turnLock(DOOR_OPEN); break;
                case 'c': turnLock(LOCK_CLOSE); break;
                case 'r': searchRef(); break;
            }
    }

}



void setup()  { 

    // LED pins
    pinMode(R_LED, OUTPUT);
    pinMode(Y_LED, OUTPUT);
    pinMode(G_LED, OUTPUT);
    
    pinMode(OPEN, OUTPUT); // Richtung 1
    pinMode(CLOSE, OUTPUT); // Richtung 2
    
    pinMode(PHOTOSENS, INPUT);  // Lichtschranke

    // serial debugging
    Serial.begin(9600);
    Serial.println("***** Welcome to fu**ing awesome Sphinkter *****");

    // start the Ethernet connection and the server:
    //Ethernet.begin(mac, ip);
    //server.begin();
    //Serial.print("server is at ");
    //Serial.println(Ethernet.localIP());
    

    //searchRef();
    position = 0;
   
}



void loop()  { 
    
    processButtonEvents();
    //process_request_if_available();
    
    processSerialEvents(); 

}

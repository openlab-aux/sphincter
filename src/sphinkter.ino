#include <SPI.h>
#include <Ethernet.h>

// arduino pins
#define OPEN  4
#define CLOSE 2
#define PWM   3
#define PHOTOSENS  8

// motor speed
#define FAST 100 
#define SLOW 100

// lock positions
#define LOCK_CLOSE  0
#define LOCK_OPEN   9
#define DOOR_OPEN   10

// Delay for photo sensor to avoid flickering
#define PS_DELAY 50

int position;
boolean was_interrupted;


String HTTP_req; // stores the HTTP request

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192,168,178,177);

EthernetServer server(80);

void process_request_variables(EthernetClient cl) {
  Serial.println("Processing variables");

  if (HTTP_req.indexOf("GET /?open_door=true") > -1) {
    Serial.println("Unlocking door");
    turnLock(DOOR_OPEN);
    delay(2000);
    turnLock(LOCK_OPEN);

  } else if (HTTP_req.indexOf("GET /?close_door=true") > -1) {
    turnLock(LOCK_CLOSE);
    Serial.println("Locking door");
  }
}


void process_request_if_available() {
  EthernetClient client = server.available();  // try to get client
    if (client) {  // got client?
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                char c = client.read(); // read 1 byte from client
                HTTP_req += c;
                // last line of client request is blank and ends with \n

                if ( HTTP_req.length() > 512 ) {
                  // Do not handle requests larger than 512 bytes
                  break;
                }
                
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println("Connection: close");
                    client.println();
                    // send web page
                    client.println("<!DOCTYPE html>");
                    client.println("<html>");
                    client.println("<head>");
                    client.println("<title>Sphinkter</title>");

                    client.println("</head>");
                    client.println("<body>");
                    client.println("<h1>OpenLap Sphinkter Control</h1>");

                    client.println("<ul style='list-style-type: none'>");

                    client.print("<li style='margin: 20px; display:inline;'><a href='/?open_door=true'>");
                    client.println("<button style='width: 100px;' type='button'>Unlock</button></a></li>");
                     
                    client.print("<li style='margin: 20px; display:inline;'><a href='/?close_door=true'>");
                    client.println("<button style='width: 100px;' type='button'>Lock</button></a></li>");

                    client.println("</ul>");

                    client.println("</body>");
                    client.println("</html>");
                    process_request_variables(client);
                    HTTP_req = "";
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
}


void turnLock(int new_position) {

    if( new_position == position || new_position < LOCK_CLOSE || new_position > DOOR_OPEN ) return;

    int step;
    int direction;
    was_interrupted = false;
    
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


    digitalWrite(direction, HIGH); // motor power on

    // wait for photo sensor to become free
    while( !digitalRead(PHOTOSENS) );
    delay(PS_DELAY);

    do {
   
        // photo sensor becomes interrupted
        if( !digitalRead(PHOTOSENS) && !was_interrupted ) {

            position += step;
            was_interrupted = true;
            digitalWrite(13,HIGH); // Debug LED

        }
        // photo sensor becomes free
        else if( digitalRead(PHOTOSENS) && was_interrupted ) {
            
            was_interrupted = false;
            digitalWrite(13,LOW); // Debug LED

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

    analogWrite(PWM, FAST);

}




void setup()  { 

    // serial debuggin
    Serial.begin(9600);
    Serial.println("Hello world");

    // start the Ethernet connection and the server:
    Ethernet.begin(mac, ip);
    server.begin();
    Serial.print("server is at ");
    Serial.println(Ethernet.localIP());

    pinMode(OPEN, OUTPUT); // Richtung 1
    pinMode(CLOSE, OUTPUT); // Richtung 2
    
    pinMode(13, OUTPUT); // Debug LED

    pinMode(PHOTOSENS, INPUT);  // Lichtschranke
    
    position = 0;

    analogWrite(PWM, FAST); // Geschwindigkeit (PWM)
    
}


void loop()  { 
    if(digitalRead(PHOTOSENS)) {
        digitalWrite(13,LOW);
    }
    else {
        digitalWrite(13,HIGH);
    }

    process_request_if_available();
    
}

/*
*******
 NOTE: 
*******

 */
#include <SPI.h>
#include <Ethernet.h>

//easy way to disable the relay for testing.
const boolean TOGGLE_RELAY = true;


//Constants
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEF};
byte ip[] = {192,168,69, 8};
byte subnet[] = {255, 255, 255, 0};
byte gateway[] = {192,168,69, 1};

//mic & light variables
boolean lightIsOn = false;
boolean clap_enabled = true; //whether or not the clapper is enabled
boolean got_first_clap = false;
unsigned long time; //time of the first clap.
int mic_value;
const int mic_delay = 40; //pause after getting a clap
const int clap_threshold = 780; //720 //minimum amplitude of a clap. lower values picks up noise
const unsigned long clap_timeout = 390; //time before resetting after only 1 clap
const int clap_final_delay = 900; //timeout after a clap has happened. this timeout is needed because I don't have a capacitor on the mic amplifier circuit, so it registers two claps after a toggle (one for the LED and one for the relay)

//button variables
const unsigned long button_longpress_delay = 790;
unsigned long button_push_time;
const int button_final_delay = 330;

//ethernet variables
const unsigned int CHARLIMIT = 512; //the limit for this value is 65535
char message[512];
char tempChar;
unsigned int numBytes = 0;
EthernetServer server = EthernetServer(23); // port 23
boolean gotAMessage = false; // whether or not you got a message from the client yet

//pin related variables
const int led_p1 = 3;
const int led_p2 = 4;
const int button_p1 = 6;
const int button_p2 = 7;
const int mic_pin = 16;
const int pull_down_pin = 5;
const int relay_pin = 9;

//other variables
int final_delay = 0;
int toggle_count = 0;

void setup() {
  //Ethernet shield stuff
  Ethernet.begin(mac, ip, gateway, subnet);  // initialize the ethernet device
  server.begin(); // start listening for clients
  Serial.begin(9600); // open the serial port

  //Light stuff
  pinMode(button_p1, OUTPUT); //button
  pinMode(button_p2, INPUT); //button
  pinMode(led_p1, OUTPUT); //LED
  pinMode(led_p2, OUTPUT); //LED
  pinMode(relay_pin, OUTPUT);
  digitalWrite(button_p1, HIGH);

  //button pull-down pin. Plz move this to a GND pin
  pinMode(pull_down_pin, OUTPUT);
  digitalWrite(pull_down_pin, LOW);
}


void loop() {
  EthernetClient client = server.available();
  
  if (client) { //true when a client sends data (no way to tell when they connect :(
    if (!gotAMessage) {
      Serial.println("We have a new client");
      server.println("Yes, this is ChristmasTree!"); 
      gotAMessage = true;
    }

    tempChar = client.read(); // read the bytes incoming from the client:
    Serial.print(tempChar);

    //Take actions based on the input
    switch(tempChar) {
    case '1':
      lightIsOn = true;
      Serial.print("(LightON)");
      writeMessage("\rChristmasTree ON\n\r", 19);
      break;
    case '0':
      lightIsOn = false;
      Serial.print("(LightON)");
      writeMessage("\rChristmasTree OFF\n\r", 20);
      break;
    case '{':
      clap_enabled = true;
      Serial.print("(ClapperON)");
      writeMessage("\rMic ON\r\n", 9);
      break;
    case '}':
      clap_enabled = false;
      Serial.print("(ClapperOFF)");
      writeMessage("\rMic OFF\r\n", 10);
      break;
    case'~':
      if (lightIsOn) {
        writeMessage("\rChristmasTree is ON\r\n", 22);
      } else {
        writeMessage("\rChristmasTree is OFF\r\n", 23);
      }
      if (clap_enabled) {
        writeMessage("Mic is ON\r\n", 11);
      } else {
        writeMessage("Mic is OFF\r\n", 12);
      }
      break;
    case '\n':
      if (numBytes < CHARLIMIT) {
        message[numBytes] = '\n';
        writeMessage(message, numBytes + 1);
        numBytes = 0;
      } else {
        message[numBytes-1] = '\n'; //truncate the last byte
        writeMessage(message, numBytes);
        numBytes = 0;
      }
      Serial.println("");
      break;
    case 8: //backspace
      Serial.print('<');
      numBytes--;
      break;
    default:
      if (numBytes < CHARLIMIT) {
        message[numBytes] = tempChar;
        numBytes++;
      }
    }
  } //end of ethernet section

  //toggle by button OR toggle clapper
  if (digitalRead(button_p2)) {
    Serial.print("(Button_Press)");
    button_push_time = millis(); //save the time of the button press
    while (digitalRead(button_p2)) {
      if (millis() > button_push_time + button_longpress_delay) {
        //we just need to wait & see what the user is doing
        break;
      }
    }
    if (millis() > button_push_time + button_longpress_delay) {
        //if the user did long press, toggle clapper
        Serial.print("(Button_Timeout)");
        clap_enabled = !clap_enabled;
    } else {
      //if the user did not long press, toggle the light
      lightIsOn = !lightIsOn;
    }
    Serial.println("(Button_Release)");
    final_delay = button_final_delay;
  }

  //toggle by claps
  if (clap_enabled) {
    mic_value = analogRead(mic_pin);
    if ((got_first_clap == false) && (mic_value > clap_threshold)) { //first clap
      Serial.print("(CLAP1[");
      Serial.print(mic_value);
      Serial.print("])");
      final_delay = mic_delay;
      time = millis();
      got_first_clap = true;
    } else if (got_first_clap) {
      if (time+clap_timeout < millis()) { //timeout on the second clap
        got_first_clap = false;
      } else { //still within the time window for a second clap
        if (mic_value > clap_threshold) { //we got a clap!
          lightIsOn = !lightIsOn;
          got_first_clap = false;
          final_delay = clap_final_delay;
          Serial.print("(CLAP2[");
          Serial.print(mic_value);
          Serial.print("])");
          Serial.print("(");
          Serial.print(toggle_count++);
          Serial.print(")");
        }
      }
    }
  }


//******************************************
//*************  Update Stuff  *************
//******************************************
  if (lightIsOn) { //Update the light
    //We used to have an indicator light.
    if (TOGGLE_RELAY) {
      digitalWrite(relay_pin, HIGH);
    }
  } else {
    digitalWrite(relay_pin, LOW);
  }
  
  if (clap_enabled) { //update clap indicator LED
    digitalWrite(led_p1, HIGH);
  } else {
    digitalWrite(led_p1, LOW);
  }


  //delay afterwards so we don't get things like repeat claps or button readings
  delay(final_delay);
  final_delay = 0;

}







void writeMessage(char message[]) {
  static int length = sizeof(message); // static makes the variable persist between function calls.
  for (int i = 0; i < length; i++) {
    server.write(message[i]);
  }
}

void writeMessage(char message[], unsigned int count) {
  for (int i = 0; i < count; i++) {
    server.write(message[i]);
  }
}







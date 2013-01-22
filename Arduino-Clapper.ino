/*

Ethernet-enabled Clapper and other goodies, Version 2

*/
#include <SPI.h>
#include <Ethernet.h>

// Ethernet 
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEF};
byte ip[] = {192,168,69,8};
byte subnet_mask[] = {255,255,255,0};
byte gateway[] = {192,168,69,1};
boolean welcomed_new_client = false;
unsigned int message_length = 0;
unsigned int message_length_limit = 512; // upper limit is 65535
char message[512]; // can't use a variable in array length
char temp_char;
Server server(23); // port 23

// mic
boolean heard_first_clap = false;
unsigned long clap_time;
unsigned long single_clap_timeout = 390; //time before resetting after only 1 clap
unsigned int mic_value;
unsigned int first_clap_delay = 40; //the pause after getting a clap, to avoid picking up echoes
unsigned int clap_threshold = 770; //minimum amplitude of a clap. Low values pick up noise
unsigned int clap_final_delay = 900; //timeout after a clap has happened. this timeout is needed because I don't have a capacitor on the mic amplifier circuit, so it registers two claps after a toggle (one for the LED and one for the relay)

// button
unsigned long button_push_time;
unsigned long button_longpress_delay = 700;
unsigned int button_final_delay = 300;
boolean button_longpress_confirmed = false;
boolean holding_button = false;  //if button is currently depressed (usually means a long-presss)

// program
boolean light_is_on = false;
boolean clapper_enabled = true;
unsigned int final_delay = clap_final_delay;


// pins
int led_pin_1 = 3;
int led_pin_2 = 4;
int button_pin_1 = 6;
int button_pin_2 = 7;
int mic_pin = 14;
int pulldown_pin = 5; // TODO: rewire so this isn't needed
int relay_pin = 9;


void setup() {
  // init ethernet and start telnet server
  Ethernet.begin(mac, ip, gateway, subnet_mask);
  server.begin();
  
  Serial.begin(9600);
  
  // set up pins
  pinMode(button_pin_1, OUTPUT);
  pinMode(button_pin_2, INPUT);
  pinMode(led_pin_1, OUTPUT);
  pinMode(led_pin_2, OUTPUT);
  pinMode(relay_pin, OUTPUT);
  pinMode(pulldown_pin, OUTPUT);
  digitalWrite(button_pin_1, HIGH);
  digitalWrite(pulldown_pin, HIGH);
}


void loop() {
  // handle inputs from the button
  if ((digitalRead(button_pin_1)) && (holding_button == false)) {
    holding_button = true;
    button_push_time = millis(); // save the current time
    Serial.print("(ButtonPress)");
  }
  
  if (holding_button) {
    if (digitalRead(button_pin_1)) {
      if ((button_push_time + button_longpress_delay < millis()) && (button_longpress_confirmed == false)) {
        // If longpress, toggle clapper
        button_longpress_confirmed = true;
        clapper_enabled = !clapper_enabled;
        writeMessage("Clapper toggled by button\r\n");
      }
    } else {
      if (button_longpress_confirmed == false) {
        // if not longpress, toggle the light
        light_is_on = !light_is_on;
        writeMessage("Light toggled by button\r\n");
      }
      holding_button = false;
      button_longpress_confirmed = false;
      Serial.print("(ButtonRelease)");
      final_delay = button_final_delay;
    }
  }
  
  // Telnet client stuff
  Client client = server.available(); //wait for a client to connect
  if (client) {  // true when client sends data
    if (!welcomed_new_client) {
      Serial.println("We have a new client");
      client.println("Hi there!");
      welcomed_new_client = true;
    }
    temp_char = client.read();  // read 1 byte from the client's message
    if (temp_char == '\n') {
      writeMessage(message, message_length);
      writeMessage("\r\n", 2);
      message_length = 0;
    } else {
      if (message_length < message_length_limit) {
        message[message_length] = temp_char;
        message_length++;
      }
    }
    
    // also print message to the serial port
    switch (temp_char) {
      case '\b':  // backspace
        Serial.print('<');
        break;
      default:
        Serial.print(temp_char);
    }
    
    // if the telnet client wants to change the light
    if (temp_char == '1') {
      light_is_on = true;
      message_length--;  // ignore this byte
      // print a message about this
      char msg[] = "(LightON)";
      writeMessage(msg);
      Serial.print(msg);
    } 
    else if (temp_char == '0') {
      light_is_on = false;
      message_length--;  // ignore this byte
      // print a message about this
      char msg[] = "(LightOFF)";
      writeMessage(msg);
      Serial.print(msg);
    }
  } //end of ethernet client section
  
  // Clapper section
  if (clapper_enabled) {
    if (heard_first_clap == false) {
      // we're listening for the first clap
      mic_value = analogRead(mic_pin);
      if (mic_value > clap_threshold) {
        Serial.print("(Clap1)");
        delay(first_clap_delay);
        clap_time = millis();
      }
    } else {
      // we're listening for the second clap
      if (clap_time + single_clap_timeout < millis()) { // if timeout
        heard_first_clap = false;
        Serial.print("(ClapTimeout)");
      }
      mic_value = analogRead(mic_pin);
      if (mic_value > clap_threshold) {
        light_is_on = !light_is_on;
        Serial.print("(Clap2)");
        writeMessage("Light toggled by clap\r\n");
        final_delay = clap_final_delay;
      }
    }
  }
  
  
  // ********************************
  // Update things based on variables
  if (light_is_on) {
    digitalWrite(relay_pin, HIGH);
  } else {
    digitalWrite(relay_pin, LOW);
  }
  
  if (clapper_enabled) {
    digitalWrite(led_pin_1, HIGH);
  } else {
    digitalWrite(led_pin_1, LOW);
  }
  
  
  //delay after doing everything for various reasons, such as:
  // - lack of smooth power to the mic amp, so it picks up power spikes as a clap
  // - lack of debounce on the button (may not be a problem anymore)
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
  for (unsigned int i = 0; i < count; i++) {
    server.write(message[i]);
  }
}


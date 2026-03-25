#include <Arduino.h>

// Define pins
const int tagPin = 23;  // IO17 input
const int anchorPin = 21;  // IO22 input
const int outputPin = 22;  // IO21 output

// Status flags
bool is_Tag = false;
bool is_Anchor = false;

bool isTag();
void checksum_setup();

bool isTag() {
  checksum_setup(); // Call checksum_setup to configure pins

  while(is_Tag == false && is_Anchor == false) {
    // Read the state of the pins
    is_Tag = digitalRead(tagPin);
    is_Anchor = digitalRead(anchorPin);
 
    // Determine device role based on jumper settings
    if (is_Tag && !is_Anchor) {
      is_Tag = true;
      is_Anchor = false;
      digitalWrite(outputPin, LOW); // Set output pin to LOW
      return true; // Device is configured as TAG
    } else if (!is_Tag && is_Anchor) {
        is_Anchor = true;
        is_Tag = false;
        digitalWrite(outputPin, LOW); // Set output pin to LOW
        return false; // Device is configured as ANCHOR
    }
  }


}

void checksum_setup() {
  // Configure input pins
  pinMode(tagPin, INPUT);
  pinMode(anchorPin, INPUT);

  // Configure output pin and set to HIGH
  pinMode(outputPin, OUTPUT);
  digitalWrite(outputPin, HIGH);  
}
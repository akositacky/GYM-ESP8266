#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <Servo.h>

const int BUFFER_SIZE = 14; // RFID DATA FRAME FORMAT: 1byte head (value: 2), 10byte data (2byte version + 8byte tag), 2byte checksum, 1byte tail (value: 3)
const int DATA_SIZE = 10; // 10byte data (2byte version + 8byte tag)
const int DATA_VERSION_SIZE = 2; // 2byte version (actual meaning of these two bytes may vary)
const int DATA_TAG_SIZE = 8; // 8byte tag
const int CHECKSUM_SIZE = 2; // 2byte checksum

SoftwareSerial ssrfid(10,11); 
SoftwareSerial mySerial(2, 3); // RX, TX

uint8_t buffer[BUFFER_SIZE]; // used to store an incoming data frame 
int buffer_index = 0;
const int buzzerPin = 7; //buzzer attach to digital 7
const int ledPin = 4; //LED green light
const int ledPin2 = 5; //LED red light

long lasttag;

// Interval before we process the same RFID
int INTERVAL = 10000;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
bool isNewCard = false;

const byte numChars = 32;
char receivedChars[numChars];;

boolean newData = false;
Servo myServo;  // create servo object to control a servo

int lockedPos = 15;
int unlockPos = 180;

void setup() {
  Serial.begin(115200); 
  mySerial.begin(4800);
  mySerial.setTimeout(5000);

  ssrfid.begin(9600);
  ssrfid.listen(); 

  pinMode(buzzerPin, OUTPUT);

  Serial.println("Waiting for RFID...");
  playAuthorized();

  myServo.attach(6);  // attaches the servo on pin 6 to the servo object
  myServo.write(lockedPos);   

  turnOnGreenLED();
}

void loop() {
  if (ssrfid.available() > 0){
    bool call_extract_tag = false;
    
    int ssvalue = ssrfid.read(); // read 
    if (ssvalue == -1) { // no data was read
      return;
    }

    if (ssvalue == 2) { // RDM630/RDM6300 found a tag => tag incoming 
      buffer_index = 0;
    } else if (ssvalue == 3) { // tag has been fully transmitted       
      call_extract_tag = true; // extract tag at the end of the function call
    }

    if (buffer_index >= BUFFER_SIZE) { // checking for a buffer overflow (It's very unlikely that an buffer overflow comes up!)
      Serial.print("_");
      return;
    }
    
    buffer[buffer_index++] = ssvalue; // everything is alright => copy current value to buffer

    if (call_extract_tag == true) {
      if (buffer_index == BUFFER_SIZE) {
        extract_tag();
      } else { // something is wrong... start again looking for preamble (value: 2)
        buffer_index = 0;
        return;
      }
    }    
  }    
}

unsigned extract_tag() {
    uint8_t msg_head = buffer[0];
    uint8_t *msg_data = buffer + 1; // 10 byte => data contains 2byte version + 8byte tag
    // uint8_t *msg_data_version = msg_data;
    uint8_t *msg_data_tag = msg_data + 2;
    // uint8_t *msg_checksum = buffer + 11; // 2 byte
    // uint8_t msg_tail = buffer[13];

    currentMillis = millis();
    String IncomingString="";
    long tag = hexstr_to_value(msg_data_tag, DATA_TAG_SIZE);
    if(tag != lasttag){
      lasttag = tag;
      isNewCard = true;
    }
    else{
      if (currentMillis - previousMillis >= INTERVAL) {
        isNewCard = true;
      } else {
        isNewCard = false;
      }
    }

    if (isNewCard) {
      previousMillis = currentMillis;
      playNotAuthorized();
      mySerial.begin(4800);

      Serial.print("Sending data to ESP-01 :: ");
      Serial.println(tag);
      mySerial.println(tag);
      Serial.println("Waiting for response from ESP-01....");

      int iCtr = 0;
      while (!mySerial.available()) {
        iCtr++;
        if (iCtr >= 40 || mySerial.available())
          break;
        delay(50);
      }
      
      mySerial.listen();
  
      if (mySerial.available() > 0) {

        IncomingString=mySerial.readString();
        StaticJsonDocument<200> doc1;
        deserializeJson(doc1, IncomingString);
        const char* isSuccess = doc1["is_success"];
        Serial.println(isSuccess);
        Serial.println(IncomingString);

        if (strcmp(isSuccess, "USE") == 0) {
          playAuthorized();
          turnOnRedLED();
          turnOffGreenLED();
          myServo.write(unlockPos);   
        } else if (strcmp(isSuccess, "NEXT") == 0) { //light up the RED LED
          playAuthorized();
          turnOffRedLED();
          turnOnGreenLED();
          myServo.write(lockedPos);   
        } else {     // If not authorized then sound the buzzer
          playNotAuthorized();
        }

        char rc;
        rc = mySerial.read();
        Serial.println(rc);
        Serial.println(mySerial.available());

        previousMillis = currentMillis + INTERVAL;
 
      }

      Serial.println("Finished processing response from ESP-01....");
    }

    ssrfid.listen(); 
    isNewCard = false;
    return tag;
}

long hexstr_to_value(char *str, unsigned int length) { // converts a hexadecimal value (encoded as ASCII string) to a numeric value
  char* copy = malloc((sizeof(char) * length) + 1); 
  memcpy(copy, str, sizeof(char) * length);
  copy[length] = '\0'; 
  // the variable "copy" is a copy of the parameter "str". "copy" has an additional '\0' element to make sure that "str" is null-terminated.
  long value = strtol(copy, NULL, 16);  // strtol converts a null-terminated string to a long value
  free(copy); // clean up 
  return value;
}

void playNotAuthorized() {
  tone(buzzerPin, 1500);
  delay(100);
  noTone(7);
}

void playAuthorized() {
  tone(buzzerPin, 1700);
  delay(200);
  noTone(7);
  tone(buzzerPin, 1900);
  delay(500);
  noTone(7);
}

void turnOnGreenLED() {
  digitalWrite(ledPin, HIGH);
}

void turnOffGreenLED() {
  digitalWrite(ledPin, LOW);
  delay(50);
}

void turnOnRedLED() {
  digitalWrite(ledPin2, HIGH);
  delay(50);
}

void turnOffRedLED() {
  digitalWrite(ledPin2, LOW);
  delay(50);
}

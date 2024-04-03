#include <StreamUtils.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <Servo.h>

Servo myservo;  // create servo object to control a servo

SoftwareSerial mySerial(2, 3); // RX, TX

MFRC522 mfrc522(10, 9);   // Create Instance of MFRC522 mfrc522(SS_PIN, RST_PIN)

const int buzzerPin = 7; //buzzer attach to digital 7
const int ledPin = 4; //LED green light
const int ledPin2 = 5; //LED red light

int isWifiInit = 0;
bool isRead = false;
bool isNewCard = false;
String tagContent = "";
String currentUID = "";

int servoPin = 6;

// Interval before we process the same RFID
int INTERVAL = 2000;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;

const byte numChars = 32;
char receivedChars[numChars];

boolean newData = false;
boolean wifiAvailable = false;

int pos = 0; 

void setup()
{
  Serial.begin(115200);
  mySerial.begin(4800);
  mySerial.setTimeout(5000);
  // Setup the buzzer
  pinMode(buzzerPin, OUTPUT);
  // Setup the led
  pinMode(ledPin, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  // Setup Servo
  myservo.attach(6);  // attaches the servo on pin 9 to the servo object

  
  SPI.begin();
  mfrc522.PCD_Init();

  Serial.println("Detecting RFID Tags");
  mfrc522.PCD_DumpVersionToSerial();
  
  // Serial.println(mySerial.available());

  // while (!mySerial.available()) {
  //   delay(500);
  //   Serial.print(".");
  //   if (mySerial.available()){
  //     turnOnRedLED();
  //     // String IncomingString=mySerial.readString();
  //     // Serial.print(IncomingString);
  //     break;
  //   }
  // }

  // int hey = 0;
  // while (!mySerial.available()) {
  //   hey++;
  //   delay(500);
  //   Serial.print(".");
  //   // if (hey >= 200)
  //   //   break;
  //   if (mySerial.available()){
  //     break;
  //   }
  //   delay(100);
  // }

  // if (mySerial.available()){
  // alternateLED();
}

void loop()
{ 
  recvWithStartEndMarkers();
  showNewData();
  readRFID();
}

void readRFID() {

  String IncomingString="";
  boolean StringReady = false;

  // Look for new cards
  if (mfrc522.PICC_IsNewCardPresent())
  {
    // Select one of the cards
    if (mfrc522.PICC_ReadCardSerial())
    {
      isRead = true;

      byte letter;
      for (byte i = 0; i < mfrc522.uid.size; i++)
      {
        tagContent.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
        tagContent.concat(String(mfrc522.uid.uidByte[i], HEX));
      }

      tagContent.toUpperCase();
    }
    if (isRead) {
      currentMillis = millis();
      // If current UID is not equal to current UID..meaning new card, then validate if it has access
      if (currentUID != tagContent) {
        currentUID =  tagContent;
        isNewCard = true;
      } else {
        // If it is the same RFID UID then we wait for interval to pass before we process the same RFID
        if (currentMillis - previousMillis >= INTERVAL) {
          isNewCard = true;
        } else {
          isNewCard = false;
        }
      }
      if (isNewCard) {
        if (tagContent != "") {
          // turn off LED if it is on for new card
          // turnOffLED();
          // loadingLED();
          playNotAuthorized();

          previousMillis = currentMillis;
          //Send the RFID Uid code to the ESP-01 for validation
          Serial.print("Sending data to ESP-01 :: ");
          // Serial.println(tagContent);
          //TODO Process read from serial here
          mySerial.println(tagContent);
          Serial.println("mySerialtagContent =xx" + urlencode(tagContent) + "xx");
          Serial.println("Waiting for response from ESP-01....");

          int iCtr = 0;
          while (!mySerial.available()) {
            // Serial.println(iCtr);
            iCtr++;
            if (iCtr >= 200)
              break;
            // if (mySerial.available())
            //   break;
            delay(50);
          }
          if (mySerial.available()) {
            // bool isAuthorized = isUserAuthorized(tagContent);
            
            IncomingString=mySerial.readString();
            StringReady=true;

            if(StringReady){
              Serial.println(IncomingString);
            }

            StaticJsonDocument<200> doc1;
            deserializeJson(doc1, IncomingString);
            const char* isSuccess = doc1["is_success"];
            Serial.println(isSuccess);

            if (isSuccess == "true") { //light up the RED LED
              turnOnLED();
              turnOnRedLED();
              playAuthorized();
              for (pos = 0; pos <= 180; pos += 2) { // goes from 0 degrees to 180 degrees
                // in steps of 1 degree
                Serial.println(pos);
                myservo.write(pos);              // tell servo to go to position in variable 'pos'
                delay(15);                       // waits 15 ms for the servo to reach the position
              }
            } else {     // If not authorized then sound the buzzer
              alternateLED();
              turnOnGreenLED();
              playNotAuthorized();
            }
          }
          Serial.println("Finished processing response from ESP-01....");
        }

      }

    } else {
      Serial.println("No card details was read!");
    }
    tagContent = "";
    isNewCard = false;
  }
}

void recvWithStartEndMarkers() {
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;

    while (mySerial.available() > 0 && newData == false) {
        rc = mySerial.read();
        // turnRedLED();
        // Serial.print(rc);
        if (rc != endMarker) {
            receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= numChars) {
                ndx = numChars - 1;
            }
        }
        else {
            receivedChars[ndx] = '\0'; // terminate the string
            ndx = 0;
            newData = true;
        }
    }
}

void showNewData() {
    if (newData == true) {
        Serial.print("This just in ... ");
        Serial.println(receivedChars);
        newData = false;
    }
}

bool isUserAuthorized(String tagContent) {
  const size_t capacity = JSON_OBJECT_SIZE(1) + 30;
  DynamicJsonDocument doc(capacity);

  // Serial.print("My capacity : ");
  // Serial.println(capacity);

  // Use during debugging
  //  ReadLoggingStream loggingStream(mySerial, Serial);
  //  DeserializationError error = deserializeJson(doc, loggingStream);

  // Use during actual running
  DeserializationError error = deserializeJson(doc, mySerial);
  if (error) {
    //    Serial.print(F("deserializeJson() failed: "));
    // Serial.println(error.c_str());
    return error.c_str();
  }

  bool   is_authorized = doc["is_authorized"] == "true";
  doc["wow"] = "+93+1C+7F+24%0D";
    // Serial.println(is_authorized);
  serializeJson(doc, Serial);
  return is_authorized;
}


String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        // encodedString+=code2;
      }
      yield();
    }
    return encodedString;
    
}

void loadingLED() {
  while (true) {
    digitalWrite(ledPin, HIGH);
    digitalWrite(ledPin2, LOW);
    delay(500);
    digitalWrite(ledPin, LOW);
    digitalWrite(ledPin2, HIGH);
    delay(500);
  }
}

void turnOnEntranceLED() {
  digitalWrite(ledPin, HIGH);
  delay(200);
  digitalWrite(ledPin, LOW);
  delay(500);
  digitalWrite(ledPin, HIGH);
  delay(200);
  digitalWrite(ledPin, LOW);
}

void playNotAuthorized() {
  tone(buzzerPin, 1500);
  delay(500);
  noTone(7);
}

void playAuthorized() {
  tone(buzzerPin, 1700);
  delay(1000);
  noTone(7);
  tone(buzzerPin, 1900);
  delay(1000);
  noTone(7);
}

void alternateLED() {
  digitalWrite(ledPin, HIGH);
  digitalWrite(ledPin2, HIGH);
  delay(1000);
  digitalWrite(ledPin, LOW);
  digitalWrite(ledPin2, LOW);
}

void turnOnLED() {
  digitalWrite(ledPin, HIGH);
  delay(1000);
  digitalWrite(ledPin, LOW);
}

void turnOnGreenLED() {
  digitalWrite(ledPin, HIGH);
}

void turnOffGreenLED() {
  digitalWrite(ledPin, LOW);
}

void turnOnRedLED() {
  digitalWrite(ledPin2, HIGH);
}

void turnRedLED() {
  digitalWrite(ledPin2, HIGH);
  delay(1000);
  digitalWrite(ledPin2, LOW);
}
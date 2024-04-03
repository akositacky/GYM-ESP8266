#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>

// NOTE:  CHANGE THIS TO YOUR WIFI SSID AND PASSWORD
const char* ssid     = "Shoyou Boi";
const char* password = "@Siomairice26";

// **** FIREBASE ****
//Define Firebase Data object
FirebaseData fbdo;

// Insert Firebase project API Key
#define API_KEY "AIzaSyAfbqSsHXE5vmHkaf3c_QjzfewwoyxQaeo"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://gym-locker-default-rtdb.asia-southeast1.firebasedatabase.app/" 

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

String rfidRead = "";
String pendingRfid = "";
String status = "";
String pR = "";

void setup() {
  Serial.begin(4800);
  delay(10);

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
   /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

const byte numChars = 20;
char receivedChars[numChars];   // an array to store the received data

boolean newData = false;

void loop() {
    recvWithEndMarker();
    processRFIDCode();
}

void recvWithEndMarker() {
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;
   
    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

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

void processRFIDCode() {
    if (newData == true) {
        newData = false;
        isUserAuthorized(receivedChars);
    }
}

void isUserAuthorized(String rfIdCode){
    // wait for WiFi connection
  if ((WiFi.status() == WL_CONNECTED)) {
    String encodedRFIDCode = urlencode(rfIdCode);

    if (Firebase.ready() && signupOK) {
      // String scannedRFID = "93 1C 7F 24"; //rfidScanned

      Firebase.getString(fbdo, F("equipments/gymEq1/status"));
      status = fbdo.to<const char *>();
      Firebase.getString(fbdo, F("equipments/gymEq1/RFID"));
      pR = fbdo.to<const char *>();

      if (status == "PENDING") {
        if (pR == encodedRFIDCode) {
          Firebase.setString(fbdo, F("equipments/gymEq1/status"), "USE");
          String jsonObject = "{\"is_success\":\"true\"}";
          Serial.println(jsonObject);
        } else {
          String jsonObject = "{\"is_success\":\"false\",\"rfid\":\""+ encodedRFIDCode +"\",\"pR\":\""+ pR +"\",\"status\":\""+ status +"\"}";
          Serial.println(jsonObject);
        }
      } else {
        String jsonObject = "{\"is_success\":\"PENDING\",\"rfid\":\""+ encodedRFIDCode +"\",\"pR\":\""+ pR +"\",\"status\":\""+ status +"\"}";
        Serial.println(jsonObject);
      }
     
    }
  }
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
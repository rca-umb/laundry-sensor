#include <SPI.h>
#include "base64.hpp"
#include <WiFi101.h>
#include "email_details.h"
#include "network_details.h"

char emailAddr[] = EMAIL_ADDR;
char emailPass[] = EMAIL_PASS;
char emailName[] = EMAIL_NAME;
char emailLink[] = EMAIL_LINK;
char emailDest[] = EMAIL_DEST;

char wifiSSID[] = WIFI_SSID;
char wifiPass[] = WIFI_PASS;

int status = WL_IDLE_STATUS;
int led = LED_BUILTIN;

WiFiClient httpClient;
WiFiServer server(80);

float vibLimit;
float currentVibs;

unsigned long lastReadingTime = 0;
const unsigned long readingInterval = 120000; // 2 minutes

int errorCode = 0;
bool retryEmail = true;

void setup() {
  Serial.begin(9600);
  WiFi.setPins(8,7,4,2);
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(wifiSSID);
    status = WiFi.begin(wifiSSID, wifiPass);
    delay(1000);
  }
  Serial.println("Connected to wifi.");

  server.begin();
  vibLimit = readVibration() * 1.1;
  Serial.print("Email will be sent when vibration drops below: ");
  Serial.println(vibLimit);
  lastReadingTime = millis();
  digitalWrite(led, 1);
}

void loop() {  
  unsigned long currentTime = millis();
  if (currentTime - lastReadingTime >= readingInterval) {
    Serial.println("Measuring vibrations.");
    currentVibs = readVibration();
    lastReadingTime = currentTime;
    if (currentVibs < vibLimit) { // Cycle done
      bool emailSent = false;
      while (retryEmail) {
        emailSent = sendEmail();
        if (emailSent) {
          Serial.println("Email sent successfully.");
          exit(0);
        } else {
          Serial.println("Email failed to send.");
          delay(2000);
        }
      }
      ledErrorBlink();
    }
  }

}

float readVibration() {
  int val = 0;
  // Take one reading every 0.1 seconds for a minute.
  for (int i = 0; i < 600; i++) {
    val += analogRead(A4);
    delay(100);
  }
  float aveVal = float(val)/600.0;
  String time = getCurrentTime();
  Serial.println("Value at " + time + ": " + String(aveVal, 2));
  return aveVal;
}

String getCurrentTime() {
  if (httpClient.connect("www.worldtimeapi.org", 80)) {
    Serial.println("Client connected.");
    httpClient.println("GET /api/ip HTTP/1.1");
    httpClient.println("Host: www.worldtimeapi.org");
    httpClient.println("Connection: close");
    httpClient.println();
  } else {
    Serial.println("Connection to client failed.");
  }

  String key;
  String val;
  String hour;
  String minute;
  while (httpClient.connected()) {
    char c = httpClient.read();
    // Look for start of key:value JSON response.
    if (c == '"') {
      // Look for end of key name.
      while (true) {
        char c = httpClient.read();
        if (c == '"') {
          break;
        }
        key += c;
      }
      if (key == "datetime") {
        // Look for start of the "time" part of the datetime value.
        while (true) {
          char c = httpClient.read();
          if (c == 'T') {
            break;
          }
        }
        // Look for the ":" delimiter of the datetime value.
        while (true) {
          char c = httpClient.read();
          if (c == ':' && hour == "") {
            hour = val;
            val = "";
          } else if (c == ':') {
            minute = val;
            break;
          } else {
            val += c;
          }
        }
        break;
      } else {
        key = "";
      }
    }
  }
  String time = convert24To12(hour, minute);
  return time;
}

String convert24To12(String hour, String minute) {
  String time;
  // Convert String to an integer.
  int firstDigit = hour[0] - 48;
  int secondDigit = hour[1] - 48;
  int intHour = firstDigit * 10 + secondDigit;

  if (!intHour) {
    // Hour 00 is 12 in the morning
    time = "12:" + minute + " AM";
  } else if (intHour == 12) {
    time = "12:" + minute + " PM";
  } else if (intHour < 12) {
    time = hour + ":" + minute + " AM";
  } else {
    // Convert 24 hour format to 12 hour.
    intHour = intHour - 12;
    firstDigit = intHour / 10;
    secondDigit = intHour % 10;
    // Convert integer to String.
    String strHour;
    strHour += char(firstDigit + 48);
    strHour += char(secondDigit + 48);
    time = strHour + ":" + minute + " PM";
  }
  return time;
}

bool sendEmail() {
  WiFiSSLClient sslClient;
  // Gmail's supports TLS on port 587 AND 465. The former relies
  // on STARTTLS which is not natively supported by the WiFi101 
  // lib, but the latter uses implict TLS (SMTPS) which can work.
  errorCode += 1000; //  error 1xxx
  if (!sslClient.connect(EMAIL_LINK, 465)) {
    Serial.println("Connection to email server failed.");
    return false;
  }
  Serial.println("Connected to email server.");

  // Initiate SMTP conversation with TLS
  errorCode += 1000; // error 2xxx
  sslClient.println("EHLO localhost");
  if (!readResponse(sslClient)) return false;
 
  // Auth with base64 encoded credentials
  errorCode += 1000; // error 3xxx
  sslClient.println("AUTH LOGIN");
  if (!readResponse(sslClient)) return false;
  // Encode username
  errorCode += 1000; // error 4xxx
  int inputLen = strlen(EMAIL_ADDR);
  int encodedLen = ((inputLen + 2) / 3) * 4 + 1;
  unsigned char encodedUser[encodedLen];
  unsigned int base64_length = encode_base64((unsigned char*)EMAIL_ADDR, inputLen, encodedUser);
  sslClient.println((char*)encodedUser);
  if (!readResponse(sslClient)) return false;
  // Encode password
  errorCode += 1000; // error 5xxx
  inputLen = strlen(EMAIL_PASS);
  encodedLen = ((inputLen + 2) / 3) * 4 + 1;
  unsigned char encodedPass[encodedLen];
  base64_length = encode_base64((unsigned char*)EMAIL_PASS, inputLen, encodedPass);
  sslClient.println((char*)encodedPass);
  if (!readResponse(sslClient)) return false;

  // Prepare email content
  errorCode += 1000; // error 6xxx
  sslClient.println("MAIL FROM:<" + String(EMAIL_ADDR) + ">");
  if (readResponse(sslClient)) return false;
  errorCode += 1000; // error 7xxx
  sslClient.println("RCPT TO:<" + String(EMAIL_DEST) + ">");
  if (!readResponse(sslClient)) return false;
  errorCode += 1000; // error 8xxx
  sslClient.println("DATA");
  if (!readResponse(sslClient)) return false;
  errorCode += 1000; // error 9xxx
  sslClient.println("From: " + String(EMAIL_NAME) + " <" + String(EMAIL_ADDR) + ">");
  sslClient.println("To: <" + String(EMAIL_DEST) + ">");
  sslClient.println("Subject: Laundry Complete");
  sslClient.println("Content-Type: text/html");
  sslClient.println();
  sslClient.println("Hello from the laundry detection device.</br></br>Your washer or dryer cycle has finished!");
  sslClient.println(".");
  if (!readResponse(sslClient)) return false;
  
  sslClient.println("QUIT");
  sslClient.stop();
  return true;
}

bool readResponse(WiFiSSLClient &client) {
  unsigned long timeout = millis() + 30000; // 10 second timeout
  String response = "";
  
  while (millis() < timeout) {
    while (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.print("Server: ");
      Serial.println(line);
      response = line;
      
      if (line.length() >= 3) {
        char continuation = line.charAt(3);
        
        // If this is the final response line (space after status code, not dash)
        if (continuation == ' ') {
          char statusCode = line.charAt(0);
          if (statusCode == '2' || statusCode == '3') {
            return true;
          } else if (statusCode == '4') {
            // https://support.google.com/a/answer/3221692?sjid=15376637176408798280-NA
            // "Error codes that start with a 4 indicate the server had temporary failure 
            // but the action will be completed with another try."
            return false;
          } else {
            String fullCode = line.substring(0, 3);
            errorCode += fullCode.toInt();
            retryEmail = false;
            return false;
          }
        }
        // If continuation == '-', keep reading more lines
      }
    }
    delay(100);
  }
  
  Serial.println("Timeout waiting for server response");
  return false;
}

void ledErrorBlink() {
  // Blink error code on built-in LED for debugging without serial access.
  // LED will be blank for 5 seconds to indicate start of error code.
  // First blink indicates where in the code the error occurred:
  // 1xxx - SMTP connection failed
  // 2xxx - EHLO failed
  // 3xxx - AUTH LOGIN failed
  // 4xxx - Username encoding/sending failed
  // 5xxx - Password encoding/sending failed
  // 6xxx - MAIL FROM failed
  // 7xxx - RCPT TO failed
  // 8xxx - DATA command failed
  // 9xxx - Email content sending failed
  // Subsequent blinks indicate specific error code defined by SMTP standard, 
  // if any, otherwise, it was likely a timeout.
  if (errorCode < 1) {
    while (true) {
      digitalWrite(led, HIGH);
      delay(100);
      digitalWrite(led, LOW);
      delay(100);
    }
  } else {
    int thousands = errorCode / 1000;
    int hundreds = (errorCode / 100) % 10;
    int tens = (errorCode / 10) % 10;
    int ones = errorCode % 10;
    while (true) {
      digitalWrite(led, LOW);
      delay(5000);
      for (int i = 0; i < thousands; i++) {
        digitalWrite(led, HIGH);
        delay(300);
        digitalWrite(led, LOW);
        delay(300);
      }
      delay(1000);
      for (int i = 0; i < hundreds; i++) {
        digitalWrite(led, HIGH);
        delay(300);
        digitalWrite(led, LOW);
        delay(300);
      }
      delay(1000);
      for (int i = 0; i < tens; i++) {
        digitalWrite(led, HIGH);
        delay(300);
        digitalWrite(led, LOW);
        delay(300);
      }
      delay(1000);
      for (int i = 0; i < ones; i++) {
        digitalWrite(led, HIGH);
        delay(300);
        digitalWrite(led, LOW);
        delay(300);
      }
    }
  }
}
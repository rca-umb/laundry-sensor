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

const float vibLimit = 1.0;
float currentVibs;

unsigned long lastReadingTime = 0;
const unsigned long readingInterval = 120000; // 2 minutes

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
      const bool emailSent = sendEmail();
      if (emailSent) {
        Serial.println("Email sent successfully.");
      } else {
        Serial.println("Email failed to send.");
      }
      exit(0);
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
    time = "12:" + minute + "AM";
  } else if (intHour < 12) {
    time = hour + ":" + minute + " AM";
  } else {
    // Convert 24 hour format to 12 hour.
    intHour = (intHour - 12) || 12;
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
  if (!sslClient.connect(EMAIL_LINK, 465)) {
    Serial.println("Connection to email server failed.");
    return false;
  }
  Serial.println("Connected to email server.");

  if (!readResponse(sslClient)) return false;
  sslClient.println("EHLO " + String(EMAIL_ADDR));
  if (!readResponse(sslClient)) return false;

  // Auth with base64 encoded credentials
  sslClient.println("AUTH LOGIN");
  if (!readResponse(sslClient)) return false;
  // Encode username
  int inputLen = strlen(EMAIL_ADDR);
  int encodedLen = ((inputLen + 2) / 3) * 4 + 1;
  unsigned char encodedUser[encodedLen];
  unsigned int base64_length = encode_base64((unsigned char*)EMAIL_ADDR, inputLen, encodedUser);
  sslClient.println((char*)encodedUser);
  if (!readResponse(sslClient)) return false;
  // Encode password
  inputLen = strlen(EMAIL_PASS);
  encodedLen = ((inputLen + 2) / 3) * 4 + 1;
  unsigned char encodedPass[encodedLen];
  base64_length = encode_base64((unsigned char*)EMAIL_PASS, inputLen, encodedPass);
  sslClient.println((char*)encodedPass);
  if (!readResponse(sslClient)) return false;

  // Prepare email content
  sslClient.println("MAIL FROM:<" + String(EMAIL_ADDR) + ">");
  if (!readResponse(sslClient)) return false;
  sslClient.println("RCPT TO:<" + String(EMAIL_DEST) + ">");
  if (!readResponse(sslClient)) return false;
  sslClient.println("DATA");
  if (!readResponse(sslClient)) return false;
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
  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line.startsWith("2") || line.startsWith("3")) {
      return true;
    }
  }
  delay(1000);
  return true;
}
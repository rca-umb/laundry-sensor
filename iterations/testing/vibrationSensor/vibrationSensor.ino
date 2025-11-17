#include <SPI.h>
#include <WiFi101.h>
#include "network_details.h"

char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;

int status = WL_IDLE_STATUS;
int led = LED_BUILTIN;

WiFiClient httpClient;
WiFiServer server(80);

String data;
unsigned long lastReadingTime = 0;
const unsigned long readingInterval = 120000; // 2 minutes

void setup() {
  Serial.begin(9600);
  WiFi.setPins(8,7,4,2);
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(1000);
  }
  Serial.println("Connected to wifi.");

  server.begin(); 

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  Serial.println("Taking initial measurements.");
  data += readVibration() + "</br>";
  lastReadingTime = millis();
  digitalWrite(led, 1);
}

void loop() {  
  unsigned long currentTime = millis();
  if (currentTime - lastReadingTime >= readingInterval) {
    Serial.println("Measuring vibrations.");
    data += readVibration() + "</br>";
    lastReadingTime = currentTime;
  }

  WiFiClient serverClient = server.available();
  if (serverClient) {
    Serial.println("New client.");
    // an http request ends with a blank line
    bool currentLineIsBlank = true;
    while (serverClient.connected()) {
      if (serverClient.available()) {
        char c = serverClient.read();
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          serverClient.println("HTTP/1.1 200 OK");
          serverClient.println("Content-Type: text/html");
          serverClient.println("Connection: close");  // the connection will be closed after completion of the response
          serverClient.println();
          serverClient.println("<!DOCTYPE HTML>");
          serverClient.println("<html>");
          // output the value of each analog input pin
          serverClient.println("<h1>Average Vibration Values</h1>");
          serverClient.println(data);
          serverClient.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    serverClient.stop();
  }
}

String readVibration() {
  int val = 0;
  // Take one reading every 0.1 seconds for a minute.
  for (int i = 0; i < 600; i++) {
    val += analogRead(A4);
    delay(100);
  }
  float aveVal = float(val)/600.0;
  String time = getCurrentTime();
  return "Value at " + time + ": " + String(aveVal, 2);
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

  if (intHour < 12) {
    time = hour + ":" + minute + " AM";
  } else {
    // Convert 24 hour format to 12 hour.
    intHour -= 12;
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

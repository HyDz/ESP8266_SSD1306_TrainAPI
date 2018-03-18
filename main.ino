//#define DEBUG

// Json Parser
#include <ArduinoJson.h>
//Wifi
#include <ESP8266WiFi.h>

//Include the SSL client
#include <WiFiClientSecure.h>

// OLED Screen
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <ESP_SSD1306.h>

#define OLED_RESET 16

int yPos = 1;

ESP_SSD1306 display(OLED_RESET); // FOR I2C

char ssid[] = "Your_SSID";       // your network SSID (name)
char password[] = "Your_WPA_PASS";  // your network key


//Add a SSL client
WiFiClientSecure client;

// Address of the API host server
char hostAPI[] = "api-ratp.pierre-grimaud.fr";

//Request send to the API Server
char getRequest[] = "/v3/schedules/rers/B/denfert+rochereau/A+R?_format=json";
//Another example request try directly on the website they're swagger so you can directly test your requests
//char getRequest[] = "/v3/schedules/metros/14/bercy/A+R";

long checkDueTime;
#define checkDelay 60000 // 60 x 1000 (1 minute)
#define responsesfromAPI 11 // Number of trains from API response to parse

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC);

#ifdef DEBUG
  Serial.begin(115200);
#endif

  printIntro();

  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Attempt to connect to Wifi network:
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
#ifdef  DEBUG
    Serial.print(".");
#endif
    printErrorAPI("Connecting", "WiFi");
  }
  IPAddress ip = WiFi.localIP();

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(35, 3);
  display.println("Wifi Ok");
  display.setTextSize(1);
  display.setCursor(2, 30);
  display.print("IP: ");
  display.println(ip);
  display.display();
  delay(500);

#ifdef DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(ip);
#endif
}

void loop() {

  long now = millis();
  if (now >= checkDueTime) {
    parseAPI(getRequest);
    checkDueTime = now + checkDelay;
  }
}

void printIntro() {
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(35, 1);
  display.println("HyDz");
  display.setTextSize(2);
  display.setCursor(10, 40);
  display.println("Train API");
  display.display();
  delay(500);
}

void parseAPI(String request) {
  String headers = "";
  String body = "";
  bool finishedHeaders = false;
  bool currentLineIsBlank = true;
  bool gotResponse = false;
  long now;


  if (client.connect(hostAPI, 443)) {

#ifdef DEBUG
    Serial.println("connected");
    Serial.println(request);
#endif
    printErrorAPI("PARSE API", " ");
    client.println("GET " + request + " HTTP/1.1");
    client.print("Host: "); client.println(hostAPI);
    //client.println("User-Agent: nodeMCU_by_HyDz");
    client.println("");

    now = millis();
    // checking the timeout
    while (millis() - now < 1500) {
      while (client.available()) {
        char c = client.read();
        if (finishedHeaders) {
          body = body + c;
        } else {
          if (currentLineIsBlank && c == '\n') {
            finishedHeaders = true;
          }
          else {
            headers = headers + c;
          }
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
        gotResponse = true;
      }
    }
    if (gotResponse) {

      display.clearDisplay();
      display.setTextSize(2);

      body.remove(0, 3);

#ifdef DEBUG
      Serial.println(body);
#endif

      const size_t bufferSize = JSON_ARRAY_SIZE(10) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 11 * JSON_OBJECT_SIZE(3) + 780;
      DynamicJsonBuffer jsonBuffer(bufferSize);
      JsonObject& root = jsonBuffer.parseObject(body);

      if (root.success()) {

        JsonObject& result_code = root["result"];
        const char* resulted_code = result_code["code"];
        String error_code = result_code["code"].as<String>();
#ifdef DEBUG
        Serial.println(resulted_code);
#endif
        if (error_code.length() > 2) { // Check if they're an error code Code 500 = No Trains
          printErrorAPI("Erreur", error_code);
          return;
        }
        JsonArray& result_schedules = root["result"]["schedules"];
        for (int i = 0; i < responsesfromAPI; i++) {
          JsonObject& resulted_schedules = result_schedules[i];
          const char* result_schedules_code = resulted_schedules["code"]; // "ECCO"
          const char* result_schedules_message = resulted_schedules["message"]; // "Train à quai"
          const char* result_schedules_destination = resulted_schedules["destination"]; // "Aeroport Charles de Gaulle 2 TGV"
#ifdef DEBUG
          Serial.println(result_schedules_code);
          Serial.println(result_schedules_message);
          Serial.println(result_schedules_destination);
#endif
          String resulted_message = resulted_schedules["message"].as<String>();
          if ( resulted_message.length() <= 6) {  // Only Print train with a time and skip No Stop trains
            if (yPos < 61) {
#ifdef DEBUG
              Serial.print("yPos: ");
              Serial.println(yPos);
              Serial.println(result_schedules_code);
              Serial.println(result_schedules_message);
              Serial.println(result_schedules_destination);
#endif
              printTrain(result_schedules_message, result_schedules_destination, yPos);
              yPos += 16;

            } else {
              yPos = 1;
              return;
            }
          }
        }
      } else {
        printErrorAPI("API", body);
        return;
      }
    }
  } else {
    printErrorAPI("API CONNECT", "ERROR");
    return;
  }
}

void printTrain(const char* horaire, const char* dest, int yCurs) {

  String destination(dest);
  destination.remove(5);
#ifdef DEBUG
  Serial.println(destination);
#endif
  display.setCursor(1, yCurs);
  display.println(horaire);
  display.setCursor(65, yCurs);
  display.println(destination);
  display.display();
  delay(500);
}

void printErrorAPI(String error1, String error2) {
#ifdef DEBUG
  Serial.println(error1);
  Serial.println(error2);
#endif
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(1, 1);
  display.println(error1);
  display.setCursor(1, 18);
  display.println(error2);
  display.display();
  delay(500);
}

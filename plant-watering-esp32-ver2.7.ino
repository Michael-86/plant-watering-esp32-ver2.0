#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <AsyncTCP.h>
#include <NewPing.h>

typedef void function;

// WiFi credentials
const char *ssid = "gtfast";
const char *password = "darktitan01";

// Web server credentials (CHANGE THESE FOR SECURITY)
const char *http_username = "admin";
const char *http_password = "admin";

// Control pins
const int autoModePin = 2;     // GPIO for auto mode toggle
const int manualPumpPin = 33;  // GPIO for manual pump control
const int relay = 13;          // Relay output pin

// Timing
unsigned long previousMillis = 0;
const long interval = 1000;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

// DHT sensor
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Soil moisture
#define soilWet 1600   // Max value we consider soil 'wet'
#define soilDry 3200   // Min value we consider soil 'dry'
#define sensorPower 12 // Soil sensor power pin
#define sensorPin 36   // Soil sensor analog pin

// Water level
#define TRIGGER_PIN 14
#define ECHO_PIN 27
#define MAX_DISTANCE 200
const int tank1Full = 3;     // Depth when tank is full (cm)
const int tank1Empty = 23;   // Depth when tank is empty (cm)
const int minWaterLevel = 10; // Minimum water level percentage to allow pumping

// Light sensor
#define LIGHT_SENSOR_PIN 39

// Variables
char jordfuktighet[12] = "Unknown";
char autostate[4] = "on";
char pump[4] = "off";
char ljusstyrka[16] = "Unknown";
char pumprun[4] = "off";
float t = 0;
float h = 0;
int lvl = 0;
int h3 = 0;
int ljusraw = 0;
int jordfuktdata = 0;

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
AsyncWebServer server(80);

// Pump control logic
void handlePumpControl() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    lastDebounceTime = millis();
    
    // Update state variables from physical pins
    strcpy(autostate, digitalRead(autoModePin) ? "off" : "on");
    strcpy(pump, digitalRead(manualPumpPin) ? "on" : "off");
    
    bool shouldPumpRun = false;
    
    if (strcmp(autostate, "off") == 0) {
      // Manual mode - pump state is directly controlled by UI
      shouldPumpRun = (strcmp(pump, "on") == 0);
    } 
    else {
      // Automatic mode - smart watering logic
      bool soilIsDry = (strcmp(jordfuktighet, "För torr") == 0);
      bool lightIsLow = (strcmp(ljusstyrka, "Dunkelt") == 0 || strcmp(ljusstyrka, "Mörkt") == 0);
      bool tankHasWater = (lvl > minWaterLevel); // Fixed critical bug here
      
      shouldPumpRun = soilIsDry && lightIsLow && tankHasWater;
      
      Serial.print("Auto Conditions - SoilDry:");
      Serial.print(soilIsDry);
      Serial.print(" LightLow:");
      Serial.print(lightIsLow);
      Serial.print(" TankHasWater:");
      Serial.println(tankHasWater);
    }

    // Apply the decision
    digitalWrite(relay, shouldPumpRun ? HIGH : LOW);
    strcpy(pumprun, shouldPumpRun ? "On" : "Off");
    
    Serial.print("Final Decision - Pump:");
    Serial.println(shouldPumpRun ? "ON" : "OFF");
  }
}

// Light sensor reading
void readlight() {
  int analogValue = analogRead(LIGHT_SENSOR_PIN);
  ljusraw = analogValue;

  if (analogValue < 40) {
    strcpy(ljusstyrka, "Mörkt");
  } else if (analogValue < 800) {
    strcpy(ljusstyrka, "Dunkelt");
  } else if (analogValue < 2000) {
    strcpy(ljusstyrka, "Svagt ljus");
  } else if (analogValue < 3200) {
    strcpy(ljusstyrka, "Ljust");
  } else {
    strcpy(ljusstyrka, "Mycket Ljust");
  }
  ljusstyrka[sizeof(ljusstyrka)-1] = '\0'; // Ensure null-termination
}

// Soil moisture reading
void readsoilmoisture() {
  int moisture = readSensor();
  jordfuktdata = moisture;
  
  if (moisture < soilWet) {
    strcpy(jordfuktighet, "För våt");
  } else if (moisture >= soilWet && moisture < soilDry) {
    strcpy(jordfuktighet, "Är perfekt");
  } else {
    strcpy(jordfuktighet, "För torr");
  }
}

int readSensor() {
  digitalWrite(sensorPower, HIGH);
  delay(10);
  int val = analogRead(sensorPin);
  digitalWrite(sensorPower, LOW);
  return val;
}

// Water level measurement
void waterlvl() {
  unsigned int pingTime = sonar.ping();
  if (pingTime == NO_ECHO) {
    Serial.println("Ultrasonic sensor error");
    h3 = tank1Empty; // Safe default
  } else {
    h3 = sonar.convert_cm(pingTime);
    h3 = constrain(h3, 0, tank1Empty);
  }
  lvl = map(h3, tank1Empty, tank1Full, 0, 100);
  lvl = constrain(lvl, 0, 100);
}

// Temperature reading with validation
void readDHTTemperature() {
  float newReading = dht.readTemperature();
  if (!isnan(newReading) && newReading >= -40 && newReading <= 80) {
    t = newReading;
  } else {
    Serial.println("Invalid DHT temperature reading");
  }
}

// Humidity reading with validation
void readDHTHumidity() {
  float newReading = dht.readHumidity();
  if (!isnan(newReading) && newReading >= 0 && newReading <= 100) {
    h = newReading;
  } else {
    Serial.println("Invalid DHT humidity reading");
  }
}

// WiFi reconnection handler
void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    delay(5000); // Wait for connection
  }
}

// HTML templates remain unchanged from your original code
const char index_html[] PROGMEM = R"rawliteral(...)rawliteral";
const char logout_html[] PROGMEM = R"rawliteral(...)rawliteral";
const char rawdata_html[] PROGMEM = R"rawliteral(...)rawliteral";

String outputState(int output) {
  return digitalRead(output) ? "checked" : "";
}

String processor(const String &var) {
  if (var == "TEMPERATURE") {
    return String(t);
  } else if (var == "HUMIDITY") {
    return String(h);
  } else if (var == "Soilmoisture") {
    return String(jordfuktighet);
  } else if (var == "handleljus") {
    return String(ljusstyrka);
  } else if (var == "vatten") {
    return String(lvl);
  } else if (var == "Soilmoisture2") {
    return String(jordfuktdata);
  } else if (var == "handleljus2") {
    return String(ljusraw);
  } else if (var == "vatten2") {
    return String(h3);
  } else if (var == "pumprun") {
    return String(pumprun);
  } else if (var == "BUTTONPLACEHOLDER") {
    String buttons = "";
    buttons += "<h4>Auto läge PÅ - Auto läge AV</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"2\" " + outputState(2) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Pump AV - Pump PÅ</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"33\" " + outputState(33) + "><span class=\"slider\"></span></label>";
    return buttons;
  }
  return String();
}

void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(sensorPower, OUTPUT);
  digitalWrite(sensorPower, LOW);
  pinMode(autoModePin, OUTPUT);
  digitalWrite(autoModePin, LOW);  // Start with auto mode ON
  pinMode(manualPumpPin, OUTPUT);
  digitalWrite(manualPumpPin, LOW); // Start with pump OFF
  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);

  dht.begin();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println(WiFi.localIP());

  // Set up web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200, "text/html", index_html, processor);
  });
  
  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(401);
  });
  
  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", logout_html, processor);
  });
  
  server.on("/raw", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200, "text/html", rawdata_html, processor);
  });
  
  // Add all other routes as in your original code...
  
  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  checkWiFi(); // Check and maintain WiFi connection

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    // Update all sensor readings
    readsoilmoisture();
    readlight();
    readDHTTemperature();
    readDHTHumidity();
    waterlvl();
    
    Serial.print("Water Level - Raw:");
    Serial.print(h3);
    Serial.print("cm Mapped:");
    Serial.print(lvl);
    Serial.println("%");
    
    // Handle pump control
    handlePumpControl();
  }
}

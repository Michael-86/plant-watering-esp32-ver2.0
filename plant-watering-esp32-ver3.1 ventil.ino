// Libraries
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <AsyncTCP.h>
#include <NewPing.h>
#include <Update.h>
#include <ESP32Servo.h>

// WiFi credentials
const char *ssid = "gtfast";
const char *password = "darktitan01";

// Web server authentication
const char *http_username = "admin";
const char *http_password = "admin";

// Pin definitions
const int autoModePin = 2;     // GPIO for auto mode toggle
const int manualPumpPin = 33;  // GPIO for manual pump control
const int relay = 13;          // Relay output
const int SERVO_PIN = 16;      // Servo control pin

// Timing constants
const unsigned long SENSOR_READ_INTERVAL = 1000;  // 1 second between sensor reads
const unsigned long MAX_PUMP_RUN_TIME = 60000;    // 1 minute pump timeout
const int SERVO_DELAY = 15;                       // ms between servo movements

// Sensor thresholds
#define soilWet 1600        // Max value we consider soil 'wet'
#define soilDry 3200        // Min value we consider soil 'dry'
#define DHTPIN 4            // DHT sensor pin
#define DHTTYPE DHT11       // DHT sensor type
#define TRIGGER_PIN 14      // Ultrasonic sensor trigger
#define ECHO_PIN 27         // Ultrasonic sensor echo
#define MAX_DISTANCE 200    // Max distance for ultrasonic sensor (cm)
#define LIGHT_SENSOR_PIN 39 // Light sensor pin
#define sensorPower 12      // Soil moisture sensor power
#define sensorPin 36        // Soil moisture sensor analog input

// Tank parameters
const int tank1Full = 3;    // Depth when tank is full (cm)
const int tank1Empty = 23;  // Depth when tank is empty (cm)
const int maxDistance = 26; // Maximum valid distance reading

// Variables
char jordfuktighet[12] = "Unknown";
char autostate[4] = "on";
char pump[4] = "off";
char ljusstyrka[16] = "Unknown";
char pumprun[4] = "off";
char valveLocation[10] = "Place 1";

// Sensor values
float t = 0;
float h = 0;
int lvl = 0;
int h3 = 0;
int ljusraw = 0;
int jordfuktdata = 0;

// Servo positions
const int VALVE_POSITION_1 = 0;    // Water to place 1
const int VALVE_POSITION_2 = 87;   // Water to place 2

// Objects
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
DHT dht(DHTPIN, DHTTYPE);
Servo valveServo;
AsyncWebServer server(80);

// HTML content - MUST be declared before any functions that use them
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="utf-8">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {
      font-family: Arial;
      display: inline-block;
      margin: 0px auto;
      text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
      }
    .switch {position: relative; display: inline-block; width: 120px; height: 68px}
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
    input:checked+.slider {background-color: #b30000}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>Drivhus kontroll panel</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i>
    <span class="dht-labels">Temperatur</span>
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i>
    <span class="dht-labels">Luft Fuktighet</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">&percnt;</sup>
  </p>
  <p>
    <i class="fas fa-solid fa-cloud-showers-heavy"></i>
    <span class="dht-labels">Jord fuktighet</span>
    <span id="Soilmoisture">%Soilmoisture%</span>
  </p>
  <p>
    <i class="fas fa-solid fa-lightbulb"></i>
    <span class="dht-labels">Ljus</span>
    <span id="handleljus">%handleljus%</span>
  </p>
    <p>
    <i class="fas fa-solid fa-water"></i>
    <span class="dht-labels">Vatten Nivå</span>
    <span id="vatten">%vatten%</span>
    <sup class="units">&percnt;</sup>
  </p>
    <p>
      <i class="fas fa-solid fa-sync"></i>
      <span class="dht-labels">Vattenventil Position:</span>
      <span id="valvePosition">%VALVE_POSITION%</span>
    </p>
  <p>
  %BUTTONPLACEHOLDER%
  </p>
    <h4>Vattenventil</h4>
    <p>
      <button onclick="setValve(0)" style="height:40px;width:150px;font-size:25px;">Plats 1 (0&deg;)</button>
      <button onclick="setValve(87)" style="height:40px;width:150px;font-size:25px;">Plats 2 (87&deg;)</button>
    </p>
  <p>
  <input type=button style="height:50px;width:200px;font-size:35px;" value="Rådata" onClick="self.location='/raw'">
  </p>
  <p>
  <input type=button style="height:50px;width:200px;font-size:35px;" value="Uppdatera" onClick="self.location='/update-firmware'">
  </p>
  <p>
  <button onclick="logoutButton()" style="height:50px;width:200px;font-size:35px;">Logga ut</button>
  </p>
</body>
<script>
function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update2?output="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/update2?output="+element.id+"&state=0", true); }
  xhr.send();
  }
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("Soilmoisture").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/Soilmoisture", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("handleljus").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/handleljus", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("vatten").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/vatten", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("valvePosition").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/valvePosition", true);
  xhttp.send();
}, 2000 ) ;

function setValve(position) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/setValve?position=" + position, true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4 && xhr.status == 200) {
      console.log("Valve set to position: " + position);
    } else if (xhr.readyState == 4) {
      alert("Failed to set valve position. Please try again.");
    }
  };
  xhr.send();
}

function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4 && xhr.status == 200) {
      window.open("/logged-out", "_self");
    } else if (xhr.readyState == 4) {
      alert("Logout failed. Please try again.");
    }
  };
  xhr.send();
}
</script>
</html>)rawliteral";

const char logout_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <p>Logged out or <a href="/">return to homepage</a>.</p>
  <p><strong>Note:</strong> close all web browser tabs to complete the logout process.</p>
</body>
</html>
)rawliteral";

const char rawdata_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="utf-8">
  <style>
    html {
      font-family: Arial;
      display: inline-block;
      margin: 0px auto;
      text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
      }
    .switch {position: relative; display: inline-block; width: 120px; height: 68px}
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
    input:checked+.slider {background-color: #b30000}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>Rådata panel</h2>
  <p>
    <span class="dht-labels">Temperatur</span>
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <span class="dht-labels">Luft Fuktighet</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">&percnt;</sup>
  </p>
  <p>
    <span class="dht-labels">Jord fuktighet</span>
    <span id="Soilmoisture2">%Soilmoisture2%</span>
    <sup class="units">&ohm;</sup>
  </p>
  <p>
    <span class="dht-labels">Ljus</span>
    <span id="handleljus2">%handleljus2%</span>
    <sup class="units">&ohm;</sup>
  </p>
  <p>
    <i class="fas fa-solid fa-water"></i>
    <span class="dht-labels">Vatten Nivå</span>
    <span id="vatten">%vatten%</span>
    <sup class="units">&percnt;</sup>
  </p>
  <p>
    <span class="dht-labels">Distans till vatten.</span>
    <span id="vatten2">%vatten2%</span>
    <sup class="units">CM</sup>
  </p>
  <p>
    <span class="dht-labels">Pumpens läge: </span>
    <span id="pumprun">%pumprun%</span>
    <sup class="units"></sup>
  </p>
    <p>
      <span class="dht-labels">Vattenventil Position:</span>
      <span id="valvePositionRaw">%VALVE_POSITION%</span>
    </p>
  <p>
  <input type=button style="height:50px;width:200px;font-size:35px;" value="Uppdatera" onClick="self.location='/update'">
  </p>
  <p>
  <input type=button style="height:50px;width:200px;font-size:35px;" value="Tillbaka" onClick="self.location='/'">
  </p>
  <p>
  <button onclick="logoutButton()" style="height:50px;width:200px;font-size:35px;">Logga ut</button>
  </p>
</body>
<script>
function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update2?output="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/update2?output="+element.id+"&state=0", true); }
  xhr.send();
  }
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("Soilmoisture2").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/Soilmoisture2", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("handleljus2").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/handleljus2", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("vatten").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/vatten", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("vatten2").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/vatten2", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("pumprun").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/pumprun", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("valvePositionRaw").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/valvePosition", true);
  xhttp.send();
}, 2000 ) ;

function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}
</script>
</html>)rawliteral";

const char ota_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Firmware Update</title>
  <style>
    body { font-family: Arial; text-align: center; margin-top: 50px; }
    .progress { width: 50%; margin: 20px auto; }
    .bar { background: #ddd; height: 20px; }
    .progress-bar { background: #4CAF50; height: 100%; width: 0%; }
  </style>
</head>
<body>
  <h1>ESP32 Firmware Update</h1>
  <form method="POST" action="/update" enctype="multipart/form-data">
    <input type="file" name="firmware" required>
    <button type="submit">Update</button>
  </form>
  <div class="progress">
    <div class="bar">
      <div class="progress-bar" id="progress"></div>
    </div>
    <div id="status"></div>
  </div>
  <script>
    document.querySelector('form').addEventListener('submit', function(e) {
      var file = document.querySelector('input[type=file]').files[0];
      var xhr = new XMLHttpRequest();
      var form = new FormData();
      form.append('firmware', file);

      xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
          var percent = (e.loaded / e.total) * 100;
          document.getElementById('progress').style.width = percent + '%';
          document.getElementById('status').innerHTML =
            "Uploading: " + Math.round(percent) + "%";
        }
      };

      xhr.onload = function() {
        document.getElementById('status').innerHTML = "Rebooting...";
        setTimeout(function(){ location.reload(); }, 5000);
      };

      xhr.open('POST', '/update', true);
      xhr.send(form);
      e.preventDefault();
    });
  </script>
</body>
</html>
)rawliteral";

// Rest of the code remains the same as in the previous version
// [All the function implementations and setup/loop remain unchanged]

void smoothServoMove(int targetPos) {
  int currentPos = valveServo.read();
  int step = (targetPos > currentPos) ? 1 : -1;
  
  while(currentPos != targetPos) {
    currentPos += step;
    valveServo.write(currentPos);
    delay(SERVO_DELAY);
  }
}

float readStableDHTTemperature(int samples = 3) {
  float sum = 0;
  int valid = 0;
  for(int i=0; i<samples; i++) {
    float reading = dht.readTemperature();
    if(!isnan(reading)) {
      sum += reading;
      valid++;
    }
    delay(100);
  }
  return valid > 0 ? sum/valid : 0;
}

float readStableDHTHumidity(int samples = 3) {
  float sum = 0;
  int valid = 0;
  for(int i=0; i<samples; i++) {
    float reading = dht.readHumidity();
    if(!isnan(reading)) {
      sum += reading;
      valid++;
    }
    delay(100);
  }
  return valid > 0 ? sum/valid : 0;
}

void handlePumpControl() {
  static unsigned long pumpStartTime = 0;
  unsigned long currentMillis = millis();
  
  // Update state variables from physical pins
  strcpy(autostate, digitalRead(autoModePin) ? "off" : "on");
  strcpy(pump, digitalRead(manualPumpPin) ? "on" : "off");

  bool shouldPumpRun = false;
  bool valveToPlace1 = false;
  bool valveToPlace2 = false;

  if (strcmp(autostate, "off") == 0) {
    // Manual mode
    shouldPumpRun = (strcmp(pump, "on") == 0);
  } else {
    // Automatic mode
    bool soilIsDry = (strcmp(jordfuktighet, "För torr") == 0);
    bool lightIsLow = (strcmp(ljusstyrka, "Dunkelt") == 0 || strcmp(ljusstyrka, "Mörkt") == 0);
    bool tankHasWater = (h3 < tank1Empty);

    shouldPumpRun = (soilIsDry || lightIsLow) && tankHasWater;

    // Valve control logic
    if (soilIsDry && tankHasWater) {
      valveToPlace1 = true;
    } else if (lightIsLow && tankHasWater) {
      valveToPlace2 = true;
    }
  }

  // Pump timeout safety (1 minute max runtime)
  if(shouldPumpRun) {
    if(pumpStartTime == 0) {
      pumpStartTime = currentMillis;
    } else if(currentMillis - pumpStartTime > MAX_PUMP_RUN_TIME) {
      shouldPumpRun = false;
      Serial.println("Pump timeout - automatic shutdown after 1 minute");
    }
  } else {
    pumpStartTime = 0;
  }

  // Apply pump control
  digitalWrite(relay, shouldPumpRun ? HIGH : LOW);
  strcpy(pumprun, shouldPumpRun ? "On" : "Off");

  // Apply valve control
  if (valveToPlace1) {
    smoothServoMove(VALVE_POSITION_1);
    strcpy(valveLocation, "Place 1");
  } else if (valveToPlace2) {
    smoothServoMove(VALVE_POSITION_2);
    strcpy(valveLocation, "Place 2");
  }
}

void checkWiFiConnection() {
  static unsigned long lastCheck = 0;
  
  if(millis() - lastCheck > 30000) { // Check every 30 seconds
    if(WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected - attempting to reconnect...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
    lastCheck = millis();
  }
}

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
  ljusstyrka[sizeof(ljusstyrka)-1] = '\0';
}

int readSensor() {
  digitalWrite(sensorPower, HIGH);
  delay(10);
  int val = analogRead(sensorPin);
  digitalWrite(sensorPower, LOW);
  return val;
}

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

void waterlvl() {
  h3 = sonar.ping_cm();
  if (h3 <= 0 || h3 > maxDistance) {
    h3 = tank1Empty;
  }
  h3 = constrain(h3, tank1Full, tank1Empty);
  lvl = map(h3, tank1Empty, tank1Full, 0, 100);
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
  } else if (var == "VALVE_POSITION") {
    return String(valveLocation);
  } else if (var == "BUTTONPLACEHOLDER") {
    String buttons = "";
    buttons += "<h4>Auto läge PÅ - Auto läge AV</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"2\" " + String(digitalRead(2) ? "checked" : "") + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Pump AV - Pump PÅ</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"33\" " + String(digitalRead(33) ? "checked" : "") + "><span class=\"slider\"></span></label>";
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
  digitalWrite(autoModePin, LOW);
  pinMode(manualPumpPin, OUTPUT);
  digitalWrite(manualPumpPin, LOW);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);

  // Initialize sensors
  dht.begin();

  // Initialize servo
  ESP32PWM::allocateTimer(0);
  valveServo.setPeriodHertz(50);
  valveServo.attach(SERVO_PIN, 500, 2500);
  smoothServoMove(VALVE_POSITION_1);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password)) {
      request->requestAuthentication();
      return;
    }
    request->send_P(200, "text/html", index_html, processor);
  });
  
  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(401);
  });
  
  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", logout_html, processor);
  });
  
  server.on("/raw", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password)) {
      request->requestAuthentication();
      return;
    }
    request->send_P(200, "text/html", rawdata_html, processor);
  });
  
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(t).c_str());
  });
  
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(h).c_str());
  });
  
  server.on("/Soilmoisture", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(jordfuktighet).c_str());
  });
  
  server.on("/handleljus", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(ljusstyrka).c_str());
  });
  
  server.on("/vatten", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(lvl).c_str());
  });
  
  server.on("/Soilmoisture2", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(jordfuktdata).c_str());
  });
  
  server.on("/handleljus2", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(ljusraw).c_str());
  });
  
  server.on("/vatten2", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(h3).c_str());
  });
  
  server.on("/pumprun", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(pumprun).c_str());
  });
  
  server.on("/valvePosition", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", String(valveLocation).c_str());
  });
  
  server.on("/setValve", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password)) {
      request->requestAuthentication();
      return;
    }
    String positionStr;
    if (request->hasParam("position")) {
      positionStr = request->getParam("position")->value();
      int position = positionStr.toInt();
      smoothServoMove(position);
      if (position == VALVE_POSITION_1) {
        strcpy(valveLocation, "Place 1");
      } else if (position == VALVE_POSITION_2) {
        strcpy(valveLocation, "Place 2");
      } else {
        strcpy(valveLocation, "Custom");
      }
      Serial.print("Valve set manually to: ");
      Serial.print(position);
      Serial.print(" degrees (");
      Serial.print(valveLocation);
      Serial.println(")");
    }
    request->send(200, "text/plain", "OK");
  });
  
  server.on("/update2", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    String inputMessage1;
    String inputMessage2;
    if (request->hasParam("output") && request->hasParam("state")) {
      inputMessage1 = request->getParam("output")->value();
      inputMessage2 = request->getParam("state")->value();
      digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
      Serial.print("GPIO: ");
      Serial.print(inputMessage1);
      Serial.print(" - Set to: ");
      Serial.println(inputMessage2);
    }
    request->send(200, "text/plain", "OK");
  });
  
  server.on("/update-firmware", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password)) {
      request->requestAuthentication();
      return;
    }
    request->send_P(200, "text/html", ota_html);
  });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    request->send(200, "text/plain", Update.hasError() ? "Update Failed" : "Update Complete!");
    ESP.restart();
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
      Serial.printf("OTA Update Start: %s\n", filename.c_str());
      Update.begin(UPDATE_SIZE_UNKNOWN);
    }
    if (Update.write(data, len) != len) {
      Serial.println("Write Error");
    }
    if (final) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %u bytes\n", index + len);
      } else {
        Serial.println("Update Failed");
      }
    }
  });

  // Start server
  server.begin();
}

void loop() {
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= SENSOR_READ_INTERVAL) {
    previousMillis = currentMillis;

    // Read all sensors
    readsoilmoisture();
    readlight();
    t = readStableDHTTemperature();
    h = readStableDHTHumidity();
    waterlvl();

    // Control pump and valve
    handlePumpControl();
  }

  checkWiFiConnection();
}
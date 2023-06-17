// Import required libraries
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <AsyncElegantOTA.h>
#include <AsyncTCP.h>

//För att få function() att fungera i javascript delen.
typedef void function;

// Replace with your network credentials
const char *ssid = "gtfast";
const char *password = "darktitan01";

const char *PARAM_INPUT_1 = "output";
const char *PARAM_INPUT_2 = "state";

//hemsidans inlogg
const char *http_username = "admin";
const char *http_password = "admin";

String autostate = "on";
String pump = "off";


// Relä utgång
const int relay = 13;

// Millis
unsigned long previousMillis = 0;
const long interval = 1000;

#define DHTPIN 4  // Digital pin connected to the DHT sensor

// Jordfuktighet

/* Change these values based on your calibration values */
#define soilWet 1600  // Define max value we consider soil 'wet'
#define soilDry 4000  // Define min value we consider soil 'dry'

// Variables
String jordfuktighet = "";

//temperatur och luft fuktighet
float t = 0;
float h = 0;


//vatten nivå
const int trig = 14;
const int echo = 27;
//#define trig 14
//#define echo 27
int lvl = 0;

// Ljus sensor
#define LIGHT_SENSOR_PIN 39
String ljusstyrka = "";

// Sensor pins
#define sensorPower 12
#define sensorPin 36

// Uncomment the type of sensor in use:
#define DHTTYPE DHT11  // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Ljus
void readlight() {
  int analogValue = analogRead(LIGHT_SENSOR_PIN);
  String RAW = String(analogValue);

  if (analogValue < 40) {
    //Serial.println(" => Dark");
    ljusstyrka = "Dark";
  } else if (analogValue < 800) {
    //Serial.println(" => Dim");
    ljusstyrka = "Dim";
  } else if (analogValue < 2000) {
    //Serial.println(" => Light");
    ljusstyrka = "Light";
  } else if (analogValue < 3200) {
    //Serial.println(" => Bright");
    ljusstyrka = "Bright";
  } else {
    //Serial.println(" => Very bright");
    ljusstyrka = "Very bright";
  }
}

// Jordfuktighet

void readsoilmoisture() {
  int moisture = readSensor();
  Serial.println("readsoil!");
  // Determine status of our soil
  if (moisture < soilWet) {
    jordfuktighet = "Soil is too wet";
  } else if (moisture >= soilWet && moisture < soilDry) {
    jordfuktighet = "Soil moisture is perfect";
  } else {
    jordfuktighet = "Soil is too dry";
  }
  //digitalWrite(sensorPower, LOW);
  //Serial.println(jordfuktighet);
}

int readSensor() {
  digitalWrite(sensorPower, HIGH);  // Turn the sensor ON
  delay(10);                        // Allow power to settle
  int val = analogRead(sensorPin);  // Read the analog value form sensor
  digitalWrite(sensorPower, LOW);   // Turn the sensor OFF
  Serial.println("readsoil2!");
  return val;  // Return analog moisture value
}

// Vatten nivå

void waterlvl() {
  
  long t2 = 0, h2 = 0;
  
  // Transmitting pulse
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  
  // Waiting for pulse
  t2 = pulseIn(echo, HIGH);
  
  // Calculating distance 
  h2 = t2 / 30;
 
  h2 = h2 - 6;  // offset correction
  h2 = 28 - h2;  // water height, 0 - 50 cm
  
  lvl = 2 * h2;  // distance in %, 0-100 %
}

void readDHTTemperature() {
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  // Read temperature as Celsius (the default)
  // Read temperature as Fahrenheit (isFahrenheit = true)
  //float t = dht.readTemperature(true);
  // Check if any reads failed and exit early (to try again).
  if (isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    t = 0;
  } else {
    t = dht.readTemperature();
  }
}

void readDHTHumidity() {
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  if (isnan(h)) {
    Serial.println("Failed to read from DHT sensor!");
    h = 0;
  } else {
    h = dht.readHumidity();
  }
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
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
  <h2>ESP32 DHT Server</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Temperature</span> 
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">Humidity</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">&percnt;</sup>
  </p>
  <p>
    <i class="fas fa-solid fa-cloud-showers-heavy"></i>
    <span class="dht-labels">Soilmoisture</span>
    <span id="Soilmoisture">%Soilmoisture%</span>
  </p>
  <p>
    <i class="fas fa-solid fa-lightbulb"></i>
    <span class="dht-labels">Light </span>
    <span id="handleljus">%handleljus%</span>
  </p>
    <p>
    <i class="fas fa-solid fa-water"></i>
    <span class="dht-labels">Water level</span>
    <span id="vatten">%vatten%</span>
    <sup class="units">&percnt;</sup>
  </p>
  <p>
  %BUTTONPLACEHOLDER%
  </p>
  <p>
  <input type=button value="Update" onClick="self.location='/update'">
  </p>
  <p>
  <button onclick="logoutButton()">Logout</button>
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

function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
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

String outputState(int output) {
  if (digitalRead(output)) {
    return "checked";
  } else {
    return "";
  }
}

// Replaces placeholder with DHT values
String processor(const String &var) {
  //Serial.println(var);
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
  } else if (var == "BUTTONPLACEHOLDER") {
    String buttons = "";
    buttons += "<h4>Auto mode ON - Auto mode OFF</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"2\" " + outputState(2) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Pump OFF - Pump ON</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"33\" " + outputState(33) + "><span class=\"slider\"></span></label>";
    return buttons;
  }
  return String();
}


void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  pinMode(33, OUTPUT);
  digitalWrite(33, LOW);
  pinMode(sensorPower, OUTPUT);
  digitalWrite(sensorPower, LOW);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);

  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT); 

  dht.begin();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200, "text/html", index_html, processor);
    Serial.println("web");
  });
  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(401);
  });
  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", logout_html, processor);
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
  // Send a GET request to <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
  server.on("/update2", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    String inputMessage1;
    String inputMessage2;
    // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
      digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
    } else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }
    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
    request->send(200, "text/plain", "OK");

    if (inputMessage1 == "2" and inputMessage2 == "0") {
      autostate = "on";
    } else if (inputMessage1 == "2" and inputMessage2 == "1") {
      autostate = "off";
    }
    if (inputMessage1 == "33" and inputMessage2 == "0") {
      pump = "off";
    } else if (inputMessage1 == "33" and inputMessage2 == "1") {
      pump = "on";
    }
  });

  // Start ElegantOTA
  AsyncElegantOTA.begin(&server, "admin", "admin");

  // Start server
  server.begin();
}

void loop() {
  //char nonting = Serial.read();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {

    // save the last time you blinked the LED
    previousMillis = currentMillis;
    Serial.println(autostate);
    readsoilmoisture();
    readlight();
    readDHTTemperature();
    readDHTHumidity();
    waterlvl();

    // if the LED is off turn it on and vice-versa:
    if (autostate == "on" && pump == "off") {
      if (jordfuktighet == "Soil is too wet") {
        digitalWrite(relay, LOW);
      } else if (jordfuktighet == "Soil moisture is perfect") {
        digitalWrite(relay, LOW);
      } else if (jordfuktighet == "Soil is too dry" && ljusstyrka == "Dim" || ljusstyrka == "Dark") {
        digitalWrite(relay, HIGH);
      } else {
        digitalWrite(relay, LOW);
      }
    } else {
      digitalWrite(relay, LOW);
    }
    if (autostate == "off" && pump == "on") {
      digitalWrite(relay, HIGH);
    } else if (autostate == "off" && pump == "off") {
      digitalWrite(relay, LOW);
    }
  }
}
// bibliotek
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
//#include <AsyncElegantOTA.h>
#include <AsyncTCP.h>
#include <NewPing.h>

typedef void function;

// Wifi nätverk och lösen
const char *ssid = "gtfast";
const char *password = "darktitan01";

const char *PARAM_INPUT_1 = "output";
const char *PARAM_INPUT_2 = "state";

// hemsidans inlogg
const char *http_username = "admin";
const char *http_password = "admin";

//mode
int autoModePin = 2;    // GPIO for auto mode toggle
int manualPumpPin = 33; // GPIO for manual pump control

// Relä utgång
const int relay = 13;

// Millis
unsigned long previousMillis = 0;
const long interval = 1000;

// Digital pin för DHT sensor
#define DHTPIN 4

// Jordfuktighet
#define soilWet 1600  // Define max value we consider soil 'wet'
#define soilDry 3200  // Define min value we consider soil 'dry'

// Variabler
char jordfuktighet[12] = "Unknown";
char autostate[4] = "on";
char pump[4] = "off";

// temperatur och luft fuktighet
float t = 0;
float h = 0;

// Constants to define the tank
const int tank1Full = 3;     // depth measured when tank 1 is full
const int tank1Empty = 23;   // depth measured when tank 1 is empty
const int maxDistance = 26;  // Sets a maximum distance for the sensor object N.B. must be greater than the empty value

// vatten nivå
int lvl = 0;

#define TRIGGER_PIN 14    // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN 27       // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 200  // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);  // NewPing setup of pins and maximum distance.

// Ljus sensor
#define LIGHT_SENSOR_PIN 39
char ljusstyrka[12] = "Unknown";

// Sensor pins
#define sensorPower 12
#define sensorPin 36

// raw data
int minlvl = 21;
int h3 = 0;
int ljusraw = 0;
int jordfuktdata = 0;
char pumprun[4] = "off";

// Vilken temperatur och luftfuktighets sensor man använder:
#define DHTTYPE DHT11  // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE);

// Port för AsyncWebServer är 80
AsyncWebServer server(80);

//pump styrning
void handlePumpControl() {
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
    bool tankHasWater = (h3 < minlvl); // Verify if this condition is correct for your setup
    
    shouldPumpRun = soilIsDry && lightIsLow && tankHasWater;
    
    // Debug output for automatic mode
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
  
  // Additional debug
  Serial.print("Final Decision - Pump:");
  Serial.println(shouldPumpRun ? "ON" : "OFF");
}

// Ljus
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
}

// Jordfuktighet
void readsoilmoisture() {
  int moisture = readSensor();
  jordfuktdata = moisture;
  // Bestäm status för vår jord
  if (moisture < soilWet) {
    strcpy(jordfuktighet, "För våt");
  } else if (moisture >= soilWet && moisture < soilDry) {
    strcpy(jordfuktighet, "Är perfekt");
  } else {
    strcpy(jordfuktighet, "För torr");
  }
}

int readSensor() {
  digitalWrite(sensorPower, HIGH);  // Sensor på
  delay(10);                        // Vänta
  int val = analogRead(sensorPin);  // Läs sensor värden
  digitalWrite(sensorPower, LOW);   // sensor av
  return val;                       // returnerar sensor värden
}

// Vatten nivå
void waterlvl() {
  h3 = sonar.ping_cm();
  // Add validation for h3 reading
  if (h3 <= 0 || h3 > maxDistance) {
    h3 = tank1Empty; // Default to empty if reading is invalid
  }
  lvl = map(h3, tank1Empty, tank1Full, 0, 100);
}

void readDHTTemperature() {
  // Kolla temperatur
  t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println("Det gick inte att läsa från DHT-sensorn!");
    t = 0;
  }
}

void readDHTHumidity() {
  // Kolla luftfuktighet
  h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println("Det gick inte att läsa från DHT-sensorn!");
    h = 0;
  }
}

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
  %BUTTONPLACEHOLDER%
  </p>
  <p>
  <input type=button style="height:50px;width:200px;font-size:35px;" value="Rådata" onClick="self.location='/raw'">
  </p>
  <p>
  <input type=button style="height:50px;width:200px;font-size:35px;" value="Uppdatera" onClick="self.location='/update'">
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

function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}

</script>
</html>)rawliteral";

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
  // Serial port för debugging
  Serial.begin(115200);

  pinMode(sensorPower, OUTPUT);
  digitalWrite(sensorPower, LOW);
  pinMode(autoModePin, OUTPUT);
  digitalWrite(autoModePin, LOW);  // Start with auto mode ON (inverted logic)
  pinMode(manualPumpPin, OUTPUT);
  digitalWrite(manualPumpPin, LOW); // Start with pump OFF
  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);

  dht.begin();

  // koppla upp till Wi-Fi
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Skriver ut Esp32's ip nummer. obs fungerar inte
  Serial.println(WiFi.localIP());

  // Route for root / web page
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
  // Send a GET request to <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
server.on("/update2", HTTP_GET, [](AsyncWebServerRequest *request) {
  if (!request->authenticate(http_username, http_password))
    return request->requestAuthentication();
  String inputMessage1;
  String inputMessage2;
  if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
    inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
    inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
    digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
    
    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
  }
  request->send(200, "text/plain", "OK");
});

  // Starta ElegantOTA. För fjärr uppdatering
  // AsyncElegantOTA.begin(&server, "admin", "admin");

  // Starta server
  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    // Update all sensor readings
    readsoilmoisture();
    readlight();
    readDHTTemperature();
    readDHTHumidity();
    waterlvl();
    
    // Debug water level reading
    Serial.print("Water Level - Raw:");
    Serial.print(h3);
    Serial.print("cm Mapped:");
    Serial.print(lvl);
    Serial.println("%");
    
    // Handle pump control
    handlePumpControl();
  }
}

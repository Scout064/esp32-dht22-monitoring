// ESP32 DHT22 Sensor Web API Server
// Provides RESTful endpoints to access temperature/humidity, update config, and manage device
// Author: Code Copilot

#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Update.h>

// Define Firmware Version
const char* firmwareVersion = "1.1.0";
const char* buildTime = __DATE__ " " __TIME__;

// Network Credentials - loaded from Preferences
String ssid, password;

// DHT22 Data Pin and Type
#define DHTPIN 5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Async Web Server and Preferences handler
AsyncWebServer server(80);
Preferences prefs;

// Cached sensor data and polling interval (ms)
unsigned long lastSensorRead = 0;
unsigned long sensorReadInterval = 2000;
float cachedTemperature = NAN;
float cachedHumidity = NAN;

// Simple log ring buffer
#define LOG_SIZE 10
String logs[LOG_SIZE];
int logIndex = 0;

// Load admin key from Preferences
String getAdminKey() {
  prefs.begin("auth", true);
  String key = prefs.getString("adminKey", "secret123");
  prefs.end();
  return key;
}

// Authenticate using admin key from HTTP query param
bool checkAdminKey(AsyncWebServerRequest *request) {
  return request->hasParam("key") && request->getParam("key")->value() == getAdminKey();
}

// Append log entry to buffer
void addLog(String msg) {
  logs[logIndex] = msg;
  logIndex = (logIndex + 1) % LOG_SIZE;
  Serial.println("LOG: " + msg);
}

// Read temperature and humidity with cache mechanism
void updateSensorCache() {
  unsigned long now = millis();
  if (now - lastSensorRead >= sensorReadInterval) {
    cachedTemperature = dht.readTemperature();
    cachedHumidity = dht.readHumidity();
    lastSensorRead = now;
    addLog("Sensor updated at " + String(now));
  }
}

// Return temperature in °C or °F
String readDHTTemperature(bool fahrenheit = false) {
  updateSensorCache();
  float t = fahrenheit ? (cachedTemperature * 1.8 + 32) : cachedTemperature;
  if (isnan(t)) {
    addLog("Failed to read temperature");
    return "--";
  }
  return String(t);
}

// Return humidity percentage
String readDHTHumidity() {
  updateSensorCache();
  float h = cachedHumidity;
  if (isnan(h)) {
    addLog("Failed to read humidity");
    return "--";
  }
  return String(h);
}

// HTML UI for basic web view
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css">
  <style>
    html { font-family: Arial; text-align: center; }
    h2, p { font-size: 3rem; }
    .units { font-size: 1.2rem; }
    .dht-labels { font-size: 1.5rem; padding-bottom: 15px; }
  </style>
</head>
<body>
  <h2>ESP32 DHT Server</h2>
  <p><i class="fas fa-thermometer-half" style="color:#059e8a;"></i> <span class="dht-labels">Temperature</span> <span id="temperature">%TEMPERATURE%</span><sup class="units">&deg;C</sup></p>
  <p><i class="fas fa-tint" style="color:#00add6;"></i> <span class="dht-labels">Humidity</span> <span id="humidity">%HUMIDITY%</span><sup class="units">&percnt;</sup></p>
</body>
<script>
setInterval(function() {
  fetch('/temperature').then(res => res.text()).then(data => { document.getElementById("temperature").innerText = data; });
  fetch('/humidity').then(res => res.text()).then(data => { document.getElementById("humidity").innerText = data; });
}, 10000);
</script>
</html>)rawliteral";

// OpenAPI Style Webpage
const char openapi_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 API Docs</title>
  <meta charset="UTF-8">
  <link rel="stylesheet" href="https://unpkg.com/swagger-ui-dist@4.18.3/swagger-ui.css">
</head>
<body>
  <div id="swagger-ui"></div>
  <script src="https://unpkg.com/swagger-ui-dist@4.18.3/swagger-ui-bundle.js"></script>
  <script>
    const spec = {
      openapi: "3.0.0",
      info: {
        title: "ESP32 DHT22 Sensor API",
        version: "1.1.0",
        description: "REST API for ESP32 sensor and device control"
      },
      servers: [{ url: location.origin }],
      paths: {
        "/temperature": { get: { summary: "Read Temperature", responses: { 200: { description: "Temperature" } } } },
        "/humidity": { get: { summary: "Read Humidity", responses: { 200: { description: "Humidity" } } } },
        "/api/logs": { get: { summary: "Logs", parameters: [{ name: "key", in: "query", required: true, schema: { type: "string" } }], responses: { 200: { description: "Log output" } } } },
        "/api/update-wifi": { post: { summary: "Set WiFi", parameters: [{ name: "key", in: "query", required: true, schema: { type: "string" } }], responses: { 200: { description: "Updated" } } } },
        "/api/update-interval": { post: { summary: "Set Interval", parameters: [{ name: "key", in: "query", required: true, schema: { type: "string" } }], responses: { 200: { description: "Updated" } } } },
        "/api/update-key": { post: { summary: "Change Admin Key", parameters: [{ name: "key", in: "query", required: true, schema: { type: "string" } }], responses: { 200: { description: "Updated" } } } },
        "/api/reboot": { post: { summary: "Reboot Device", parameters: [{ name: "key", in: "query", required: true, schema: { type: "string" } }], responses: { 200: { description: "Rebooting" } } } },
        "/api/factory-reset": { post: { summary: "Factory Reset", parameters: [{ name: "key", in: "query", required: true, schema: { type: "string" } }], responses: { 200: { description: "Resetting" } } } },
        "/metrics": { get: { summary: "Prometheus Metrics", responses: { 200: { description: "Exposes Prometheus format metrics" } } } },
        "/api/status": { get: { summary: "Device Status", responses: { 200: { description: "Returns temperature, humidity, IP, uptime" } } } },
        "/update": { post: { summary: "OTA Upload", parameters: [{ name: "key", in: "query", required: true, schema: { type: "string" } }], requestBody: { content: { "multipart/form-data": { schema: { type: "object", properties: { update: { type: "string", format: "binary" } } } } }, required: true }, responses: { 200: { description: "Firmware update result" } } } }
      }
    };
    window.onload = () => SwaggerUIBundle({ spec, dom_id: '#swagger-ui' });
  </script>
</body>
</html>
)rawliteral";

// Firmware Upload Webpage
const char fw_upload_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><body>
<h2>OTA Firmware Update</h2>
<form id='form' method='POST' action='/update?key=secret123' enctype='multipart/form-data'>
  <input type='file' name='update'><br>
  <progress id='bar' value='0' max='100' style='width:300px;'></progress><br>
  <input type='submit' value='Update'>
</form>
<p id='msg'></p>
<script>
  const form = document.getElementById('form');
  const bar = document.getElementById('bar');
  const msg = document.getElementById('msg');
  form.onsubmit = e => {
    e.preventDefault();
    const xhr = new XMLHttpRequest();
    xhr.upload.onprogress = e => {
      bar.value = (e.loaded / e.total) * 100;
    };
    xhr.onload = () => {
      msg.textContent = 'Update ' + xhr.responseText;
      form.style.display = 'none';
    };
    xhr.open('POST', form.action);
    xhr.send(new FormData(form));
  };
</script>
</body>
</html>
)rawliteral";

// Replaces HTML template vars with live values
String processor(const String& var){
  if (var == "TEMPERATURE") return readDHTTemperature();
  if (var == "HUMIDITY") return readDHTHumidity();
  return String();
}

void setup(){
  Serial.begin(115200);
  
  // Load WiFi credentials from Preferences
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "Obi-Wlan-Kenobi");
  password = prefs.getString("password", "Qaywsx1234!");
  prefs.end();

  // Load sensor interval
  prefs.begin("config", true);
  sensorReadInterval = prefs.getULong("interval", 2000);
  prefs.end();

  // Connect to WiFi
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected! IP: " + WiFi.localIP().toString());

  dht.begin();

  // Root HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });
  //Swagger UI
    server.on("/docs", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", openapi_html);
  });

  // Sensor endpoints
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", readDHTTemperature());
  });

  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", readDHTHumidity());
  });

  // Update WiFi creds
  server.on("/api/update-wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAdminKey(request)) return request->send(403, "application/json", "{\"error\":\"Unauthorized\"}");
    if (!request->hasParam("ssid") || !request->hasParam("password")) return request->send(400, "application/json", "{\"error\":\"Missing ssid or password\"}");
    prefs.begin("wifi", false);
    prefs.putString("ssid", request->getParam("ssid")->value());
    prefs.putString("password", request->getParam("password")->value());
    prefs.end();
    request->send(200, "application/json", "{\"status\":\"WiFi credentials updated. Restarting...\"}");
    delay(100);
    ESP.restart();
  });

  // Update read interval
  server.on("/api/update-interval", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAdminKey(request)) return request->send(403, "application/json", "{\"error\":\"Unauthorized\"}");
    if (!request->hasParam("interval")) return request->send(400, "application/json", "{\"error\":\"Missing interval param\"}");
    unsigned long interval = request->getParam("interval")->value().toInt();
    if (interval < 500) interval = 500;
    prefs.begin("config", false);
    prefs.putULong("interval", interval);
    prefs.end();
    sensorReadInterval = interval;
    request->send(200, "application/json", "{\"status\":\"Interval updated\"}");
  });

  // Factory reset: clears wifi, config, auth
  server.on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAdminKey(request)) return request->send(403, "application/json", "{\"error\":\"Unauthorized\"}");
    prefs.begin("wifi", false); prefs.clear(); prefs.end();
    prefs.begin("config", false); prefs.clear(); prefs.end();
    prefs.begin("auth", false); prefs.clear(); prefs.end();
    addLog("Factory reset triggered");
    request->send(200, "application/json", "{\"status\":\"Factory reset complete. Restarting...\"}");
    delay(100);
    ESP.restart();
  });

  // Soft reboot
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAdminKey(request)) return request->send(403, "application/json", "{\"error\":\"Unauthorized\"}");
    addLog("Reboot requested via API");
    request->send(200, "application/json", "{\"status\":\"Rebooting...\"}");
    delay(100);
    ESP.restart();
  });

  // Add CORS for all API routes
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  // Prometheus metrics format
  server.on("/metrics", HTTP_GET, [](AsyncWebServerRequest *request) {
    String response;
    response += "# TYPE esp32_temperature gauge\n";
    response += "esp32_temperature " + readDHTTemperature() + "\n";
    response += "# TYPE esp32_humidity gauge\n";
    response += "esp32_humidity " + readDHTHumidity() + "\n";
    response += "# TYPE esp32_uptime_seconds counter\n";
    response += "esp32_uptime_seconds " + String(millis() / 1000) + "\n";
    request->send(200, "text/plain", response);
  });

  // Combined status endpoint (sensor + system)
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<256> doc;
    doc["temperature"] = readDHTTemperature().toFloat();
    doc["humidity"] = readDHTHumidity().toFloat();
    doc["uptime"] = millis() / 1000;
    doc["ip"] = WiFi.localIP().toString();
    doc["ssid"] = ssid;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Serve OTA upload page (GET)
server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
  if (!checkAdminKey(request)) {
    return request->send(403, "text/plain", "Unauthorized");
  }
    request->send(200, "text/html", fw_upload_html);
  });

// Handle firmware upload (POST)
server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
  if (!checkAdminKey(request)) {
    return request->send(403, "text/plain", "Unauthorized");
  }
  request->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  delay(100);
  ESP.restart();
}, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index){
    Update.begin(UPDATE_SIZE_UNKNOWN);
  }
  Update.write(data, len);
  if (final){
    Update.end(true);
  }
});

// Firmware version endpoint
server.on("/about", HTTP_GET, [](AsyncWebServerRequest *request){
  StaticJsonDocument<128> doc;
  doc["version"] = firmwareVersion;
  doc["build"] = buildTime;
  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json);
});

/* Build Instructions
1. In Arduino IDE: Sketch > Export compiled Binary
2. Locate the `.bin` file in sketch folder
3. Navigate to http://<ESP-IP>/update?key=secret123
4. Upload the .bin file to update firmware
*/


  server.begin();
}

void loop() {
  // Nothing required here
}

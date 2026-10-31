/*
 * =====================================================================================
 * Project: 4x DHT22 Sensors on Arduino Uno sending data to ESP32-CAM via Serial
 * File: esp32cam.ino
 * Description: 
 *   - Both Hardware Serial (U0T/U0R) and UART2 (GPIO 13/15) are set to 9600 BAUD
 *     to eliminate garbled/scrambled characters!
 *   - Receives DHT22 data from Arduino Uno.
 *   - Connects to Wi-Fi, hosts web server on port 80.
 *   - Sends clean telemetry and IP back to Arduino Uno every 2 seconds.
 *
 * Board Setting in Arduino IDE: "AI Thinker ESP32-CAM"
 * =====================================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "soc/soc.h"           // Disable brownout detector
#include "soc/rtc_cntl_reg.h"  // Disable brownout detector

// ======================== WI-FI CONFIGURATION ========================
// Replace with your Wi-Fi credentials
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Fallback SoftAP mode credentials if home Wi-Fi is unavailable
const char* ap_ssid  = "ESP32-CAM-TEMP";
const char* ap_pass  = "12345678";

// ======================== PIN DEFINITIONS ============================
// Supports BOTH wiring options at 9600 baud:
// OPTION A (Standard & Recommended): Connect to U0T (GPIO 1) and U0R (GPIO 3)
// OPTION B (Secondary pins): Connect to GPIO 15 (TX) and GPIO 13 (RX)
#define ALT_RX_PIN 13
#define ALT_TX_PIN 15

HardwareSerial altSerial(2); // UART2
WebServer server(80);

// ======================== SENSOR DATA STRUCT =========================
struct SensorReading {
  float temperature;
  float humidity;
  bool isValid;
};

SensorReading sensors[4];
unsigned long packetCount = 0;
unsigned long lastPacketTime = 0;
unsigned long lastPrintTime = 0;

// Packet Buffer
const int BUFFER_SIZE = 128;
char serialBuffer[BUFFER_SIZE];
int bufferIndex = 0;
bool receivingData = false;

// Function Prototypes
void parsePacket(char* packet);
void handleDataJson();
void handleRoot();
void sendToBothSerials(const String& msg);

void setup() {
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // CRITICAL: Set baud rate to 9600 to match Arduino Uno SoftwareSerial!
  // (Prevents all garbled / scrambled characters)
  Serial.begin(9600);
  altSerial.begin(9600, SERIAL_8N1, ALT_RX_PIN, ALT_TX_PIN);

  // Allow voltage to stabilize
  delay(1000);

  for (int i = 0; i < 4; i++) {
    sensors[i].temperature = -999.0;
    sensors[i].humidity = -999.0;
    sensors[i].isValid = false;
  }

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 8000) {
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    sendToBothSerials("\r\n>>> [ESP32-CAM] Connected to Wi-Fi!\r\n");
    sendToBothSerials(">>> [ESP32-CAM] IP Address: http://" + WiFi.localIP().toString() + "\r\n");
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_pass);
    sendToBothSerials("\r\n>>> [ESP32-CAM] SoftAP Mode: " + String(ap_ssid) + "\r\n");
    sendToBothSerials(">>> [ESP32-CAM] Open: http://" + WiFi.softAPIP().toString() + "\r\n");
  }

  // Web Server Endpoints
  server.on("/", HTTP_GET, handleRoot);
  server.on("/temp.html", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleDataJson);

  // Enable CORS
  server.on("/data", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.send(204);
  });

  server.begin();
}

void loop() {
  server.handleClient();

  // Read incoming from either Serial (U0R) or altSerial (GPIO 13)
  while (Serial.available() > 0 || altSerial.available() > 0) {
    char inChar = (Serial.available() > 0) ? (char)Serial.read() : (char)altSerial.read();

    if (inChar == '$') {
      bufferIndex = 0;
      receivingData = true;
      continue;
    }

    if (receivingData && inChar == '#') {
      serialBuffer[bufferIndex] = '\0';
      receivingData = false;
      parsePacket(serialBuffer);
      bufferIndex = 0;
      continue;
    }

    if (receivingData) {
      if (bufferIndex < BUFFER_SIZE - 1) {
        serialBuffer[bufferIndex++] = inChar;
      } else {
        bufferIndex = 0;
        receivingData = false;
      }
    }
  }

  // Every 2 seconds: send formatted telemetry
  unsigned long currentMillis = millis();
  if (currentMillis - lastPrintTime >= 2000) {
    lastPrintTime = currentMillis;

    IPAddress currentIP = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP() : WiFi.softAPIP();

    char line[128];
    String report = "\r\n+--------------------------------------------------------+\r\n";
    snprintf(line, sizeof(line), "| ESP32-CAM IP: http://%-15s (Pkt #%-5lu)    |\r\n", 
             currentIP.toString().c_str(), packetCount);
    report += line;
    report += "+--------------------------------------------------------+\r\n";

    for (int i = 0; i < 4; i++) {
      if (sensors[i].isValid) {
        snprintf(line, sizeof(line), "| Sensor %d (Pin D%d):   %5.1f C  |  %5.1f %%  [OK]          |\r\n", 
                 i + 1, i + 4, sensors[i].temperature, sensors[i].humidity);
      } else {
        snprintf(line, sizeof(line), "| Sensor %d (Pin D%d):    --.- C  |   --.- %%  [DISCONNECTED] |\r\n", 
                 i + 1, i + 4);
      }
      report += line;
    }
    report += "+--------------------------------------------------------+\r\n";

    sendToBothSerials(report);
  }
}

void sendToBothSerials(const String& msg) {
  Serial.print(msg);
  altSerial.print(msg);
}

void parsePacket(char* packet) {
  char* token = strtok(packet, ",");
  if (token == NULL || strcmp(token, "DATA") != 0) return;

  token = strtok(NULL, ",");
  if (token == NULL) return;
  packetCount = strtoul(token, NULL, 10);

  for (int i = 0; i < 4; i++) {
    token = strtok(NULL, ",");
    if (token == NULL) return;
    float t = atof(token);

    token = strtok(NULL, ",");
    if (token == NULL) return;
    float h = atof(token);

    sensors[i].temperature = t;
    sensors[i].humidity = h;
    sensors[i].isValid = (t > -900.0 && h > -900.0);
  }

  lastPacketTime = millis();
}

void handleDataJson() {
  IPAddress currentIP = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP() : WiFi.softAPIP();

  float humSum = 0;
  int validCount = 0;
  for (int i = 0; i < 4; i++) {
    if (sensors[i].isValid) {
      humSum += sensors[i].humidity;
      validCount++;
    }
  }
  float avgHum = (validCount > 0) ? (humSum / validCount) : 0.0;

  String json = "{";
  json += "\"link\":\"ok\",";
  json += "\"seq\":" + String(packetCount) + ",";
  json += "\"ip\":\"" + currentIP.toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"camera\":\"online\",";
  json += "\"t1\":" + (sensors[0].isValid ? String(sensors[0].temperature, 1) : "null") + ",";
  json += "\"t2\":" + (sensors[1].isValid ? String(sensors[1].temperature, 1) : "null") + ",";
  json += "\"t3\":" + (sensors[2].isValid ? String(sensors[2].temperature, 1) : "null") + ",";
  json += "\"t4\":" + (sensors[3].isValid ? String(sensors[3].temperature, 1) : "null") + ",";
  json += "\"h1\":" + (sensors[0].isValid ? String(sensors[0].humidity, 1) : "0.0") + ",";
  json += "\"h2\":" + (sensors[1].isValid ? String(sensors[1].humidity, 1) : "0.0") + ",";
  json += "\"h3\":" + (sensors[2].isValid ? String(sensors[2].humidity, 1) : "0.0") + ",";
  json += "\"h4\":" + (sensors[3].isValid ? String(sensors[3].humidity, 1) : "0.0") + ",";
  json += "\"hum\":" + String(avgHum, 1);
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(200, "application/json", json);
}

void handleRoot() {
  String html = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>ESP32-CAM DHT22</title>
<style>body{font-family:sans-serif;background:#0f172a;color:#f8fafc;padding:20px;text-align:center;}
.box{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:15px;max-width:800px;margin:20px auto;}
.c{background:#1e293b;border:1px solid #334155;border-radius:10px;padding:15px;}
.t{font-size:28px;color:#f43f5e;font-weight:bold;margin:8px 0;}.h{font-size:18px;color:#38bdf8;}
</style></head><body>
<h2>ESP32-CAM 4x DHT22 Station</h2><div id="ip" style="color:#4ade80;font-family:monospace;">Connecting...</div>
<div class="box">
  <div class="c"><div>Sensor 1 (D4)</div><div class="t" id="t1">--.- °C</div><div class="h" id="h1">RH: -- %</div></div>
  <div class="c"><div>Sensor 2 (D5)</div><div class="t" id="t2">--.- °C</div><div class="h" id="h2">RH: -- %</div></div>
  <div class="c"><div>Sensor 3 (D6)</div><div class="t" id="t3">--.- °C</div><div class="h" id="h3">RH: -- %</div></div>
  <div class="c"><div>Sensor 4 (D7)</div><div class="t" id="t4">--.- °C</div><div class="h" id="h4">RH: -- %</div></div>
</div>
<script>
async function upd(){
  try{
    let r=await fetch('/data');let d=await r.json();
    document.getElementById('ip').innerText='IP: '+d.ip+' (Pkt #'+d.seq+')';
    for(let i=1;i<=4;i++){
      document.getElementById('t'+i).innerText=(d['t'+i]!==null)?d['t'+i]+' °C':'--.- °C';
      document.getElementById('h'+i).innerText='RH: '+(d['h'+i]||'--')+' %';
    }
  }catch(e){}
}
setInterval(upd,2000);upd();
</script></body></html>)rawliteral";
  server.send(200, "text/html", html);
}

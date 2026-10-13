/*
 * =====================================================================================
 * PROJECT: MINISATELLITE TELEMETRY DOWNLINK & OPTICAL RECON (ESP32-CAM)
 * BOARD:   AI-Thinker ESP32-CAM (with OV2640 Camera Module)
 * FILE:    tempesp32code.ino
 *
 * DESCRIPTION:
 * Hosts the MiniSatellite wireless downlink node and optical sensor:
 *   1. Captures live orbital/recon imagery using the onboard OV2640 camera (/cam, /stream).
 *   2. Receives 4-channel DHT22 thermal & humidity telemetry from Arduino Uno over Serial.
 *   3. Serves HTTP REST Telemetry API (/data) and high-speed image streams with CORS.
 *
 * ARDUINO IDE BOARD CONFIGURATION:
 *   - Board: "AI Thinker ESP32-CAM"
 *   - CPU Frequency: 240MHz (WiFi/BT)
 *   - Flash Frequency: 80MHz
 *   - Flash Mode: QIO
 *   - Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"
 *   - PSRAM: "Enabled"
 *
 * HARDWARE CONNECTIONS:
 * Arduino Uno Pin 11 (TX) ---> [1kΩ Resistor] ---> ESP32-CAM Pin U0R (GPIO 3 RX)
 *                                                    |
 *                                              [2kΩ Resistor]
 *                                                    |
 * Arduino Uno GND -----------> ESP32-CAM GND <-------+
 * Arduino Uno Pin 10 (RX) <--- ESP32-CAM Pin U0T (GPIO 1 TX) (Direct 3.3V is safe)
 * ESP32-CAM 5V --------------> 5V (from 5V 2A Power Source or Arduino 5V/USB)
 * =====================================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// =====================================================================================
// 1. WI-FI CREDENTIALS
// =====================================================================================
const char* ssid = "Hydra";
const char* password = "blackdevil9787";

// Web Server on standard HTTP port 80
WebServer server(80);

// =====================================================================================
// 2. AI-THINKER ESP32-CAM OV2640 PIN DEFINITIONS
// =====================================================================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Optional On-board Flash LED
#define FLASH_LED_PIN      4

// Camera Hardware State
bool cameraInitialized = false;

// =====================================================================================
// 3. TELEMETRY STATE & SENSOR BUFFERS (4x DHT22)
// =====================================================================================
// Temperatures (°C)
float t1 = 24.5; // OBC Core
float t2 = 26.2; // Solar Array
float t3 = 22.8; // EPS Battery
float t4 = 25.0; // Hull Radiator

// Relative Humidities (%RH)
float h1 = 45.0;
float h2 = 46.2;
float h3 = 44.5;
float h4 = 47.0;
float hum = 45.7;

// Link & Packet Tracking
bool arduinoConnected = false;
unsigned long lastArduinoPacketTime = 0;
const unsigned long ARDUINO_TIMEOUT_MS = 4500; // 4.5s without packet = link offline
unsigned long packetCount = 0;

// WiFi Health Check Interval
unsigned long lastWifiCheck = 0;
const unsigned long wifiCheckInterval = 10000;

// UART Line Buffer
String uartBuffer = "";

// =====================================================================================
// 4. PARSE ARDUINO UNO 4x DHT22 TELEMETRY PACKET
// =====================================================================================
// Parses JSON: {"t1":24.50,"t2":26.30,"t3":23.00,"t4":24.90,"h1":45.1,"h2":46.0,"h3":44.8,"h4":47.2,"hum":45.8,"pkt":1}
void parseIncomingTelemetry(String line) {
  line.trim();
  if (line.length() < 7) return;

  // Key search in JSON
  int idxT1 = line.indexOf("\"t1\":");
  int idxT2 = line.indexOf("\"t2\":");
  int idxT3 = line.indexOf("\"t3\":");
  int idxT4 = line.indexOf("\"t4\":");

  if (idxT1 != -1 && idxT2 != -1 && idxT3 != -1 && idxT4 != -1) {
    t1 = line.substring(idxT1 + 5).toFloat();
    t2 = line.substring(idxT2 + 5).toFloat();
    t3 = line.substring(idxT3 + 5).toFloat();
    t4 = line.substring(idxT4 + 5).toFloat();

    // Parse individual humidities
    int idxH1 = line.indexOf("\"h1\":");
    int idxH2 = line.indexOf("\"h2\":");
    int idxH3 = line.indexOf("\"h3\":");
    int idxH4 = line.indexOf("\"h4\":");
    int idxHum = line.indexOf("\"hum\":");

    if (idxH1 != -1) h1 = line.substring(idxH1 + 5).toFloat();
    if (idxH2 != -1) h2 = line.substring(idxH2 + 5).toFloat();
    if (idxH3 != -1) h3 = line.substring(idxH3 + 5).toFloat();
    if (idxH4 != -1) h4 = line.substring(idxH4 + 5).toFloat();
    if (idxHum != -1) hum = line.substring(idxHum + 6).toFloat();

    arduinoConnected = true;
    lastArduinoPacketTime = millis();
    packetCount++;
    return;
  }

  // Fallback: Check comma-separated values (CSV)
  int c1 = line.indexOf(',');
  int c2 = line.indexOf(',', c1 + 1);
  int c3 = line.indexOf(',', c2 + 1);
  if (c1 != -1 && c2 != -1 && c3 != -1) {
    t1 = line.substring(0, c1).toFloat();
    t2 = line.substring(c1 + 1, c2).toFloat();
    t3 = line.substring(c2 + 1, c3).toFloat();
    t4 = line.substring(c3 + 1).toFloat();
    arduinoConnected = true;
    lastArduinoPacketTime = millis();
    packetCount++;
  }
}

// Reads available serial stream from Arduino Uno
void readArduinoTelemetry() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (uartBuffer.length() > 0) {
        parseIncomingTelemetry(uartBuffer);
        uartBuffer = "";
      }
    } else {
      if (uartBuffer.length() < 240) {
        uartBuffer += c;
      }
    }
  }
}

// =====================================================================================
// 5. CAMERA INITIALIZATION (OV2640)
// =====================================================================================
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000; // 10MHz is much more stable than 20MHz on AI-Thinker clones
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_LATEST; // Always grab latest frame

  // Adjust quality based on PSRAM presence - Default to QVGA for rock-solid stability & fast downlink
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_QVGA; // 320x240 default (switchable to VGA via HIGH mode)
    config.jpeg_quality = 14;             // 0-63, balanced quality
    config.fb_count     = 2;              // Double buffer in PSRAM
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_QVGA; // 320x240 fallback for internal SRAM
    config.jpeg_quality = 16;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAMERA ERR] esp_camera_init failed with error 0x%x. Retrying with QVGA...\n", err);
    // Fallback attempt: lower clock & QVGA
    config.xclk_freq_hz = 8000000;
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 16;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
    err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.printf("[CAMERA ERR] Second init attempt failed with error 0x%x\n", err);
      return false;
    }
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 1);     // -2 to 2
    s->set_contrast(s, 1);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
    s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
    s->set_wb_mode(s, 0);        // 0 to 4 - 0: Auto
    s->set_vflip(s, 0);          // 0 = normal, 1 = flipped
    s->set_hmirror(s, 0);        // 0 = normal, 1 = mirrored
  }

  cameraInitialized = true;
  return true;
}

// =====================================================================================
// 6. HTTP ROUTES, CORS & CAMERA HANDLERS
// =====================================================================================

// Helper to attach universal CORS headers for temp.html access
void setCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
}

void handleOptions() {
  setCorsHeaders();
  server.send(204);
}

// REST Telemetry Endpoint: GET /data
void handleData() {
  setCorsHeaders();

  // Evaluate Arduino link timeout
  if (arduinoConnected && (millis() - lastArduinoPacketTime > ARDUINO_TIMEOUT_MS)) {
    arduinoConnected = false;
  }

  String json = "{";
  json += "\"t1\":" + String(t1, 2) + ",";
  json += "\"t2\":" + String(t2, 2) + ",";
  json += "\"t3\":" + String(t3, 2) + ",";
  json += "\"t4\":" + String(t4, 2) + ",";
  json += "\"h1\":" + String(h1, 1) + ",";
  json += "\"h2\":" + String(h2, 1) + ",";
  json += "\"h3\":" + String(h3, 1) + ",";
  json += "\"h4\":" + String(h4, 1) + ",";
  json += "\"hum\":" + String(hum, 1) + ",";
  json += "\"camera\":\"" + String(cameraInitialized ? "online" : "offline") + "\",";
  json += "\"source\":\"" + String(arduinoConnected ? "arduino_uno_dht22_x4" : "offline_cache") + "\",";
  json += "\"arduino_link\":" + String(arduinoConnected ? "true" : "false") + ",";
  json += "\"packets\":" + String(packetCount) + ",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"uptime_s\":" + String(millis() / 1000);
  json += "}";

  server.send(200, "application/json", json);
}

// Adjust Camera Quality / Resolution on the fly (Low / High Mode): GET /cam/set?mode=low or high
void handleCamSetQuality() {
  setCorsHeaders();
  if (!cameraInitialized) {
    server.send(503, "text/plain", "Camera offline");
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (!s) {
    server.send(500, "text/plain", "Sensor not found");
    return;
  }

  String mode = server.hasArg("mode") ? server.arg("mode") : "low";
  mode.toLowerCase();

  if (mode == "high") {
    // High Quality: VGA (640x480), finer JPEG compression
    s->set_framesize(s, FRAMESIZE_VGA);
    s->set_quality(s, 10);
    server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"high\",\"res\":\"640x480\",\"quality\":10}");
  } else {
    // Low / Optimal Performance: QVGA (320x240), fast JPEG compression
    s->set_framesize(s, FRAMESIZE_QVGA);
    s->set_quality(s, 18);
    server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"low\",\"res\":\"320x240\",\"quality\":18}");
  }
}

// Camera Snapshot Endpoint: GET /cam or GET /capture (Every 30 seconds downlink)
void handleCamSnapshot() {
  if (!cameraInitialized) {
    setCorsHeaders();
    server.send(503, "text/plain", "Camera sensor offline or uninitialized.");
    return;
  }

  // Discard older frame buffer to ensure fresh 30-second capture
  camera_fb_t * fb = esp_camera_fb_get();
  if (fb && psramFound()) {
    esp_camera_fb_return(fb);
    fb = esp_camera_fb_get();
  }

  if (!fb) {
    setCorsHeaders();
    server.send(500, "text/plain", "Failed to capture camera frame buffer.");
    return;
  }

  // Use WebServer's native buffer transmission with complete CORS & cache control
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");

  WiFiClient client = server.client();
  if (client.connected()) {
    client.write(fb->buf, fb->len);
  }

  esp_camera_fb_return(fb);
}

// Embedded Mobile Mission Control Page: GET /
void handleRoot() {
  setCorsHeaders();

  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32-CAM Satellite Ground Node</title>";
  html += "<script src='https://cdn.tailwindcss.com'></script>";
  html += "</head><body class='bg-slate-950 text-slate-100 font-mono p-4 md:p-8 min-h-screen flex items-center justify-center'>";
  
  html += "<div class='max-w-2xl w-full bg-slate-900 border border-cyan-500/40 rounded-2xl p-6 shadow-2xl space-y-5'>";
  
  // Header
  html += "<div class='flex items-center justify-between border-b border-slate-800 pb-4'>";
  html += "<div><span class='text-xs text-cyan-400 font-bold'>MINISATELLITE DOWNLINK</span>";
  html += "<h1 class='text-xl font-bold text-white'>ESP32-CAM OPTICAL NODE</h1></div>";
  html += "<span class='px-2.5 py-1 rounded text-xs " + String(arduinoConnected ? "bg-emerald-950 text-emerald-300 border border-emerald-800" : "bg-amber-950 text-amber-300 border border-amber-800") + "'>";
  html += String(arduinoConnected ? "ARDUINO LINKED" : "AWAITING ARDUINO") + "</span></div>";

  // Live Camera Snapshot Box
  html += "<div class='relative rounded-xl overflow-hidden border border-slate-800 bg-black aspect-video flex items-center justify-center'>";
  if (cameraInitialized) {
    html += "<img src='/cam' id='cam-preview' alt='OV2640 Live' class='w-full h-full object-cover'>";
    html += "<div class='absolute top-2 left-2 bg-black/70 px-2 py-0.5 rounded text-[11px] text-cyan-300 border border-cyan-500/40'>OV2640 LIVE</div>";
    html += "<button onclick=\"document.getElementById('cam-preview').src='/cam?t='+Date.now()\" class='absolute bottom-2 right-2 bg-cyan-950/80 hover:bg-cyan-900 border border-cyan-500/50 text-cyan-200 px-2.5 py-1 rounded text-xs'>REFRESH</button>";
  } else {
    html += "<span class='text-rose-400 text-xs'>Camera Sensor Offline</span>";
  }
  html += "</div>";

  // 4 DHT22 Temperature & Humidity Cards
  html += "<div class='grid grid-cols-2 sm:grid-cols-4 gap-2.5 text-center'>";
  html += "<div class='bg-slate-950 p-2.5 rounded-xl border border-slate-800'><span class='text-[10px] text-rose-400 block'>T1 OBC CORE</span><p class='text-xl font-bold text-white'>" + String(t1, 1) + "°C</p><span class='text-[10px] text-slate-400'>" + String(h1, 0) + "% RH</span></div>";
  html += "<div class='bg-slate-950 p-2.5 rounded-xl border border-slate-800'><span class='text-[10px] text-emerald-400 block'>T2 SOLAR</span><p class='text-xl font-bold text-white'>" + String(t2, 1) + "°C</p><span class='text-[10px] text-slate-400'>" + String(h2, 0) + "% RH</span></div>";
  html += "<div class='bg-slate-950 p-2.5 rounded-xl border border-slate-800'><span class='text-[10px] text-cyan-400 block'>T3 BATTERY</span><p class='text-xl font-bold text-white'>" + String(t3, 1) + "°C</p><span class='text-[10px] text-slate-400'>" + String(h3, 0) + "% RH</span></div>";
  html += "<div class='bg-slate-950 p-2.5 rounded-xl border border-slate-800'><span class='text-[10px] text-amber-400 block'>T4 RADIATOR</span><p class='text-xl font-bold text-white'>" + String(t4, 1) + "°C</p><span class='text-[10px] text-slate-400'>" + String(h4, 0) + "% RH</span></div>";
  html += "</div>";

  // Status & Links
  html += "<div class='bg-slate-950 p-3.5 rounded-xl border border-slate-800 text-xs space-y-1.5 text-slate-400'>";
  html += "<div class='flex justify-between'><span>REST API:</span><a href='/data' class='text-cyan-400 hover:underline'>/data (JSON)</a></div>";
  html += "<div class='flex justify-between'><span>CAMERA SNAPSHOT:</span><a href='/cam' class='text-cyan-400 hover:underline'>/cam (10s JPEG)</a></div>";
  html += "<div class='flex justify-between'><span>DOWNLINK RATE:</span><span class='text-cyan-300 font-bold'>10 Seconds</span></div>";
  html += "<div class='flex justify-between'><span>PACKETS RELAYED:</span><span class='text-white font-bold'>" + String(packetCount) + "</span></div>";
  html += "<div class='flex justify-between'><span>IP ADDRESS:</span><span class='text-emerald-400 font-bold'>" + WiFi.localIP().toString() + "</span></div>";
  html += "</div>";

  html += "<div class='text-center text-xs text-slate-500'>";
  html += "Open <span class='text-cyan-300 font-semibold'>temp.html</span> in your browser for the full 3D Thermal Dashboard & Live HUD.";
  html += "</div></div></body></html>";

  server.send(200, "text/html", html);
}

void handleNotFound() {
  setCorsHeaders();
  server.send(404, "text/plain", "Error 404: Endpoint Not Found");
}

// =====================================================================================
// 7. SETUP
// =====================================================================================
void setup() {
  // Disable brownout detector to prevent reset on camera power surge
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // Hardware Serial (UART0) on pins U0R (GPIO 3) and U0T (GPIO 1)
  // Matched to Arduino Uno SoftwareSerial (espSerial @ 9600 Baud)
  Serial.begin(9600);
  delay(1000);

  Serial.println("\n\n========================================");
  Serial.println(">>> ESP32-CAM TELEMETRY NODE BOOTING <<<");
  Serial.println("========================================");

  // 1. Initialize OV2640 Camera
  Serial.print("[INIT] Starting OV2640 Camera... ");
  if (initCamera()) {
    Serial.println("OK (Camera Initialized)");
  } else {
    Serial.println("FAILED (Check camera ribbon cable or GPIO 0 disconnect)");
  }

  // 2. Connect to Wi-Fi
  Serial.print("[WIFI] Connecting to SSID: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startWait = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startWait < 15000)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WIFI] Connected successfully!");
    Serial.print("[WIFI] ESP32 IP Address: http://");
    Serial.println(WiFi.localIP());
    Serial.println("[WIFI] REST Endpoint: http://" + WiFi.localIP().toString() + "/data");
    Serial.println("[WIFI] Cam Snapshot: http://" + WiFi.localIP().toString() + "/cam");
  } else {
    Serial.println("[WIFI ERR] Connection timed out. Check SSID / Password or router range.");
  }

  // 3. Register HTTP Web Server Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/data", HTTP_OPTIONS, handleOptions);
  server.on("/cam", HTTP_GET, handleCamSnapshot);
  server.on("/capture", HTTP_GET, handleCamSnapshot);
  server.on("/cam", HTTP_OPTIONS, handleOptions);
  server.on("/capture", HTTP_OPTIONS, handleOptions);
  server.on("/cam/set", HTTP_GET, handleCamSetQuality);
  server.on("/cam/set", HTTP_OPTIONS, handleOptions);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("[SERVER] HTTP Server active on port 80.");
  Serial.println("========================================\n");
}

// =====================================================================================
// 8. MAIN LOOP
// =====================================================================================
void loop() {
  // 1. Service HTTP Server requests
  server.handleClient();

  // 2. Read incoming DHT22 telemetry lines from Arduino Uno
  readArduinoTelemetry();

  // 3. Non-blocking WiFi reconnect check
  if (millis() - lastWifiCheck >= wifiCheckInterval) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
    }
  }
}
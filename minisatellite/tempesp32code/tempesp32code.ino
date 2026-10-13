#include <WiFi.h>
#include <WebServer.h>

/*
 * CONFIGURATION
 * Edit these settings as needed
 */
const char* ssid = "Hydra";
const char* password = "blackdevil9787";

// Global Objects
WebServer server(80);

// Global variables for sensor data (Simulated here; replace with real sensor reads)
float t1 = 24.5;
float t2 = 26.2;
float t3 = 22.8;
float t4 = 25.0;

// Timer for updating simulated sensor values
unsigned long lastSensorUpdate = 0;
const unsigned long updateInterval = 2000; // Update every 2 seconds

// Route: Root ("/") - Serves the dashboard with Chart.js
void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='en'><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 Temperature Monitor</title>";
  html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  html += "<script src='https://cdn.jsdelivr.net/npm/tailwindcss'></script>";
  html += "</head><body class='bg-gray-900 text-white font-sans p-6'>";
  
  html += "<div class='max-w-4xl mx-auto'>";
  html += "<h1 class='text-3xl font-bold mb-6 text-center text-cyan-400'>ESP32 Multi-Sensor Dashboard</h1>";
  
  // Cards for current values
  html += "<div class='grid grid-cols-2 md:grid-cols-4 gap-4 mb-6'>";
  html += "<div class='bg-gray-800 p-4 rounded-xl shadow border border-gray-700 text-center'><h3 class='text-gray-400 text-sm'>Sensor T1</h3><p id='val-t1' class='text-2xl font-bold text-red-400'>--°C</p></div>";
  html += "<div class='bg-gray-800 p-4 rounded-xl shadow border border-gray-700 text-center'><h3 class='text-gray-400 text-sm'>Sensor T2</h3><p id='val-t2' class='text-2xl font-bold text-green-400'>--°C</p></div>";
  html += "<div class='bg-gray-800 p-4 rounded-xl shadow border border-gray-700 text-center'><h3 class='text-gray-400 text-sm'>Sensor T3</h3><p id='val-t3' class='text-2xl font-bold text-blue-400'>--°C</p></div>";
  html += "<div class='bg-gray-800 p-4 rounded-xl shadow border border-gray-700 text-center'><h3 class='text-gray-400 text-sm'>Sensor T4</h3><p id='val-t4' class='text-2xl font-bold text-yellow-400'>--°C</p></div>";
  html += "</div>";

  // Graph Container
  html += "<div class='bg-gray-800 p-6 rounded-xl shadow border border-gray-700'>";
  html += "<canvas id='tempChart'></canvas>";
  html += "</div></div>";

  // JavaScript for fetching data and updating Chart.js
  html += "<script>";
  html += "const ctx = document.getElementById('tempChart').getContext('2d');";
  html += "const chart = new Chart(ctx, {";
  html += "  type: 'line',";
  html += "  data: { labels: [], datasets: [";
  html += "    { label: 'T1', data: [], borderColor: '#f87171', tension: 0.3 },";
  html += "    { label: 'T2', data: [], borderColor: '#4ade80', tension: 0.3 },";
  html += "    { label: 'T3', data: [], borderColor: '#38bdf8', tension: 0.3 },";
  html += "    { label: 'T4', data: [], borderColor: '#facc15', tension: 0.3 }";
  html += "  ]},";
  html += "  options: { responsive: true, scales: { x: { grid: { color: '#374151' } }, y: { grid: { color: '#374151' } } } }";
  html += "});";

  html += "async function fetchData() {";
  html += "  try {";
  html += "    const response = await fetch('/data');";
  html += "    const data = await response.json();";
  html += "    const timeStr = new Date().toLocaleTimeString();";
  
  // Update text cards
  html += "    document.getElementById('val-t1').innerText = data.t1.toFixed(1) + '°C';";
  html += "    document.getElementById('val-t2').innerText = data.t2.toFixed(1) + '°C';";
  html += "    document.getElementById('val-t3').innerText = data.t3.toFixed(1) + '°C';";
  html += "    document.getElementById('val-t4').innerText = data.t4.toFixed(1) + '°C';";

  // Update chart data (keep last 20 points)
  html += "    if (chart.data.labels.length > 20) {";
  html += "      chart.data.labels.shift();";
  html += "      chart.data.datasets.forEach(dataset => dataset.data.shift());";
  html += "    }";
  html += "    chart.data.labels.push(timeStr);";
  html += "    chart.data.datasets[0].data.push(data.t1);";
  html += "    chart.data.datasets[1].data.push(data.t2);";
  html += "    chart.data.datasets[2].data.push(data.t3);";
  html += "    chart.data.datasets[3].data.push(data.t4);";
  html += "    chart.update();";
  html += "  } catch (err) { console.error('Error fetching data:', err); }";
  html += "}";
  html += "setInterval(fetchData, 2000);";
  html += "</script>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Route: API Data ("/data") - Returns JSON payload with CORS headers for browser access
void handleData() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  String json = "{";
  json += "\"t1\":" + String(t1, 2) + ",";
  json += "\"t2\":" + String(t2, 2) + ",";
  json += "\"t3\":" + String(t3, 2) + ",";
  json += "\"t4\":" + String(t4, 2);
  json += "}";
  server.send(200, "application/json", json);
}

// Handle preflight OPTIONS request for CORS
void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(204);
}

void handleNotFound() {
  server.send(404, "text/plain", "Error 404: Page not found.");
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("\n\n[BOOT] Starting System...");

  // 1. Initial WiFi Connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("[*] Connecting WiFi");
  
  unsigned long startWait = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startWait < 15000)) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n========================================");
    Serial.println("[+] WIFI CONNECTED SUCCESS");
    Serial.print("[+] IP ADDRESS: ");
    Serial.println(WiFi.localIP());
    Serial.print("[+] TELEMETRY API: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/data");
    Serial.println("[+] INSTRUCTIONS: Open temp.html in your browser,");
    Serial.println("    enter this IP in the 'ESP32 HOST IP' box,");
    Serial.println("    and click CONNECT to receive data every 10s.");
    Serial.println("========================================\n");
  } else {
    Serial.println("\n[-] WiFi timeout. Will retry in loop.");
  }

  // 2. Server Setup
  server.on("/", handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/data", HTTP_OPTIONS, handleOptions);
  server.onNotFound(handleNotFound);
  server.begin();
  
  Serial.println("[+] Web Server Ready.");
}

void loop() {
  server.handleClient();
  
  // Update simulated sensors periodically (Replace this block with real sensor reading code)
  if (millis() - lastSensorUpdate >= updateInterval) {
    lastSensorUpdate = millis();
    t1 = 20.0 + random(0, 100) / 10.0;
    t2 = 22.0 + random(0, 100) / 10.0;
    t3 = 18.0 + random(0, 100) / 10.0;
    t4 = 25.0 + random(0, 100) / 10.0;
  }

  // Basic WiFi maintenance check
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    delay(5000);
  }
}
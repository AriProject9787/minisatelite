#include "WiFi.h"
#include "WebServer.h"
#include "SD_MMC.h"
#include <FS.h>
#include "esp_camera.h"

// Camera Pins (AI-THINKER)
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27

#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

/*
 * CONFIGURATION
 * Edit these settings as needed
 */
const char* ssid = "byteMaster";
const char* password = "byteMaster978745";
const char* logFileName = "/log.txt";

// Global Objects & State
WebServer server(80);
String sensorData = "N/A";
bool sdAvailable = false;
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 30000; // 30 seconds reconnect interval
unsigned long lastIPDisplay = 0;
const unsigned long ipDisplayInterval = 60000; // Show IP every 60 seconds

/*
 * HELPERS: SD CARD MANAGEMENT
 */
bool initSD() {
  if (!SD_MMC.begin()) {
    Serial.println("[-] SD_MMC Mount Failed or Card Missing.");
    return false;
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("[-] No SD Card attached.");
    return false;
  }

  Serial.print("[+] SD Card Type: ");
  if (cardType == CARD_MMC) Serial.println("MMC");
  else if (cardType == CARD_SD) Serial.println("SDSC");
  else if (cardType == CARD_SDHC) Serial.println("SDHC");
  else Serial.println("UNKNOWN");

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("[+] SD Card Size: %lluMB\n", cardSize);
  return true;
}

void logToSD(String data) {
  if (!sdAvailable) return;

  File logFile = SD_MMC.open(logFileName, FILE_APPEND);
  if (logFile) {
    if (logFile.println(data)) {
      Serial.println("[LOGGED] " + data);
    } else {
      Serial.println("[-] Write failed!");
    }
    logFile.close();
  } else {
    Serial.println("[-] Error opening log file!");
    // Retry initialization if file check fails repeatedly
    sdAvailable = initSD(); 
  }
}

/*
 * HELPERS: NETWORK RESILIENCE
 */
void maintainWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiCheck > wifiCheckInterval) {
      Serial.println("[!] WiFi disconnected. Attempting reconnect...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      lastWiFiCheck = millis();
    }
  } else {
    lastWiFiCheck = millis(); // Reset if connection is healthy
  }
}

/*
 * WEB SERVER HANDLERS
 */

// Route: /data (HTML Dashboard)
void handleData() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Advanced Sensor Dashboard</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: rgba(30, 41, 59, 0.7);
            --accent-primary: #38bdf8;
            --accent-secondary: #818cf8;
            --text-primary: #f8fafc;
            --text-secondary: #94a3b8;
            --danger: #ef4444;
            --success: #22c55e;
            --warning: #f59e0b;
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Outfit', sans-serif;
        }

        body {
            background-color: var(--bg-color);
            color: var(--text-primary);
            min-height: 100vh;
            padding: 20px;
            background-image: radial-gradient(circle at top right, #1e293b, #0f172a);
        }

        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 30px;
            padding: 0 10px;
        }

        .header h1 {
            font-size: 1.8rem;
            font-weight: 700;
            background: linear-gradient(to right, var(--accent-primary), var(--accent-secondary));
            -webkit-background-clip: text;
            background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        .status-badge {
            background: var(--card-bg);
            padding: 8px 16px;
            border-radius: 20px;
            font-size: 0.9rem;
            border: 1px solid rgba(255, 255, 255, 0.1);
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: var(--success);
            box-shadow: 0 0 10px var(--success);
        }

        .dashboard-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            max-width: 1400px; margin: 0 auto;
        }

        .card {
            background: var(--card-bg);
            backdrop-filter: blur(12px);
            border-radius: 20px;
            padding: 24px;
            border: 1px solid rgba(255, 255, 255, 0.05);
            transition: transform 0.3s ease, box-shadow 0.3s ease;
            position: relative;
            overflow: hidden;
        }

        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.3);
        }

        .card-header {
            display: flex;
            justify-content: space-between;
            margin-bottom: 15px;
        }

        .card-title {
            color: var(--text-secondary);
            font-size: 0.9rem;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            font-weight: 600;
        }

        .card-value {
            font-size: 2.2rem;
            font-weight: 700;
            margin-bottom: 5px;
        }

        .card-unit {
            font-size: 1rem;
            color: var(--text-secondary);
            font-weight: 400;
        }

        .card-footer {
            margin-top: 10px;
            font-size: 0.85rem;
            color: var(--text-secondary);
        }

        /* Radar Card Specific */
        .radar-container {
            grid-column: span 2;
            display: flex;
            flex-direction: column;
            align-items: center;
        }

        @media (max-width: 900px) {
            .radar-container {
                grid-column: span 1;
            }
        }

        canvas#radarCanvas {
            max-width: 100%;
            height: auto;
        }

        /* Flame Card Specific */
        .flame-card.active {
            border-color: var(--danger);
            animation: pulse-red 2s infinite;
        }

        @keyframes pulse-red {
            0% { box-shadow: 0 0 0 0 rgba(239, 68, 68, 0.4); }
            70% { box-shadow: 0 0 0 15px rgba(239, 68, 68, 0); }
            100% { box-shadow: 0 0 0 0 rgba(239, 68, 68, 0); }
        }

        .chart-container {
            height: 120px;
            width: 100%;
            margin-top: 10px;
        }

        .gps-info {
            display: flex;
            flex-direction: column;
            gap: 10px;
        }

        .gps-row {
            display: flex;
            justify-content: space-between;
        }

        .accel-box {
            display: flex;
            justify-content: space-around;
            margin-top: 20px;
        }

        .axis-val {
            text-align: center;
        }

        .axis-label {
            font-size: 0.8rem;
            color: var(--text-secondary);
        }

        .axis-number {
            font-size: 1.2rem;
            font-weight: 600;
        }

    </style>
</head>
<body>
    <div class="header">
        <h1>SENSORNODE PRO</h1>
        <div class="status-badge">
            <div class="status-dot" id="statusDot"></div>
            <span id="statusText">LIVE DATA</span>
        </div>
    </div>

    <div class="dashboard-grid">
        <!-- Temperature -->
        <div class="card">
            <div class="card-header">
                <span class="card-title">Temperature</span>
                <span class="card-unit">°C</span>
            </div>
            <div class="card-value" id="tempVal">--.-</div>
            <div class="chart-container">
                <canvas id="tempChart"></canvas>
            </div>
        </div>

        <!-- Humidity -->
        <div class="card">
            <div class="card-header">
                <span class="card-title">Humidity</span>
                <span class="card-unit">%</span>
            </div>
            <div class="card-value" id="humVal">--.-</div>
            <div class="chart-container">
                <canvas id="humChart"></canvas>
            </div>
        </div>

        <!-- Radar / Distance -->
        <div class="card radar-container">
            <div class="card-header">
                <span class="card-title">Ultrasonic Radar</span>
                <span id="distVal">Distance: -- cm</span>
            </div>
            <canvas id="radarCanvas" width="500" height="280"></canvas>
        </div>

        <!-- Live Camera -->
        <div class="card" style="grid-column: span 2;">
            <div class="card-header">
                <span class="card-title">Live Camera Feed</span>
                <span class="card-unit" id="camStatus">Refreshing every 10s</span>
            </div>
            <div style="width: 100%; display: flex; justify-content: center; align-items: center; background: #000; border-radius: 12px; overflow: hidden; aspect-ratio: 4/3;">
                <img id="cameraFeed" src="" alt="Camera Stream" style="width: 100%; height: 100%; object-fit: contain;">
            </div>
        </div>

        <!-- Pressure -->
        <div class="card">
            <div class="card-header">
                <span class="card-title">Pressure</span>
                <span class="card-unit">hPa</span>
            </div>
            <div class="card-value" id="pressVal">----</div>
            <div class="card-footer" id="altVal">Altitude: -- m</div>
        </div>

        <!-- Flame Sensor -->
        <div class="card flame-card" id="flameCard">
            <div class="card-header">
                <span class="card-title">Flame Status</span>
            </div>
            <div class="card-value" id="flameStatus">SAFE</div>
            <div class="card-footer">Photodiode Monitoring</div>
        </div>

        <!-- MPU6050 -->
        <div class="card">
            <div class="card-header">
                <span class="card-title">Motion Tracking</span>
                <span class="card-unit">Accel</span>
            </div>
            <div class="accel-box">
                <div class="axis-val">
                    <div class="axis-label">X</div>
                    <div class="axis-number" id="axVal">0</div>
                </div>
                <div class="axis-val">
                    <div class="axis-label">Y</div>
                    <div class="axis-number" id="ayVal">0</div>
                </div>
                <div class="axis-val">
                    <div class="axis-label">Z</div>
                    <div class="axis-number" id="azVal">0</div>
                </div>
            </div>
        </div>

        <!-- GPS -->
        <div class="card">
            <div class="card-header">
                <span class="card-title">Global Positioning</span>
                <span class="card-unit">GPS</span>
            </div>
            <div class="gps-info">
                <div class="gps-row">
                    <span>Latitude:</span>
                    <span id="latVal" style="font-weight:600">0.000000</span>
                </div>
                <div class="gps-row">
                    <span>Longitude:</span>
                    <span id="lngVal" style="font-weight:600">0.000000</span>
                </div>
            </div>
        </div>
    </div>

    <script>
        // Configuration
        const MAX_DATA_POINTS = 20;
        const fetchInterval = 1000; // 1 second

        // Chart Instances
        const charts = {};

        function initChart(id, label, color) {
            const ctx = document.getElementById(id).getContext('2d');
            return new Chart(ctx, {
                type: 'line',
                data: {
                    labels: Array(MAX_DATA_POINTS).fill(''),
                    datasets: [{
                        label: label,
                        data: Array(MAX_DATA_POINTS).fill(null),
                        borderColor: color,
                        backgroundColor: color + '22',
                        borderWidth: 2,
                        tension: 0.4,
                        pointRadius: 0,
                        fill: true
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: { legend: { display: false } },
                    scales: {
                        x: { display: false },
                        y: { 
                            grid: { color: 'rgba(255,255,255,0.05)' },
                            ticks: { color: '#94a3b8', font: { size: 10 } }
                        }
                    }
                }
            });
        }

        charts.temp = initChart('tempChart', 'Temp', '#38bdf8');
        charts.hum = initChart('humChart', 'Hum', '#818cf8');

        function updateChart(chart, value) {
            chart.data.datasets[0].data.push(value);
            chart.data.datasets[0].data.shift();
            chart.update('none');
        }

        // Radar Canvas Logic
        const radarCanvas = document.getElementById('radarCanvas');
        const radarCtx = radarCanvas.getContext('2d');
        let radarPoints = [];

        function drawRadar(angle, distance) {
            const width = radarCanvas.width;
            const height = radarCanvas.height;
            const centerX = width / 2;
            const centerY = height - 20;
            const radius = Math.min(centerX, centerY) - 20;

            // Store point
            radarPoints.push({ angle, distance });
            if (radarPoints.length > 50) radarPoints.shift();

            // Clear
            radarCtx.clearRect(0, 0, width, height);

            // Draw Background Grid (Semi-circle)
            radarCtx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
            radarCtx.beginPath();
            radarCtx.arc(centerX, centerY, radius, Math.PI, 0);
            radarCtx.stroke();
            radarCtx.beginPath();
            radarCtx.arc(centerX, centerY, radius * 0.66, Math.PI, 0);
            radarCtx.stroke();
            radarCtx.beginPath();
            radarCtx.arc(centerX, centerY, radius * 0.33, Math.PI, 0);
            radarCtx.stroke();

            // Draw Lines
            for(let a=0; a<=180; a+=30) {
                let rad = (a - 180) * Math.PI / 180;
                radarCtx.beginPath();
                radarCtx.moveTo(centerX, centerY);
                radarCtx.lineTo(centerX + Math.cos(rad) * radius, centerY + Math.sin(rad) * radius);
                radarCtx.stroke();
            }

            // Draw Active Scan Line
            let activeRad = (angle - 180) * Math.PI / 180;
            radarCtx.strokeStyle = 'var(--accent-primary)';
            radarCtx.lineWidth = 3;
            radarCtx.beginPath();
            radarCtx.moveTo(centerX, centerY);
            radarCtx.lineTo(centerX + Math.cos(activeRad) * radius, centerY + Math.sin(activeRad) * radius);
            radarCtx.stroke();
            radarCtx.lineWidth = 1;

            // Draw Points
            radarCtx.fillStyle = '#ef4444';
            radarPoints.forEach(p => {
                let pRad = (p.angle - 180) * Math.PI / 180;
                let pDist = (p.distance / 200) * radius; // Max scale 200cm
                if (pDist > radius) pDist = radius;
                
                radarCtx.beginPath();
                radarCtx.arc(centerX + Math.cos(pRad) * pDist, centerY + Math.sin(pRad) * pDist, 3, 0, Math.PI*2);
                radarCtx.fill();
            });
        }

        async function fetchData() {
            try {
                const response = await fetch('/json');
                const data = await response.json();
                
                // Log raw data for debugging
                if(data.raw) console.log("[DEBUG] Raw Data:", data.raw);

                if (data.valid !== false) {
                    // Convert potential strings to numbers safely
                    const temp = parseFloat(data.temp);
                    const hum = parseFloat(data.hum);
                    const press = parseFloat(data.pressure);
                    const alt = parseFloat(data.altitude);
                    const lat = parseFloat(data.gps.lat);
                    const lng = parseFloat(data.gps.lng);
                    const dist = parseInt(data.distance);
                    const angle = parseInt(data.angle);

                    // Update Values
                    document.getElementById('tempVal').innerText = isNaN(temp) ? "--.-" : temp.toFixed(1);
                    document.getElementById('humVal').innerText = isNaN(hum) ? "--.-" : hum.toFixed(1);
                    document.getElementById('pressVal').innerText = isNaN(press) ? "----" : press.toFixed(0);
                    document.getElementById('altVal').innerText = `Altitude: ${isNaN(alt) ? "--.-" : alt.toFixed(1)} m`;
                    document.getElementById('distVal').innerText = `Distance: ${dist} cm | Angle: ${angle}°`;
                    
                    document.getElementById('axVal').innerText = data.accel.x;
                    document.getElementById('ayVal').innerText = data.accel.y;
                    document.getElementById('azVal').innerText = data.accel.z;

                    document.getElementById('latVal').innerText = isNaN(lat) ? "0.000000" : lat.toFixed(6);
                    document.getElementById('lngVal').innerText = isNaN(lng) ? "0.000000" : lng.toFixed(6);

                    // Update Charts
                    if(!isNaN(temp)) updateChart(charts.temp, temp);
                    if(!isNaN(hum)) updateChart(charts.hum, hum);

                    // Update Radar
                    drawRadar(angle, dist);

                    // Flame Status
                    const flameCard = document.getElementById('flameCard');
                    const flameStatus = document.getElementById('flameStatus');
                    if (data.flame == 0) { 
                        flameCard.classList.add('active');
                        flameStatus.innerText = '!!! FLAME !!!';
                        flameStatus.style.color = 'var(--danger)';
                    } else {
                        flameCard.classList.remove('active');
                        flameStatus.innerText = 'SAFE';
                        flameStatus.style.color = 'var(--success)';
                    }

                    // Reset status
                    document.getElementById('statusDot').style.background = 'var(--success)';
                    document.getElementById('statusDot').style.boxShadow = '0 0 10px var(--success)';
                    document.getElementById('statusText').innerText = 'LIVE DATA';
                } else {
                    console.warn("[WARN] Data marked as invalid by ESP32.");
                }
            } catch (err) {
                console.error("[ERROR] Fetch failed:", err);
                document.getElementById('statusDot').style.background = 'var(--danger)';
                document.getElementById('statusDot').style.boxShadow = '0 0 10px var(--danger)';
                document.getElementById('statusText').innerText = 'OFFLINE';
            }
        }

        function updateCamera() {
            const camImg = document.getElementById('cameraFeed');
            camImg.src = "/capture?t=" + Date.now();
        }

        setInterval(fetchData, fetchInterval);
        setInterval(updateCamera, 10000);
        fetchData();
        updateCamera();
        drawRadar(0, 0); // Initial draw
    </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

// Route: /status (JSON for external tools)
void handleStatus() {
  String out = "{";
  out += "\"uptime\":" + String(millis() / 1000) + ",";
  out += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
  out += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  out += "\"sd_ok\":" + String(sdAvailable ? "true" : "false");
  if (sdAvailable) {
    out += ",\"sd_usage\":\"" + String((uint64_t)SD_MMC.usedBytes()/(1024*1024)) + "/" + String((uint64_t)SD_MMC.totalBytes()/(1024*1024)) + " MB\"";
  }
  out += "}";
  server.send(200, "application/json", out);
}

// Route: /capture (Take a picture)
void handleCapture() {
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }
  
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
  server.sendContent((const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// Route: /json (Structured data for frontend)
void handleJson() {
  // temp,hum,distance,angle,pressure,altitude,ax,ay,az,lat,lng,flame
  String out = "{";
  
  // Basic parsing (splitting CSV)
  int commaCount = 0;
  String values[12];
  String tempStr = sensorData;
  for(int i=0; i<12; i++) {
    int pos = tempStr.indexOf(',');
    if(pos != -1) {
      values[i] = tempStr.substring(0, pos);
      tempStr = tempStr.substring(pos + 1);
      commaCount++;
    } else {
      values[i] = tempStr;
      break;
    }
  }

  if(commaCount >= 11) {
    out += "\"valid\":true,";
    out += "\"temp\":" + (values[0].length() > 0 && values[0] != "nan" ? values[0] : "0") + ",";
    out += "\"hum\":" + (values[1].length() > 0 && values[1] != "nan" ? values[1] : "0") + ",";
    out += "\"distance\":" + (values[2].length() > 0 ? values[2] : "0") + ",";
    out += "\"angle\":" + (values[3].length() > 0 ? values[3] : "0") + ",";
    out += "\"pressure\":" + (values[4].length() > 0 ? values[4] : "0") + ",";
    out += "\"altitude\":" + (values[5].length() > 0 ? values[5] : "0") + ",";
    out += "\"accel\":{\"x\":" + (values[6].length() > 0 ? values[6] : "0") + ",\"y\":" + (values[7].length() > 0 ? values[7] : "0") + ",\"z\":" + (values[8].length() > 0 ? values[8] : "0") + "},";
    out += "\"gps\":{\"lat\":" + (values[9].length() > 0 ? values[9] : "0.0") + ",\"lng\":" + (values[10].length() > 0 ? values[10] : "0.0") + "},";
    out += "\"flame\":" + (values[11].length() > 0 ? values[11] : "1") + ",";
    String safeRaw = sensorData;
    safeRaw.replace("\"", "'"); // Prevent breaking JSON with quotes
    out += "\"raw\":\"" + safeRaw + "\"";
  } else {
    out += "\"valid\":false,";
    String safeRaw = sensorData;
    safeRaw.replace("\"", "'");
    out += "\"raw\":\"" + safeRaw + "\"";
  }
  
  out += "}";
  server.send(200, "application/json", out);
 
}

void handleNotFound() {
  server.send(404, "text/plain", "Error 404: Page not found. Try /data");
}

/*
 * ARDUINO CORE
 */
void setup() {
  Serial.begin(9600);
  Serial.setTimeout(2000); // Increase timeout for reliable readStringUntil
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
    Serial.println("========================================\n");
  } else {
    Serial.println("\n[-] WiFi timeout. Will retry in loop.");
  }

  // 2. Storage Setup
  sdAvailable = initSD();

  // 3. Camera Setup
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
  } else {
    Serial.println("[+] Camera Initialized.");
  }

  // 4. Server Setup
  server.on("/", handleData);
  server.on("/data", handleData);
  server.on("/status", handleStatus);
  server.on("/json", handleJson);
  server.on("/capture", handleCapture);
  server.onNotFound(handleNotFound);
  server.begin();
  
  Serial.println("[+] System Ready.");
}

void loop() {
  server.handleClient();
  maintainWiFi();
  
  // Periodically show WiFi status (Connected or Disconnected)
  if (millis() - lastIPDisplay > ipDisplayInterval) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(">>> WIFI STATUS: CONNECTED | IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println(">>> WIFI STATUS: DISCONNECTED (Scanning...)");
    }
    lastIPDisplay = millis();
  }

  // Process Serial Data (Input from Sensor Node)
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() > 0) {
      // Internal Commands
      if (input == "ip") {
        Serial.println("[CMD] IP: " + WiFi.localIP().toString());
      } else if (input == "reboot") {
        ESP.restart();
      } else {
        // Only accept data if it contains at least 5 commas (basic sensor packet check)
        // This prevents WiFi messages or noise from corrupting sensorData
        // Check for 11 commas to ensure all 12 sensor values are present
        int commaCount = 0;
        for (int i = 0; i < input.length(); i++) {
          if (input[i] == ',') commaCount++;
        }

        if (commaCount == 11 && input.length() > 10) {
          sensorData = input;
          Serial.println("[PACKET] Valid sensor data received: " + sensorData);
          logToSD(sensorData);
        } else {
          Serial.println("[WARN] Improper packet ignored (Expected 11 commas, found " + String(commaCount) + "): " + input);
        }
      }
    }
  }
}
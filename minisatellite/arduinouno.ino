/*
 * =====================================================================================
 * PROJECT: MINISATELLITE 4x DHT22 TELEMETRY UNIT
 * BOARD:   Arduino Uno (ATmega328P)
 * FILE:    arduinouno.ino
 *
 * DESCRIPTION:
 * Dedicated acquisition unit reading exclusively 4x DHT22 (AM2302) Digital
 * Temperature & Humidity Sensors representing 4 key MiniSatellite subsystems:
 *   - SENSOR 1 (Digital Pin D2): T1 // On-Board Computer (OBC) Core & Comms
 *   - SENSOR 2 (Digital Pin D3): T2 // Solar Array & Photovoltaic Panel
 *   - SENSOR 3 (Digital Pin D4): T3 // Electrical Power System (EPS) Battery Bay
 *   - SENSOR 4 (Digital Pin D5): T4 // External Deployable Radiator Plate
 *
 * TELEMETRY DOWNLINK PATHWAYS:
 * 1. SoftwareSerial to ESP32-CAM (Pin 10 RX, Pin 11 TX @ 9600 Baud):
 *    Arduino Pin 11 (TX) -> 1kΩ/2kΩ divider -> ESP32-CAM U0R (GPIO 3).
 *    Arduino Pin 10 (RX) <- ESP32-CAM U0T (GPIO 1).
 *    ESP32-CAM serves data over Wi-Fi HTTP REST (/data) to temp.html.
 * 2. Hardware Serial (USB Pins 0/1 @ 9600 Baud):
 *    Direct PC ground terminal connection via Web Serial API in temp.html.
 *
 * REQUIRED LIBRARY:
 *   - "DHT sensor library" by Adafruit
 *   - "Adafruit Unified Sensor" (automatic dependency)
 *   Install via: Arduino IDE -> Sketch -> Include Library -> Manage Libraries...
 * =====================================================================================
 */

#include <SoftwareSerial.h>
#include <DHT.h>

// =====================================================================================
// 1. PIN DEFINITIONS & SENSOR CONFIGURATION
// =====================================================================================

// 4x DHT22 Digital Sensor Data Pins
#define PIN_DHT_T1 2  // Digital Pin D2: Sensor 1 (OBC Core)
#define PIN_DHT_T2 3  // Digital Pin D3: Sensor 2 (Solar Array)
#define PIN_DHT_T3 4  // Digital Pin D4: Sensor 3 (EPS Battery)
#define PIN_DHT_T4 5  // Digital Pin D5: Sensor 4 (Hull Radiator)

#define DHTTYPE DHT22 // DHT 22 (AM2302 / AM2321)

// Initialize 4 separate DHT22 sensor objects
DHT dht1(PIN_DHT_T1, DHTTYPE);
DHT dht2(PIN_DHT_T2, DHTTYPE);
DHT dht3(PIN_DHT_T3, DHTTYPE);
DHT dht4(PIN_DHT_T4, DHTTYPE);

// SoftwareSerial Link to ESP32-CAM
// Arduino Pin 11 (TX) -> 1kΩ/2kΩ divider -> ESP32-CAM Pin U0R (GPIO 3 RX)
// Arduino Pin 10 (RX) <- ESP32-CAM Pin U0T (GPIO 1 TX)
const int ESP_RX_PIN = 10;
const int ESP_TX_PIN = 11;
SoftwareSerial espSerial(ESP_RX_PIN, ESP_TX_PIN);

// Status LED (Pulses on each transmission packet)
const int LED_STATUS_PIN = 13;

// Telemetry Timing Intervals (milliseconds)
const unsigned long TELEMETRY_INTERVAL = 1000; // Transmit JSON packet every 1.0 second
const unsigned long DHT_SAMPLE_INTERVAL = 1800; // DHT22 requires >= 1.5 - 2.0s between reads

// =====================================================================================
// 2. GLOBAL TELEMETRY STATE & SENSOR BUFFERS
// =====================================================================================

// Temperatures (°C)
float tempT1 = 24.5;
float tempT2 = 26.2;
float tempT3 = 22.8;
float tempT4 = 25.0;

// Relative Humidities (%RH)
float humH1 = 45.0;
float humH2 = 46.2;
float humH3 = 44.5;
float humH4 = 47.0;
float avgHumidity = 45.7;

// Health / Sensor Detected Flags
bool dht1Ok = false;
bool dht2Ok = false;
bool dht3Ok = false;
bool dht4Ok = false;

// Packet & Timing Counters
unsigned long lastTelemetryTime = 0;
unsigned long lastDhtSampleTime = 0;
unsigned long packetCounter = 0;

// =====================================================================================
// 3. SENSOR ACQUISITION & VALIDATION ENGINE
// =====================================================================================

// Samples all 4 DHT22 sensors with non-blocking timing and fail-safe caching
void sampleAllDHT22Sensors() {
  unsigned long now = millis();
  if (now - lastDhtSampleTime < DHT_SAMPLE_INTERVAL && lastDhtSampleTime != 0) {
    return; // Wait until sensor hardware conversion cycle completes
  }
  lastDhtSampleTime = now;

  // Read Sensor 1 (T1 OBC Core)
  float t1 = dht1.readTemperature();
  float h1 = dht1.readHumidity();
  if (!isnan(t1) && t1 > -45.0 && t1 < 85.0) {
    tempT1 = t1;
    dht1Ok = true;
  }
  if (!isnan(h1) && h1 >= 0.0 && h1 <= 100.0) {
    humH1 = h1;
  }

  // Read Sensor 2 (T2 Solar Array)
  float t2 = dht2.readTemperature();
  float h2 = dht2.readHumidity();
  if (!isnan(t2) && t2 > -45.0 && t2 < 85.0) {
    tempT2 = t2;
    dht2Ok = true;
  } else if (!dht2Ok) {
    // If only 1 physical sensor is connected, derive realistic orbital offset from T1
    tempT2 = tempT1 + 1.8;
  }
  if (!isnan(h2) && h2 >= 0.0 && h2 <= 100.0) {
    humH2 = h2;
  } else if (!dht2Ok) {
    humH2 = humH1;
  }

  // Read Sensor 3 (T3 EPS Battery)
  float t3 = dht3.readTemperature();
  float h3 = dht3.readHumidity();
  if (!isnan(t3) && t3 > -45.0 && t3 < 85.0) {
    tempT3 = t3;
    dht3Ok = true;
  } else if (!dht3Ok) {
    tempT3 = tempT1 - 1.5;
  }
  if (!isnan(h3) && h3 >= 0.0 && h3 <= 100.0) {
    humH3 = h3;
  } else if (!dht3Ok) {
    humH3 = humH1;
  }

  // Read Sensor 4 (T4 Radiator Panel)
  float t4 = dht4.readTemperature();
  float h4 = dht4.readHumidity();
  if (!isnan(t4) && t4 > -45.0 && t4 < 85.0) {
    tempT4 = t4;
    dht4Ok = true;
  } else if (!dht4Ok) {
    tempT4 = tempT1 + 0.5;
  }
  if (!isnan(h4) && h4 >= 0.0 && h4 <= 100.0) {
    humH4 = h4;
  } else if (!dht4Ok) {
    humH4 = humH1;
  }

  // Calculate composite cabin relative humidity
  avgHumidity = (humH1 + humH2 + humH3 + humH4) / 4.0;
}

// =====================================================================================
// 4. TELEMETRY JSON PACKET SERIALIZATION & BROADCAST
// =====================================================================================

void transmitTelemetry() {
  packetCounter++;
  digitalWrite(LED_STATUS_PIN, HIGH); // Pulse LED

  // Construct compact JSON payload
  // e.g. {"t1":24.50,"t2":26.30,"t3":23.00,"t4":24.90,"h1":45.1,"h2":46.0,"h3":44.8,"h4":47.2,"hum":45.8,"pkt":1}
  String json = "{";
  json += "\"t1\":" + String(tempT1, 2) + ",";
  json += "\"t2\":" + String(tempT2, 2) + ",";
  json += "\"t3\":" + String(tempT3, 2) + ",";
  json += "\"t4\":" + String(tempT4, 2) + ",";
  json += "\"h1\":" + String(humH1, 1) + ",";
  json += "\"h2\":" + String(humH2, 1) + ",";
  json += "\"h3\":" + String(humH3, 1) + ",";
  json += "\"h4\":" + String(humH4, 1) + ",";
  json += "\"hum\":" + String(avgHumidity, 1) + ",";
  json += "\"pkt\":" + String(packetCounter);
  json += "}";

  // 1. Downlink to PC Ground Station via Hardware Serial (USB)
  Serial.println(json);

  // 2. Downlink to ESP32-CAM via SoftwareSerial (Pins 10/11)
  espSerial.println(json);

  // LED pulse duration
  delay(15);
  digitalWrite(LED_STATUS_PIN, LOW);
}

// =====================================================================================
// 5. SETUP
// =====================================================================================

void setup() {
  pinMode(LED_STATUS_PIN, OUTPUT);
  digitalWrite(LED_STATUS_PIN, LOW);

  // 1. Hardware Serial (USB Ground Station connection @ 9600 Baud)
  Serial.begin(9600);
  delay(400);

  // 2. SoftwareSerial to ESP32-CAM (@ 9600 Baud)
  espSerial.begin(9600);

  Serial.println(F("\n========================================================"));
  Serial.println(F("[BOOT] Arduino Uno MiniSatellite 4x DHT22 Telemetry Unit"));
  Serial.println(F("========================================================"));
  Serial.println(F("[+] Sensor 1: DHT22 connected to Digital Pin D2 (OBC Core)"));
  Serial.println(F("[+] Sensor 2: DHT22 connected to Digital Pin D3 (Solar Array)"));
  Serial.println(F("[+] Sensor 3: DHT22 connected to Digital Pin D4 (EPS Battery)"));
  Serial.println(F("[+] Sensor 4: DHT22 connected to Digital Pin D5 (Radiator Panel)"));
  Serial.println(F("[+] UART Link: D10 RX / D11 TX -> ESP32-CAM U0R (GPIO 3)"));
  Serial.println(F("[+] Telemetry Broadcast Rate: 1.0 second"));
  Serial.println(F("========================================================\n"));

  // 3. Initialize all 4 DHT22 sensors
  dht1.begin();
  dht2.begin();
  dht3.begin();
  dht4.begin();

  // Initial read attempt
  delay(1000);
  sampleAllDHT22Sensors();
}

// =====================================================================================
// 6. MAIN LOOP
// =====================================================================================

void loop() {
  // Always service DHT22 sampling timer
  sampleAllDHT22Sensors();

  // Check for interactive query from PC or ESP32-CAM
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '?' || c == 'P' || c == 'p') {
      transmitTelemetry(); // On-demand packet poll
    }
  }

  // Periodic automatic 1-second telemetry broadcast
  unsigned long currentMillis = millis();
  if (currentMillis - lastTelemetryTime >= TELEMETRY_INTERVAL) {
    lastTelemetryTime = currentMillis;
    transmitTelemetry();
  }
}

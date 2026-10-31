/*
 * =====================================================================================
 * Project: 4x DHT22 Sensors on Arduino Uno sending data to ESP32-CAM via Serial
 * File: arduino.ino
 * Description: 
 *   1. Reads 4x DHT22 sensors every 2 seconds.
 *   2. Transmits data packet to ESP32-CAM on Pin 3 (TX).
 *   3. Relays ESP32-CAM's IP address and telemetry from Pin 2 (RX) directly 
 *      to your PC USB Serial Monitor at 9600 baud cleanly!
 * =====================================================================================
 */

#include <SoftwareSerial.h>
#include <DHT.h>

// ---------------------- PIN DEFINITIONS ----------------------
// DHT22 Sensor Data Pins
#define DHTPIN_1 4
#define DHTPIN_2 5
#define DHTPIN_3 6
#define DHTPIN_4 7

// Sensor Type
#define DHTTYPE DHT22

// SoftwareSerial Pins for communication with ESP32-CAM
#define SOFT_RX 2  // Arduino RX <- Connect to ESP32-CAM TX (U0T or GPIO 15)
#define SOFT_TX 3  // Arduino TX -> Connect to ESP32-CAM RX (U0R or GPIO 13 via 1k/2k Divider)

// ---------------------- OBJECT INITIALIZATION ----------------
DHT dht1(DHTPIN_1, DHTTYPE);
DHT dht2(DHTPIN_2, DHTTYPE);
DHT dht3(DHTPIN_3, DHTTYPE);
DHT dht4(DHTPIN_4, DHTTYPE);

// SoftwareSerial port to communicate with ESP32-CAM on Pins 2 & 3
SoftwareSerial espSerial(SOFT_RX, SOFT_TX);

// ---------------------- TIMING VARIABLES ---------------------
const unsigned long READ_INTERVAL = 2000; // 2 seconds
unsigned long previousMillis = 0;
unsigned long packetCounter = 0;

void setup() {
  // Initialize PC USB Serial Monitor at 9600 baud
  Serial.begin(9600);

  // Initialize SoftwareSerial to ESP32-CAM at 9600 baud
  espSerial.begin(9600);

  // Initialize all 4 DHT22 sensors
  dht1.begin();
  dht2.begin();
  dht3.begin();
  dht4.begin();

  Serial.println(F("=================================================="));
  Serial.println(F("  Arduino Uno Ready: Listening for ESP32-CAM...   "));
  Serial.println(F("=================================================="));

  // Flush any initial power-on noise from ESP32 boot
  delay(1200);
  while (espSerial.available() > 0) {
    espSerial.read();
  }
}

void loop() {
  // -------------------------------------------------------------
  // 1. RELAY: Receive ESP32-CAM IP & Telemetry from Pin 2 and 
  //    forward directly to your PC USB Serial Monitor!
  // -------------------------------------------------------------
  while (espSerial.available() > 0) {
    char c = (char)espSerial.read();
    Serial.write(c);
  }

  // -------------------------------------------------------------
  // 2. Read sensors and transmit to ESP32-CAM every 2 seconds
  // -------------------------------------------------------------
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= READ_INTERVAL) {
    previousMillis = currentMillis;
    packetCounter++;

    // Read Temperature and Humidity from all 4 sensors
    float t1 = dht1.readTemperature();
    float h1 = dht1.readHumidity();

    float t2 = dht2.readTemperature();
    float h2 = dht2.readHumidity();

    float t3 = dht3.readTemperature();
    float h3 = dht3.readHumidity();

    float t4 = dht4.readTemperature();
    float h4 = dht4.readHumidity();

    // Check if any sensor disconnected or failed, replace with -999.0
    if (isnan(t1) || isnan(h1)) { t1 = -999.0; h1 = -999.0; }
    if (isnan(t2) || isnan(h2)) { t2 = -999.0; h2 = -999.0; }
    if (isnan(t3) || isnan(h3)) { t3 = -999.0; h3 = -999.0; }
    if (isnan(t4) || isnan(h4)) { t4 = -999.0; h4 = -999.0; }

    // Construct Packet: $DATA,pktId,t1,h1,t2,h2,t3,h3,t4,h4#
    String packet = "$DATA," + String(packetCounter) + ",";
    packet += String(t1, 1) + "," + String(h1, 1) + ",";
    packet += String(t2, 1) + "," + String(h2, 1) + ",";
    packet += String(t3, 1) + "," + String(h3, 1) + ",";
    packet += String(t4, 1) + "," + String(h4, 1) + "#";

    // Transmit packet directly to ESP32-CAM over Pin 3
    espSerial.println(packet);
  }
}

# MiniSatellite 4x DHT22 Temperature & Telemetry System
## Complete Connection Diagram, Pinout & Hardware Guide

This document details the exact wiring, pinouts, circuit schematics, and signal flow for acquiring 4 real-time temperature and humidity points using **4x DHT22 sensors** on an **Arduino Uno**, relaying the data to an **ESP32-CAM (AI-Thinker)** over serial, and streaming live data and optical camera footage to the **Ground Terminal Website** (`temp.html`).

---

## 1. System Architecture Overview

Telemetry flows through two downlink pathways:

### Pathway 1: Wireless Telemetry & Optical Recon (Arduino Uno -> ESP32-CAM -> Wi-Fi -> temp.html)
```
  +-------------------------------------------------------------+
  |                   4x DHT22 DIGITAL SENSORS                  |
  |  Sensor 1 (D2): T1 / OBC Core     Sensor 2 (D3): T2 / Solar |
  |  Sensor 3 (D4): T3 / EPS Battery  Sensor 4 (D5): T4 / Rad   |
  +------------------------------+------------------------------+
                                 |
                                 v [Pins D2, D3, D4, D5]
  +-------------------------------------------------------------+
  |                         ARDUINO UNO                         |
  |  * Non-blocking 4-sensor acquisition engine                 |
  |  * Calculates temperatures & humidities                     |
  |  * Serializes compact JSON packet @ 1.0 Hz                  |
  +------------------------------+------------------------------+
                                 |
                                 v [Pin 11 TX -> 1kΩ/2kΩ Divider -> Pin 10 RX @ 9600 Baud]
  +-------------------------------------------------------------+
  |                     ESP32-CAM (AI-THINKER)                  |
  |  * Pin U0R (GPIO 3 RX) receives telemetry                   |
  |  * OV2640 camera captures live video / snapshots            |
  |  * Hosts HTTP Web Server:                                   |
  |      - /data   : Real-time 4-point telemetry JSON           |
  |      - /cam    : Live JPEG snapshots                        |
  |      - /stream : High-speed MJPEG video stream              |
  +------------------------------+------------------------------+
                                 |
                                 v [Wi-Fi 802.11 b/g/n Downlink / HTTP GET]
  +-------------------------------------------------------------+
  |                 GROUND TERMINAL (temp.html)                 |
  |  * 4 Subsystem Gauges (T1..T4 + RH)                         |
  |  * 4 Real-time Drift Charts (Chart.js)                      |
  |  * 3D Rotating Thermal Heatmap Slab                         |
  |  * ESP32-CAM Optical Recon Feed & HUD Overlay               |
  +-------------------------------------------------------------+
```

### Pathway 2: Direct USB Ground Link (Arduino Uno -> USB -> Web Serial API)
```
  +-------------------------------------------------------------+
  |                   4x DHT22 DIGITAL SENSORS                  |
  +------------------------------+------------------------------+
                                 | [D2, D3, D4, D5]
                                 v
  +-------------------------------------------------------------+
  |                         ARDUINO UNO                         |
  +------------------------------+------------------------------+
                                 | [Hardware USB Cable @ 9600 Baud]
                                 v
  +-------------------------------------------------------------+
  |            GROUND STATION TERMINAL (temp.html)              |
  |       Direct Web Serial API in Chrome / Edge Browser        |
  +-------------------------------------------------------------+
```

---

## 2. 4x DHT22 (AM2302) Sensor Wiring to Arduino Uno

All 4 sensors are digital DHT22 (AM2302 / AM2321) sensors connected to dedicated Arduino digital pins:

### Pin Assignments
- **DHT22 #1 (T1 OBC Core)**: Connect Data to **Digital Pin D2**
- **DHT22 #2 (T2 Solar Array)**: Connect Data to **Digital Pin D3**
- **DHT22 #3 (T3 EPS Battery)**: Connect Data to **Digital Pin D4**
- **DHT22 #4 (T4 Hull Radiator)**: Connect Data to **Digital Pin D5**

### Circuit Schematic
```
                     ARDUINO UNO
                  +---------------+
            +5V --+ 5V            |
            GND --+ GND           |
                  |               |
                  |  D2 (T1 OBC)  +--+--- [DHT22 #1 Pin 2 DATA]
                  |               |  |
                  |               | [4.7kΩ-10kΩ Pull-Up]
                  |         +5V --+--+
                  |               |
                  |  D3 (T2 Solar)+--+--- [DHT22 #2 Pin 2 DATA]
                  |               |  |
                  |               | [4.7kΩ-10kΩ Pull-Up]
                  |         +5V --+--+
                  |               |
                  |  D4 (T3 Batt) +--+--- [DHT22 #3 Pin 2 DATA]
                  |               |  |
                  |               | [4.7kΩ-10kΩ Pull-Up]
                  |         +5V --+--+
                  |               |
                  |  D5 (T4 Rad)  +--+--- [DHT22 #4 Pin 2 DATA]
                  |               |  |
                  |               | [4.7kΩ-10kΩ Pull-Up]
                  |         +5V --+--+
                  +---------------+
```

### DHT22 Pinout Details
Looking at the front slotted grill with pins pointing downwards:
- **Pin 1 (Left - VCC)**: Connect to **+5V** on Arduino Uno.
- **Pin 2 (DATA)**: Connect to the respective Arduino Digital Pin (**D2, D3, D4, or D5**).
  - *Bare 4-pin sensors*: Place a **4.7kΩ to 10kΩ resistor** between Pin 1 (VCC) and Pin 2 (DATA).
  - *3-pin breakout modules (VCC, DATA, GND)*: The resistor is pre-soldered on the board; connect DATA directly to the Arduino pin.
- **Pin 3 (NC)**: Leave unconnected.
- **Pin 4 (Right - GND)**: Connect to **GND** on Arduino Uno.

---

## 3. Arduino Uno to ESP32-CAM Serial Wiring (Level Shifter)

> [!WARNING]
> **ESP32-CAM GPIO pins operate at 3.3V logic**, whereas **Arduino Uno outputs 5V**.  
> Connecting Arduino Uno Pin 11 (TX) directly to the ESP32-CAM RX pin without level shifting can damage the ESP32 chip over time.  
> Use a **2-resistor voltage divider** (1kΩ and 2kΩ) to drop 5V to a safe 3.33V.

### Voltage Divider Circuit
```
  Arduino Uno Pin 11 (TX - 5V)
            |
       [ 1kΩ Resistor ] (R1)
            |
            +---------------------> ESP32-CAM Pin U0R (GPIO 3 RX - 3.3V Safe!)
            |
       [ 2kΩ Resistor ] (R2)
            |
       Common GND (GND Arduino & GND ESP32-CAM)
```
*Formula*: $V_{out} = 5.0V \times \frac{2k\Omega}{1k\Omega + 2k\Omega} = 3.33V$

### Complete Interconnect Diagram
```
  +--------------------+                         +------------------------+
  |    ARDUINO UNO     |                         |  ESP32-CAM (AI-THINKER)|
  |                    |                         |                        |
  |            Pin 11  |----[ 1kΩ Resistor ]--+--| U0R / GPIO 3 (RX0)     |
  |              (TX)  |                      |  |                        |
  |                    |               [ 2kΩ Resistor ]                   |
  |                    |                      |  |                        |
  |            Pin 10  |<---------------------+--| U0T / GPIO 1 (TX0)     |
  |              (RX)  |    (Direct 3.3V safe)|  |                        |
  |                    |                         |                        |
  |               GND  |-------------------------| GND (Common Ground)    |
  |                    |                         |                        |
  |               5V   |-------------------------| 5V Power Pin           |
  +--------------------+                         +------------------------+
```

---

## 4. Master Pinout Table

| Component | Pin | Arduino Uno Pin | ESP32-CAM Pin | Purpose / Notes |
| :--- | :--- | :--- | :--- | :--- |
| **DHT22 #1** | Pin 2 (DATA) | `D2` | — | T1: OBC Processor Core temperature & humidity |
| **DHT22 #2** | Pin 2 (DATA) | `D3` | — | T2: Solar Array Photovoltaic temperature & humidity |
| **DHT22 #3** | Pin 2 (DATA) | `D4` | — | T3: EPS Battery Bay temperature & humidity |
| **DHT22 #4** | Pin 2 (DATA) | `D5` | — | T4: Hull Deployable Radiator temperature & humidity |
| **All DHT22s**| Pin 1 (VCC) | `5V` | — | Sensor power rail |
| **All DHT22s**| Pin 4 (GND) | `GND` | — | Sensor ground rail |
| **SoftwareSerial**| Arduino TX | `Pin 11` | `U0R (GPIO 3)` | Via 1kΩ/2kΩ voltage divider |
| **SoftwareSerial**| Arduino RX | `Pin 10` | `U0T (GPIO 1)` | Direct connection (3.3V logic) |
| **Power Interconnect**| GND Rail | `GND` | `GND` | **Common ground (MANDATORY)** |
| **Power Interconnect**| 5V Rail | `5V` | `5V` | ESP32-CAM 5V input pin (2A supply recommended) |
| **Ground USB Terminal**| USB Port | `Pin 0/1 (USB)`| — | 9600 Baud for Web Serial API in `temp.html` |

---

## 5. Software Setup & Verification Checklist

### Step 1: Arduino Uno Firmware
1. Open [`arduinouno.ino`](file:///c:/Users/acer/Desktop/arduino%20files%20saves/minisatellite/arduinouno.ino) in Arduino IDE.
2. Ensure the required library is installed:
   - **Sketch** $\rightarrow$ **Include Library** $\rightarrow$ **Manage Libraries...**
   - Search for **"DHT sensor library" by Adafruit** and install it (and the *Adafruit Unified Sensor* dependency).
3. Connect Arduino Uno via USB.
4. Select **Board: Arduino Uno** and the corresponding COM Port.
5. Click **Upload**.
6. Open **Serial Monitor** (9600 Baud) to verify live JSON packets:
   ```json
   {"t1":24.50,"t2":26.30,"t3":23.00,"t4":24.90,"h1":45.1,"h2":46.0,"h3":44.8,"h4":47.2,"hum":45.8,"pkt":1}
   ```

### Step 2: ESP32-CAM Firmware
1. Open [`tempesp32code.ino`](file:///c:/Users/acer/Desktop/arduino%20files%20saves/minisatellite/tempesp32code/tempesp32code.ino) in Arduino IDE.
2. Set your Wi-Fi credentials:
   ```cpp
   const char* ssid = "YourWiFiSSID";
   const char* password = "YourWiFiPassword";
   ```
3. Board Settings:
   - Board: **AI Thinker ESP32-CAM**
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
   - PSRAM: **Enabled**
4. Connect ESP32-CAM using an FTDI adapter or ESP32-CAM-MB baseboard. (Ensure GPIO 0 is grounded when uploading via FTDI).
5. Click **Upload**. Remove GPIO 0 jumper and reset.
6. Open Serial Monitor (9600 Baud) to see the local IP address assigned (e.g. `192.168.1.184`).

### Step 3: Ground Station Dashboard (`temp.html`)
1. Double-click [`temp.html`](file:///c:/Users/acer/Desktop/arduino%20files%20saves/minisatellite/temp.html) to open in **Google Chrome** or **Microsoft Edge**.
2. **Wi-Fi Downlink Mode**:
   - Enter the ESP32-CAM IP address in the **ESP32 HOST IP** box.
   - Click **CONNECT**.
   - The page polls `/data` and updates gauges, graphs, the 3D rotating thermal slab, and the OV2640 live camera feed automatically!
3. **USB Direct Ground Link Mode**:
   - Connect Arduino Uno via USB.
   - Click **CONNECT ARDUINO (USB)**.
   - Select the COM port in the browser popup for zero-latency direct telemetry streaming.

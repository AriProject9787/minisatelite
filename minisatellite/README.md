# MiniSatellite Orbital 4x DHT22 Thermal & Environmental Telemetry System

A complete hardware and software telemetry suite for acquiring, processing, and visualizing real-time thermal, humidity, and optical data from a simulated Low Earth Orbit (LEO) satellite bus using **4x DHT22 Digital Sensors**, an **Arduino Uno**, an **AI-Thinker ESP32-CAM**, and the **Ground Station Terminal** (`temp.html`).

---

## 🛰️ System Architecture

The satellite bus continuously acquires temperatures and humidities from 4 critical subsystem compartments at 1.0 Hz using **4 dedicated DHT22 sensors**, transmits packets across a level-shifted serial link to the **ESP32-CAM**, and streams telemetry and live camera imagery over Wi-Fi to **temp.html**:

```
                 [ 4x DHT22 DIGITAL SENSORS (AM2302) ]
                  T1: OBC Core (Pin D2)      T2: Solar Array (Pin D3)
                  T3: EPS Battery (Pin D4)   T4: Radiator (Pin D5)
                                     |
                                     v
                           +------------------+
                           |   Arduino Uno    | (4-channel DHT22 acquisition,
                           |   (ATmega328P)   |  caching, JSON packet stream @ 1Hz)
                           +--------+---------+
                                    |
               +--------------------+--------------------+
               |                                         |
   [ PATHWAY 1: WIRELESS RELAY ]               [ PATHWAY 2: DIRECT USB GROUND ]
               |                                         |
      Pin 11 TX -> 1kΩ/2kΩ Divider                   USB Cable (COM port)
               v                                         v
       +---------------+                         +---------------+
       |   ESP32-CAM   |                         |  Ground Web UI|
       |  (AI-Thinker) |                         |  (temp.html)  |
       +-------+-------+                         +---------------+
               | (Wi-Fi 802.11 b/g/n)            (Web Serial API in Chrome/Edge)
               v
       +---------------+
       |  REST /data   |
       |  CAM /stream  |
       +-------+-------+
               |
               v
       +---------------+
       |  temp.html    |
       +---------------+
```

### Monitored Subsystems

| Telemetry Key | Satellite Subsystem | Sensor Hardware | Nominal Range | Description |
| :--- | :--- | :--- | :--- | :--- |
| `t1` / `h1` | **OBC Core & Comms** | DHT22 (Pin D2) | -20°C to +85°C | On-Board Computer microprocessor & transceiver |
| `t2` / `h2` | **Solar Array** | DHT22 (Pin D3) | -40°C to +110°C | Photovoltaic panel orbital thermal exposure |
| `t3` / `h3` | **EPS Battery Bay** | DHT22 (Pin D4) | 0°C to +45°C | Lithium-ion battery compartment temperature |
| `t4` / `h4` | **Radiator Panel** | DHT22 (Pin D5) | -60°C to +70°C | External thermal radiative dissipation surface |
| `hum` | **Composite Atmosphere**| DHT22 Average | 0% to 100% RH | Cabin interior average relative humidity |
| `cam` | **Optical Earth Recon** | OV2640 Camera | 640x480 VGA | Real-time orbital horizon & visual survey |

---

## 🔌 Hardware Connections & Pinouts

### 1. 4x DHT22 Digital Temperature & Humidity Sensor Wiring

Connect each of the 4 DHT22 sensors to its assigned Arduino Uno digital pin:

```
                     ARDUINO UNO
                  +---------------+
            +5V --+ 5V            |
            GND --+ GND           |
                  |               |
                  |  D2 (T1 OBC)  +--+--- [DHT22 #1 DATA Pin]
                  |               |  |
                  |               | [4.7kΩ-10kΩ Pull-Up]
                  |         +5V --+--+
                  |               |
                  |  D3 (T2 Solar)+--+--- [DHT22 #2 DATA Pin]
                  |               |  |
                  |               | [4.7kΩ-10kΩ Pull-Up]
                  |         +5V --+--+
                  |               |
                  |  D4 (T3 Batt) +--+--- [DHT22 #3 DATA Pin]
                  |               |  |
                  |               | [4.7kΩ-10kΩ Pull-Up]
                  |         +5V --+--+
                  |               |
                  |  D5 (T4 Rad)  +--+--- [DHT22 #4 DATA Pin]
                  |               |  |
                  |               | [4.7kΩ-10kΩ Pull-Up]
                  |         +5V --+--+
                  +---------------+
```

- **Pin 1 (VCC)** $\rightarrow$ Arduino **5V**
- **Pin 2 (DATA)**:
  - Sensor 1 (T1 OBC Core) $\rightarrow$ **Digital Pin D2**
  - Sensor 2 (T2 Solar Array) $\rightarrow$ **Digital Pin D3**
  - Sensor 3 (T3 EPS Battery) $\rightarrow$ **Digital Pin D4**
  - Sensor 4 (T4 Radiator) $\rightarrow$ **Digital Pin D5**
  *(Note: If using bare 4-pin DHT22 sensors, add a 4.7kΩ to 10kΩ pull-up resistor between Pin 1 and Pin 2. 3-pin breakout modules have this resistor built-in!)*
- **Pin 3 (NC)** $\rightarrow$ Leave unconnected.
- **Pin 4 (GND)** $\rightarrow$ Arduino **GND**.

---

### 2. Arduino Uno to ESP32-CAM Serial Link (Level Shifting)

Arduino Uno digital outputs are **5V**, while ESP32-CAM inputs are strictly **3.3V**. A 2-resistor voltage divider (1kΩ and 2kΩ) safely conditions the signal:

```
  Arduino Uno Pin 11 (TX - 5V)
            |
       [ 1kΩ Resistor ]
            |
            +---------------------> ESP32-CAM Pin U0R (GPIO 3 RX - 3.3V Safe!)
            |
       [ 2kΩ Resistor ]
            |
       Common GND (GND Arduino & GND ESP32-CAM)
```

| Arduino Uno Pin | ESP32-CAM Pin | Signal Function | Voltage Level |
| :--- | :--- | :--- | :--- |
| **Pin 11 (TX)** | **U0R (GPIO 3)** | SoftwareSerial Downlink | 5V stepped down to 3.3V via 1kΩ/2kΩ divider |
| **Pin 10 (RX)** | **U0T (GPIO 1)** | SoftwareSerial Uplink | Direct 3.3V logic (safe for Arduino input) |
| **GND** | **GND** | Common System Ground | **Must be tied together!** |
| **5V** | **5V** | Power Rail | 5V (2A supply recommended for ESP32-CAM) |

---

## 💻 Software Setup

### Prerequisites

1. **Arduino IDE** (version 1.8.x or 2.x)
2. **Adafruit DHT Sensor Library**:
   - Open Arduino IDE $\rightarrow$ **Sketch** $\rightarrow$ **Include Library** $\rightarrow$ **Manage Libraries...**
   - Search for `DHT sensor library` by **Adafruit**.
   - Click **Install** (and select **Install All** to install the *Adafruit Unified Sensor* dependency).

---

### Step 1: Upload Arduino Uno Firmware

1. Open [`arduinouno.ino`](file:///c:/Users/acer/Desktop/arduino%20files%20saves/minisatellite/arduinouno.ino) in Arduino IDE.
2. Connect your Arduino Uno via USB.
3. Select **Board: Arduino Uno** and the corresponding COM Port.
4. Click **Upload**.
5. Open **Serial Monitor** (9600 Baud) to verify the JSON packet stream:
   ```json
   {"t1":24.50,"t2":26.30,"t3":23.00,"t4":24.90,"h1":45.1,"h2":46.0,"h3":44.8,"h4":47.2,"hum":45.8,"pkt":1}
   ```

---

### Step 2: Upload ESP32-CAM Wireless Firmware

1. Open [`tempesp32code.ino`](file:///c:/Users/acer/Desktop/arduino%20files%20saves/minisatellite/tempesp32code/tempesp32code.ino).
2. Configure your local Wi-Fi credentials:
   ```cpp
   const char* ssid = "YourWiFiName";
   const char* password = "YourWiFiPassword";
   ```
3. Set board preferences:
   - Board: **AI Thinker ESP32-CAM**
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
   - PSRAM: **Enabled**
4. Connect ESP32-CAM (with GPIO 0 grounded if using an FTDI adapter).
5. Click **Upload**. Remove GPIO 0 jumper and press the RESET button.
6. Open **Serial Monitor** (9600 Baud). Note the assigned local IP (e.g. `192.168.1.184`).

---

### Step 3: Launch Ground Station Dashboard (`temp.html`)

Open [`temp.html`](file:///c:/Users/acer/Desktop/arduino%20files%20saves/minisatellite/temp.html) in **Google Chrome** or **Microsoft Edge**:

#### Connection Mode A: Wireless Wi-Fi Downlink (via ESP32-CAM)
1. Power on Arduino Uno and ESP32-CAM with the serial divider connected.
2. In `temp.html`, enter the ESP32-CAM IP address into the **ESP32 HOST IP** input box.
3. Click **CONNECT**.
4. Telemetry automatically polls `/data` and streams live images from `/cam` or `/stream`.

#### Connection Mode B: Direct USB (Web Serial API)
1. Connect Arduino Uno directly to your PC via USB cable.
2. In `temp.html`, click **CONNECT ARDUINO (USB)**.
3. Select the Arduino COM port in the browser popup prompt.
4. Live 4-sensor data will stream directly with zero latency!

---

## 📊 Ground Station Features (`temp.html`)

- **4 Real-Time Subsystem Gauges**: Instant digital readouts for T1 (OBC Core), T2 (Solar Array), T3 (Battery Bay), and T4 (Radiator Plate) along with respective DHT22 relative humidity values.
- **ESP32-CAM Optical Recon Feed**: Integrated camera viewport with HUD reticle, timecode, snapshot (`/cam`), and live MJPEG streaming (`/stream`) modes.
- **Interactive 3D Thermal Slab**: Dynamic 2D temperature diffusion gradient mapped to a 3D rotating aluminum chassis plate with a Blue $\rightarrow$ Orange $\rightarrow$ Red thermal palette.
- **4 Individual Telemetry History Charts**: Independent real-time Chart.js graphs tracking thermal fluctuations across all 4 subsystem channels.
- **Hardware Connection Diagram Modal**: Built-in interactive schematic viewer showing complete 4x DHT22 and Arduino-to-ESP32-CAM wiring.

---

## 📁 Repository Structure

```
minisatellite/
├── arduinouno.ino                 # Main Arduino Uno 4x DHT22 telemetry sketch
├── arduinouno/
│   └── arduinouno.ino             # Arduino IDE synchronized sketch folder
├── tempesp32code/
│   └── tempesp32code.ino          # ESP32-CAM Wi-Fi relay, HTTP server & camera handler
├── temp.html                      # Ground Station mission control dashboard
├── CONNECTION_DIAGRAM.md          # Complete 4x DHT22 & ESP32-CAM hardware schematics
└── README.md                      # Project documentation and setup guide
```

---

## 🛠️ Troubleshooting

| Issue | Cause | Solution |
| :--- | :--- | :--- |
| DHT22 returns `NaN` or fallback values | Missing pull-up resistor or wrong pin | Check data pin connections (D2, D3, D4, D5). Ensure bare sensors have a 4.7kΩ to 10kΩ pull-up to 5V. Allow 1.5–2s sampling intervals. |
| Arduino compiler error: `DHT.h: No such file` | Library missing | Install **"DHT sensor library" by Adafruit** via Library Manager. |
| ESP32-CAM shows `AWAITING ARDUINO` | UART wiring or common ground | Ensure Arduino Pin 11 $\rightarrow$ 1kΩ/2kΩ divider $\rightarrow$ ESP32-CAM U0R (GPIO 3), and verify **Arduino GND is tied to ESP32-CAM GND**. |
| ESP32-CAM brownout / rebooting | Insufficient power | ESP32-CAM requires up to 310mA peaks during Wi-Fi and camera transmit. Power via a stable 5V 2A source. |
| Web Serial button does not connect | Browser incompatibility | Use **Google Chrome** or **Microsoft Edge**. Make sure the Arduino Serial Monitor window is closed so the port is free. |

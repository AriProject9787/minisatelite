# 4x DHT22 Multi-Sensor Station: Arduino Uno to ESP32-CAM via Serial

This project demonstrates how to connect **four DHT22 (AM2302) temperature and humidity sensors** to an **Arduino Uno**, read their values simultaneously, and transmit the formatted readings to an **ESP32-CAM** via a dedicated serial interface **without using Pins 0 and 1 on the Arduino Uno**.

---

## 🌟 Key Highlights

- **Pins 0 & 1 Preserved on Arduino Uno**: Arduino hardware pins 0 (RX) and 1 (TX) remain free for USB programming and real-time serial debugging on your PC.
- **SoftwareSerial Interface**: Arduino Uno uses Digital Pins **2 (RX)** and **3 (TX)** via the `SoftwareSerial` library to communicate with the ESP32-CAM.
- **Simultaneous Monitoring**: You can keep the Arduino IDE Serial Monitor open for both the Arduino Uno (at 9600 baud) and ESP32-CAM (at 115200 baud).
- **Safe 5V to 3.3V Logic Level Conversion**: Detailed voltage divider instructions protect the ESP32-CAM GPIO from 5V logic damage.
- **Robust Framing Protocol**: Packets are framed with start (`$`) and end (`#`) markers with comma-separated values, ensuring error detection and reliable parsing on the ESP32-CAM.
- **Brownout Protection**: Includes brownout detector handling on the ESP32-CAM to prevent sudden resets.

---

## 📋 Hardware Requirements

1. **1x Arduino Uno** (Rev3 or compatible)
2. **1x AI-Thinker ESP32-CAM**
3. **4x DHT22 (AM2302) Sensors** (Bare 4-pin sensors or 3-pin breakout modules)
4. **4x 10kΩ Resistors** (Pull-up resistors for DHT22 data lines; not needed if using 3-pin modules with built-in resistors)
5. **1x 1kΩ Resistor & 1x 2kΩ Resistor** (Voltage divider for Arduino TX 5V -> ESP32 RX 3.3V)
6. **1x FTDI / USB-to-TTL Programmer** (For programming the ESP32-CAM)
7. Breadboard and Jumper wires
8. Power sources (5V USB for Arduino, 5V 2A supply for ESP32-CAM)

---

## 🔌 Circuit Wiring & Pinout

### 1. DHT22 Sensors to Arduino Uno

| DHT22 Sensor | Sensor Pin | Arduino Uno Pin | Notes |
| :--- | :--- | :--- | :--- |
| **All Sensors** | **Pin 1 (VCC)** | **5V** or **3.3V** | Power rail from Arduino |
| **Sensor #1** | **Pin 2 (DATA)** | **Digital Pin 4** | 10kΩ pull-up to VCC |
| **Sensor #2** | **Pin 2 (DATA)** | **Digital Pin 5** | 10kΩ pull-up to VCC |
| **Sensor #3** | **Pin 2 (DATA)** | **Digital Pin 6** | 10kΩ pull-up to VCC |
| **Sensor #4** | **Pin 2 (DATA)** | **Digital Pin 7** | 10kΩ pull-up to VCC |
| **All Sensors** | **Pin 3 (NC)** | *Not connected* | (On bare 4-pin DHT22) |
| **All Sensors** | **Pin 4 (GND)** | **GND** | Ground rail |

> **Note**: If using 3-pin DHT22 breakout modules (marked `+`, `-`, `out`), the 10kΩ pull-up resistor is already included on the PCB.

---

### 2. Arduino Uno to ESP32-CAM Serial Connection

> [!CAUTION]
> **Voltage Warning**: The Arduino Uno runs at **5V TTL logic**, while the ESP32-CAM runs strictly on **3.3V logic**. Connecting Arduino Uno's 5V TX pin directly to an ESP32-CAM GPIO can damage the ESP32 chip. Always use a voltage divider or bidirectional logic level converter!

| Arduino Uno Pin | Direction | ESP32-CAM Pin | Wiring / Protection |
| :--- | :---: | :--- | :--- |
| **Pin 3 (TX)** | ───► | **GPIO 13 (RX)** | **Pass through 5V to 3.3V Voltage Divider** (see below) |
| **Pin 2 (RX)** | ◄─── | **GPIO 12 (TX)** | Direct connection (3.3V HIGH is safely detected by Uno) |
| **GND** | ─── | **GND** | **MANDATORY**: Common Ground connection |
| **Pin 0 (RX)** | ─── | *Unconnected* | Reserved for Arduino USB Debugging on PC |
| **Pin 1 (TX)** | ─── | *Unconnected* | Reserved for Arduino USB Debugging on PC |

---

### ⚡ 5V to 3.3V Voltage Divider Diagram

To safely connect **Arduino Pin 3 (TX)** to **ESP32-CAM GPIO 13 (RX)**:

```text
Arduino Uno Pin 3 (5V TX)
       │
      [1kΩ Resistor]
       │
       ├────────────────► To ESP32-CAM GPIO 13 (Receives ~3.33V)
       │
      [2kΩ Resistor]
       │
Arduino / ESP32 Common GND
```

**Formula**:
$$\text{V}_{\text{out}} = \text{V}_{\text{in}} \times \frac{R_2}{R_1 + R_2} = 5.0\text{V} \times \frac{2000}{1000 + 2000} \approx 3.33\text{V}$$

---

## 📦 Required Arduino Libraries

Before uploading the sketches, open the **Arduino IDE** and install the following libraries via **Tools > Manage Libraries...**:

1. **DHT sensor library** (by Adafruit)
2. **Adafruit Unified Sensor** (by Adafruit)

*(The `SoftwareSerial` library is already built into the Arduino AVR core).*

---

## 🚀 How to Flash & Run

### Step 1: Upload to Arduino Uno
1. Open [`arduino.ino`](file:///c:/Users/acer/Desktop/haridemoone/arduino.ino) in Arduino IDE.
2. Under **Tools > Board**, select **Arduino Uno**.
3. Under **Tools > Port**, select your Arduino Uno COM port.
4. Click **Upload**.
5. Open the Serial Monitor at **9600 baud** to see real-time sensor readings and outgoing transmission logs.

### Step 2: Upload to ESP32-CAM
1. Connect your FTDI programmer to the ESP32-CAM:
   - FTDI **5V** ──► ESP32-CAM **5V**
   - FTDI **GND** ──► ESP32-CAM **GND**
   - FTDI **TX** ──► ESP32-CAM **U0R (GPIO 3)**
   - FTDI **RX** ──► ESP32-CAM **U0T (GPIO 1)**
   - **ESP32-CAM GPIO 0 connected to GND** (enters flashing mode).
2. Open [`esp32cam.ino`](file:///c:/Users/acer/Desktop/haridemoone/esp32cam.ino) in Arduino IDE.
3. Under **Tools > Board**, select **AI Thinker ESP32-CAM**.
4. Select the corresponding FTDI COM port.
5. Press the **RST** button on ESP32-CAM and click **Upload**.
6. Once uploaded:
   - **Disconnect GPIO 0 from GND**.
   - Press the **RST** button to start standard execution.
   - Open Serial Monitor at **115200 baud**.

---

## 📡 Serial Data Protocol

The Arduino Uno compiles all 4 sensor readings into a structured message:

```text
$DATA,<packet_id>,<temp1>,<hum1>,<temp2>,<hum2>,<temp3>,<hum3>,<temp4>,<hum4>#
```

### Example Packet:
```text
$DATA,12,25.4,58.2,26.1,60.0,24.8,59.5,25.0,61.2#
```

- **`$`**: Start-of-packet delimiter.
- **`DATA`**: Packet identifier header.
- **`<packet_id>`**: Incremental counter tracking packet delivery.
- **`<tempX>,<humX>`**: Floating point values for each sensor. If a sensor fails to respond, it sends `-999.0`.
- **`#`**: End-of-packet delimiter.

---

## 🖥️ Expected Serial Monitor Output

### Arduino Uno (9600 baud):
```text
==================================================
 Arduino Uno: 4x DHT22 Multi-Sensor Station
 SoftwareSerial TX on Pin 3, RX on Pin 2
 Hardware Pins 0 & 1 reserved for USB Debugging
==================================================
[PKT #1] Transmitting sensor data to ESP32-CAM...
  Sensor 1: Temp = 24.5 °C | Humidity = 55.2 %
  Sensor 2: Temp = 25.1 °C | Humidity = 52.3 %
  Sensor 3: Temp = 24.8 °C | Humidity = 54.0 %
  Sensor 4: Temp = 25.0 °C | Humidity = 53.8 %
Raw Sent -> $DATA,1,24.5,55.2,25.1,52.3,24.8,54.0,25.0,53.8#
--------------------------------------------------
```

### ESP32-CAM (115200 baud):
```text
==================================================
       ESP32-CAM Multi-Sensor Receiver Node       
==================================================
Listening for Arduino Uno on RX (GPIO 13), TX (GPIO 12) @ 9600 baud
PC Debug Serial Monitor active @ 115200 baud
Waiting for sensor packets from Arduino Uno...
==================================================

┌────────────────────────────────────────────────────────┐
│  ESP32-CAM INCOMING SENSOR REPORT | Packet #1          │
├──────────┬─────────────────┬─────────────────┬─────────┤
│  Sensor  │ Temperature (°C)│  Humidity (%)   │ Status  │
├──────────┼─────────────────┼─────────────────┼─────────┤
│  DHT22 #1│       24.5 °C   │       55.2 %   │   OK    │
│  DHT22 #2│       25.1 °C   │       52.3 %   │   OK    │
│  DHT22 #3│       24.8 °C   │       54.0 %   │   OK    │
│  DHT22 #4│       25.0 °C   │       53.8 %   │   OK    │
└──────────┴─────────────────┴─────────────────┴─────────┘
```

---

## 🛠️ Troubleshooting & Tips

1. **Common Ground Missing**:
   - If ESP32-CAM receives nothing or corrupted characters, make sure the **GND of Arduino Uno is connected to the GND of ESP32-CAM**. Without a shared reference ground, serial communication cannot function.

2. **Sensor reports "FAILED TO READ" (`-999.0` / `NaN`)**:
   - Verify power connections (VCC and GND).
   - Verify that bare DHT22 sensors have a **10kΩ pull-up resistor** between Pin 1 (VCC) and Pin 2 (DATA).
   - Ensure you are not polling faster than every 2 seconds (the code is set to poll every 3 seconds).

3. **ESP32-CAM Constant Resets / Brownout**:
   - The ESP32-CAM requires up to 500mA peak current when initializing its radio/camera. Ensure the 5V power supply provides at least **1.5A to 2A**.
   - The sketch includes `WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);` to disable the brownout detector during voltage sags.

4. **Why not use GPIO 16 on ESP32-CAM?**:
   - On the AI-Thinker ESP32-CAM, **GPIO 16 is internally connected to the PSRAM chip select (CS)**. Using GPIO 16 causes memory corruption and crashes if PSRAM is enabled. We use **GPIO 13** and **GPIO 12** instead.


#include <Wire.h>
#include <Servo.h>
#include <DHT.h>
#include <Adafruit_BMP280.h>
#include <MPU6050.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

#define DHTPIN 2
#define DHTTYPE DHT22
#define TRIG 4
#define ECHO 5
#define FLAME 3

Servo radarServo;
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP280 bmp;
MPU6050 mpu;

SoftwareSerial gpsSerial(8,9);
TinyGPSPlus gps;
SoftwareSerial espSerial(10,11); // RX, TX for ESP32

long duration;
int distance;
int angle=0;
unsigned long lastPrintTime = 0;
const unsigned long printInterval = 10000; // 10 seconds

void setup()
{
Serial.begin(115200);

  dht.begin();
  Wire.begin();
  
  Serial.println("[BMP280] Initialization...");
  if (!bmp.begin(0x76)) {
    if (!bmp.begin(0x77)) {
      Serial.println("[-] Could not find a valid BMP280 sensor at 0x76 or 0x77, check wiring!");
    } else {
      Serial.println("[+] BMP280 initialized at 0x77.");
    }
  } else {
    Serial.println("[+] BMP280 initialized at 0x76.");
  }
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("[-] MPU6050 connection failed");
  } else {
    Serial.println("[+] MPU6050 connection successful");
  }

pinMode(FLAME,INPUT);

  gpsSerial.begin(9600);
  espSerial.begin(9600);
  
  gpsSerial.listen(); // Start by listening to GPS
  
  radarServo.attach(6);
}

void loop()
{
  
  // Pass-through: If ESP32 sends something (like its IP), print it to Serial Monitor
  espSerial.listen();
  while (espSerial.available()) {
    String fromESP = espSerial.readStringUntil('\n');
    fromESP.trim();
    if (fromESP.length() > 0) {
      Serial.println("[ESP32-MSG] " + fromESP);
    }
  }

// RADAR
radarServo.write(angle);

digitalWrite(TRIG,LOW);
delayMicroseconds(2);

digitalWrite(TRIG,HIGH);
delayMicroseconds(10);
digitalWrite(TRIG,LOW);

duration=pulseIn(ECHO,HIGH);
distance=duration*0.034/2;

angle+=10;
if(angle>180) angle=0;


// DHT
float temp=dht.readTemperature();
float hum=dht.readHumidity();

// BMP280
float pressure=bmp.readPressure()/100;
float altitude=bmp.readAltitude(1013.25);

// MPU6050
int16_t ax,ay,az,gx,gy,gz;
mpu.getMotion6(&ax,&ay,&az,&gx,&gy,&gz);

// GPS Reading Window
gpsSerial.listen();
unsigned long startGPS = millis();
while (millis() - startGPS < 100) { // Give GPS 100ms to send data each loop
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
}

double lat = gps.location.isValid() ? gps.location.lat() : 0.000000;
double lng = gps.location.isValid() ? gps.location.lng() : 0.000000;
bool gpsFix = gps.location.isValid();
int satellites = gps.satellites.value();

// FLAME
int flame=digitalRead(FLAME);

  // Send Data to ESP32 every 1 minute
  if (millis() - lastPrintTime >= printInterval) {
    // Handle NaN for all floats
    float sTemp = isnan(temp) ? 0.0 : temp;
    float sHum = isnan(hum) ? 0.0 : hum;
    float sPres = isnan(pressure) ? 0.0 : pressure;
    float sAlt = isnan(altitude) ? 0.0 : altitude;
    
    // Build packet string for atomic transmission
    String packet = String(sTemp) + "," + String(sHum) + "," + String(distance) + "," + 
                    String(angle) + "," + String(sPres) + "," + String(sAlt) + "," + 
                    String(ax) + "," + String(ay) + "," + String(az) + "," + 
                    String(lat, 6) + "," + String(lng, 6) + "," + String(flame);
    
    // Send to ESP32
    espSerial.listen();
    espSerial.println(packet);
    
    // Print all sensor data to Serial Monitor for local debugging
    Serial.println("==========================================");
    Serial.println("[LOCAL] Data sent to ESP32:");
    Serial.print("  Temp: "); Serial.print(sTemp); Serial.println(" C");
    Serial.print("  Hum:  "); Serial.print(sHum); Serial.println(" %");
    Serial.print("  Dist: "); Serial.print(distance); Serial.print(" cm @ "); Serial.print(angle); Serial.println(" deg");
    Serial.print("  Pres: "); Serial.print(pressure); Serial.print(" hPa | Alt: "); Serial.print(altitude); Serial.println(" m");
    Serial.print("  Accel: X:"); Serial.print(ax); Serial.print(" Y:"); Serial.print(ay); Serial.print(" Z:"); Serial.println(az);
    Serial.print("  GPS:  Lat:"); Serial.print(lat, 6); Serial.print(" Lng:"); Serial.print(lng, 6); 
    Serial.print(" Sats: "); Serial.print(satellites);
    if (!gpsFix) Serial.println(" (NO FIX)"); else Serial.println(" (FIXED)");
    Serial.print("  Flame: "); Serial.println(flame == 0 ? "!!! FIRE !!!" : "SAFE");
    Serial.println("==========================================");

    lastPrintTime = millis();
  }

  delay(10); // Small stability delay
}
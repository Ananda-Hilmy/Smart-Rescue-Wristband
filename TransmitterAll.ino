//====================================================
// LIBRARY
//====================================================

#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>

#include "MAX30105.h"
#include "heartRate.h"

//====================================================
// MAX30102
//====================================================

MAX30105 particleSensor;

//====================================================
// GPS
//====================================================

TinyGPSPlus gps;
HardwareSerial GPS(1);

//====================================================
// PIN I2C MAX30102
//====================================================

#define SDA_PIN 21
#define SCL_PIN 22

//====================================================
// PIN GPS Neo-6M
//====================================================

#define GPS_RX 13
#define GPS_TX 15

//====================================================
// PIN LORA TTGO LoRa32
//====================================================

#define LORA_SCK   5
#define LORA_MISO 19
#define LORA_MOSI 27

#define LORA_SS   18
#define LORA_RST  23
#define LORA_DIO0 26

#define LORA_BAND 915E6

//====================================================
// HEART RATE VARIABLE
//====================================================

const byte RATE_SIZE = 4;

byte rates[RATE_SIZE];
byte rateSpot = 0;

long lastBeat = 0;

float beatsPerMinute = 0;
int beatAvg = 0;

long irValue = 0;
long redValue = 0;

int spo2 = 0;

//====================================================
// GPS VARIABLE
//====================================================

double latitude = 0.0;
double longitude = 0.0;
double altitude = 0.0;
double speedGPS = 0.0;

int satellites = 0;

bool gpsFix = false;

//====================================================
// TIMER
//====================================================

unsigned long previousMillis = 0;

const unsigned long interval = 2000;

//====================================================
// SETUP
//====================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("=====================================");
    Serial.println(" SMART LIFE JACKET");
    Serial.println(" TRANSMITTER");
    Serial.println("=====================================");

    //------------------------------------------------
    // I2C
    //------------------------------------------------

    Wire.begin(SDA_PIN, SCL_PIN);

    //------------------------------------------------
    // MAX30102
    //------------------------------------------------

    if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD))
    {
        Serial.println("ERROR : MAX30102 NOT FOUND");

        while (1);
    }

    Serial.println("MAX30102 OK");

    particleSensor.setup(
        100,      // LED Brightness
        8,        // Sample Average
        2,        // LED Mode (Red + IR)
        400,      // Sample Rate
        411,      // Pulse Width
        4096      // ADC Range
    );

    particleSensor.setPulseAmplitudeRed(0x32);
    particleSensor.setPulseAmplitudeIR(0x32);

    //------------------------------------------------
    // GPS
    //------------------------------------------------

    GPS.begin(
        9600,
        SERIAL_8N1,
        GPS_RX,
        GPS_TX
    );

    Serial.println("GPS OK");

    //------------------------------------------------
    // LORA
    //------------------------------------------------

    SPI.begin(
        LORA_SCK,
        LORA_MISO,
        LORA_MOSI,
        LORA_SS
    );

    LoRa.setPins(
        LORA_SS,
        LORA_RST,
        LORA_DIO0
    );

    if (!LoRa.begin(LORA_BAND))
    {
        Serial.println("ERROR : LoRa Failed");

        while (1);
    }

    Serial.println("LoRa OK");

    Serial.println();
    Serial.println("=====================================");
    Serial.println(" SYSTEM READY ");
    Serial.println("=====================================");
}

//====================================================
// MEMBACA GPS
//====================================================

void readGPS()
{
    while (GPS.available())
    {
        gps.encode(GPS.read());
    }

    if (gps.location.isValid())
    {
        gpsFix = true;

        latitude = gps.location.lat();
        longitude = gps.location.lng();

        altitude = gps.altitude.meters();

        speedGPS = gps.speed.kmph();

        satellites = gps.satellites.value();
    }
    else
    {
        gpsFix = false;
    }
}

//====================================================
// MEMBACA HEART RATE
//====================================================

void readHeartRate()
{
    irValue = particleSensor.getIR();
    redValue = particleSensor.getRed();

    //------------------------------------------
    // Tidak ada jari
    //------------------------------------------

    if (irValue < 50000)
    {
        beatAvg = 0;
        spo2 = 0;
        return;
    }

    //------------------------------------------
    // BPM
    //------------------------------------------

    if (checkForBeat(irValue))
    {
        long delta = millis() - lastBeat;

        lastBeat = millis();

        beatsPerMinute = 60.0 / (delta / 1000.0);

        if (beatsPerMinute > 40 && beatsPerMinute < 180)
        {
            rates[rateSpot++] = (byte)beatsPerMinute;

            rateSpot %= RATE_SIZE;

            beatAvg = 0;

            for (byte i = 0; i < RATE_SIZE; i++)
            {
                beatAvg += rates[i];
            }

            beatAvg /= RATE_SIZE;
        }
    }

    //------------------------------------------
    // Estimasi SpO2
    //------------------------------------------

    if (irValue > 105000)
        spo2 = 99;

    else if (irValue > 95000)
        spo2 = 98;

    else if (irValue > 85000)
        spo2 = 97;

    else if (irValue > 75000)
        spo2 = 96;

    else if (irValue > 65000)
        spo2 = 95;

    else
        spo2 = 94;
}

//====================================================
// SERIAL MONITOR
//====================================================

void printSerial()
{
    Serial.println();
    Serial.println("==========================================");

    if (gpsFix)
    {
        Serial.println("GPS STATUS : FIX");

        Serial.print("Latitude   : ");
        Serial.println(latitude, 6);

        Serial.print("Longitude  : ");
        Serial.println(longitude, 6);

        Serial.print("Altitude   : ");
        Serial.print(altitude);
        Serial.println(" m");

        Serial.print("Satellite  : ");
        Serial.println(satellites);

        Serial.print("Speed      : ");
        Serial.print(speedGPS, 2);
        Serial.println(" km/h");
    }
    else
    {
        Serial.println("GPS STATUS : WAITING FIX...");
    }

    Serial.println("------------------------------------------");

    Serial.print("IR Value   : ");
    Serial.println(irValue);

    Serial.print("Red Value  : ");
    Serial.println(redValue);

    Serial.print("Heart Rate : ");
    Serial.print(beatAvg);
    Serial.println(" BPM");

    Serial.print("SpO2       : ");
    Serial.print(spo2);
    Serial.println(" %");

    Serial.println("==========================================");
}

//====================================================
// KIRIM DATA LORA
//====================================================

void sendLoRa()
{
    //------------------------------------------------
    // Format Data
    //
    // Latitude,
    // Longitude,
    // BPM,
    // SpO2,
    // Satellite,
    // Speed
    //------------------------------------------------

    String packet = "";

    packet += String(latitude,6);
    packet += ",";

    packet += String(longitude,6);
    packet += ",";

    packet += String(beatAvg);
    packet += ",";

    packet += String(spo2);
    packet += ",";

    packet += String(satellites);
    packet += ",";

    packet += String(speedGPS,2);

    //------------------------------------------------
    // Kirim LoRa
    //------------------------------------------------

    LoRa.beginPacket();

    LoRa.print(packet);

    LoRa.endPacket();

    //------------------------------------------------
    // Serial Monitor
    //------------------------------------------------

    Serial.println();

    Serial.println("=========== LoRa SEND ===========");

    Serial.println(packet);

    Serial.println("===============================");
}

//====================================================
// LOOP
//====================================================

void loop()
{
    //----------------------------------------------
    // Update GPS
    //----------------------------------------------

    readGPS();

    //----------------------------------------------
    // Update Heart Rate
    //----------------------------------------------

    readHeartRate();

    //----------------------------------------------
    // Kirim setiap 2 detik
    //----------------------------------------------

    if (millis() - previousMillis >= interval)
    {
        previousMillis = millis();

        printSerial();

        sendLoRa();
    }

    delay(10);
}
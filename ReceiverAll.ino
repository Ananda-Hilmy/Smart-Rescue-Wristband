/******************************************************
 * PROJECT : Smart Life Jacket Monitoring
 * DEVICE  : TTGO LoRa32 V1.6.1 (RECEIVER)
 * AUTHOR  : Ananda Hilmy
 ******************************************************/

//====================================================
// LIBRARY
//====================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <SPI.h>
#include <LoRa.h>

//====================================================
// WIFI
//====================================================

const char* ssid     = "---";
const char* password = "---";

//====================================================
// GOOGLE APPS SCRIPT
//====================================================

const char* GAS_URL =
"https://script.google.com/macros/s/---";

//====================================================
// LORA PIN TTGO LoRa32
//====================================================

#define LORA_SCK   5
#define LORA_MISO 19
#define LORA_MOSI 27

#define LORA_SS   18
#define LORA_RST  23
#define LORA_DIO0 26

#define LORA_BAND 915E6

//====================================================
// DATA DARI TRANSMITTER
//====================================================

String latitude  = "";
String longitude = "";

String bpm       = "";
String spo2      = "";

String satellite = "";
String speed     = "";

//====================================================
// DATA LORA
//====================================================

int rssi = 0;
float snr = 0;

//====================================================
// SETUP
//====================================================

void setup()
{
    Serial.begin(115200);

    Serial.println();
    Serial.println("====================================");
    Serial.println(" SMART LIFE JACKET");
    Serial.println(" RECEIVER");
    Serial.println("====================================");

    //------------------------------------------------
    // WIFI
    //------------------------------------------------

    Serial.print("Menghubungkan WiFi");

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    Serial.println("WiFi Connected");

    Serial.print("IP Address : ");
    Serial.println(WiFi.localIP());

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

    Serial.println("LoRa Ready");

    Serial.println();
    Serial.println("====================================");
    Serial.println(" RECEIVER READY ");
    Serial.println("====================================");
}

//====================================================
// MENERIMA DATA LORA
//====================================================

void receiveLoRa()
{
    //------------------------------------------------
    // Cek apakah ada paket masuk
    //------------------------------------------------

    int packetSize = LoRa.parsePacket();

    if (!packetSize)
    {
        return;
    }

    //------------------------------------------------
    // Membaca paket
    //------------------------------------------------

    String packet = "";

    while (LoRa.available())
    {
        packet += (char)LoRa.read();
    }

    //------------------------------------------------
    // RSSI & SNR
    //------------------------------------------------

    rssi = LoRa.packetRssi();

    snr = LoRa.packetSnr();

    //------------------------------------------------
    // Serial Raw Packet
    //------------------------------------------------

    Serial.println();
    Serial.println("=========== DATA MASUK ===========");

    Serial.println(packet);

    //------------------------------------------------
    // Parsing Data
    //
    // lat,lng,bpm,spo2,satellite,speed
    //------------------------------------------------

    int p1 = packet.indexOf(',');

    int p2 = packet.indexOf(',', p1 + 1);

    int p3 = packet.indexOf(',', p2 + 1);

    int p4 = packet.indexOf(',', p3 + 1);

    int p5 = packet.indexOf(',', p4 + 1);

    //------------------------------------------------
    // Validasi Format
    //------------------------------------------------

    if (p1 > 0 &&
        p2 > 0 &&
        p3 > 0 &&
        p4 > 0 &&
        p5 > 0)
    {
        latitude = packet.substring(0, p1);

        longitude = packet.substring(p1 + 1, p2);

        bpm = packet.substring(p2 + 1, p3);

        spo2 = packet.substring(p3 + 1, p4);

        satellite = packet.substring(p4 + 1, p5);

        speed = packet.substring(p5 + 1);

        //------------------------------------------------
        // Serial Monitor
        //------------------------------------------------

        Serial.println();

        Serial.println("========= HASIL PARSING =========");

        Serial.print("Latitude   : ");
        Serial.println(latitude);

        Serial.print("Longitude  : ");
        Serial.println(longitude);

        Serial.print("BPM        : ");
        Serial.println(bpm);

        Serial.print("SpO2       : ");
        Serial.println(spo2);

        Serial.print("Satellite  : ");
        Serial.println(satellite);

        Serial.print("Speed      : ");
        Serial.print(speed);
        Serial.println(" km/h");

        Serial.println("--------------------------------");

        Serial.print("RSSI       : ");
        Serial.print(rssi);
        Serial.println(" dBm");

        Serial.print("SNR        : ");
        Serial.print(snr);
        Serial.println(" dB");

        Serial.println("================================");

        //------------------------------------------------
        // Kirim ke Google Spreadsheet
        //------------------------------------------------

        sendToGoogleSheet();
    }
    else
    {
        Serial.println("ERROR : Format Data Salah");
    }
}

//====================================================
// KIRIM DATA KE GOOGLE APPS SCRIPT
//====================================================

void sendToGoogleSheet()
{
    //------------------------------------------------
    // Cek WiFi
    //------------------------------------------------

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi Terputus...");
        Serial.println("Reconnect WiFi...");

        WiFi.disconnect();
        WiFi.begin(ssid, password);

        unsigned long timeout = millis();

        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");

            if (millis() - timeout > 10000)
            {
                Serial.println();
                Serial.println("Reconnect Gagal");
                return;
            }
        }

        Serial.println();
        Serial.println("WiFi Connected");
    }

    //------------------------------------------------
    // HTTPS Client
    //------------------------------------------------

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    //------------------------------------------------
    // URL
    //------------------------------------------------

    String url = String(GAS_URL);

    url += "?action=save";

    url += "&bpm=" + bpm;
    url += "&spo2=" + spo2;

    url += "&lat=" + latitude;
    url += "&lng=" + longitude;

    url += "&sat=" + satellite;
    url += "&speed=" + speed;

    url += "&rssi=" + String(rssi);
    url += "&snr=" + String(snr,2);

    //------------------------------------------------
    // Debug
    //------------------------------------------------

    Serial.println();
    Serial.println("======= SEND TO GOOGLE =======");

    Serial.println(url);

    //------------------------------------------------
    // Kirim
    //------------------------------------------------

    http.begin(client, url);

    http.setFollowRedirects(
        HTTPC_STRICT_FOLLOW_REDIRECTS
    );

    int httpCode = http.GET();

    if (httpCode > 0)
    {
        Serial.print("HTTP Code : ");
        Serial.println(httpCode);

        Serial.print("Response  : ");
        Serial.println(http.getString());
    }
    else
    {
        Serial.print("HTTP Error : ");
        Serial.println(
            http.errorToString(httpCode)
        );
    }

    http.end();

    Serial.println("==============================");
}

//====================================================
// LOOP
//====================================================

void loop()
{
    receiveLoRa();
}
#include <BLEDevice.h>
#include <BLEServer.h>
#include <Arduino_JSON.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

String ssid_name;
String ssid_pass;

WebServer server(80);

class MyCallbacks: public BLECharacteristicCallbacks {
public:
    bool initWiFi(const char* ssid, const char* pass) {
        WiFi.mode(WIFI_STA);

        // --- Static IP - because PWAs can't do anything smart with UDP or Hostnames
        // Set your Static IP address
        IPAddress local_IP(192, 168, 1, 253);
        // Set your Gateway IP address
        IPAddress gateway(192, 168, 1, 1);

        IPAddress subnet(255, 255, 0, 0);
        WiFi.config(local_IP, gateway, subnet);

        WiFi.begin(ssid, pass);
        Serial.print("Connecting to WiFi ..");
        int count = 0;
        while (WiFi.status() != WL_CONNECTED && count < 10) {
            Serial.print('.');
            delay(1000);
            count++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(WiFi.localIP());
            return true;
        }
        return false;
    }

    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        Serial.print(value.c_str());
        JSONVar message = JSON.parse(value.c_str());

        // if message is object with "ssid" and "pass", connect to wifi with these
        if ((const char*) message["ssid"] != null && (const char*) message["pass"] != null) {
            Serial.println("Received Wifi credentials");

            const bool wifi_connected = initWiFi((const char*) message["ssid"], (const char*) message["pass"]);

            if (wifi_connected) {
                writeStringToEEPROM(0, String((const char*) message["ssid"]));
                writeStringToEEPROM(strlen((const char*) message["ssid"]), String((const char*) message["pass"]));
            } else {
                Serial.println("Not connected");
            }
        }
    }

    void writeStringToEEPROM(int addrOffset, const String &strToWrite)
    {
        byte len = strToWrite.length();
        EEPROM.write(addrOffset, len);
        for (int i = 0; i < len; i++)
        {
            EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
        }
        EEPROM.commit();
    }
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        Serial.println("CONNECTED...");
    }

    void onDisconnect(BLEServer* pServer) {
        Serial.println("DISCONNECTED...");
        pServer->startAdvertising();
    }
};

String readStringFromEEPROM(int addrOffset)
{
    int newStrLen = EEPROM.read(addrOffset);
    char data[newStrLen + 1];
    for (int i = 0; i < newStrLen; i++)
    {
        data[i] = EEPROM.read(addrOffset + 1 + i);
    }
    data[newStrLen] = '\0';
    return String(data);
}

void setup() {
    Serial.begin(115200);

    EEPROM.begin(512);

    ssid_name = readStringFromEEPROM(0);
    ssid_pass = readStringFromEEPROM(ssid_name.length()+1);

    if (ssid_name != null && ssid_pass != null) {
        MyCallbacks wifi_handler; // instantiate this Class just to use the initWifi method
        bool wifi_con = wifi_handler.initWiFi(ssid_name.c_str(), ssid_pass.c_str());
        Serial.print("Connected to Wifi: " + wifi_con);
        server.begin();
    }

    BLEDevice::init("cdawgs_dongle");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    BLECharacteristic *pCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE
    );
    pCharacteristic->setCallbacks(new MyCallbacks());

    pCharacteristic->setValue("Hello World says Cdawg");
    pService->start();

    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);

//  pAdvertising->setScanResponse(true);
//  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
//  pAdvertising->setMinPreferred(0x12);
//  BLEDevice::startAdvertising();
    pAdvertising->start();
    Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
    byte len = strToWrite.length();
    EEPROM.write(addrOffset, len);
    for (int i = 0; i < len; i++)
    {
        EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
    }
}

void loop() {
    server.handleClient();
}
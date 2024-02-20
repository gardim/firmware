#include <BLEDevice.h>
#include <BLEServer.h>
#include <Arduino_JSON.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>

#define SID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

String sn;
String sp;
WebServer srv(80);

class cb: public BLECharacteristicCallbacks {
public:
    static bool iWF(const char* ssid, const char* p) {
        WiFiClass::mode(WIFI_STA);
        IPAddress lip(192, 168, 1, 253);
        IPAddress gw(192, 168, 1, 1);
        IPAddress sbn(255, 255, 0, 0);
        WiFi.config(lip, gw, sbn);

        WiFi.begin(ssid, p);
        Serial.print("C 2 wifi");
        int c = 0;
        while (WiFiClass::status() != WL_CONNECTED && c < 10) {
            delay(1000);
            c++;
        }

        if (WiFiClass::status() == WL_CONNECTED) {
            Serial.println(WiFi.localIP());
            return true;
        }
        return false;
    }

    void onWrite(BLECharacteristic *pCh) override {
        std::string value = pCh->getValue();
        Serial.print(value.c_str());
        JSONVar msg = JSON.parse(value.c_str());
        if ((const char*) msg["ssid"] != null && (const char*) msg["pass"] != null) {
            const bool is_c = iWF((const char*) msg["ssid"], (const char*) msg["pass"]);
            if (is_c) {
                w2mem(0, String((const char*) msg["ssid"]));
                w2mem(strlen((const char*) msg["ssid"]), String((const char*) msg["pass"]));
            }
        }
    }

    static void w2mem(int adoffs, const String &strToWrite)
    {
        byte len = strToWrite.length();
        EEPROM.write(adoffs, len);
        for (int i = 0; i < len; i++)
        {
            EEPROM.write(adoffs + 1 + i, strToWrite[i]);
        }
        EEPROM.commit();
    }
};

class srvCb: public BLEServerCallbacks {
};

String rFMem(int adoffs)
{
    int newStrLen = EEPROM.read(adoffs);
    char data[newStrLen + 1];
    for (int i = 0; i < newStrLen; i++)
    {
        data[i] = EEPROM.read(adoffs + 1 + i);
    }
    data[newStrLen] = '\0';
    return {data};
}

void setup() {
    Serial.begin(115200);
    EEPROM.begin(512);
    sn = rFMem(0);
    sp = rFMem(sn.length()+1);
    if (sn != null && sp != null) {
        cb _;
        bool con = cb::iWF(sn.c_str(), sp.c_str());
        Serial.print(&"Connected to Wifi: " [ con]);
        srv.begin();
    }
    BLEDevice::init("cdawgs_dongle");
    BLEServer *psrv = BLEDevice::createServer();
    psrv->setCallbacks(new srvCb());
    BLEService *pService = psrv->createService(SID);
    BLECharacteristic *pCh = pService->createCharacteristic(
            CID,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE
    );
    pCh->setCallbacks(new cb());
    pCh->setValue("Hello World says Cdawg");
    pService->start();
    BLEAdvertising *pAd = psrv->getAdvertising();
    pAd->addServiceUUID(SID);
    pAd->start();
}

void loop() {
    srv.handleClient();
}
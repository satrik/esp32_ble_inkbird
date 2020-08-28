#include <WiFiMulti.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

const char* mdns     = "inkbird";

WiFiMulti wifiMulti;
AsyncWebServer server(80);

static uint8_t enableAccess[] = {0x21, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0xb8, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t enableUnitCelsius[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t getRealTimeData[] = {0x0B, 0x01, 0x00, 0x00, 0x00, 0x00};

static BLEUUID serviceUUID("0000fff0-0000-1000-8000-00805f9b34fb");
static BLEUUID characteristicSettingsResultsUUID("0000fff1-0000-1000-8000-00805f9b34fb");
static BLEUUID characteristicAuthUUID("0000fff2-0000-1000-8000-00805f9b34fb");
static BLEUUID characteristicRealTimeDataUUID("0000fff4-0000-1000-8000-00805f9b34fb");
static BLEUUID characteristicSettingsUUID("0000fff5-0000-1000-8000-00805f9b34fb");

uint16_t littleEndianInt(uint8_t *pData);

static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;

String html = "<html> <head> <title>ESP32 - Inkbird</title> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.7.0/css/font-awesome.min.css\"> <style> body { background: #000; color:#fff; font-size: 28px; font-family: Helvetica Neue,Helvetica,Arial,sans-serif; } .boxes { background: #404040; margin:5px; height:100px; padding-top: 28px; } </style> <script type=\"text/javascript\"> setInterval(function() { getP1(); getP2(); changeColor(); }, 3000); function changeColor() { var color = \"\"; var temp1 = parseFloat(document.getElementById(\"probe1\").innerHTML); var temp2 = parseFloat(document.getElementById(\"probe2\").innerHTML); if(temp1 < 80.0){ color = \"blue\"; } else if (temp1 < 100.0) { color = \"orange\"; } else if (temp1 > 135.0) { color = \"red\"; } else { color = \"\"; } document.getElementById(\"box1\").style.backgroundColor = color; if(temp2 < 80.0){ color = \"blue\"; } else if (temp2 < 100.0) { color = \"orange\"; } else if (temp2 > 135.0) { color = \"red\"; } else { color = \"\"; } document.getElementById(\"box2\").style.backgroundColor = color; } function getP1() { var xhttp = new XMLHttpRequest(); xhttp.onreadystatechange = function() { if (this.readyState == 4 && this.status == 200) { document.getElementById(\"probe1\").innerHTML = this.responseText; } }; xhttp.open(\"GET\", \"p1\", true); xhttp.send(); } function getP2() { var xhttp = new XMLHttpRequest(); xhttp.onreadystatechange = function() { if (this.readyState == 4 && this.status == 200) { document.getElementById(\"probe2\").innerHTML = this.responseText; } }; xhttp.open(\"GET\", \"p2\", true); xhttp.send(); } </script> </head> <body> <center> <br> <i class=\"fa fa-thermometer-half\"></i> Inkbird <br><br> <div class=\"boxes\" id=\"box1\"> Probe 1<br> <span id=\"probe1\">0</span>&#176;<br> </div> <div class=\"boxes\" id=\"box2\"> Probe 2<br> <span id=\"probe2\">0</span>&#176;<br> </div> </center> </body> </html>";

static void notifyCallback(
  BLERemoteCharacteristic *pBLERemoteCharacteristic,
  uint8_t *pData,
  size_t length,
  bool isNotify)
{
  uint16_t val1 = littleEndianInt(&pData[0]);
  uint16_t val2 = littleEndianInt(&pData[2]);
  float temp1 = val1 / 10;
  float temp2 = val2 / 10;
  server.on("/p1", HTTP_GET, [&temp1](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", String(temp1));
  });
  server.on("/p2", HTTP_GET, [&temp2](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", String(temp2));
  });
}

bool connectToBLEServer(BLEAddress pAddress)
{
  BLEClient *pClient = BLEDevice::createClient();
  pClient->connect(pAddress);
  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr)
  {
    return false;
  }
  BLERemoteCharacteristic *pAuthCharacteristic = pRemoteService->getCharacteristic(characteristicAuthUUID);
  if (pAuthCharacteristic == nullptr)
  {
    return false;
  }
  pAuthCharacteristic->writeValue((uint8_t *)enableAccess, sizeof(enableAccess), true);
  BLERemoteCharacteristic *pRemoteCharacteristic = pRemoteService->getCharacteristic(characteristicRealTimeDataUUID);
  if (pRemoteCharacteristic == nullptr)
  {
    return false;
  }
  BLERemoteCharacteristic *pSettingsCharacteristic = pRemoteService->getCharacteristic(characteristicSettingsUUID);
  if (pSettingsCharacteristic == nullptr)
  {
    return false;
  }
  pSettingsCharacteristic->writeValue((uint8_t *)getRealTimeData, sizeof(getRealTimeData), true);
  pSettingsCharacteristic->writeValue((uint8_t *)enableUnitCelsius, sizeof(enableUnitCelsius), true);
  pRemoteCharacteristic->registerForNotify(notifyCallback);
  return true;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
      if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID))
      {
        advertisedDevice.getScan()->stop();
        pServerAddress = new BLEAddress(advertisedDevice.getAddress());
        doConnect = true;
      }
    }
};

uint16_t littleEndianInt(uint8_t *pData)
{
  uint16_t val = pData[1] << 8;
  val = val | pData[0];
  return val;
}

void bleInit() {

  BLEDevice::init("");
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);

}

void setup()
{
  Serial.begin(115200);
  delay(10);
  wifiMulti.addAP("", "");
  wifiMulti.addAP("", "");
  wifiMulti.addAP("", "");

  if (wifiMulti.run() == WL_CONNECTED) {
  }
  if (mdns != "" && MDNS.begin(mdns)) {
    MDNS.addService("http", "tcp", 80);
  } else {
  }
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/html", html);
  });
  server.begin();
  bleInit();
}

void loop()
{
  if (wifiMulti.run() != WL_CONNECTED)
  {
    delay(1000);
  }

  if (doConnect == true)
  {
    if (connectToBLEServer(*pServerAddress))
    {
      connected = true;
    }
    else
    {
    }
    doConnect = false;
  }
  delay(1000);
}

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

const char* ssid = "";
const char* password = "";
const char* mdns = "inkbird";

String globalTemp;
 
WebServer server(80);

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

const char* htmlIndex = R"=====(
<!DOCTYPE html>
<html>
  <head>
    <title>ESP32 - Inkbird</title>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css' integrity='sha512-iBBXm8fW90+nuLcSKlbmrPcLa0OT92xO1BIsZ+ywDWZCvqsWgccV3gFoRBv0z+8dLJgyAHIhR35VZc2oM/gI1w==' crossorigin='anonymous'
      referrerpolicy='no-referrer' />
    <style>
      body {
        background: #1a1a1a;
        color: #cccccc;
        font-size: 3vh;
        font-family: Helvetica Neue, Helvetica, Arial, sans-serif;
      }
      table {
        border: 2px solid #404040;
        border-radius: 0.5vh;
        width: 90%;
        margin-bottom: 1vh;
      }
      table td {
        padding: 0.5vh;
      }
      table tr {
        height: 5vh;
      }
      input[type=text] {
        width: auto;
        margin: 0;
        font-size: 2vh;
        box-sizing: border-box;
        border-bottom: 1px solid #404040;
        border-top: none;
        border-left: none;
        border-right: none;
        background-color: rgba(255, 255, 255, 0);
        color: #404040;
      }
      input[type=color] {
        width: 4vh;
        height: 4vh;
        padding: 0;
        border: none;
        border-radius: 0.5vh;
        background: none;
      }
      input[type='color']::-webkit-color-swatch-wrapper {
        padding: 0;
      }
      input[type='color']::-webkit-color-swatch {
        border: solid 3px #404040;
        border-radius: 0.5vh;
      }
      .probes-container {
        height:100vh;
        overflow-y: auto;
      }
      .boxes {
        width: 90vw;
        height: auto;
        margin-bottom: 1vh;
        padding: 2vh 0;
        background: #404040;
        border-radius: 0.5vh;
      }
      .active {
        color: #cccccc;
      }
      .config {
        position: fixed;
        font-size: 2vh;
        width: 100vw;
        height: 8vh;
        bottom: 0;
        left: 0;
        padding-top: 1vh;
        background-color: #ccc;
        color: #404040;
        transition: height 1s;
        border-radius: 0.5vh 0.5vh 0 0;
      }
      .config-inner {
        max-height: 96vh;
        overflow: auto;
      }
      .config-probe-settings {
        margin-top: 10vh;
      }
      .config-visible {
        height: 98vh;
      }
      .config-header {
        display: block;
        position: absolute;
        height: 6vh;
        width: 100%;
        background-color: #ccc;
        padding: 1vh 0;
        font-size: 5vh;
        border-bottom: 2px solid #404040;
      }
      .slider {
        -webkit-appearance: none;
        width: 100%;
        height: 4vh;
        margin: 1vh 0 1vh 0;
        background: #404040;
        outline: none;
        -webkit-transition: .2s;
        transition: opacity .2s;
        border-radius: 0.5vh;
      }
      .slider::-webkit-slider-thumb {
        -webkit-appearance: none;
        appearance: none;
        width: 4vh;
        height: 4vh;
        background: #cccccc;
        border: 2px solid #404040;
        border-radius: 0.5vh;
        cursor: pointer;
      }
      .slider::-moz-range-thumb {
        width: 3vh;
        height: 3vh;
        background: #cccccc;
        cursor: pointer;
      }
      .custom-button {
        padding: 1vh;
        margin: 1vh 0;
        height: 6vh;
        width: 90vw;
        font-size: 2vh;
        border: none;
        border-radius: 0.5vh;
        background-color: #377632;
        color: #cccccc;
        cursor: pointer;
        text-decoration: none;
      }
      .custom-button-clear {
        background-color: #933939;
      }
      .right {
        text-align: right;
      }
    </style>
    <script>
      var tempLower = 100;
      var tempUpper = 120;
      var colorDefault = '#377632';
      var colorLower = '#2058a2';
      var colorUpper = '#933939';
      var probeData = [];

      setInterval(function() {
        getProbes(false);
        updateTempAndColor();
      }, 5000);

      function updateTempAndColor() {
        for(var i = 0; i < probeData.length; i++){
          document.getElementById('probe'+[i+1]).innerHTML = probeData[i][1];
          document.getElementById('box'+[i+1]).style.backgroundColor = chooseColor(probeData[i][0], probeData[i][1]);
        }
      }
    
    function getProbes(firstTime) {
      var req = new XMLHttpRequest();
      req.overrideMimeType('application/json');
      req.open('GET', 'probes', true);
      req.onload  = function() {
        var jsonResponse = req.response;
        probeData = [];
          var splitJson = JSON.parse(this.responseText);
          keys = Object.keys(splitJson[0]);
          vals = Object.values(splitJson[0]);
          for(var i = 0; i < keys.length; i++){
            console.log(keys[i]);
            console.log(vals[i]);
            probeData.push([]);
            probeData[i].push(keys[i]);
            probeData[i].push(vals[i]);
          }
          if(firstTime){
            createElements(probeData.length);
          }
      }
      req.send(null);  
    }

      function chooseColor(elm, value) {
        if (typeof Storage !== 'undefined') {
          if(localStorage.getItem(elm+'ColorPickerDefault') != null){
            var lowThreshold = localStorage.getItem(elm+'LowerTemp');
            var upThreshold = localStorage.getItem(elm+'UpperTemp');
            if (value < localStorage.getItem(elm+'LowerTemp')) {
              return localStorage.getItem(elm+'ColorPickerLower');
            } else if (value > localStorage.getItem(elm+'UpperTemp')) {
              return localStorage.getItem(elm+'ColorPickerUpper');
            } else {
              return localStorage.getItem(elm+'ColorPickerDefault');
            }
          } else {
            if (value < tempLower) {
              return colorLower;
            } else if (value > tempUpper) {
              return colorUpper;
            } else {
              return colorDefault;
            }
          }
        }
      }

      function menuClicked(elm) {
        if (document.getElementById('configContainer').classList.contains('config-visible')) {
          document.getElementById('configContainer').classList.remove('config-visible');
          elm.firstElementChild.classList.add('fa-cog');
          elm.firstElementChild.classList.remove('fa-chevron-down');
        } else {
          document.getElementById('configContainer').classList.add('config-visible');
          elm.firstElementChild.classList.add('fa-chevron-down');
          elm.firstElementChild.classList.remove('fa-cog');
        }
      }

      function loadFromStorage() {
        if (typeof Storage !== 'undefined') {
          for(var i = 1; i <= probeData.length; i++){
            if(localStorage.getItem('p'+i+'ColorPickerDefault') != null){
              document.getElementById('p'+i+'ColorPickerDefault').value = localStorage.getItem('p'+i+'ColorPickerDefault');
              document.getElementById('p'+i+'ColorPickerLower').value = localStorage.getItem('p'+i+'ColorPickerLower');
              document.getElementById('p'+i+'ColorPickerUpper').value = localStorage.getItem('p'+i+'ColorPickerUpper');
              document.getElementById('p'+i+'LowerTemp').value = localStorage.getItem('p'+i+'LowerTemp');
              document.getElementById('p'+i+'UpperTemp').value = localStorage.getItem('p'+i+'UpperTemp');
            }
            refreshLabel(document.getElementById('p'+[i]+'LowerTemp'));
            refreshLabel(document.getElementById('p'+[i]+'UpperTemp'));
          }
        }
      }

      function saveToStorage() {
        if (typeof Storage !== 'undefined') {
          for(var i = 1; i <= probeData.length; i++){
            localStorage.setItem('p'+i+'ColorPickerDefault', document.getElementById('p'+i+'ColorPickerDefault').value)
            localStorage.setItem('p'+i+'ColorPickerLower', document.getElementById('p'+i+'ColorPickerLower').value);
            localStorage.setItem('p'+i+'ColorPickerUpper', document.getElementById('p'+i+'ColorPickerUpper').value);
            localStorage.setItem('p'+i+'LowerTemp', document.getElementById('p'+i+'LowerTemp').value);
            localStorage.setItem('p'+i+'UpperTemp', document.getElementById('p'+i+'UpperTemp').value);
          }
          alert('Settings saved, page will reload now.')
          window.location.reload();
        } else {
          alert('Settings could not be saved. It seems that your browser does not support localStorage.')
        }
      }

      function clearStorage(){
        if (confirm('Are you sure you want to delete the saved settings?')) {
          if (typeof Storage !== 'undefined') {
            localStorage.clear();
            window.location.reload();
          } else {
            alert('It seems that your browser does not support localStorage')
          }
        }
      }

      function refreshLabel(elm) {
        document.querySelector('label[for=\''+elm.id+'\']').innerText = elm.value + '\u00B0';
      }

      function createElements(numberOfProbes) {
        for (var i = 1; i <= numberOfProbes; i++) {
          var table = document.createElement('table');
          table.innerHTML = `
          <tr>
            <td colspan='3'>
              <span>Probe `+i+`</span>
            </td>
          </tr>
          <tr class='p`+i+`Row'>
            <td colspan='2'>
              Default color:
            </td>
            <td class='right'>
              <input type='color' id='p`+i+`ColorPickerDefault' value='`+colorDefault+`'>
            </td>
          </tr>
          <tr class='p`+i+`Row'>
            <td colspan='2'>
              Lower threshold color:
            </td>
            <td class='right'>
              <input type='color' id='p`+i+`ColorPickerLower' value='`+colorLower+`'>
            </td>
          </tr>
          <tr class='p`+i+`Row'>
            <td colspan='2'>
              Lower threshold temperature:
            </td>
            <td class='right'><label for='p`+i+`LowerTemp'>0&#176;</label></td>
          </tr>
          <tr class='p`+i+`Row'>
            <td colspan='3'>
              <input id='p`+i+`LowerTemp' oninput='refreshLabel(this)' class='slider' type='range' min='0' max='300' step='1' value='0'/>
            </td>
          </tr>
          <tr class='p`+i+`Row'>
            <td colspan='2'>
              Upper threshold color:
            </td>
            <td class='right'>
              <input type='color' id='p`+i+`ColorPickerUpper' value='`+colorUpper+`'>
            </td>
          </tr>
          <tr class='p`+i+`Row'>
            <td colspan='2'>
              Upper threshold temperature:
            </td>
            <td class='right'><label for='p`+i+`UpperTemp'>0&#176;</label></td>
          </tr>
          <tr class='p`+i+`Row'>
            <td colspan='3'>
              <input id='p`+i+`UpperTemp' onload='refreshLabel(this)' oninput='refreshLabel(this)' class='slider' type='range' min='0' max='300' step='1' value='300'/>
            </td>
          </tr>
          `;
          document.getElementById('probesSettings').appendChild(table);
          var div = document.createElement('div');
          div.innerHTML = `
          <div class='boxes' id='box`+i+`'> Probe `+i+`<br>
            <span id='probe`+i+`'>0</span>&#176;<br>
          </div>
          `;
          document.getElementById('probesIndex').appendChild(div);
          if (i == numberOfProbes) {
            loadFromStorage();
            updateTempAndColor();
          }
        }
      }
      document.addEventListener('DOMContentLoaded', function(event) {
        getProbes(true);
      });
    </script>
  </head>
  <body>
    <center>
      <div class='probes-container' id='probesIndex'>
        <br><i class='fa fa-thermometer-half'></i> Inkbird <br><br>
      </div>
      <div class='config' id='configContainer'>
        <div class='config-inner'>
          <div class='config-header' onclick='menuClicked(this)'>
            <i class='fas fa-cog'></i>
          </div>
          <div class='config-probe-settings' id='probesSettings'></div><br>
          <button class='custom-button' onclick='saveToStorage()'><i class='fas fa-save'></i> Save Settings</button><br>
          <button class='custom-button custom-button-clear' onclick='clearStorage()'><i class='far fa-trash-alt'></i> Delete Settings</button><br>
        </div>
      </div>
    </center>
  </body>
</html>
)=====";
 
static void notifyCallback(
  BLERemoteCharacteristic *pBLERemoteCharacteristic,
  uint8_t *pData,
  size_t length,
  bool isNotify)
{
  
  int count = 0;  
  globalTemp = "[{";
 
  while (count < length) {
    
    uint16_t rawTemp = littleEndianInt(&pData[count]);
    float temp = rawTemp / 10;
    String stringProbeNumber = String(count/2+1);
    String stringTemp = String(temp);

    Serial.println("Probe"+stringProbeNumber+" = "+stringTemp);
        
    if(count == 0){
      globalTemp+= "\"p"+stringProbeNumber+"\":"+stringTemp;
    } else {
      globalTemp+= ",\"p"+stringProbeNumber+"\":"+stringTemp;
    }
    count += 2;
  }
  globalTemp+= "}]";
}

bool connectToBLEServer(BLEAddress pAddress) {
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

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
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

uint16_t littleEndianInt(uint8_t *pData) {
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

void wifiInit() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  
  if(!MDNS.begin(mdns)) {
     Serial.println("Error starting mDNS");
     return;
  }
  
  Serial.println(" ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MDNS: ");
  Serial.println(mdns);
  
}

void handleIndex() {
  String index = String(htmlIndex);
  server.send(200, "text/html", index);
}

void handleProbes() {
  server.send(200, "text/json", globalTemp);
}

void setup() {
  Serial.begin(115200);
    
  wifiInit();

  server.on("/", handleIndex); 
  server.on("/probes", handleProbes);
  server.begin();  

  bleInit();
  
}

void loop() {
  
  if (doConnect == true) {
    connected = (connectToBLEServer(*pServerAddress)) ? true : false;
    doConnect = false;
  }
  
  server.handleClient();
  delay(1000);

}

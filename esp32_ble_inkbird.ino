#include <WebServer.h>
#include <ESPmDNS.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>
#include "config.h"

String jsonData = "[{\"connecting\": 0}]";
String stringBattery = "-";
String stayConnectedResponse;

int batteryRequestCounter = 48;
int disconnectCounter = 12;
int resetScanRunningCounter = 3;
int wifiErrorCounter = 0;

static uint8_t enableAccess[] = {0x21, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0xb8, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t enableUnitCelsius[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t enableRealTimeData[] = {0x0B, 0x01, 0x00, 0x00, 0x00, 0x00};
static uint8_t batteryLevel[] = {0x08, 0x24, 0x00, 0x00, 0x00, 0x00};

static BLEUUID serviceUUID("0000fff0-0000-1000-8000-00805f9b34fb");
static BLEUUID characteristicSettingsResultsUUID("0000fff1-0000-1000-8000-00805f9b34fb");
static BLEUUID characteristicAuthUUID("0000fff2-0000-1000-8000-00805f9b34fb");
static BLEUUID characteristicRealTimeDataUUID("0000fff4-0000-1000-8000-00805f9b34fb");
static BLEUUID characteristicSettingsUUID("0000fff5-0000-1000-8000-00805f9b34fb");

BLEClient *pClient;
BLEScan *pBLEScan;
Preferences preferences;

WebServer server(80);

static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static boolean requestConnection = false;
static boolean scanRunning = false;
static boolean stayConnected = false;

static BLERemoteCharacteristic *pRemoteCharacteristic;
static BLERemoteCharacteristic *pSettingsCharacteristic;
static BLERemoteCharacteristic *pAuthCharacteristic;
static BLERemoteCharacteristic *pSettingsResultsCharacteristic;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
    <head>
        <title>ESP32 - Inkbird</title>
        <meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1'>
        <meta charset="utf-8"/>
        <style>
            :root {
                --color-background: #1a1a1a;
                --color-dark: #404040;
                --color-light: #cccccc;
                --color-red: #933939;
                --color-orange: #f1951c;
                --color-green: #377632;
                --color-temp-upper: #933939;
                --color-temp-lower: #2058a2;
                --color-temp-default: #377632;
                --border-radius: 0.5vh;
                --border-solid: 2px solid;
            }
            body {
                background: var(--color-background);;
                color: var(--color-light);
                font-size: 3vh;
                margin: 0;
                font-family: Helvetica Neue, Helvetica, Arial, sans-serif;
            }
            a {
                text-decoration: none;
                color: var(--color-light);
            }
            input[type=text]::placeholder {
                color: var(--color-light);
            }
            input[type=text]:focus-visible {
                outline: none;
            }
            input[type=text] {
                background-color: var(--color-dark);
                color: var(--color-light);
                border: var(--border-solid) var(--color-dark);
                border-radius: var(--border-radius);
                font-size: 2vh;
                width: 50%;
            }
            input[type=color] {
                width: 4vh;
                height: 4vh;
                padding: 0;
                border: none;
                border-radius: var(--border-radius);
                background: none;
            }
            input[type='color']::-webkit-color-swatch-wrapper {
                padding: 0;
            }
            input[type='color']::-webkit-color-swatch {
                border: var(--border-solid) var(--color-dark);
                border-radius: var(--border-radius);
            }
            input[type='color']:hover {
                cursor: pointer;
            }
            .switch {
                display: inline-block;
                height: 4vh;
                position: relative;
                width: 8vh;
            }
            .switch .switch-checkbox {
                display:none;
            }
            .switch-slider {
                background-color: var(--color-dark);
                position: absolute;
                bottom: 0;
                left: 0;
                right: 0;
                top: 0;
                cursor: pointer;
                transition: .4s;
                border: var(--border-solid) var(--color-dark);
                border-radius: var(--border-radius);
            }
            .switch-slider:before {
                background-color: var(--color-light);;
                height: calc(4vh - 4px);
                width: calc(4vh - 4px);
                bottom: 0;
                left: 0;
                content: "";
                position: absolute;
                transition: .4s;
                border-radius: var(--border-radius);
            }
            .switch-checkbox:checked + .switch-slider {
                background-color: var(--color-green);
            }
            .switch-checkbox:checked + .switch-slider:before {
                transform: translateX(4vh);
            }
            .probes-container {
                height:fit-content;
                overflow-y: auto;
                opacity: 1;
                transition: opacity .4s;
            }
            .probes-container-header {
                margin: 3vh;
            }
            .no-settings {
                font-size: 6vh;
                margin-top: 20vh;
            }
            .boxes {
                width: 90vw;
                height: auto;
                margin-bottom: 1vh;
                padding: 2vh 0;
                background: var(--color-dark);
                border-radius: var(--border-radius);
            }
            .box-connecting {
                background: var(--color-background);
                margin-top: 30vh;
            }
            .box-error {
                margin-top: 22vh;
            }
            .config-inner {
                display: none;
                font-size: 2vh;
                opacity: 0;
                transition: opacity .4s;
                margin: 4vh 0 8vh 0;
            }
            .config-table {
                border: var(--border-solid) var(--color-dark);
                border-radius: var(--border-radius);
                width: 90%;
                margin-bottom: 1vh;
            }
            .config-table td {
                padding: 0.5vh;
            }
            .config-table tr {
                height: 5vh;
            }
            .visible {
                opacity: 1;
            }
            .invisible {
                opacity: 0;
            }
            .menubar {
                bottom: 0;
                left: 0;
                position: fixed;
                width: 100%;
                height: 5vh;
                padding: 1vh 0;
                background-color: var(--color-background);
                color: var(--color-light);
                border-top: var(--border-solid) var(--color-dark);
            }
            .menubar:hover {
                cursor: pointer;
            }
            .slider {
                -webkit-appearance: none;
                width: 100%;
                height: 4vh;
                margin: 1vh 0 1vh 0;
                background: var(--color-dark);
                outline: none;
                -webkit-transition: .2s;
                transition: opacity .2s;
                border-radius: var(--border-radius);
            }
            .slider::-webkit-slider-thumb {
                -webkit-appearance: none;
                appearance: none;
                width: 4vh;
                height: 4vh;
                background: var(--color-light);
                border: var(--border-solid) var(--color-dark);
                border-radius: var(--border-radius);
                cursor: pointer;
            }
            .slider::-moz-range-thumb {
                appearance: none;
                width: 4vh;
                height: 4vh;
                background: var(--color-light);
                border: var(--border-solid) var(--color-dark);
                border-radius: var(--border-radius);
                cursor: pointer;
            }
            .custom-button {
                padding: 1vh;
                margin: 1vh 0;
                height: 6vh;
                width: 90vw;
                font-size: 2vh;
                border: none;
                border-radius: var(--border-radius);
                color: var(--color-light);
                cursor: pointer;
                text-decoration: none;
            }
            .button-delete {
                background-color: var(--color-red);;
            }
            .button-save {
                background-color: var(--color-green);;
            }
            .right {
                text-align: right;
            }
            .center {
                text-align: center;
            }
            .vertical-center {
                display: inline-flex;
                align-items: center;
            }
            .svg-icon-big {
                height: 5vh;
            }
            .svg-icon-normal {
                height: 3vh;
                margin: auto;
                position: relative;
            }
            .svg-icon-small {
                height: 2vh;
            }
            .svg-icon-dropdown {
                width: clamp(2vh, 3vh, 3vh);
                height: clamp(2vh, 3vh, 3vh);
                margin: auto;
            }
            .icon-github {
                position: relative;
                top: 0.5vh;
            }
            .dropbtn {
                color: var(--color-dark);
                background-color: var(--color-light);
                cursor: pointer;
                padding: 0;
                padding-top: revert;
                width: 4vh;
                height: 4vh;
                border: var(--border-solid) var(--color-dark);
                border-radius: var(--border-radius);
            }
            .dropdown {
                position: relative;
                display: inline-block;
            }
            .dropdown-content {
                display: none;
                position: absolute;
                background-color: var(--color-light);
                overflow: auto;
                left: -1vh;
                width: 6vh;
                border: var(--border-solid) var(--color-dark);
                border-radius: var(--border-radius);
                cursor: pointer;
            }
            .dropdown-item {
                padding: 1vh 0;
                color: var(--color-dark);
                display: inline-flex;
                align-items: center;
                width: 100%;
            }
            .show {
                display: block;
            }
            .hide {
                display: none;
            }
            .battery-bar {
                position: absolute;
                right: 10px;
                top: 10px;
            }
            .battery-percent {
                font-size: 12px;
                float: left;
                margin-right: 3px;
            }
            .battery-icon {
                border: var(--border-solid) var(--color-light);
                width: 14px;
                height: 6px;
                padding: 2px;
                float: left;
                border-radius: 3px;
                position: relative;
            }
            .battery-icon::before {
                content: "";
                height: 6px;
                width: 2px;
                background: var(--color-light);
                display: block;
                position: absolute;
                top: 2px;
                left: 20px;
                border-radius: 0 3px 3px 0;
            }

            .battery-icon::after {
                content: '';
                display: block;
                position: absolute;
                top: -1px;
                left: -1px;
                right: -1px;
                bottom: -1px;
                border: 1px solid var(--color-background);
                border-radius: 3px;
            }
            .battery-level {
                background: var(--color-green);;  
                position: absolute;
                height:100%;
                bottom: 0px;
                left: 0;
                right: 0;
            }
            .battery-warn {
                background-color: var(--color-orange);
            }
                
            .battery-alert {
                background-color: var(--color-red);
            }
            .spinner {
                margin: auto;
                width: 100%;
                text-align: center;
            }
            .spinner-text {
                text-align: center;
                margin-top: 3vh;
            }
            .spinner > div {
                width: 3vh;
                height: 3vh;
                background-color: #ccc;
                border-radius: 100%;
                display: inline-block;
                -webkit-animation: bouncedelay 1.4s infinite ease-in-out both;
                animation: bouncedelay 1.4s infinite ease-in-out both;
            }
            .spinner .bounce1 {
                -webkit-animation-delay: -0.32s;
                animation-delay: -0.32s;
            }
            .spinner .bounce2 {
                -webkit-animation-delay: -0.16s;
                animation-delay: -0.16s;
            }
            @-webkit-keyframes bouncedelay {
                0%, 80%, 100% { 
                    -webkit-transform: scale(0) 
                }
                40% { 
                    -webkit-transform: scale(1.0) 
                }
            }
            @keyframes bouncedelay {
                0%, 80%, 100% { 
                    -webkit-transform: scale(0);
                    transform: scale(0);
                } 40% { 
                    -webkit-transform: scale(1.0);
                    transform: scale(1.0);
                }
            }
        </style>
        <script>

            let tempLower = 0;
            let tempUpper = 300;
            let colorDefault = getComputedStyle(document.documentElement).getPropertyValue('--color-temp-default').trim();
            let colorLower = getComputedStyle(document.documentElement).getPropertyValue('--color-temp-lower').trim();
            let colorUpper = getComputedStyle(document.documentElement).getPropertyValue('--color-temp-upper').trim();
            let probeData = [];
            let stayConnectedState;
            let stayConnectedStateLoaded = false;
            let batteryPercent;
            let elementsCreated = false;

            let probesContainerInnerHtml = `
                <div class='battery-bar'>
                    <div class="battery-percent"></div>
                    <div class="battery-icon">
                        <div class="battery-level" style="width:0%;"></div>
                    </div>
                </div>
                <div class='probes-container-header'>
                    <table>
                        <tr>
                            <td class='vertical-center'>` + generateIcon('thermometer', 'normal') + `</td>
                            <td class='vertical-center'>&nbsp;Inkbird</td>
                        </tr>
                    </table>
                </div>
                <div class='boxes box-connecting' id='boxConnecting' style='display: none;'>
                    <div class="spinner">
                        <div class="bounce1"></div>
                        <div class="bounce2"></div>
                        <div class="bounce3"></div>
                    </div>
                    <div class="spinner-text">Connecting</div>
                </div>
                <div class='boxes box-error' id='boxError' style='background-color: var(--color-red); display: none;'>
                    Error<br><br>
                    No Device found/connected.<br>
                    Check if the Inkbird is switched on and within the bluetooth range of the ESP.<br>
                </div>
                `;

            let mainInterval;
            let mainIntervalTime = 5000;


            function handleInterval(start) {

                if(start) {

                    mainInterval = setInterval(function() {

                        getData();

                    }, mainIntervalTime);

                } else {

                    if(mainInterval != undefined) {

                        clearInterval(mainInterval);

                    }

                }

            }


            function generateIcon(icon, size) {

                let iconStart       = '<svg xmlns="http://www.w3.org/2000/svg" ';
                let iconClass       = 'class="svg-icon-';
                let iconCog         = '" viewBox="0 0 512 512" id="cog"><path class="icon-path" fill="currentColor" d="M444.788 291.1l42.616 24.599c4.867 2.809 7.126 8.618 5.459 13.985-11.07 35.642-29.97 67.842-54.689 94.586a12.016 12.016 0 0 1-14.832 2.254l-42.584-24.595a191.577 191.577 0 0 1-60.759 35.13v49.182a12.01 12.01 0 0 1-9.377 11.718c-34.956 7.85-72.499 8.256-109.219.007-5.49-1.233-9.403-6.096-9.403-11.723v-49.184a191.555 191.555 0 0 1-60.759-35.13l-42.584 24.595a12.016 12.016 0 0 1-14.832-2.254c-24.718-26.744-43.619-58.944-54.689-94.586-1.667-5.366.592-11.175 5.459-13.985L67.212 291.1a193.48 193.48 0 0 1 0-70.199l-42.616-24.599c-4.867-2.809-7.126-8.618-5.459-13.985 11.07-35.642 29.97-67.842 54.689-94.586a12.016 12.016 0 0 1 14.832-2.254l42.584 24.595a191.577 191.577 0 0 1 60.759-35.13V25.759a12.01 12.01 0 0 1 9.377-11.718c34.956-7.85 72.499-8.256 109.219-.007 5.49 1.233 9.403 6.096 9.403 11.723v49.184a191.555 191.555 0 0 1 60.759 35.13l42.584-24.595a12.016 12.016 0 0 1 14.832 2.254c24.718 26.744 43.619 58.944 54.689 94.586 1.667 5.366-.592 11.175-5.459 13.985L444.788 220.9a193.485 193.485 0 0 1 0 70.2zM336 256c0-44.112-35.888-80-80-80s-80 35.888-80 80 35.888 80 80 80 80-35.888 80-80z"></path></svg>';
                let iconHome        = '" viewBox="0 0 576 512" id="home"><path class="icon-path" fill="currentColor" d="M575.8 255.5C575.8 273.5 560.8 287.6 543.8 287.6H511.8L512.5 447.7C512.5 450.5 512.3 453.1 512 455.8V472C512 494.1 494.1 512 472 512H456C454.9 512 453.8 511.1 452.7 511.9C451.3 511.1 449.9 512 448.5 512H392C369.9 512 352 494.1 352 472V384C352 366.3 337.7 352 320 352H256C238.3 352 224 366.3 224 384V472C224 494.1 206.1 512 184 512H128.1C126.6 512 125.1 511.9 123.6 511.8C122.4 511.9 121.2 512 120 512H104C81.91 512 64 494.1 64 472V360C64 359.1 64.03 358.1 64.09 357.2V287.6H32.05C14.02 287.6 0 273.5 0 255.5C0 246.5 3.004 238.5 10.01 231.5L266.4 8.016C273.4 1.002 281.4 0 288.4 0C295.4 0 303.4 2.004 309.5 7.014L564.8 231.5C572.8 238.5 576.9 246.5 575.8 255.5L575.8 255.5z"></path></svg>';
                let iconThermometer = '" viewBox="0 0 320 512" id="thermometer"><path class="icon-path" fill="currentColor" d="M176 322.9l.0002-114.9c0-8.75-7.25-16-16-16s-15.1 7.25-15.1 16L144 322.9c-18.62 6.625-32 24.25-32 45.13c0 26.5 21.5 48 48 48s48-21.5 48-48C208 347.1 194.6 329.5 176 322.9zM272 278.5V112c0-61.87-50.12-112-111.1-112S48 50.13 48 112v166.5c-19.75 24.75-32 55.5-32 89.5c0 79.5 64.5 143.1 144 143.1S304 447.5 304 368C304 334 291.8 303.1 272 278.5zM160 448c-44.13 0-80-35.87-80-79.1c0-25.5 12.25-48.88 32-63.75v-192.3c0-26.5 21.5-48 48-48s48 21.5 48 48v192.3c19.75 14.75 32 38.25 32 63.75C240 412.1 204.1 448 160 448z"></path></svg>';
                let iconSave        = '" viewBox="0 0 448 512"><path class="icon-path" fill="currentColor" d="M433.941 129.941l-83.882-83.882A48 48 0 0 0 316.118 32H48C21.49 32 0 53.49 0 80v352c0 26.51 21.49 48 48 48h352c26.51 0 48-21.49 48-48V163.882a48 48 0 0 0-14.059-33.941zM224 416c-35.346 0-64-28.654-64-64 0-35.346 28.654-64 64-64s64 28.654 64 64c0 35.346-28.654 64-64 64zm96-304.52V212c0 6.627-5.373 12-12 12H76c-6.627 0-12-5.373-12-12V108c0-6.627 5.373-12 12-12h228.52c3.183 0 6.235 1.264 8.485 3.515l3.48 3.48A11.996 11.996 0 0 1 320 111.48z"></path></svg>';
                let iconTrash       = '" viewBox="0 0 448 512" id="trash"><path class="icon-path" fill="currentColor" d="M0 84V56c0-13.3 10.7-24 24-24h112l9.4-18.7c4-8.2 12.3-13.3 21.4-13.3h114.3c9.1 0 17.4 5.1 21.5 13.3L312 32h112c13.3 0 24 10.7 24 24v28c0 6.6-5.4 12-12 12H12C5.4 96 0 90.6 0 84zm416 56v324c0 26.5-21.5 48-48 48H80c-26.5 0-48-21.5-48-48V140c0-6.6 5.4-12 12-12h360c6.6 0 12 5.4 12 12zm-272 68c0-8.8-7.2-16-16-16s-16 7.2-16 16v224c0 8.8 7.2 16 16 16s16-7.2 16-16V208zm96 0c0-8.8-7.2-16-16-16s-16 7.2-16 16v224c0 8.8 7.2 16 16 16s16-7.2 16-16V208zm96 0c0-8.8-7.2-16-16-16s-16 7.2-16 16v224c0 8.8 7.2 16 16 16s16-7.2 16-16V208z"></path></svg>';
                let iconGithub      = '" viewBox="0 0 496 512" id="github"><path class="icon-path" fill="currentColor" d="M165.9 397.4c0 2-2.3 3.6-5.2 3.6-3.3.3-5.6-1.3-5.6-3.6 0-2 2.3-3.6 5.2-3.6 3-.3 5.6 1.3 5.6 3.6zm-31.1-4.5c-.7 2 1.3 4.3 4.3 4.9 2.6 1 5.6 0 6.2-2s-1.3-4.3-4.3-5.2c-2.6-.7-5.5.3-6.2 2.3zm44.2-1.7c-2.9.7-4.9 2.6-4.6 4.9.3 2 2.9 3.3 5.9 2.6 2.9-.7 4.9-2.6 4.6-4.6-.3-1.9-3-3.2-5.9-2.9zM244.8 8C106.1 8 0 113.3 0 252c0 110.9 69.8 205.8 169.5 239.2 12.8 2.3 17.3-5.6 17.3-12.1 0-6.2-.3-40.4-.3-61.4 0 0-70 15-84.7-29.8 0 0-11.4-29.1-27.8-36.6 0 0-22.9-15.7 1.6-15.4 0 0 24.9 2 38.6 25.8 21.9 38.6 58.6 27.5 72.9 20.9 2.3-16 8.8-27.1 16-33.7-55.9-6.2-112.3-14.3-112.3-110.5 0-27.5 7.6-41.3 23.6-58.9-2.6-6.5-11.1-33.3 2.6-67.9 20.9-6.5 69 27 69 27 20-5.6 41.5-8.5 62.8-8.5s42.8 2.9 62.8 8.5c0 0 48.1-33.6 69-27 13.7 34.7 5.2 61.4 2.6 67.9 16 17.7 25.8 31.5 25.8 58.9 0 96.5-58.9 104.2-114.8 110.5 9.2 7.9 17 22.9 17 46.4 0 33.7-.3 75.4-.3 83.6 0 6.5 4.6 14.4 17.3 12.1C428.2 457.8 496 362.9 496 252 496 113.3 383.5 8 244.8 8zM97.2 352.9c-1.3 1-1 3.3.7 5.2 1.6 1.6 3.9 2.3 5.2 1 1.3-1 1-3.3-.7-5.2-1.6-1.6-3.9-2.3-5.2-1zm-10.8-8.1c-.7 1.3.3 2.9 2.3 3.9 1.6 1 3.6.7 4.3-.7.7-1.3-.3-2.9-2.3-3.9-2-.6-3.6-.3-4.3.7zm32.4 35.6c-1.6 1.3-1 4.3 1.3 6.2 2.3 2.3 5.2 2.6 6.5 1 1.3-1.3.7-4.3-1.3-6.2-2.2-2.3-5.2-2.6-6.5-1zm-11.4-14.7c-1.6 1-1.6 3.6 0 5.9 1.6 2.3 4.3 3.3 5.6 2.3 1.6-1.3 1.6-3.9 0-6.2-1.4-2.3-4-3.3-5.6-2z"></path></svg>';
                let iconFood        = '" viewBox="0 0 448 512" id="food"><path class="icon-path" fill="currentColor" d="M221.6 148.7C224.7 161.3 224.8 174.5 222.1 187.2C219.3 199.1 213.6 211.9 205.6 222.1C191.1 238.6 173 249.1 151.1 254.1V472C151.1 482.6 147.8 492.8 140.3 500.3C132.8 507.8 122.6 512 111.1 512C101.4 512 91.22 507.8 83.71 500.3C76.21 492.8 71.1 482.6 71.1 472V254.1C50.96 250.1 31.96 238.9 18.3 222.4C10.19 212.2 4.529 200.3 1.755 187.5C-1.019 174.7-.8315 161.5 2.303 148.8L32.51 12.45C33.36 8.598 35.61 5.197 38.82 2.9C42.02 .602 45.97-.4297 49.89 .0026C53.82 .4302 57.46 2.303 60.1 5.259C62.74 8.214 64.18 12.04 64.16 16V160H81.53L98.62 11.91C99.02 8.635 100.6 5.621 103.1 3.434C105.5 1.248 108.7 .0401 111.1 .0401C115.3 .0401 118.5 1.248 120.9 3.434C123.4 5.621 124.1 8.635 125.4 11.91L142.5 160H159.1V16C159.1 12.07 161.4 8.268 163.1 5.317C166.6 2.366 170.2 .474 174.1 .0026C178-.4262 181.1 .619 185.2 2.936C188.4 5.253 190.6 8.677 191.5 12.55L221.6 148.7zM448 472C448 482.6 443.8 492.8 436.3 500.3C428.8 507.8 418.6 512 408 512C397.4 512 387.2 507.8 379.7 500.3C372.2 492.8 368 482.6 368 472V352H351.2C342.8 352 334.4 350.3 326.6 347.1C318.9 343.8 311.8 339.1 305.8 333.1C299.9 327.1 295.2 320 291.1 312.2C288.8 304.4 287.2 296 287.2 287.6L287.1 173.8C288 136.9 299.1 100.8 319.8 70.28C340.5 39.71 369.8 16.05 404.1 2.339C408.1 .401 414.2-.3202 419.4 .2391C424.6 .7982 429.6 2.62 433.9 5.546C438.2 8.472 441.8 12.41 444.2 17.03C446.7 21.64 447.1 26.78 448 32V472z"></path></svg>';
                let iconPig         = '" viewBox="0 0 576 512" id="pig"><path class="icon-path" fill="currentColor" d="M 400 96 L 399.1 96.66 z M 384 128 C 387.5 128 390.1 128.1 394.4 128.3 C 398.7 128.6 402.9 129 407 129.6 C 424.6 109.1 450.8 96 480 96 H 512 L 493.2 171.1 C 509.1 185.9 521.9 203.9 530.7 224 H 544 C 561.7 224 576 238.3 576 256 V 352 C 576 369.7 561.7 384 544 384 H 512 C 502.9 396.1 492.1 406.9 480 416 V 480 C 480 497.7 465.7 512 448 512 H 416 C 398.3 512 384 497.7 384 480 V 448 H 256 V 480 C 256 497.7 241.7 512 224 512 H 192 C 174.3 512 160 497.7 160 480 V 416 C 125.1 389.8 101.3 349.8 96.79 304 H 68 C 30.44 304 0 273.6 0 236 C 0 198.4 30.44 168 68 168 H 72 C 85.25 168 96 178.7 96 192 C 96 205.3 85.25 216 72 216 H 68 C 56.95 216 48 224.1 48 236 C 48 247 56.95 256 68 256 H 99.2 C 111.3 196.2 156.9 148.5 215.5 133.2 C 228.4 129.8 241.1 128 256 128 H 384 z M 424 240 C 410.7 240 400 250.7 400 264 C 400 277.3 410.7 288 424 288 C 437.3 288 448 277.3 448 264 C 448 250.7 437.3 240 424 240 z"></path></svg>';
                let iconCow         = '" viewBox="0 0 640 512" id="cow"><path class="icon-path" fill="currentColor" d="M634 276.8l-9.999-13.88L624 185.7c0-11.88-12.5-19.49-23.12-14.11c-10.88 5.375-19.5 13.5-26.38 23l-65.75-90.92C490.6 78.71 461.8 64 431 64H112C63.37 64 24 103.4 24 152v86.38C9.5 250.1 0 267.9 0 288v32h8c35.38 0 64-28.62 64-64L72 152c0-16.88 10.5-31.12 25.38-37C96.5 119.1 96 123.5 96 128l.0002 304c0 8.875 7.126 16 16 16h63.1c8.875 0 16-7.125 16-16l.0006-112c9.375 9.375 20.25 16.5 32 21.88V368c0 8.875 7.252 16 16 16c8.875 0 15.1-7.125 15.1-16v-17.25c9.125 1 12.88 2.25 32-.125V368c0 8.875 7.25 16 16 16c8.875 0 16-7.125 16-16v-26.12C331.8 336.5 342.6 329.2 352 320l-.0012 112c0 8.875 7.125 16 15.1 16h64c8.75 0 16-7.125 16-16V256l31.1 32l.0006 41.55c0 12.62 3.752 24.95 10.75 35.45l41.25 62C540.8 440.1 555.5 448 571.4 448c22.5 0 41.88-15.88 46.25-38l21.75-108.6C641.1 292.8 639.1 283.9 634 276.8zM377.3 167.4l-22.88 22.75C332.5 211.8 302.9 224 272.1 224S211.5 211.8 189.6 190.1L166.8 167.4C151 151.8 164.4 128 188.9 128h166.2C379.6 128 393 151.8 377.3 167.4zM576 352c-8.875 0-16-7.125-16-16s7.125-16 16-16s16 7.125 16 16S584.9 352 576 352z"></path></svg>';
                let iconFish        = '" viewBox="0 0 576 512" id="fish"><path class="icon-path" fill="currentColor" d="M180.5 141.5C219.7 108.5 272.6 80 336 80C399.4 80 452.3 108.5 491.5 141.5C530.5 174.5 558.3 213.1 572.4 241.3C577.2 250.5 577.2 261.5 572.4 270.7C558.3 298 530.5 337.5 491.5 370.5C452.3 403.5 399.4 432 336 432C272.6 432 219.7 403.5 180.5 370.5C164.3 356.7 150 341.9 137.8 327.3L48.12 379.6C35.61 386.9 19.76 384.9 9.474 374.7C-.8133 364.5-2.97 348.7 4.216 336.1L50 256L4.216 175.9C-2.97 163.3-.8133 147.5 9.474 137.3C19.76 127.1 35.61 125.1 48.12 132.4L137.8 184.7C150 170.1 164.3 155.3 180.5 141.5L180.5 141.5zM416 224C398.3 224 384 238.3 384 256C384 273.7 398.3 288 416 288C433.7 288 448 273.7 448 256C448 238.3 433.7 224 416 224z"></path></svg>';
                let iconFire        = '" viewBox="0 0 448 512" id="fire"><path class="icon-path" fill="currentColor" d="M323.5 51.25C302.8 70.5 284 90.75 267.4 111.1C240.1 73.62 206.2 35.5 168 0C69.75 91.12 0 210 0 281.6C0 408.9 100.2 512 224 512s224-103.1 224-230.4C448 228.4 396 118.5 323.5 51.25zM304.1 391.9C282.4 407 255.8 416 226.9 416c-72.13 0-130.9-47.73-130.9-125.2c0-38.63 24.24-72.64 72.74-130.8c7 8 98.88 125.4 98.88 125.4l58.63-66.88c4.125 6.75 7.867 13.52 11.24 19.9C364.9 290.6 353.4 357.4 304.1 391.9z"></path></svg>';
                let iconNone        = '" viewBox="0 0 448 512" id="none"><path class="icon-path" fill="currentColor" d="M400 288h-352c-17.69 0-32-14.32-32-32.01s14.31-31.99 32-31.99h352c17.69 0 32 14.3 32 31.99S417.7 288 400 288z"></path></svg>';

                let iconSvg;
                let iconSize = size;

                switch (icon) {
                    case 'cog':
                        iconSvg = iconCog;        
                        break;
                    case 'home':
                        iconSvg = iconHome;        
                        break;
                    case 'thermometer':
                        iconSvg = iconThermometer;        
                        break;
                    case 'save':
                        iconSvg = iconSave;        
                        break;
                    case 'trash':
                        iconSvg = iconTrash;        
                        break;
                    case 'github':
                        iconSvg = iconGithub;        
                        break;
                    case 'food':
                        iconSvg = iconFood;        
                        break;
                    case 'pig':
                        iconSvg = iconPig;        
                        break;
                    case 'cow':
                        iconSvg = iconCow;        
                        break;
                    case 'fish':
                        iconSvg = iconFish;        
                        break;
                    case 'fire':
                        iconSvg = iconFire;        
                        break;
                    case 'none':
                        iconSvg = iconNone;        
                        break;
                    default:
                        break;
                }

                return iconStart + iconClass + iconSize + iconSvg;

            }


            function updateTempAndColor() {

                probeData.forEach(el => {

                    if(el[0] != 'error' && el[1] < 6550) {

                        document.getElementById('boxConnecting').style.display = 'none';
                        document.getElementById('boxError').style.display = 'none';
                        document.getElementById('probe' + el[0]).style.display = ''; 
                        document.getElementById('box' + el[0]).style.display = '';
                        document.getElementById('probe' + el[0]).innerHTML = el[1];
                        document.getElementById('box' + el[0]).style.backgroundColor = chooseColor(el[0], el[1]);

                    } else {

                        if(document.getElementById('probe' + el[0])) {

                            document.getElementById('probe' + el[0]).style.display = 'none'; 
                            document.getElementById('box' + el[0]).style.display = 'none';

                        }

                    }

                })
            
                let batteryIcon = document.querySelector(".battery-level");
                let batteryText = document.querySelector(".battery-percent");

                if(!isNaN(batteryPercent)){

                    batteryIcon.style.width = batteryPercent + '%';
                    batteryText.innerText = batteryPercent + '%';

                    if(batteryPercent < 20){
                        batteryIcon.classList.add("battery-alert");
                    } else if (batteryPercent < 50) {
                        batteryIcon.classList.add("battery-warn");
                    }

                } 

            }


            async function getData() {

                probeData = [];

                try {

                    let noCacheHeaders = new Headers();
                    noCacheHeaders.append('pragma', 'no-cache');
                    noCacheHeaders.append('cache-control', 'no-cache');

                    const response = await fetch('data', { 
                        method: 'GET',
                        headers: noCacheHeaders
                    });

                    const content = await response.json();

                    if (response.status === 200) {

                        keys = Object.keys(content[0]);
                        vals = Object.values(content[0]);

                        if(keys[0] == 'error') {

                            deleteElements();
                            document.getElementById('boxError').style.display = '';

                        } else if(keys[0] == 'connecting') {

                            deleteElements();
                            document.getElementById('boxConnecting').style.display = '';

                        } else {

                            for(var i = 0; i < keys.length; i++) {

                                if(keys[i] == "battery") {

                                    batteryPercent = vals[i];

                                } else if(keys[i] == "stayconnected") {

                                    stayConnectedState = (vals[i] == "1") ? true : false;

                                } else {

                                    probeData.push([]);
                                    probeData[i].push(keys[i]);
                                    probeData[i].push(vals[i]);
                            
                                }
                            
                            }

                            if(!elementsCreated && keys[0] != 'error') {

                                createElements(probeData.length);

                            }

                        }

                        if(mainInterval == undefined) {

                            handleInterval(true);

                        }

                        updateTempAndColor();

                    } 

                } catch (error) {

                    console.log(error);

                }

            }


            async function setStayConnected(on) {

                let url = "sc?toggle=";

                if(on) {
                    url+= "1"
                } else {
                    url+= "0"
                }

                try {

                    let noCacheHeaders = new Headers();
                    noCacheHeaders.append('pragma', 'no-cache');
                    noCacheHeaders.append('cache-control', 'no-cache');

                    const response = await fetch(url, { 
                        method: 'GET',
                        headers: noCacheHeaders
                    });

                    const content = await response.text();

                    if (response.status === 200 && response.statusText === "OK") {
                        
                    } 

                } catch (error) {

                    console.log(error);

                }

            }


            function chooseColor(elm, value) {

                if (typeof Storage !== 'undefined') {

                    if(localStorage.getItem('p' + elm + 'ColorPickerDefault') != null) {

                        let lowerThreshold = localStorage.getItem('p' + elm + 'LowerTemp');
                        let uppperThreshold = localStorage.getItem('p' + elm + 'UpperTemp');

                        if (value < lowerThreshold) {

                            return localStorage.getItem('p' + elm + 'ColorPickerLower');

                        } else if (value > uppperThreshold) {

                            return localStorage.getItem('p' + elm + 'ColorPickerUpper');

                        } else {

                            return localStorage.getItem('p' + elm + 'ColorPickerDefault');

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


            function menuClicked() {

                if (document.getElementsByClassName('config-inner')[0].classList.contains('visible')) {

                    document.querySelector('.menubar').innerHTML = generateIcon('cog', 'big');

                    document.getElementsByClassName('config-inner')[0].classList.remove('visible');
                    document.getElementsByClassName('probes-container')[0].classList.remove('invisible');

                    setTimeout(() => {

                        document.getElementsByClassName('probes-container')[0].style.display = 'block';
                        document.getElementsByClassName('config-inner')[0].style.display = 'none';

                    }, 410);

                } else {

                    if(probeData.length == 0) {

                        document.querySelectorAll(".custom-button").forEach(e => {

                            e.classList.add("hide")

                        })

                        document.querySelector(".no-settings").classList.remove("hide");
                    
                    } else {

                        document.querySelectorAll(".custom-button").forEach(e => {

                            e.classList.remove("hide")

                        })

                        document.querySelector(".no-settings").classList.add("hide");

                    }

                    document.querySelector('.menubar').innerHTML = generateIcon('home', 'big');

                    document.getElementsByClassName('config-inner')[0].classList.add('visible');
                    document.getElementsByClassName('probes-container')[0].classList.add('invisible');

                    setTimeout(() => {

                        document.getElementsByClassName('config-inner')[0].style.display = 'block';
                        document.getElementsByClassName('probes-container')[0].style.display = 'none';

                    }, 410);

                }

            }


            function loadFromStorage() {

                if (typeof Storage !== 'undefined') {

                    probeData.forEach(el => {

                        if(localStorage.getItem('p' + el[0] + 'ColorPickerDefault') != null) {

                            document.getElementById('p' + el[0] + 'ColorPickerDefault').value = localStorage.getItem('p' + el[0] + 'ColorPickerDefault');
                            document.getElementById('p' + el[0] + 'ColorPickerLower').value = localStorage.getItem('p' + el[0] + 'ColorPickerLower');
                            document.getElementById('p' + el[0] + 'ColorPickerUpper').value = localStorage.getItem('p' + el[0] + 'ColorPickerUpper');
                            document.getElementById('p' + el[0] + 'LowerTemp').value = localStorage.getItem('p' + el[0] + 'LowerTemp');
                            document.getElementById('p' + el[0] + 'UpperTemp').value = localStorage.getItem('p' + el[0] + 'UpperTemp');
                            document.getElementById('p' + el[0] + 'IconDropdownBtn').innerHTML = generateIcon(localStorage.getItem('p' + el[0] + 'IconDropdownBtn'), 'dropdown');
                            document.getElementById('p' + el[0] + 'DisplayName').placeholder = localStorage.getItem('p' + el[0] + 'DisplayName');

                            if(localStorage.getItem('p' + el[0] + 'IconDropdownBtn') != 'none') {

                                document.getElementById('p' + el[0] + 'boxIcon').innerHTML = generateIcon(localStorage.getItem('p' + el[0] + 'IconDropdownBtn'), 'normal');

                            }

                            if(localStorage.getItem('p' + el[0] + 'DisplayName') != '-') {

                                document.getElementById('p' + el[0] + 'boxName').innerHTML = localStorage.getItem('p' + el[0] + 'DisplayName');

                            } else {

                                document.getElementById('p' + el[0] + 'boxName').innerHTML = '&nbsp;';

                            }

                        }

                        refreshLabel(document.getElementById('p' + el[0] + 'LowerTemp'));
                        refreshLabel(document.getElementById('p' + el[0] + 'UpperTemp'));

                    })

                }

            }


            function saveToStorage() {

                if (typeof Storage !== 'undefined') {

                    setStayConnected(document.querySelector("#sc-checkbox").checked);

                    probeData.forEach(el => {

                        localStorage.setItem('p' + el[0] + 'ColorPickerDefault', document.getElementById('p' + el[0] + 'ColorPickerDefault').value);
                        localStorage.setItem('p' + el[0] + 'ColorPickerLower', document.getElementById('p' + el[0] + 'ColorPickerLower').value);
                        localStorage.setItem('p' + el[0] + 'ColorPickerUpper', document.getElementById('p' + el[0] + 'ColorPickerUpper').value);
                        localStorage.setItem('p' + el[0] + 'LowerTemp', document.getElementById('p' + el[0] + 'LowerTemp').value);
                        localStorage.setItem('p' + el[0] + 'UpperTemp', document.getElementById('p' + el[0] + 'UpperTemp').value);
                        localStorage.setItem('p' + el[0] + 'IconDropdownBtn', document.getElementById('p' + el[0] + 'IconDropdownBtn').firstChild.id);

                        let displayNameValue = document.getElementById('p' + el[0] + 'DisplayName').value;
                        let displayNamePlaceholder = document.getElementById('p' + el[0] + 'DisplayName').placeholder;

                        if(displayNameValue != '') {

                            localStorage.setItem('p' + el[0] + 'DisplayName', displayNameValue);

                        } else {

                            localStorage.setItem('p' + el[0] + 'DisplayName', displayNamePlaceholder);

                        }

                    })

                    alert('Settings saved, page will reload now.');
                    window.location.reload();

                } else {

                   alert('Settings could not be saved. It seems that your browser does not support localStorage.');

                }

            }


            function clearStorage() {

                if (confirm('Are you sure you want to delete the saved settings?')) {

                    if (typeof Storage !== 'undefined') {

                        localStorage.clear();
                        window.location.reload();

                    } else {

                        alert('It seems that your browser does not support localStorage');

                    }

                }

            }


            function refreshLabel(elm) {

                document.querySelector('label[for=\''+elm.id+'\']').innerText = elm.value + '\u00B0';

            }


            function deleteElements() {

                document.querySelector('.probes-container').innerHTML = probesContainerInnerHtml;
                document.querySelector('.config-probe-settings').innerHTML = '';
                elementsCreated = false;

            }

            function dropdownClicked(id) {

                document.getElementById('p' + id + 'IconDropdown').classList.toggle('show');
            
            }

            function dropdownItemClicked(el) {

                el.parentElement.parentElement.previousElementSibling.innerHTML = el.innerHTML;
                dropdownHideAll();

            }

            function dropdownHideAll() {

                let dropdowns = document.querySelectorAll('.dropdown-content.show');

                dropdowns.forEach(e => {

                    e.classList.remove('show');

                })

            }


            function createElements(numberOfProbes) {

                probeData.forEach(function(el, index) {

                    let table = document.createElement('table');
                    table.classList = ['config-table'];
                    table.innerHTML = `
                        <tr>
                            <td colspan='3'>
                                <span>Probe ` + el[0] + `</span>
                            </td>
                        </tr>
                        <tr class='p` + el[0] + `Row'>
                            <td colspan='2'>
                                Display name:
                            </td>
                            <td class='right'>
                                <input type='text' id='p` + el[0] + `DisplayName' placeholder='Probe ` + el[0] + `'>
                            </td>
                        </tr>
                        <tr class='p` + el[0] + `Row'>
                            <td colspan='2'>
                                Icon:
                            </td>
                            <td class='right'>
                                <div class='dropdown'>
                                    <button id='p` + el[0] + `IconDropdownBtn' onclick='dropdownClicked(` + el[0] + `)' class='dropbtn'>` + generateIcon('none','dropdown') + `</button>
                                    <div id='p` + el[0] + `IconDropdown' class='dropdown-content'>
                                        <center>
                                            <div onclick='dropdownItemClicked(this)' class='dropdown-item'>` + generateIcon('food','dropdown') + `</div>
                                            <div onclick='dropdownItemClicked(this)' class='dropdown-item'>` + generateIcon('pig','dropdown') + `</div>
                                            <div onclick='dropdownItemClicked(this)' class='dropdown-item'>` + generateIcon('cow','dropdown') + `</div>
                                            <div onclick='dropdownItemClicked(this)' class='dropdown-item'>` + generateIcon('fish','dropdown') + `</div>
                                            <div onclick='dropdownItemClicked(this)' class='dropdown-item'>` + generateIcon('fire','dropdown') + `</div>
                                            <div onclick='dropdownItemClicked(this)' class='dropdown-item'>` + generateIcon('thermometer','dropdown') + `</div>
                                            <div onclick='dropdownItemClicked(this)' class='dropdown-item'>` + generateIcon('none','dropdown') + `</div>
                                        </center>
                                    </div>
                                </div>
                            </td>
                        </tr>
                        <tr class='p` + el[0] + `Row'>
                            <td colspan='2'>
                                Default color:
                            </td>
                            <td class='right'>
                                <input type='color' id='p`+ el[0] +`ColorPickerDefault' value='` + colorDefault + `'>
                            </td>
                        </tr>
                        <tr class='p` + el[0] + `Row'>
                            <td colspan='2'>
                                Lower threshold color:
                            </td>
                            <td class='right'>
                                <input type='color' id='p` + el[0] + `ColorPickerLower' value='` + colorLower + `'>
                            </td>
                        </tr>
                        <tr class='p` + el[0] + `Row'>
                            <td colspan='2'>
                                Lower threshold temperature:
                            </td>
                            <td class='right'>
                                <label for='p` + el[0] + `LowerTemp'>0&#176;</label>
                            </td>
                        </tr>
                        <tr class='p` + el[0] + `Row'>
                            <td colspan='3'>
                                <input type='range' id='p` + el[0] + `LowerTemp' oninput='refreshLabel(this)' class='slider' min='0' max='300' step='1' value='0'/>
                            </td>
                        </tr>
                        <tr class='p` + el[0] + `Row'>
                            <td colspan='2'>
                                Upper threshold color:
                            </td>
                            <td class='right'>
                                <input type='color' id='p` + el[0] + `ColorPickerUpper' value='` + colorUpper + `'>
                            </td>
                        </tr>
                        <tr class='p` + el[0] + `Row'>
                            <td colspan='2'>
                                Upper threshold temperature:
                            </td>
                            <td class='right'>
                                <label for='p` + el[0] + `UpperTemp'>0&#176;</label>
                            </td>
                        </tr>
                        <tr class='p` + el[0] + `Row'>
                            <td colspan='3'>
                                <input id='p` + el[0] + `UpperTemp' onload='refreshLabel(this)' oninput='refreshLabel(this)' class='slider' type='range' min='0' max='300' step='1' value='300'/>
                            </td>
                        </tr>
                    `;

                    document.querySelector('.config-probe-settings').appendChild(table);

                    let div = document.createElement('div');

                    div.innerHTML = `
                    <div class='boxes' id='box` + el[0] + `'>
                        <table class='center'>
                            <tr>
                                <td id='p` + el[0] + `boxIcon'></td>
                                <td id='p` + el[0] + `boxName'> Probe ` + el[0] + `</td> 
                            </tr>
                            <tr>
                                <td colspan='2'><span id='probe` + el[0] + `'>0</span>&#176;</td>
                            </tr> 
                        </table>
                    </div>
                    `;

                    document.querySelector('.probes-container').appendChild(div);

                    if (index == probeData.length - 1) {

                        loadFromStorage();

                    }

                })
                
                let table2 = document.createElement('table');
                table2.classList = ['config-table'];
                table2.innerHTML = `
                    <tr>
                        <td colspan='2'>
                            Stay connected to Inkbird
                        </td>
                        <td class='right'>
                            <label class='switch' for='sc-checkbox'>
                                <input class='switch-checkbox' type='checkbox' id='sc-checkbox' />
                                <div class='switch-slider'></div>
                            </label>
                        </td>
                    </tr>
                    
                    `;
                document.querySelector('.config-probe-settings').appendChild(table2);

                elementsCreated = true;

                if(!stayConnectedStateLoaded) {

                    document.querySelector("#sc-checkbox").checked = stayConnectedState;
                    stayConnectedStateLoaded = true;

                }

            }

            document.addEventListener('DOMContentLoaded', function() {

                document.querySelector('.probes-container').innerHTML = probesContainerInnerHtml;
                document.querySelector('.icon-github').innerHTML = generateIcon('github', 'small');
                document.querySelector('.menubar').innerHTML = generateIcon('cog', 'big');
                document.querySelector('.button-save').innerHTML = generateIcon('save', 'small') + ' Save Settings';
                document.querySelector('.button-delete').innerHTML = generateIcon('trash', 'small') + ' Delete Settings';
                document.querySelector('.current-year').innerHTML = new Date().getFullYear();

                getData();

            });

            window.onclick = function(event) {

                if (!event.target.matches('.dropbtn') && !event.target.matches('.icon-path') && !event.target.matches('[class*="svg-icon"')) {

                    dropdownHideAll();

                }

            }
        </script>
    </head>
    <body>
        <center>
            <div class='probes-container'></div>
            <div class='config'>
                <div class='config-inner'>
                    <div class='config-probe-settings'></div>
                    <br>
                    <div class='no-settings hide'>
                        NO INKBIRD = NO SETTINGS<br>
                        ¯\_(ツ)_/¯<br>
                    </div>
                    <button class='custom-button button-save' onclick='saveToStorage()'></button>
                    <br>
                    <button class='custom-button button-delete' onclick='clearStorage()'></button>
                    <br><br>
                    <a href='https://github.com/satrik/esp32_ble_inkbird' target='_blank' rel='noopener noreferrer'><span class='icon-github'></span> esp32_ble_inkbird &copy; <span class='current-year'></span> satrik</a>
                    <br><br>
                </div>
            </div>
            <div class='menubar' onclick='menuClicked()'></div>
        </center>
    </body>
</html>
)rawliteral";


uint16_t littleEndianInt(uint8_t *pData) {

    uint16_t val = pData[1] << 8;
    val = val | pData[0];
    return val;

}


// listen for thermometer data
static void notifyCallback(
  BLERemoteCharacteristic *pBLERemoteCharacteristic,
  uint8_t *pData,
  size_t length,
  bool isNotify)
{
  
  int count = 0;  

  jsonData = "[{";
 
    while (count < length) {
      
      uint16_t rawTemp = littleEndianInt(&pData[count]);
      float temp = rawTemp / 10;
      String stringProbeNumber = String(count/2+1);
      String stringTemp = String(temp);
       
      if(count == 0){
        jsonData+= "\""+stringProbeNumber+"\":"+stringTemp;
      } else {
        jsonData+= ",\""+stringProbeNumber+"\":"+stringTemp;
      }

      count += 2;
    
    }

    jsonData+= ",\"battery\":"+stringBattery;

    String stringStayConnected = stayConnected == true ? "1" : "0";
    jsonData+= ",\"stayconnected\":"+stringStayConnected;

    jsonData+= "}]";

}


int getiBBQBatteryPercentage(uint16_t current,double maxVoltage) {

    const uint16_t voltages[] = {5580, 5595, 5609, 5624, 5639, 5644, 5649, 5654, 5661, 5668, 5676, 5683, 5698, 5712, 5727, 5733, 5739, 5744, 5750, 5756, 5759, 5762, 5765, 5768, 5771, 5774, 5777, 5780, 5783, 5786, 5789, 5792, 5795, 5798, 5801, 5807, 5813, 5818, 5824, 5830, 5830, 5830, 5835, 5840, 5845, 5851, 5857, 5864, 5870, 5876, 5882, 5888, 5894, 5900, 5906, 5915, 5924, 5934, 5943, 5952, 5961, 5970, 5980, 5989, 5998, 6007, 6016, 6026, 6035, 6044, 6052, 6062, 6072, 6081, 6090, 6103, 6115, 6128, 6140, 6153, 6172, 6191, 6211, 6230, 6249, 6265, 6280, 6296, 6312, 6328, 6344, 6360, 6370, 6381, 6391, 6407, 6423, 6431, 6439, 6455};
    int length = sizeof(voltages)/sizeof(uint16_t);
    double factor = maxVoltage / 6550.0;

    if (current > voltages[length - 1]*factor) {

        return 100;
    
    }

    if (current <= voltages[0]*factor) {
    
        return 0;
    
    }

    int i = 0;
    
    while ((current > (voltages[i]*factor)) && (i < length)) {

        if ( i > 100) {
            return 100;
        }

        i++;
    
    }

    return i;

}


// listen for battery data
static void notifyResultsCallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{

    switch (pData[0]) {

        case 0x24: {

            uint16_t currentVoltage = littleEndianInt(&pData[1]);
            uint16_t maxVoltage = littleEndianInt(&pData[3]);
        
            maxVoltage = maxVoltage == 0 ? 6550 : maxVoltage;
        
            double battery_percent = getiBBQBatteryPercentage(currentVoltage, maxVoltage);
        
            stringBattery = String((int)battery_percent);
            
            break;
        
        }

        default: {
            break;
        }
  
    }

}


// "say" the thermometer that we want to access the battery data
void getBatteryState() {

  if (pSettingsCharacteristic != nullptr && pSettingsResultsCharacteristic != nullptr) {
  
    pSettingsCharacteristic->writeValue((uint8_t *)batteryLevel, sizeof(batteryLevel), true);
  
  }

}


// generic BLE callbacks. just used to re-/set the connected state
class MyClientCallback : public BLEClientCallbacks {

    void onConnect(BLEClient* pclient) {
        Serial.println("Inkbird connected");
        connected = true;
        batteryRequestCounter = 48;
    }

    void onDisconnect(BLEClient* pclient) {
        Serial.println("Inkbird disconnected");
        connected = false;
    }

};


// connecting to the thermometer
bool connectToBLEServer(BLEAddress pAddress) {

    pClient = BLEDevice::createClient();

    pClient->setClientCallbacks(new MyClientCallback());
    pClient->connect(pAddress);

    BLERemoteService *pRemoteService = pClient->getService(serviceUUID);

    if (pRemoteService == nullptr) {
        return false;
    }

    pAuthCharacteristic = pRemoteService->getCharacteristic(characteristicAuthUUID);

    if (pAuthCharacteristic == nullptr) {
        return false;
    }

    pAuthCharacteristic->writeValue((uint8_t *)enableAccess, sizeof(enableAccess), true);

    pRemoteCharacteristic = pRemoteService->getCharacteristic(characteristicRealTimeDataUUID);

    if (pRemoteCharacteristic == nullptr) {
        return false;
    }

    pSettingsCharacteristic = pRemoteService->getCharacteristic(characteristicSettingsUUID);

    if (pSettingsCharacteristic == nullptr) {
        return false;
    }

    pSettingsCharacteristic->writeValue((uint8_t *)enableRealTimeData, sizeof(enableRealTimeData), true);
    pSettingsCharacteristic->writeValue((uint8_t *)enableUnitCelsius, sizeof(enableUnitCelsius), true);

    pRemoteCharacteristic->registerForNotify(notifyCallback);

    pSettingsResultsCharacteristic = pRemoteService->getCharacteristic(characteristicSettingsResultsUUID);

    if (pSettingsResultsCharacteristic == nullptr) {
        return false;
    }

    pSettingsResultsCharacteristic->registerForNotify(notifyResultsCallback);

    // get the battery data on connection once
    // in the loop() we will request it just every ~60 seconds
    getBatteryState();

    return true;
    
}


// BLE scan results
// as we don't connect directly to the mac address of the thermometer
// we have to scan for the service UUID and connect to the first device which equals the inkbird UUID
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {

    void onResult(BLEAdvertisedDevice advertisedDevice) {

        if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID)) {

            advertisedDevice.getScan()->stop();
            pServerAddress = new BLEAddress(advertisedDevice.getAddress());
            doConnect = true;

        }

    }

};


// init the ESP32 as a BLE device and set some scan parameters
void bleInit() {

    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);

}


// start the BLE scan
void bleDoScan() {
    
    pBLEScan->start(20);
    scanRunning = true;

}


// re-/connect wifi
void handleWifi() {

    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);

    delay(500);
    
    WiFi.mode(WIFI_STA);    
    WiFi.begin(wifiSsid, wifiPw);

    while (WiFi.status() != WL_CONNECTED) {
    
        Serial.print('.');
        delay(500);
    
    }

    MDNS.end();
    if(!MDNS.begin(wifiMdns)) {
        
        Serial.println("Error starting mDNS");
        return;

    }

    Serial.println(" ");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MDNS: ");
    Serial.println(wifiMdns);

}


void handleIndex() {

    // show the main website
    server.send_P(200, "text/html", index_html);
    Serial.println("Handle index");

}

void handleData() {

    // send the raw data as JSON
    server.send(200, "text/json", jsonData);
    disconnectCounter = 12;
    requestConnection = true;
    Serial.println("Handle data");

}


// function to toggle the stayConnected variable
void handleSc() {

    if(server.args() != 0){
    // if "[IP]/sc?toggle=[0/1]" is called, we set the variable to 1=true or 0=false and response with 200/OK
        for (int i = 0; i < server.args(); i++) {

            if(server.argName(i) == "toggle"){

                switch (server.arg(i).toInt()){

                    case 0:
                        stayConnected = false;
                        server.send(200, "text/plain", "OK");
                        break;
                    case 1:
                        stayConnected = true;
                        server.send(200, "text/plain", "OK");
                        break;
                    default:
                    break;

                }

            }

        }

    }
        
    // save the new state into falsh
    preferences.putBool("stayconnected", stayConnected);

}


void setup() {

    Serial.begin(115200);
  
    preferences.begin("inkbird", false); 
  
    stayConnected = preferences.getBool("stayconnected", false);

    handleWifi();
    
    server.on("/", handleIndex); 
    server.on("/data", handleData);
    server.on("/sc", handleSc);
    server.begin();  
    

    bleInit();
        
}


void loop() {

    // check the wifi connection and reconnect if not connected
    if(WiFi.status() != WL_CONNECTED){
    
      wifiErrorCounter++;
    
      if(wifiErrorCounter >= 10) {

        handleWifi();
        wifiErrorCounter = 0;

      }

    } 

    // doConnect is only true after the BLE scan finds an Inkbird device
    // rights afterwards it is false again - also if the scan isn't successfully
    if (doConnect == true) {

        // true if the ESP is connected to the Inkbird
        connected = (connectToBLEServer(*pServerAddress)) ? true : false;
        doConnect = false;

    } else {

        if(!connected) {

            if(!scanRunning) {

                // json connecting leads to the loading animation on the webinterface
                jsonData = "[{\"connecting\": 0}]";

                if(requestConnection) {

                    // if the ESP is not connected, there is no scan ongoing and we requested the connection -> start the ble scan
                    bleDoScan();

                }

            } else {

                 // json error leads to the red error message on the webinterface
                jsonData = "[{\"error\": 0}]";

                if(resetScanRunningCounter >= 0) {

                    // if 0, the scanRunning get set to false again
                    resetScanRunningCounter--;

                }

            }

        } else {

            // requestConnection is true if handleData is called
            if(!requestConnection) {

                // battery state gets requested as last step while connecting to the Inkbird
                // after the first request, we just wait ~60 seconds before we request it again
                if(batteryRequestCounter <= 0) {

                    getBatteryState();
                    // 4/5 times requestConnection is false because handleData() is called every 5 seconds but the loop runs every seconds
                    // so every 5th loop we skip the counter and to get ~60 seconds we calculate like this: 60s / 5 * 4 = 48s  
                    batteryRequestCounter = 48;

                }

                batteryRequestCounter--;  

                // everytime the handleData function is called from the website, the disconnectCounter is 12
                // this will happen every ~5 seconds and we assume the site is closed if it won't happen 2 times in a row (plus ~2 seconds)
                // ===
                // stayConnected is for the case if someone uses just the "/data" of the webserver and not the webinterface
                if(disconnectCounter <= 0 && !stayConnected) {
                    // disconnect the ESP from the Inkbird
                    pClient->disconnect();

                }

            }

        }

    }

    // reset the state. it will be true everytime the handleData function is called
    requestConnection = false;

    // countdown the disconnect "timer"
    if(disconnectCounter >= 0) {

        disconnectCounter--;

    }

    if(resetScanRunningCounter <= 0) {

        // reset the state after a few "saftey" loops
        scanRunning = false;
        resetScanRunningCounter = 3;

    }

    server.handleClient();      

    delay(1000);

}

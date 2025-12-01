/*
  Project: Web UI Testbench (updated) + OTA via AP (Arduino IDE)
  Device: ESP32 + TFT_eSPI + XPT2046 + Servo + DS18B20 + FS300A + Web UI + ArduinoOTA
  Author: taiz - tirta maju jaya cv (base)
  Added: ArduinoOTA support when device runs as AP (so you can update firmware
         from Arduino IDE by connecting your PC to the ESP32 AP).
  Note: Set OTA password below if desired. Connect PC to ESP AP (SSID/TestBench-AP)
        then choose the network port in Arduino IDE (or use espota.py if needed).
  - Wifi AP
  ssid : TestBench-AP
  pass : testbench123
  - OTA AP
  *ota_hostname = "testbench-ota"
  *ota_password = "kopisusu"
*/

#include <TFT_eSPI.h>
#include <ESP32Servo.h>
#include <EEPROM.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoOTA.h>              // <-- added for OTA

// -------- COLOR PALETTE --------
#define COLOR_BG         0x18E3
#define COLOR_PANEL      0x2108
#define COLOR_HEADER     0x03B6
#define COLOR_TEXT       0xFFFF
#define COLOR_ACCENT     0x03B6
#define COLOR_OK         0x03B6
#define COLOR_WARN       0xFBE0
#define COLOR_ERR        0x03B6
#define COLOR_BTN        0x03B6
#define COLOR_BTN_SEL    0x07FF

// -------- PIN MAPPING --------
#define BUTTON_A_PIN 4
#define BUTTON_B_PIN 5
#define LED1_PIN 16
#define LED2_PIN 17
#define SERVO1_PIN 33
#define SERVO2_PIN 13
#define SERVO3_PIN 14
#define BUZZER_PIN 27

// Sensors pins
#define DS18_PIN 22
#define FLOW_PIN 21

// -------- TFT & Touch --------
TFT_eSPI tft = TFT_eSPI();
Servo servo1, servo2, servo3;

#define TOUCH_CS   35
#define TOUCH_IRQ  34
#define TOUCH_CLK  25
#define TOUCH_DOUT 26
#define TOUCH_DIN  32
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// -------- EEPROM ADDRESSES --------
#define EEPROM_ADDR_KLIRING     0
#define EEPROM_ADDR_TUNGGU      4
#define EEPROM_ADDR_KALIBRASI   8
#define EEPROM_ADDR_STABIL      12
#define EEPROM_ADDR_BUZZER      16
#define EEPROM_ADDR_S1_CLOSE    17
#define EEPROM_ADDR_S1_OPEN     18
#define EEPROM_ADDR_S2_CLOSE    19
#define EEPROM_ADDR_S2_OPEN     20
#define EEPROM_ADDR_WIRELESS    21
#define EEPROM_ADDR_S3_CLOSE    22
#define EEPROM_ADDR_S3_OPEN     23
#define EEPROM_ADDR_INPUT_SERVO 24
#define EEPROM_ADDR_INPUT_SPD_CLOSE 25
#define EEPROM_ADDR_INPUT_SPD_OPEN  29
#define EEPROM_ADDR_FLOW_PPL    33
#define EEPROM_ADDR_TEMP_OFFSET 37

// servo degrees
int servoDegree[3][2] = { {30,120}, {30,120}, {30,120} };
int servoPos1 = 0, servoPos2 = 0, servoPos3 = 0;
int inputServo = 1;
unsigned long inputServoSpeedCloseMs = 500;
unsigned long inputServoSpeedOpenMs  = 500;
unsigned long inputServoSpeedCloseTmp = 500;
unsigned long inputServoSpeedOpenTmp  = 500;

// buttons state
bool btnA_last = HIGH, btnB_last = HIGH;
unsigned long btnA_down_time = 0, btnB_down_time = 0;
bool btnA_handled = false, btnB_handled = false;

// state machine
enum State { SIAP_UJI, PERSIAPAN_KLIRING, PROSES_KLIRING, TUNGGU_PROSES, PERSIAPAN_KALIBRASI, PROSES_KALIBRASI, TUNGGU_STABIL };
State state = SIAP_UJI;
unsigned long state_start_time = 0;
bool led1_flash = false, led2_flash = false;
unsigned long LONG_PRESS = 2000;
unsigned long KLIRING = 10000;
unsigned long TUNGGU = 5000;
unsigned long KALIBRASI = 30000;
unsigned long STABIL = 10000;
unsigned long led1_flash_timer = 0, led2_flash_timer = 0;
bool led1_on = false, led2_on = false;

// menu
const char* paramNames[] = {"KLIRING","TUNGGU","KALIBRASI","STABIL"};
unsigned long paramValues[] = {KLIRING,TUNGGU,KALIBRASI,STABIL};
int paramIndex = 0;
enum MenuLevel { NONE_MENU, MAIN_MENU, SETTINGS_MENU, DURATION_MENU, BUZZER_MENU, TEST_SERVO_MENU, WIRELESS_MENU, CONFIRM_RESET_MENU, CONFIRM_REBOOT_MENU, SERVO_DEGREE_MENU, INPUT_SERVO_MENU, INPUT_SERVO_SPEED_MENU, CALIBRATION_MENU };
MenuLevel menuLevel = NONE_MENU;

enum ServoDegreeSettingStep { SD_SELECT_SERVO, SD_SELECT_POS, SD_EDIT_DEGREE };
ServoDegreeSettingStep sdStep = SD_SELECT_SERVO;
int sd_servo_idx = 0, sd_pos_idx = 0, sd_degree_tmp = 0;

const char* mainMenuItems[] = {"Test Bench","Pengaturan"};
int mainMenuIndex = 0;

const char* settingsMenuItems[] = {
  "Test Servo","Set Posisi Servo","Pilih Servo Input","Set Kecepatan Servo Input",
  "Set Durasi Pengujian","Set Buzzer","Kalibrasi Sensor","Wireless","Reboot","Reset EEPROM"
};
int settingsMenuIndex = 0;
const char* testServoMenuItems[] = {"Servo1","Servo2","Servo3"};
int testServoMenuIndex = 0;

unsigned long menuModeStart = 0;
bool dataInputMode = false;
unsigned long dataInputValues[4];
int dataInputIndex = 0;
char lastSerialInput[32] = "";
unsigned long lastSerialInputTime = 0;
const unsigned long SERIAL_DISPLAY_TIME = 5000;
bool infoSavedFlag = false;
unsigned long infoSavedTime = 0;
const unsigned long INFO_SAVED_DISPLAY = 2000;
bool invalidSerialFlag = false;
unsigned long invalidSerialTime = 0;
const unsigned long INVALID_SERIAL_DISPLAY = 2000;
bool buzzerEnabled = true;
bool wirelessEnabled = true;

// virtual buttons
#define BTN_Y 200
#define BTN_H 40
int BTN_W; // calculated after tft init
#define BTN_GAP 0
#define BTN_C_X 0
#define BTN_D_X (BTN_W)
#define BTN_E_X (BTN_W*2)
#define BTN_F_X (BTN_W*3)
volatile bool vBtnC=false, vBtnD=false, vBtnE=false, vBtnF=false;
unsigned long lastTouchTime=0;
bool lastTouch=false;
const unsigned long touchCooldown=200;
const unsigned long DEBOUNCE_DELAY=50;
unsigned long btnA_lastTime=0, btnB_lastTime=0;
bool btnA_stable=HIGH, btnB_stable=HIGH;

// buzzer blink
bool buzzerBlinkActive=false;
unsigned long buzzerBlinkEndTime=0;
unsigned long buzzerToggleTime=0;
bool buzzerState=false;
const unsigned long DEFAULT_BUZZER_TOTAL=5000;
const unsigned long DEFAULT_BUZZER_INTERVAL=500;

// wifi + web
const char *ssid = "TestBench-AP";
const char *password = "testbench123";
AsyncWebServer server(80);
// SSE: event broadcaster
AsyncEventSource events("/events");

bool rebootRequested=false;
unsigned long rebootAt=0;

// OTA config
const char *ota_hostname = "testbench-ota";    // mDNS/hostname for OTA (hostname.local)
const char *ota_password = "kopisusu";      // set a password for OTA uploads (change as desired)

// -------- DS18B20 + Flowmeter --------
OneWire oneWire(DS18_PIN);
DallasTemperature sensors(&oneWire);
float latestTemperatureC = NAN;
DeviceAddress tempDeviceAddress;
unsigned long tempNextReadAt = 0;
const unsigned long TEMP_CONV_MS = 375; // 11-bit ~375ms

// Flowmeter variables
volatile unsigned long flowPulseCount = 0;
volatile unsigned long cumulativePulseCount = 0; // total pulses since last reset
unsigned long lastFlowCalcTime = 0;
unsigned long lastFlowPulseSnapshot = 0;
float flowRateLPerHour = 0.0;
float flowRateSmoothed = 0.0;
const unsigned long FLOW_MEASURE_INTERVAL_MS = 500; // 500ms update
float flowPulsePerLiter = 450.0; // default, calibrate via UI
float tempOffsetC = 0.0;

// Calibration tmp values
float tempOffsetTmp = 0.0;
float flowPPLTmp = 450.0;
int calibrationIndex = 0;
bool calibrationEditing = false;

// ---- global vars for input servo speed menu (used by updateDisplay & handleButtons) ----
int inputServoSpeedIndex = 0;         // 0 = CLOSE->OPEN, 1 = OPEN->CLOSE
bool inputServoSpeedEditing = false;  // true if editing

// Helper EEPROM float read
float readFloatFromEEPROMOrDefault(int addr, float def) {
  byte b0 = EEPROM.read(addr);
  byte b1 = EEPROM.read(addr+1);
  byte b2 = EEPROM.read(addr+2);
  byte b3 = EEPROM.read(addr+3);
  if (b0==0xFF && b1==0xFF && b2==0xFF && b3==0xFF) return def;
  float v;
  byte *p=(byte*)&v;
  p[0]=b0; p[1]=b1; p[2]=b2; p[3]=b3;
  return v;
}

// Forward declarations
void saveFlowCalibrationToEEPROM(float);
void saveTempCalibrationToEEPROM(float);
void resetVolume();
float getTotalVolumeLiters();

// -------- index_html (embedded) - includes SSE client to receive calibration updates ----------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Test Bench Control</title>
<style>
  /* Theme variables (light = default) */
  :root{
    --accent: #0aa;                 /* light accent for light mode */
    --bg: #e9ecef;
    --card: #fff;
    --muted: #666;
    --danger: #d33;
    --ok: #07e0;
    --text: #222;
    --panel-bg: #fafafa;
    --header-bg: #03B6;
    --btn-text: #fff;
    --input-bg: #fff;
    --border: #eee;
    --small-muted: #444;
    --shadow: rgba(0,0,0,0.08);

    /* toggle colors (use variables so we can override per theme) */
    --switch-track: #0aa;
    --switch-track-active: #0aa;
    --switch-knob: #ffffff;
  }

  /* Dark theme override (Arduino IDE–like colors) */
  .dark-theme{
    --accent: #878A88;              /* cyan-blue accent similar to Arduino IDE */
    --bg: #2b2b2b;                  /* editor background like many dark IDEs */
    --card: #232526;                /* card/panel background */
    --muted: #9aa6b2;
    --danger: #ff6b6b;
    --ok: #3be290;
    --text: #e6eef2;
    --panel-bg: #222426;            /* slightly lighter than card for separation */
    --header-bg: #1b2730;
    --btn-text: #0b1116;            /* darker text on bright accent */
    --input-bg: #1b1d1f;
    --border: #3b4040;
    --small-muted: #a8bcc6;
    --shadow: rgba(0,0,0,0.6);

    /* switch / toggle colors for dark theme */
    --switch-track: #25948E;        /* dark track */
    --switch-track-active: #878A88; /* active track = cyan */
    --switch-knob: #ffffff;         /* knob stays white for contrast */
  }

  body { font-family: Arial, Helvetica, sans-serif; background:var(--bg); margin:0; color:var(--text); -webkit-font-smoothing:antialiased; }
  .container { max-width:920px; margin:14px auto; background:var(--card); padding:12px; border-radius:10px; box-shadow:0 2px 10px var(--shadow); }
  h1 { margin:6px 0 12px 0; color:var(--accent); font-size:20px; }

  .top-row { display:flex; align-items:center; gap:8px; margin-bottom:10px; }
  .tabs { display:flex; gap:6px; margin-bottom:12px; flex-wrap:wrap; flex:1; }
  .tab-btn { flex:1; padding:8px 12px; background:var(--panel-bg); border-radius:8px; border:1px solid var(--border); cursor:pointer; text-align:center; font-weight:600; color:var(--accent); }
  .tab-btn.active { background:var(--accent); color:var(--btn-text); box-shadow:0 2px 8px rgba(0,0,0,0.06); }

  .panel { display:none; padding:8px 2px 0 2px; }
  .panel.active { display:block; }

  .row { display:flex; gap:12px; align-items:center; margin-bottom:8px; flex-wrap:wrap; }
  .card { background:var(--panel-bg); border-radius:8px; padding:10px; border:1px solid var(--border); min-width:160px; flex:1; box-sizing:border-box; color:var(--text); }
  .card.large { flex:2; }
  .label { font-size:12px; color:var(--muted); }
  .value { font-size:18px; font-weight:700; margin-top:6px; color:var(--text); }
  .btn{ padding:8px 12px; border-radius:8px; border:none; background:var(--accent); color:var(--btn-text); cursor:pointer; margin-right:6px; }
  .btn.secondary { background:#777; color:#fff; }
  .btn.danger { background:var(--danger); color:#fff; }
  input[type=number], input[type=text], select { padding:6px 8px; border:1px solid var(--border); border-radius:6px; width:110px; background:var(--input-bg); color:var(--text); }
  form .row { align-items:center; }
  .small { font-size:12px; color:var(--small-muted); }
  hr { border:0; border-top:1px solid var(--border); margin:8px 0; }
  .foot { font-size:12px; color:var(--small-muted); text-align:right; margin-top:6px; }
  pre { background:#111; color:#0f0; padding:8px; border-radius:6px; overflow:auto; max-height:140px; }

  @media(max-width:520px){ .row { flex-direction:column; align-items:stretch; } input[type=number], select { width:100%; } }

  .small-label { font-size:12px; color:var(--small-muted); vertical-align:middle; display:inline-block; margin-right:8px; line-height:1.4; }

  .field { display:flex; align-items:center; gap:8px; }
  .field .label-inline { min-width:80px; font-size:12px; color:var(--muted); }
  .field input[type="number"] { width:120px; padding:6px 8px; border-radius:6px; border:1px solid var(--border); background:var(--input-bg); color:var(--text); }
  .controls { display:flex; gap:18px; flex-wrap:wrap; margin-top:12px; align-items:center; }
  .controls .btn { min-width:88px; padding:8px 12px; }

  /* system card small rows */
  .sys-row { display:flex; gap:10px; align-items:center; flex-wrap:wrap; margin-top:6px; }
  .sys-kv { min-width:120px; color:var(--text); font-weight:600; }
  .sys-val { color:var(--text); font-weight:700; }

  /* Theme toggle inside system card - now uses variables for colors */
  .theme-inline { display:flex; align-items:center; gap:10px; margin-left:auto; }
  .theme-label { font-size:13px; color:var(--muted); cursor:pointer; min-width:90px; text-align:right; }
  .switch { width:48px; height:28px; background:var(--switch-track); border-radius:16px; border:1px solid var(--border); position:relative; cursor:pointer; display:inline-block; }
  .knob { width:22px; height:22px; background:var(--switch-knob); border-radius:50%; position:absolute; top:3px; left:3px; transition:left .18s, background .18s; box-shadow:0 1px 3px rgba(0,0,0,0.2); }
  .switch[aria-checked="true"] { background: var(--switch-track-active); border-color: var(--switch-track-active); }
  .switch[aria-checked="true"] .knob { left:25px; }
  .switch:focus { outline:2px solid rgba(0,0,0,0.12); outline-offset:2px; }

</style>
</head>
<body>
<div class="container" id="app">
  <div class="top-row">
    <h1 style="margin:0; flex:1;">Test Bench Control</h1>
    <!-- Removed top theme toggle: now inside Settings -> Sistem card -->
  </div>

  <div class="tabs">
    <div class="tab-btn active" data-tab="main">Utama</div>
    <div class="tab-btn" data-tab="control">Kontrol</div>
    <div class="tab-btn" data-tab="settings">Pengaturan</div>
    <div class="tab-btn" data-tab="cal">Sensor</div>
    <div class="tab-btn" data-tab="test">Test Servo</div>
  </div>

  <!-- MAIN TAB -->
  <div id="main" class="panel active">
    <div class="row">
      <div class="card large">
        <div class="label">Status Mesin</div>
        <div id="state" class="value">--</div>
        <div id="state_time" class="small">--</div>
        <div class="small"></div>
      </div>

      <div class="card">
        <div class="label">Suhu Air (&deg;C)</div>
        <div id="temp" class="value">--</div>
        <div class="small"></div>
      </div>

      <div class="card">
        <div class="label">Debit Air (L/jam)</div>
        <div id="flow" class="value">--</div>
        <div class="small"></div>
      </div>

      <div class="card">
        <div class="label">Volume (L)</div>
        <div id="volume" class="value">--</div>
        <div class="small"></div>
      </div>
    </div>

    <div class="row">
      <button class="btn" onclick="ambil()">Refresh</button>
      <button class="btn secondary" onclick="ambilSensors()">Refresh Sensors</button>
      <button class="btn danger" onclick="resetVolume()">Reset Volume</button>
      <div style="flex:1"></div>
      <div class="small">Realtime update: setiap <span id="pollInterval">500</span> ms</div>
    </div>
    <hr>
  </div>

  <!-- CONTROL TAB -->
  <div id="control" class="panel">
    <div class="row">
      <button class="btn" onclick="aksi('A1')">Persiapan Kliring</button>
      <button class="btn" onclick="aksi('A2')">Proses Kliring</button>
      <button class="btn" onclick="aksi('A3')">Tunggu Proses</button>
      <button class="btn" onclick="aksi('B1')">Persiapan Kalibrasi</button>
      <button class="btn" onclick="aksi('B2')">Proses Kalibrasi</button>
      <button class="btn" onclick="aksi('B3')">Tunggu Stabil</button>
      <button class="btn secondary" onclick="aksi('S')">Siap Uji</button>
    </div>

    <hr>
    <div class="row">
      <div class="card">
        <div class="label">Buzzer</div>
        <div class="small">Status: <span id="buzzer">--</span></div>
        <div style="margin-top:8px;">
          <button class="btn" onclick="toggleBuzzer()">Toggle Buzzer</button>
        </div>
      </div>

      <div class="card">
        <div class="label">Wireless</div>
        <div class="small">Status: <span id="wireless">--</span></div>
        <div style="margin-top:8px;">
          <button class="btn" onclick="toggleWireless()">Toggle Wireless</button>
        </div>
      </div>
    </div>
  </div>

  <!-- SETTINGS TAB -->
  <div id="settings" class="panel">
    <form onsubmit="return kirimData();" style="width:100%;">
      <div class="row">
        <div class="card large">
          <div class="label">Atur Durasi Pengujian (detik)</div>
          <div style="margin-top:8px;">
            <div class="row" style="align-items:flex-start;">
              <div class="field">
                <span class="label-inline">KLIRING</span>
                <input id="kliring" type="number" min="1" value="10">
              </div>
              <div class="field">
                <span class="label-inline">TUNGGU</span>
                <input id="tunggu" type="number" min="1" value="5">
              </div>
              <div class="field">
                <span class="label-inline">KALIBRASI</span>
                <input id="kalibrasi" type="number" min="1" value="30">
              </div>
              <div class="field">
                <span class="label-inline">STABIL</span>
                <input id="stabil" type="number" min="1" value="10">
              </div>
            </div>

            <div style="margin-top:12px;">
              <button class="btn" type="submit">Simpan Durasi</button>
            </div>
          </div>
        </div>
      </div>
    </form>

    <hr>
    <div class="row">
      <div class="card">
        <div class="label">Pilih Servo Input</div>
        <div style="margin-top:8px;">
        <select id="inputServoSelect">
          <option value="1">Servo 1</option>
          <option value="3">Servo 3</option>
        </select>
        <div style="margin-top:8px;"><button class="btn" onclick="setInputServo()">Set</button></div>
        </div>
      </div>

      <div class="card">
        <div class="label">Kecepatan Servo Input (detik)</div>
        <div style="margin-top:8px;">
        <div class="small" style="margin-top:6px;">
          <span class="small-label">CLOSE &rarr; OPEN</span>
          <input id="inputSpeedClose" type="number" min="0.2" max="5" step="0.1"><br>
          <span class="small-label">OPEN &rarr; CLOSE</span>
          <input id="inputSpeedOpen" type="number" min="0.2" max="5" step="0.1">
        </div>
        <div style="margin-top:8px;"><button class="btn" onclick="setInputServoSpeed()">Simpan Kecepatan</button></div>
        </div>
      </div>
    </div>

    <hr>
    <div class="row">
      <div class="card">
        <div class="label">Set Sudut Servo (0&deg;-180&deg;)  *Default: CLOSE 30&deg; - OPEN 120&deg;</div>
        <div style="margin-top:8px">
        <span class="small-label">
          S1 CLOSE <input id="s1c" type="number" min="0" max="180"> S1 OPEN <input id="s1o" type="number" min="0" max="180"><br>
          S2 CLOSE <input id="s2c" type="number" min="0" max="180"> S2 OPEN <input id="s2o" type="number" min="0" max="180"><br>
          S3 CLOSE <input id="s3c" type="number" min="0" max="180"> S3 OPEN <input id="s3o" type="number" min="0" max="180">
        </div>
        <div style="margin-top:8px;"><button class="btn" onclick="setServo()">Simpan Servo</button></div>
      </div>
    </div>

    <hr>
    <!-- System card (now includes Theme toggle) -->
    <div class="row">
      <div class="card large">
        <div class="label">Sistem</div>

        <!-- Theme toggle row inside system card -->
        <div class="sys-row" style="margin-top:8px; align-items:center;">
          <div class="small">Tema :</div>
          <div class="theme-inline" style="margin-left:0px;">
            <div class="theme-label" id="sys_theme_label">Mode: Terang</div>
            <div id="sys_theme_switch" class="switch" role="switch" aria-checked="false" tabindex="0" aria-label="Toggle theme">
              <div class="knob" aria-hidden="true"></div>
            </div>
          </div>
        </div>

        <div class="sys-row" style="margin-top:22px;">
          <button class="btn danger" onclick="confirmResetEEPROM()">Reset EEPROM</button>
          <button class="btn secondary" onclick="confirmReboot()">Reboot System</button>
          <div style="flex:1"></div>
        </div>

        <div class="small" style="margin-top:8px; color:var(--small-muted);">
          Reset EEPROM akan mengembalikan semua pengaturan ke default. Reboot system akan melakukan restart perangkat.
        </div>
      </div>
    </div>

  </div>

  <!-- CALIBRATION TAB -->
  <div id="cal" class="panel">
   <div class="row">
     <div class="card">
       <div class="label">Temp Offset (&deg;C)</div>
       <div style="margin-top:8px;">
       <input id="tempOffset" type="number" step="0.1" min="-10" max="10">
       <div style="margin-top:8px;"><button class="btn" onclick="calTemp()">Simpan Offset</button></div>
       </div>
     </div>
     <div class="card">
       <div class="label">Flow pulses/L</div>
       <div style="margin-top:8px;">
       <input id="flowPPL" type="number" step="1" min="1">
       <div style="margin-top:8px;"><button class="btn" onclick="calFlow()">Simpan Flow</button></div>
       </div>
     </div>
   </div>

   <hr>
   <div class="row">
     <div class="card">
       <div class="label">Volume (L)</div>
       <div style="margin-top:8px;">
       <div id="volume_cal" class="value">--</div>
       
       <div style="margin-top:16px;">
         <div style="margin-top:4px;"><button class="btn danger" onclick="resetVolume()">Reset Volume</button></div>
         </div>
       </div>
       
     </div>
   </div>
 </div>
 
  <!-- TEST SERVO TAB -->
  <div id="test" class="panel">
    <div class="row">
      <div class="card">
        <div class="label">Test Servo 1</div>
        <div class="small">
          <div class="controls">
            <button class="btn" onclick="testServo(1,'c')">S1 CLOSE</button>
            <button class="btn" onclick="testServo(1,'o')">S1 OPEN</button>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="label">Test Servo 2</div>
        <div class="small">
          <div class="controls">
            <button class="btn" onclick="testServo(2,'c')">S2 CLOSE</button>
            <button class="btn" onclick="testServo(2,'o')">S2 OPEN</button>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="label">Test Servo 3</div>
        <div class="small">
          <div class="controls">
            <button class="btn" onclick="testServo(3,'c')">S3 CLOSE</button>
            <button class="btn" onclick="testServo(3,'o')">S3 OPEN</button>
          </div>
        </div>
      </div>
    </div>
  </div>

  <div class="foot">Web UI v1.2.OTA - Realtime <span id="pollIntervalFoot">500</span> ms. *Author Project by Taiz</div>
</div>

<script>
/* Tab handling */
document.querySelectorAll('.tab-btn').forEach(btn=>{
  btn.addEventListener('click', ()=> {
    document.querySelectorAll('.tab-btn').forEach(b=>b.classList.remove('active'));
    document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById(btn.dataset.tab).classList.add('active');
  });
});

// Polling interval (ms)
const POLL_MS = 500;
document.getElementById('pollInterval').innerText = POLL_MS;
document.getElementById('pollIntervalFoot').innerText = POLL_MS;

/* Theme handling placed in Settings -> Sistem card
   Uses localStorage key 'tb_theme' ('light'|'dark').
   Switch element: #sys_theme_switch, label: #sys_theme_label
*/
const THEME_KEY = 'tb_theme';
const themeSwitch = document.getElementById('sys_theme_switch');
const themeLabel = document.getElementById('sys_theme_label');

function applyTheme(theme) {
  if (theme === 'dark') {
    document.documentElement.classList.add('dark-theme');
    themeSwitch.setAttribute('aria-checked','true');
    themeLabel.innerText = 'Mode Gelap';
  } else {
    document.documentElement.classList.remove('dark-theme');
    themeSwitch.setAttribute('aria-checked','false');
    themeLabel.innerText = 'Mode Terang';
  }
  try { localStorage.setItem(THEME_KEY, theme); } catch(e) {}
}

function getPreferredTheme() {
  try {
    const saved = localStorage.getItem(THEME_KEY);
    if (saved) return saved;
  } catch(e) {}
  const prefersDark = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
  return prefersDark ? 'dark' : 'light';
}

function toggleTheme() {
  const current = (themeSwitch.getAttribute('aria-checked') === 'true') ? 'dark' : 'light';
  const next = (current === 'dark') ? 'light' : 'dark';
  applyTheme(next);
}

// Initialize theme on load
applyTheme(getPreferredTheme());

// Make switch & label interactive (click/touch/keyboard)
themeSwitch.addEventListener('click', (e)=>{ e.preventDefault(); toggleTheme(); });
themeSwitch.addEventListener('keydown', (e)=>{ if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); toggleTheme(); } });
themeSwitch.addEventListener('touchstart', (e)=>{ e.preventDefault(); toggleTheme(); }, {passive:false});
themeLabel.addEventListener('click', (e)=>{ e.preventDefault(); toggleTheme(); });

/* --- avoid overwriting inputs while user edits ---
   Track editing state for duration and speed inputs so ambil()
   will not overwrite values being typed.
*/
const durationIds = ['kliring','tunggu','kalibrasi','stabil'];
const editableIds = ['inputSpeedClose','inputSpeedOpen','tempOffset','flowPPL','inputServoSelect'];
const editing = {};
[...durationIds, ...editableIds].forEach(id => editing[id] = false);

function attachEditingHandlers() {
  [...durationIds, ...editableIds].forEach(id => {
    const el = document.getElementById(id);
    if (!el) return;
    el.addEventListener('focus', () => { editing[id] = true; });
    el.addEventListener('input', () => { editing[id] = true; });
    el.addEventListener('blur', () => { setTimeout(() => { editing[id] = false; }, 400); });
  });

  // handlers for inputServoSelect
  const inpServo = document.getElementById('inputServoSelect');
  if (inpServo) {
    inpServo.addEventListener('change', () => { editing['inputServoSelect'] = true; setTimeout(()=>{ editing['inputServoSelect'] = false; }, 600); });
    inpServo.addEventListener('focus', ()=>{ editing['inputServoSelect'] = true; });
    inpServo.addEventListener('blur', ()=>{ setTimeout(()=>{ editing['inputServoSelect'] = false; }, 400); });
  }
}

/* Fetch general info (only update inputs not being edited) */
async function ambil() {
  try {
    const r = await fetch('/info');
    if (!r.ok) return;
    const d = await r.json();

    // readonly / status fields
    document.getElementById('state').innerText = d.state || 'UNK';
    // show remaining time when provided
    const stimeEl = document.getElementById('state_time');
    if (d.state_remaining !== undefined && d.state_duration !== undefined) {
      const rem = parseInt(d.state_remaining);
      const dur = parseInt(d.state_duration);
      if (rem > 0 && dur > 0) {
        stimeEl.innerText = 'Sisa: ' + rem + ' s / ' + dur + ' s';
      } else {
        if (d.state_elapsed !== undefined && d.state_elapsed > 0 && dur > 0) stimeEl.innerText = 'Elapsed: ' + parseInt(d.state_elapsed) + ' s';
        else stimeEl.innerText = '';
      }
    } else {
      if (stimeEl) stimeEl.innerText = '';
    }

    document.getElementById('buzzer').innerText = d.buzzer ? 'ON':'OFF';
    document.getElementById('wireless').innerText = d.wireless ? 'ON':'OFF';
    if (d.inputServo !== undefined) {
      if (!editing['inputServoSelect']) document.getElementById('inputServoSelect').value = d.inputServo;
    }
    // sensors & display
    if (d.temperature !== undefined) {
      document.getElementById('temp').innerText = parseFloat(d.temperature).toFixed(2);
      const disp = document.getElementById('dispTempOffset');
      if (disp) disp.innerText = (d.tempOffset !== undefined) ? parseFloat(d.tempOffset).toFixed(2) : '--';
      const el = document.getElementById('tempOffset');
      if (el && !editing['tempOffset'] && document.activeElement !== el) el.value = (d.tempOffset !== undefined) ? parseFloat(d.tempOffset).toFixed(2) : '';
    }
    if (d.flowrate !== undefined) {
      document.getElementById('flow').innerText = parseFloat(d.flowrate).toFixed(2);
      const disp = document.getElementById('dispPPL');
      if (disp) disp.innerText = (d.flowPPL !== undefined) ? parseFloat(d.flowPPL).toFixed(0) : '--';
      const el = document.getElementById('flowPPL');
      if (el && !editing['flowPPL'] && document.activeElement !== el) el.value = (d.flowPPL !== undefined) ? parseFloat(d.flowPPL).toFixed(0) : '';
    }
    if (d.volume_l !== undefined) {
      const elMain = document.getElementById('volume');
      if (elMain) elMain.innerText = parseFloat(d.volume_l).toFixed(2);
      const elCal = document.getElementById('volume_cal');
      if (elCal) elCal.innerText = parseFloat(d.volume_l).toFixed(2);
      const sysVol = document.getElementById('sys_volume');
      if (sysVol) sysVol.innerText = parseFloat(d.volume_l).toFixed(2);
    }

    // durations: only update if not being edited
    if (d.KLIRING !== undefined && !editing['kliring']) document.getElementById('kliring').value = d.KLIRING;
    if (d.TUNGGU !== undefined  && !editing['tunggu'])  document.getElementById('tunggu').value  = d.TUNGGU;
    if (d.KALIBRASI !== undefined && !editing['kalibrasi']) document.getElementById('kalibrasi').value = d.KALIBRASI;
    if (d.STABIL !== undefined   && !editing['stabil'])   document.getElementById('stabil').value   = d.STABIL;

    // update the combined duration info display
    const di = document.getElementById('dur_info');
    if (di && (d.KLIRING !== undefined || d.TUNGGU !== undefined || d.KALIBRASI !== undefined || d.STABIL !== undefined)) {
      const k = (d.KLIRING !== undefined) ? d.KLIRING : '--';
      const t = (d.TUNGGU !== undefined) ? d.TUNGGU : '--';
      const ka = (d.KALIBRASI !== undefined) ? d.KALIBRASI : '--';
      const s = (d.STABIL !== undefined) ? d.STABIL : '--';
      di.innerText = `KLIRING: ${k} s, TUNGGU: ${t} s, KALIBRASI: ${ka} s, STABIL: ${s} s`;
    }

    // input speeds: only if not editing
    if (d.inputServoSpeedClose !== undefined && !editing['inputSpeedClose']) {
      const el = document.getElementById('inputSpeedClose');
      if (el && document.activeElement !== el) el.value = d.inputServoSpeedClose;
      const sc = document.getElementById('sys_spd_c');
      if (sc) sc.innerText = (d.inputServoSpeedClose !== undefined) ? d.inputServoSpeedClose + ' s' : '-';
    }
    if (d.inputServoSpeedOpen !== undefined && !editing['inputSpeedOpen']) {
      const el = document.getElementById('inputSpeedOpen');
      if (el && document.activeElement !== el) el.value = d.inputServoSpeedOpen;
      const so = document.getElementById('sys_spd_o');
      if (so) so.innerText = (d.inputServoSpeedOpen !== undefined) ? d.inputServoSpeedOpen + ' s' : '-';
    }

    // system card items
    const hostEl = document.getElementById('sys_host');
    const ipEl = document.getElementById('sys_ip');
    if (hostEl) hostEl.innerText = window.location.hostname || '-';
    if (ipEl) ipEl.innerText = window.location.hostname || '-'; // hostname typically shows IP or host
    const sb = document.getElementById('sys_buzzer');
    if (sb) sb.innerText = d.buzzer ? 'ON' : 'OFF';
    const sw = document.getElementById('sys_wireless');
    if (sw) sw.innerText = d.wireless ? 'ON' : 'OFF';
    const sis = document.getElementById('sys_inputservo');
    if (sis) sis.innerText = d.inputServo !== undefined ? d.inputServo : '-';

  } catch (e) {
    console.log('ambil err', e);
  }
}

/* SSE client: listen for calibration broadcasts so Web UI remains sync with TFT saves */
if (!!window.EventSource) {
  const es = new EventSource('/events');
  es.addEventListener('calib', function(evt) {
    try {
      const d = JSON.parse(evt.data);
      if (d.tempOffset !== undefined) {
        const disp = document.getElementById('dispTempOffset');
        if (disp) disp.innerText = parseFloat(d.tempOffset).toFixed(2);
        const tempInput = document.getElementById('tempOffset');
        if (tempInput && document.activeElement !== tempInput) tempInput.value = parseFloat(d.tempOffset).toFixed(2);
      }
      if (d.flowPPL !== undefined) {
        const disp = document.getElementById('dispPPL');
        if (disp) disp.innerText = parseFloat(d.flowPPL).toFixed(0);
        const flowInput = document.getElementById('flowPPL');
        if (flowInput && document.activeElement !== flowInput) flowInput.value = parseFloat(d.flowPPL).toFixed(0);
      }
    } catch (e) {
      console.warn('SSE calib parse error', e);
    }
  });
  es.onerror = function(e) { console.log('SSE connection error', e); };
}

/* Optional smaller sensor-only fetch */
async function ambilSensors() {
  try {
    const r = await fetch('/sensors');
    if (!r.ok) return;
    const d = await r.json();
    if (d.temperature !== undefined) document.getElementById('temp').innerText = parseFloat(d.temperature).toFixed(2);
    if (d.flowrate !== undefined) document.getElementById('flow').innerText = parseFloat(d.flowrate).toFixed(2);
    if (d.volume_l !== undefined) {
      const elMain = document.getElementById('volume');
      if (elMain) elMain.innerText = parseFloat(d.volume_l).toFixed(2);
      const elCal = document.getElementById('volume_cal');
      if (elCal) elCal.innerText = parseFloat(d.volume_l).toFixed(2);
      const sysVol = document.getElementById('sys_volume');
      if (sysVol) sysVol.innerText = parseFloat(d.volume_l).toFixed(2);
    }
  } catch(e) { console.log('ambilSensors err', e); }
}

/* Controls */
function aksi(act) { fetch('/state?set='+act).then(ambil); }
function kirimData() {
  let k=document.getElementById('kliring').value;
  let t=document.getElementById('tunggu').value;
  let ka=document.getElementById('kalibrasi').value;
  let s=document.getElementById('stabil').value;
  fetch(`/set?KLIRING=${k}&TUNGGU=${t}&KALIBRASI=${ka}&STABIL=${s}`).then(ambil);
  return false;
}
function toggleBuzzer() { fetch('/buzzer?toggle=1').then(ambil); }
function toggleWireless() { fetch('/wireless?toggle=1').then(ambil); }
function ambilServo() {
  fetch("/servo").then(r=>r.json()).then(d=>{
    if (d.S1_CLOSE !== undefined) document.getElementById('s1c').value = d.S1_CLOSE;
    if (d.S1_OPEN  !== undefined) document.getElementById('s1o').value = d.S1_OPEN;
    if (d.S2_CLOSE !== undefined) document.getElementById('s2c').value = d.S2_CLOSE;
    if (d.S2_OPEN  !== undefined) document.getElementById('s2o').value = d.S2_OPEN;
    if (d.S3_CLOSE !== undefined) document.getElementById('s3c').value = d.S3_CLOSE;
    if (d.S3_OPEN  !== undefined) document.getElementById('s3o').value = d.S3_OPEN;
  });
}
function setServo() {
  let s1c = document.getElementById('s1c').value;
  let s1o = document.getElementById('s1o').value;
  let s2c = document.getElementById('s2c').value;
  let s2o = document.getElementById('s2o').value;
  let s3c = document.getElementById('s3c').value;
  let s3o = document.getElementById('s3o').value;
  fetch(`/servo?s1c=${s1c}&s1o=${s1o}&s2c=${s2c}&s2o=${s2o}&s3c=${s3c}&s3o=${s3o}`).then(ambilServo);
}

/* TEST SERVO: fallback behavior included */
async function testServo(id, mode) {
  try {
    let setFieldId = "";
    if (id === 1) setFieldId = (mode === 'c') ? 's1c' : 's1o';
    else if (id === 2) setFieldId = (mode === 'c') ? 's2c' : 's2o';
    else if (id === 3) setFieldId = (mode === 'c') ? 's3c' : 's3o';

    let posVal = "";
    const setEl = document.getElementById(setFieldId);
    if (setEl && setEl.value !== "") posVal = setEl.value;

    if (posVal === "" || posVal === undefined || posVal === null) {
      try {
        const r = await fetch('/servo');
        if (r.ok) {
          const d = await r.json();
          if (id === 1) posVal = (mode === 'c') ? d.S1_CLOSE : d.S1_OPEN;
          else if (id === 2) posVal = (mode === 'c') ? d.S2_CLOSE : d.S2_OPEN;
          else if (id === 3) posVal = (mode === 'c') ? d.S3_CLOSE : d.S3_OPEN;
        }
      } catch (err) {
        console.warn('Gagal ambil /servo fallback:', err);
      }
    }

    if (posVal !== "" && posVal !== undefined && posVal !== null) {
      let pnum = parseInt(posVal);
      if (isNaN(pnum)) { alert('Nilai posisi tidak valid'); return; }
      if (pnum < 0) pnum = 0; if (pnum > 180) pnum = 180;
      fetch(`/testservo?id=${id}&pos=${pnum}`).catch(e=>console.warn('testservo err',e));
    } else {
      alert('Tidak ada nilai posisi: isi "Set Sudut Servo" terlebih dahulu atau simpan konfigurasi servo.');
    }
  } catch (e) {
    console.error('testServo error', e);
  }
}

function setInputServo() {
  const el = document.getElementById('inputServoSelect');
  if (!el) return;
  const val = el.value;
  editing['inputServoSelect'] = true;
  fetch(`/inputservo?set=${val}`)
    .then(()=> { setTimeout(()=>{ editing['inputServoSelect'] = false; ambil(); }, 300); })
    .catch((e) => { console.warn('setInputServo err', e); setTimeout(()=>{ editing['inputServoSelect'] = false; }, 300); });
}
function setInputServoSpeed() {
  let vc = parseFloat(document.getElementById('inputSpeedClose').value);
  let vo = parseFloat(document.getElementById('inputSpeedOpen').value);
  if (!isNaN(vc)) { if (vc < 0.2) vc = 0.2; if (vc > 5) vc = 5; fetch(`/inputservospeed?setClose=${vc}`); }
  if (!isNaN(vo)) { if (vo < 0.2) vo = 0.2; if (vo > 5) vo = 5; fetch(`/inputservospeed?setOpen=${vo}`).then(ambil); } else { ambil(); }
}
function resetEEPROM() { if(confirm("Yakin reset semua data?")) fetch("/reset").then(()=>{ambil();ambilServo();}); }
function rebootDevice() { if(confirm("Yakin ingin me-reboot device sekarang?")) { fetch("/reboot").then(()=>{ alert("Perintah reboot dikirim. Perangkat akan melakukan restart."); }); } }
function calTemp() {
  let v = parseFloat(document.getElementById('tempOffset').value);
  if (isNaN(v)) { alert("Masukkan angka valid"); return; }
  fetch(`/calibrate_temp?offset=${v}`).then(ambil);
}
function calFlow() {
  let v = parseFloat(document.getElementById('flowPPL').value);
  if (isNaN(v) || v <= 0) { alert("Masukkan pulses-per-liter valid (angka positif)"); return; }
  fetch(`/calibrate_flow?ppl=${v}`).then(ambil);
}
function resetVolume() {
  if (!confirm("Reset total volume ke 0 L?")) return;
  fetch('/reset_volume').then(()=>{ ambil(); ambilSensors(); alert("Volume di-reset."); });
}

/* Moved Reset & Reboot confirm wrappers */
function confirmResetEEPROM() {
  if (!confirm("Yakin reset semua data ke default?")) return;
  fetch("/reset").then(()=>{ alert("Reset EEPROM: selesai. Memuat ulang data..."); ambil(); ambilServo(); });
}
function confirmReboot() {
  if (!confirm("Yakin ingin me-reboot device sekarang?")) return;
  fetch("/reboot").then(()=>{ alert("Perintah reboot dikirim. Perangkat akan melakukan restart."); });
}

/* initial setup: attach handlers, load current values and start polling */
attachEditingHandlers();
ambil();
ambilServo();
setInterval(ambil, POLL_MS);
</script>
</body>
</html>
)rawliteral";

// -------- Utilities & EEPROM saves ------------
void saveParamsToEEPROM() {
  EEPROM.put(EEPROM_ADDR_KLIRING, KLIRING);
  EEPROM.put(EEPROM_ADDR_TUNGGU, TUNGGU);
  EEPROM.put(EEPROM_ADDR_KALIBRASI, KALIBRASI);
  EEPROM.put(EEPROM_ADDR_STABIL, STABIL);
  EEPROM.commit();
}
void saveBuzzerToEEPROM() { EEPROM.write(EEPROM_ADDR_BUZZER, buzzerEnabled ? 1 : 0); EEPROM.commit(); }
void saveServoDegreeToEEPROM() {
  EEPROM.write(EEPROM_ADDR_S1_CLOSE, servoDegree[0][0]);
  EEPROM.write(EEPROM_ADDR_S1_OPEN, servoDegree[0][1]);
  EEPROM.write(EEPROM_ADDR_S2_CLOSE, servoDegree[1][0]);
  EEPROM.write(EEPROM_ADDR_S2_OPEN, servoDegree[1][1]);
  EEPROM.write(EEPROM_ADDR_S3_CLOSE, servoDegree[2][0]);
  EEPROM.write(EEPROM_ADDR_S3_OPEN, servoDegree[2][1]);
  EEPROM.commit();
}
void saveWirelessToEEPROM() { EEPROM.write(EEPROM_ADDR_WIRELESS, wirelessEnabled ? 1 : 0); EEPROM.commit(); }
void saveInputServoToEEPROM() { EEPROM.write(EEPROM_ADDR_INPUT_SERVO, inputServo); EEPROM.commit(); }
void saveInputServoSpeedsToEEPROM() {
  EEPROM.put(EEPROM_ADDR_INPUT_SPD_CLOSE, inputServoSpeedCloseMs);
  EEPROM.put(EEPROM_ADDR_INPUT_SPD_OPEN,  inputServoSpeedOpenMs);
  EEPROM.commit();
}
void saveFlowCalibrationToEEPROM(float pulsesPerLiter) { EEPROM.put(EEPROM_ADDR_FLOW_PPL, pulsesPerLiter); EEPROM.commit(); }
void saveTempCalibrationToEEPROM(float offsetC) { EEPROM.put(EEPROM_ADDR_TEMP_OFFSET, offsetC); EEPROM.commit(); }

void loadParamsFromEEPROM() {
  EEPROM.get(EEPROM_ADDR_KLIRING, KLIRING);
  EEPROM.get(EEPROM_ADDR_TUNGGU, TUNGGU);
  EEPROM.get(EEPROM_ADDR_KALIBRASI, KALIBRASI);
  EEPROM.get(EEPROM_ADDR_STABIL, STABIL);
  if (KLIRING==0xFFFFFFFF || KLIRING==0) KLIRING=10000;
  if (TUNGGU==0xFFFFFFFF || TUNGGU==0) TUNGGU=5000;
  if (KALIBRASI==0xFFFFFFFF || KALIBRASI==0) KALIBRASI=30000;
  if (STABIL==0xFFFFFFFF || STABIL==0) STABIL=10000;
  paramValues[0]=KLIRING; paramValues[1]=TUNGGU; paramValues[2]=KALIBRASI; paramValues[3]=STABIL;
  byte bz = EEPROM.read(EEPROM_ADDR_BUZZER);
  if (bz==0xFF) buzzerEnabled=true; else buzzerEnabled = (bz==1);
  int s1c = EEPROM.read(EEPROM_ADDR_S1_CLOSE);
  int s1o = EEPROM.read(EEPROM_ADDR_S1_OPEN);
  int s2c = EEPROM.read(EEPROM_ADDR_S2_CLOSE);
  int s2o = EEPROM.read(EEPROM_ADDR_S2_OPEN);
  int s3c = EEPROM.read(EEPROM_ADDR_S3_CLOSE);
  int s3o = EEPROM.read(EEPROM_ADDR_S3_OPEN);
  if (s1c>=0 && s1c<=180) servoDegree[0][0]=s1c;
  if (s1o>=0 && s1o<=180) servoDegree[0][1]=s1o;
  if (s2c>=0 && s2c<=180) servoDegree[1][0]=s2c;
  if (s2o>=0 && s2o<=180) servoDegree[1][1]=s2o;
  if (s3c>=0 && s3c<=180) servoDegree[2][0]=s3c;
  if (s3o>=0 && s3o<=180) servoDegree[2][1]=s3o;
  byte wl = EEPROM.read(EEPROM_ADDR_WIRELESS);
  if (wl==0xFF) wirelessEnabled=true; else wirelessEnabled = (wl==1);
  byte inp = EEPROM.read(EEPROM_ADDR_INPUT_SERVO);
  if (inp==0xFF) inputServo=1; else { if (inp==1 || inp==3) inputServo=inp; else inputServo=1; }
  unsigned long spdC=0xFFFFFFFFUL, spdO=0xFFFFFFFFUL;
  EEPROM.get(EEPROM_ADDR_INPUT_SPD_CLOSE, spdC);
  EEPROM.get(EEPROM_ADDR_INPUT_SPD_OPEN, spdO);
  if (spdC==0xFFFFFFFFUL || spdC==0) inputServoSpeedCloseMs=500; else if (spdC>=200 && spdC<=5000) inputServoSpeedCloseMs=spdC; else inputServoSpeedCloseMs=500;
  if (spdO==0xFFFFFFFFUL || spdO==0) inputServoSpeedOpenMs=500; else if (spdO>=200 && spdO<=5000) inputServoSpeedOpenMs=spdO; else inputServoSpeedOpenMs=500;
  inputServoSpeedCloseTmp = inputServoSpeedCloseMs;
  inputServoSpeedOpenTmp  = inputServoSpeedOpenMs;
  flowPulsePerLiter = readFloatFromEEPROMOrDefault(EEPROM_ADDR_FLOW_PPL, 450.0);
  tempOffsetC = readFloatFromEEPROMOrDefault(EEPROM_ADDR_TEMP_OFFSET, 0.0);
}

// reset defaults
void resetEEPROMToDefault() {
  KLIRING=10000; TUNGGU=5000; KALIBRASI=30000; STABIL=10000;
  buzzerEnabled=true;
  servoDegree[0][0]=30; servoDegree[0][1]=120;
  servoDegree[1][0]=30; servoDegree[1][1]=120;
  servoDegree[2][0]=30; servoDegree[2][1]=120;
  wirelessEnabled=true; inputServo=1;
  inputServoSpeedCloseMs=500; inputServoSpeedOpenMs=500;
  flowPulsePerLiter=450.0; tempOffsetC=0.0;
  saveParamsToEEPROM(); saveBuzzerToEEPROM(); saveServoDegreeToEEPROM(); saveWirelessToEEPROM();
  saveInputServoToEEPROM(); saveInputServoSpeedsToEEPROM();
  saveFlowCalibrationToEEPROM(flowPulsePerLiter);
  saveTempCalibrationToEEPROM(tempOffsetC);
}

// wireless apply
void applyWirelessStatus() {
  if (wirelessEnabled) {
    // Use AP + STA mode so OTA (mDNS) and webserver can be reachable when connected to AP
    if (WiFi.getMode() != WIFI_AP_STA) {
      WiFi.mode(WIFI_AP_STA);
      // Start SoftAP (ESP creates its own network)
      WiFi.softAP(ssid,password);
      Serial.print("SoftAP started. IP (AP): ");
      Serial.println(WiFi.softAPIP());
    } else {
      // ensure softAP is running
      if (WiFi.softAPgetStationNum() >= 0) {
        // nothing to do here
      }
    }
    // Optional: keep STA disconnected (no WiFi.begin) — user must connect PC to this AP
  } else {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
  }
}

// servo movement (non-blocking)
bool inputServoMoving=false;
unsigned long inputServoMoveStartTime=0;
unsigned long inputServoMoveDuration=0;
int inputServoMoveStartPos=0, inputServoMoveTargetPos=0;

void startInputServoMove(int targetPos, unsigned long durationMs) {
  targetPos = constrain(targetPos,0,180);
  durationMs = constrain(durationMs,200,5000);
  int currentPos = (inputServo==1) ? servoPos1 : servoPos3;
  if (currentPos == targetPos) {
    if (inputServo==1) { servo1.write(targetPos); servoPos1=targetPos; } else { servo3.write(targetPos); servoPos3=targetPos; }
    inputServoMoving=false; return;
  }
  if (inputServoMoving && inputServoMoveTargetPos==targetPos) return;
  inputServoMoving=true; inputServoMoveStartTime=millis(); inputServoMoveDuration=durationMs;
  inputServoMoveStartPos=currentPos; inputServoMoveTargetPos=targetPos;
}
void handleInputServoMovement() {
  if (!inputServoMoving) return;
  unsigned long now=millis();
  long elapsed=(long)(now-inputServoMoveStartTime);
  if (elapsed >= (long)inputServoMoveDuration) {
    if (inputServo==1) { servo1.write(inputServoMoveTargetPos); servoPos1=inputServoMoveTargetPos; } else { servo3.write(inputServoMoveTargetPos); servoPos3=inputServoMoveTargetPos; }
    inputServoMoving=false; return;
  }
  float t = (float)elapsed / (float)inputServoMoveDuration;
  int newPos = inputServoMoveStartPos + (int)round((inputServoMoveTargetPos - inputServoMoveStartPos) * t);
  newPos = constrain(newPos,0,180);
  if (inputServo==1) { servo1.write(newPos); servoPos1=newPos; } else { servo3.write(newPos); servoPos3=newPos; }
}
void setServoPositions(int sInputPos, int s2pos) {
  sInputPos = constrain(sInputPos,0,180);
  s2pos = constrain(s2pos,0,180);
  int idx = (inputServo==1) ? 0 : 2;
  int closePos = servoDegree[idx][0], openPos = servoDegree[idx][1];
  int curInputPos = (inputServo==1) ? servoPos1 : servoPos3;
  unsigned long dur = inputServoSpeedCloseMs;
  if (curInputPos == closePos && sInputPos == openPos) dur = inputServoSpeedCloseMs;
  else if (curInputPos == openPos && sInputPos == closePos) dur = inputServoSpeedOpenMs;
  else { if (sInputPos == openPos) dur = inputServoSpeedCloseMs; else if (sInputPos == closePos) dur = inputServoSpeedOpenMs; else dur = inputServoSpeedCloseMs; }
  if (sInputPos != curInputPos) startInputServoMove(sInputPos, dur);
  servo2.write(s2pos); servoPos2 = s2pos;
}
void setPhysicalServoPos(int id, int pos) {
  pos = constrain(pos,0,180);
  if (id==1) { servo1.write(pos); servoPos1=pos; }
  else if (id==2) { servo2.write(pos); servoPos2=pos; }
  else if (id==3) { servo3.write(pos); servoPos3=pos; }
}

// BUZZER
void startBlinkBuzzer(unsigned long totalDurasiMs = DEFAULT_BUZZER_TOTAL, unsigned long intervalMs = DEFAULT_BUZZER_INTERVAL) {
  if (!buzzerEnabled) return;
  buzzerBlinkActive=true; buzzerBlinkEndTime=millis()+totalDurasiMs; buzzerToggleTime=millis()+intervalMs; buzzerState=true; digitalWrite(BUZZER_PIN,HIGH);
}
void stopBlinkBuzzer() { buzzerBlinkActive=false; buzzerState=false; digitalWrite(BUZZER_PIN,LOW); }
void handleBlinkBuzzer(unsigned long intervalMs = DEFAULT_BUZZER_INTERVAL) {
  if (!buzzerBlinkActive) return;
  unsigned long now=millis();
  if ((long)(now - buzzerBlinkEndTime) >= 0) { stopBlinkBuzzer(); return; }
  if ((long)(now - buzzerToggleTime) >= 0) { buzzerState = !buzzerState; digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW); buzzerToggleTime = now + intervalMs; }
}

// ISR for flow pulses
void IRAM_ATTR flowPulseISR() { flowPulseCount++; cumulativePulseCount++; }

// Flow update every 500ms
void updateFlowMeasurement() {
  unsigned long now = millis();
  unsigned long interval = now - lastFlowCalcTime;
  if (interval >= FLOW_MEASURE_INTERVAL_MS) {
    noInterrupts(); unsigned long pulses = flowPulseCount; interrupts();
    unsigned long deltaP = pulses - lastFlowPulseSnapshot;
    lastFlowPulseSnapshot = pulses;
    lastFlowCalcTime = now;
    float pps = (float)deltaP / ((float)interval / 1000.0);
    float rateLh = 0.0;
    if (flowPulsePerLiter > 0.0f) rateLh = (pps * 3600.0f) / flowPulsePerLiter;
    flowRateSmoothed = (flowRateSmoothed * 0.7f) + (rateLh * 0.3f);
    flowRateLPerHour = flowRateSmoothed;
  }
}

// DS18B20 non-blocking update
void updateTemperature() {
  unsigned long now = millis();
  if ((long)(now - tempNextReadAt) >= 0) {
    float raw = sensors.getTempCByIndex(0);
    if (raw != DEVICE_DISCONNECTED_C) latestTemperatureC = raw + tempOffsetC;
    sensors.requestTemperatures(); tempNextReadAt = millis() + TEMP_CONV_MS;
  }
}

// Volume helpers
float getTotalVolumeLiters() {
  if (flowPulsePerLiter <= 0.0f) return 0.0f;
  noInterrupts(); unsigned long pulses = cumulativePulseCount; interrupts();
  return ((float)pulses) / flowPulsePerLiter;
}
void resetVolume() {
  noInterrupts(); cumulativePulseCount = 0; interrupts();
  infoSavedFlag = true; infoSavedTime = millis();
}

// ---- API / Web handlers ----
void handleAPI() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200,"text/html",index_html); });

  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request){
    // compute state duration & remaining
    unsigned long durationMs = 0;
    unsigned long remainingMs = 0;
    unsigned long elapsedMs = 0;
    unsigned long now = millis();
    switch(state){
      case PROSES_KLIRING: durationMs = KLIRING; break;
      case TUNGGU_PROSES: durationMs = TUNGGU; break;
      case PROSES_KALIBRASI: durationMs = KALIBRASI; break;
      case TUNGGU_STABIL: durationMs = STABIL; break;
      default: durationMs = 0; break;
    }
    if (durationMs > 0) {
      long diff = (long)(now - state_start_time);
      if (diff < 0) diff = 0;
      elapsedMs = (unsigned long)diff;
      if ((unsigned long)diff >= durationMs) remainingMs = 0;
      else remainingMs = durationMs - (unsigned long)diff;
    } else {
      elapsedMs = 0; remainingMs = 0;
    }

    String json = "{";
    json += "\"state\":\"";
    switch(state){
      case SIAP_UJI: json+="SIAP UJI - READY";break;
      case PERSIAPAN_KLIRING: json+="PERSIAPAN KLIRING";break;
      case PROSES_KLIRING: json+="PROSES KLIRING";break;
      case TUNGGU_PROSES: json+="TUNGGU PROSES";break;
      case PERSIAPAN_KALIBRASI: json+="PERSIAPAN KALIBRASI";break;
      case PROSES_KALIBRASI: json+="PROSES KALIBRASI";break;
      case TUNGGU_STABIL: json+="TUNGGU STABIL";break;
      default: json+="UNK";break;
    }
    json += "\",";
    json += "\"state_duration\":" + String(durationMs/1000) + ",";    // total duration in seconds (0 if none)
    json += "\"state_remaining\":" + String(remainingMs/1000) + ",";  // remaining seconds
    json += "\"state_elapsed\":" + String(elapsedMs/1000) + ",";      // elapsed seconds since start (0 if none)
    json += "\"KLIRING\":" + String(KLIRING/1000) + ",";
    json += "\"TUNGGU\":" + String(TUNGGU/1000) + ",";
    json += "\"KALIBRASI\":" + String(KALIBRASI/1000) + ",";
    json += "\"STABIL\":" + String(STABIL/1000) + ",";
    json += "\"buzzer\":" + String(buzzerEnabled ? "true":"false") + ",";
    json += "\"wireless\":" + String(wirelessEnabled ? "true":"false") + ",";
    json += "\"inputServo\":" + String(inputServo) + ",";
    json += "\"inputServoSpeedClose\":" + String((float)inputServoSpeedCloseMs/1000.0) + ",";
    json += "\"inputServoSpeedOpen\":"  + String((float)inputServoSpeedOpenMs/1000.0) + ",";
    json += "\"temperature\":" + String(isnan(latestTemperatureC) ? 0.0 : latestTemperatureC) + ",";
    json += "\"flowrate\":" + String(flowRateLPerHour) + ",";
    json += "\"tempOffset\":" + String(tempOffsetC) + ",";
    json += "\"flowPPL\":" + String(flowPulsePerLiter) + ",";
    json += "\"volume_l\":" + String(getTotalVolumeLiters(), 2);
    json += "}";
    request->send(200,"application/json",json);
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("KLIRING")) KLIRING = request->getParam("KLIRING")->value().toInt() * 1000;
    if(request->hasParam("TUNGGU")) TUNGGU = request->getParam("TUNGGU")->value().toInt() * 1000;
    if(request->hasParam("KALIBRASI")) KALIBRASI = request->getParam("KALIBRASI")->value().toInt() * 1000;
    if(request->hasParam("STABIL")) STABIL = request->getParam("STABIL")->value().toInt() * 1000;
    paramValues[0]=KLIRING; paramValues[1]=TUNGGU; paramValues[2]=KALIBRASI; paramValues[3]=STABIL;
    saveParamsToEEPROM();
    request->send(200,"text/plain","OK");
  });

  server.on("/buzzer", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("toggle")) { buzzerEnabled = !buzzerEnabled; saveBuzzerToEEPROM(); }
    request->send(200,"text/plain", buzzerEnabled ? "ON":"OFF");
  });

  server.on("/wireless", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("toggle")) { wirelessEnabled = !wirelessEnabled; saveWirelessToEEPROM(); applyWirelessStatus(); }
    request->send(200,"text/plain", wirelessEnabled ? "ON":"OFF");
  });

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("set")) {
      String v = request->getParam("set")->value();
      if(v=="S") { state = SIAP_UJI; menuLevel=NONE_MENU; state_start_time=millis(); led1_flash=false; led2_flash=false; digitalWrite(LED1_PIN,LOW); digitalWrite(LED2_PIN,LOW); setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][1]); }
      else if(v=="A1") state = PERSIAPAN_KLIRING;
      else if(v=="A2") { state = PROSES_KLIRING; state_start_time=millis(); }
      else if(v=="A3") { state = TUNGGU_PROSES; state_start_time=millis(); }
      else if(v=="B1") state = PERSIAPAN_KALIBRASI;
      else if(v=="B2") { state = PROSES_KALIBRASI; state_start_time=millis(); }
      else if(v=="B3") { state = TUNGGU_STABIL; state_start_time=millis(); }
    }
    request->send(200,"text/plain","OK");
  });

  server.on("/servo", HTTP_GET, [](AsyncWebServerRequest *request){
    bool updated=false;
    if(request->hasParam("s1c")) { servoDegree[0][0]=request->getParam("s1c")->value().toInt(); updated=true; }
    if(request->hasParam("s1o")) { servoDegree[0][1]=request->getParam("s1o")->value().toInt(); updated=true; }
    if(request->hasParam("s2c")) { servoDegree[1][0]=request->getParam("s2c")->value().toInt(); updated=true; }
    if(request->hasParam("s2o")) { servoDegree[1][1]=request->getParam("s2o")->value().toInt(); updated=true; }
    if(request->hasParam("s3c")) { servoDegree[2][0]=request->getParam("s3c")->value().toInt(); updated=true; }
    if(request->hasParam("s3o")) { servoDegree[2][1]=request->getParam("s3o")->value().toInt(); updated=true; }
    if(updated) saveServoDegreeToEEPROM();
    String resp = "{";
    resp += "\"S1_CLOSE\":" + String(servoDegree[0][0]) + ",";
    resp += "\"S1_OPEN\":"  + String(servoDegree[0][1]) + ",";
    resp += "\"S2_CLOSE\":" + String(servoDegree[1][0]) + ",";
    resp += "\"S2_OPEN\":"  + String(servoDegree[1][1]) + ",";
    resp += "\"S3_CLOSE\":" + String(servoDegree[2][0]) + ",";
    resp += "\"S3_OPEN\":"  + String(servoDegree[2][1]);
    resp += "}";
    request->send(200,"application/json",resp);
  });

  server.on("/testservo", HTTP_GET, [](AsyncWebServerRequest *request){
   int id = request->hasParam("id") ? request->getParam("id")->value().toInt() : 0;
   int pos = request->hasParam("pos") ? request->getParam("pos")->value().toInt() : 0;
   pos = constrain(pos, 0, 180);
   Serial.printf("HTTP /testservo called -> id=%d pos=%d\n", id, pos);

   // set the servo directly
   if (id == 1) {
   servo1.write(pos);
   servoPos1 = pos;
   Serial.printf("Moved servo1 to %d\n", pos);
   } else if (id == 2) {
   servo2.write(pos);
   servoPos2 = pos;
   Serial.printf("Moved servo2 to %d\n", pos);
   } else if (id == 3) {
   servo3.write(pos);
   servoPos3 = pos;
   Serial.printf("Moved servo3 to %d\n", pos);
   } else {
   Serial.printf("Invalid servo id: %d\n", id);
   }

   // Prevent state machine from immediately overwriting:
   // enter TEST_SERVO_MENU so handleStateMachine() will return early
   menuLevel = TEST_SERVO_MENU;
   testServoMenuIndex = (id >= 1 && id <= 3) ? (id - 1) : 0;
   menuModeStart = millis();

   request->send(200, "text/plain", "OK");
  });

  server.on("/testservomenu", HTTP_GET, [](AsyncWebServerRequest *request){
    if (state == SIAP_UJI) { menuLevel = TEST_SERVO_MENU; testServoMenuIndex = 0; menuModeStart = millis(); request->send(200,"text/plain","OK"); }
    else request->send(403,"text/plain","Not allowed");
  });

  server.on("/inputservo", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("set")) {
      String v = request->getParam("set")->value(); int val = v.toInt();
      if (val==1 || val==3) { inputServo = val; saveInputServoToEEPROM(); }
    }
    request->send(200,"text/plain", String(inputServo).c_str());
  });

  server.on("/inputservospeed", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("setClose")) {
      float v = request->getParam("setClose")->value().toFloat();
      if (v<0.2) v=0.2; if (v>5.0) v=5.0;
      inputServoSpeedCloseMs = (unsigned long)round(v*1000.0); saveInputServoSpeedsToEEPROM();
    }
    if(request->hasParam("setOpen")) {
      float v = request->getParam("setOpen")->value().toFloat();
      if (v<0.2) v=0.2; if (v>5.0) v=5.0;
      inputServoSpeedOpenMs = (unsigned long)round(v*1000.0); saveInputServoSpeedsToEEPROM();
    }
    String resp = "{";
    resp += "\"close\":" + String((float)inputServoSpeedCloseMs/1000.0) + ",";
    resp += "\"open\":"  + String((float)inputServoSpeedOpenMs/1000.0);
    resp += "}";
    request->send(200,"application/json",resp);
  });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    resetEEPROMToDefault(); applyWirelessStatus(); request->send(200,"text/plain","RESET OK");
  });

  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
    rebootRequested = true; rebootAt = millis() + 200; request->send(200,"text/plain","REBOOTING");
  });

  server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request){
    String resp = "{";
    resp += "\"temperature\":" + String(isnan(latestTemperatureC) ? 0.0 : latestTemperatureC) + ",";
    resp += "\"flowrate\":" + String(flowRateLPerHour) + ",";
    resp += "\"tempOffset\":" + String(tempOffsetC) + ",";
    resp += "\"flowPPL\":" + String(flowPulsePerLiter) + ",";
    resp += "\"volume_l\":" + String(getTotalVolumeLiters(), 2);
    resp += "}";
    request->send(200,"application/json",resp);
  });

  server.on("/calibrate_temp", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("offset")) {
      float v = request->getParam("offset")->value().toFloat();
      tempOffsetC = v; saveTempCalibrationToEEPROM(tempOffsetC);
      // broadcast to web clients
      String payload = String("{\"tempOffset\":") + String(tempOffsetC, 4) + String(",\"flowPPL\":") + String(flowPulsePerLiter, 2) + String("}");
      events.send(payload.c_str(), "calib", millis());
    }
    request->send(200,"text/plain",String(tempOffsetC).c_str());
  });

  server.on("/calibrate_flow", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("ppl")) {
      float v = request->getParam("ppl")->value().toFloat();
      if (v > 0.0f) { flowPulsePerLiter = v; saveFlowCalibrationToEEPROM(flowPulsePerLiter);
        // broadcast to web clients
        String payload = String("{\"tempOffset\":") + String(tempOffsetC, 4) + String(",\"flowPPL\":") + String(flowPulsePerLiter, 2) + String("}");
        events.send(payload.c_str(), "calib", millis());
      }
    }
    request->send(200,"text/plain",String(flowPulsePerLiter).c_str());
  });

  server.on("/reset_volume", HTTP_GET, [](AsyncWebServerRequest *request){
    resetVolume(); request->send(200,"text/plain","OK");
  });

  // register SSE handler
  server.addHandler(&events);
}

// -------- Display helpers (TFT) --------
void drawModernButton(int x,int y,const char *label,uint16_t btnColor,uint16_t textColor,bool selected=false) {
  tft.fillRoundRect(x,y,BTN_W,BTN_H,8,btnColor);
  if (selected) tft.drawRoundRect(x,y,BTN_W,BTN_H,8,COLOR_BTN_SEL);
  tft.setTextColor(textColor,btnColor);
  tft.setFreeFont(&FreeSansBold9pt7b);
  int tw = tft.textWidth(label);
  tft.setCursor(x + (BTN_W-tw)/2, y + 28);
  tft.print(label);
}
void drawModernButtons(int selectedIdx=-1) {
  uint16_t lineColor = TFT_WHITE; int lineThickness = 2;
  tft.fillRect(BTN_C_X, BTN_Y, BTN_W, BTN_H, COLOR_BTN);
  tft.setTextColor(TFT_WHITE,COLOR_BTN);
  tft.setFreeFont(&FreeMonoBold9pt7b);
  tft.setCursor(BTN_C_X + BTN_W / 2 - tft.textWidth("+") / 2, BTN_Y + 25); tft.print("+");
  tft.fillRect(BTN_W - lineThickness / 2, BTN_Y, lineThickness, BTN_H, lineColor);
  tft.fillRect(BTN_D_X, BTN_Y, BTN_W, BTN_H, COLOR_BTN);
  tft.setCursor(BTN_D_X + BTN_W / 2 - tft.textWidth("-") / 2, BTN_Y + 25); tft.print("-");
  tft.fillRect(BTN_W * 2 - lineThickness / 2, BTN_Y, lineThickness, BTN_H, lineColor);
  tft.fillRect(BTN_E_X, BTN_Y, BTN_W, BTN_H, COLOR_OK);
  tft.setCursor(BTN_E_X + BTN_W / 2 - tft.textWidth("M") / 2, BTN_Y + 25); tft.print("M");
  tft.fillRect(BTN_W * 3 - lineThickness / 2, BTN_Y, lineThickness, BTN_H, lineColor);
  tft.fillRect(BTN_F_X, BTN_Y, BTN_W, BTN_H, COLOR_ERR);
  tft.setCursor(BTN_F_X + BTN_W / 2 - tft.textWidth("B") / 2, BTN_Y + 25); tft.print("B");
}

void drawModernHeader(const char *title) {
  static char prevHeader[32] = "";
  if (strcmp(prevHeader,title)==0) return;
  strncpy(prevHeader,title,sizeof(prevHeader)-1); prevHeader[sizeof(prevHeader)-1]=0;
  tft.fillRect(0,0,tft.width(),40,COLOR_HEADER);
  tft.setTextColor(TFT_YELLOW,COLOR_HEADER);
  tft.setFreeFont(&FreeMonoBold12pt7b);
  int w = tft.textWidth(title);
  int x = (tft.width()-w)/2;
  tft.setCursor(x,28); tft.print(title);
}
void drawModernPanel(int y,const char *line0,const char *line1) {
  static char prevLine0[32]="", prevLine1[32]="";
  if (strcmp(prevLine0,line0)==0 && strcmp(prevLine1,line1)==0) return;
  strncpy(prevLine0,line0,sizeof(prevLine0)-1); prevLine0[sizeof(prevLine0)-1]=0;
  strncpy(prevLine1,line1,sizeof(prevLine1)-1); prevLine1[sizeof(prevLine1)-1]=0;
  tft.fillRect(0,y,tft.width(),70,COLOR_PANEL);
  tft.setTextColor(COLOR_TEXT,COLOR_PANEL);
  tft.setFreeFont(&FreeMonoBold12pt7b);
  int w0 = tft.textWidth(line0), x0=(tft.width()-w0)/2;
  tft.setCursor(x0,y+28); tft.print(line0);
  tft.setFreeFont(&FreeMono9pt7b);
  int w1 = tft.textWidth(line1), x1=(tft.width()-w1)/2;
  tft.setCursor(x1,y+57); tft.print(line1);
}

void drawProgressBar(int x,int y,int w,int h,float progress,uint16_t barColor,uint16_t bgColor) {
  tft.drawRoundRect(x,135,w,25,6,COLOR_WARN);
  tft.fillRoundRect(x+2,135+2,w-4,25-4,4,bgColor);
  int pw = (int)((w-4)*progress);
  tft.fillRoundRect(x+2,135+2,pw,25-4,4,barColor);
}

void checkVirtualButton() {
  bool nowTouch = ts.touched();
  unsigned long now = millis();
  if (nowTouch && !lastTouch && now - lastTouchTime > touchCooldown) {
    lastTouchTime = now;
    TS_Point p = ts.getPoint();
    int x = map(p.x,320,3890,0,tft.width());
    int y = map(p.y,410,3920,0,tft.height());
    y = tft.height() - y;
    if (y > BTN_Y && y < BTN_Y + BTN_H) {
      if (x > BTN_C_X && x < BTN_C_X + BTN_W) vBtnC=true;
      else if (x > BTN_D_X && x < BTN_D_X + BTN_W) vBtnD=true;
      else if (x > BTN_E_X && x < BTN_E_X + BTN_W) vBtnE=true;
      else if (x > BTN_F_X && x < BTN_F_X + BTN_W) vBtnF=true;
    }
  }
  lastTouch = nowTouch;
}

void updateDisplay() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 100) return;
  lastUpdate = millis();
  char line0[32]="", line1[32]="";
  if (infoSavedFlag) { strncpy(line0,"Data Tersimpan!",31); strncpy(line1," ",31); if (millis()-infoSavedTime > INFO_SAVED_DISPLAY) infoSavedFlag=false; }
  else if (invalidSerialFlag) { strncpy(line0,"SERIAL SALAH",31); strncpy(line1,"INPUT!",31); if (millis()-invalidSerialTime > INVALID_SERIAL_DISPLAY) invalidSerialFlag=false; }
  else if (menuLevel == CONFIRM_RESET_MENU) { strncpy(line0,"Yakin reset ??",31); strncpy(line1,"M:Ya  B:Tidak",31); }
  else if (menuLevel == CONFIRM_REBOOT_MENU) { strncpy(line0,"Yakin reboot ??",31); strncpy(line1,"M:Ya  B:Tidak",31); }
  else if (dataInputMode) { snprintf(line0,31,"Input %s",paramNames[dataInputIndex]); snprintf(line1,31,"%lus +/-", dataInputValues[dataInputIndex]); }
  else if (menuLevel == MAIN_MENU) { strncpy(line0,"MENU",31); snprintf(line1,31,"> %s", mainMenuItems[mainMenuIndex]); }
  else if (menuLevel == SETTINGS_MENU) { strncpy(line0,"PENGATURAN",31); snprintf(line1,31,"> %s", settingsMenuItems[settingsMenuIndex]); }
  else if (menuLevel == DURATION_MENU) { strncpy(line0,"SET DURASI:",31); snprintf(line1,31,"%s %lus", paramNames[paramIndex], paramValues[paramIndex]/1000); }
  else if (menuLevel == BUZZER_MENU) { snprintf(line0,31,"Buzzer: %s", buzzerEnabled ? "ON " : "OFF"); snprintf(line1,31,"+:ON/OFF  M:Save"); }
  else if (menuLevel == WIRELESS_MENU) { snprintf(line0,31,"Wireless: %s", wirelessEnabled ? "ON" : "OFF"); snprintf(line1,31,"+:ON/OFF  M:Save"); }
  else if (menuLevel == TEST_SERVO_MENU) { snprintf(line0,31,"TEST %s", testServoMenuItems[testServoMenuIndex]); snprintf(line1,31,"+:Close  -:Open"); }
  else if (menuLevel == SERVO_DEGREE_MENU) {
    if (sdStep == SD_SELECT_SERVO) { snprintf(line0,31,"Pilih Servo:"); snprintf(line1,31, sd_servo_idx==0 ? "Servo1 " : (sd_servo_idx==1 ? "Servo2 " : "Servo3 ")); }
    else if (sdStep == SD_SELECT_POS) { snprintf(line0,31,"Posisi Servo :"); snprintf(line1,31, sd_pos_idx==0 ? "CLOSE" : "OPEN"); }
    else { snprintf(line0,31,"%s%s", sd_servo_idx==0 ? "Servo1 " : (sd_servo_idx==1 ? "Servo2 " : "Servo3 "), sd_pos_idx==0 ? "CLOSE" : "OPEN"); snprintf(line1,31,"Deg:%d +/-  M:Save", sd_degree_tmp); }
  }
  else if (menuLevel == INPUT_SERVO_MENU) { snprintf(line0,31,"Pilih Servo Input:"); snprintf(line1,31, inputServo==1 ? "Menggunakan Servo S1":"Menggunakan Servo S3"); }
  else if (menuLevel == INPUT_SERVO_SPEED_MENU) {
    if (!inputServoSpeedEditing) {
      if (inputServoSpeedIndex == 0) { snprintf(line0,31,"Kecepatan Servo Input"); snprintf(line1,31,"> CLOSE->OPEN   M:Edit"); }
      else { snprintf(line0,31,"Kecepatan Servo Input"); snprintf(line1,31,"> OPEN->CLOSE   M:Edit"); }
    } else {
      if (inputServoSpeedIndex == 0) { snprintf(line0,31,"Edit CLOSE->OPEN"); snprintf(line1,31,"Durasi: %.1fs +/-  M:Save", (float)inputServoSpeedCloseTmp/1000.0); }
      else { snprintf(line0,31,"Edit OPEN->CLOSE"); snprintf(line1,31,"Durasi: %.1fs +/-  M:Save", (float)inputServoSpeedOpenTmp/1000.0); }
    }
  }
  else if (menuLevel == CALIBRATION_MENU) {
    if (!calibrationEditing) {
      if (calibrationIndex == 0) { snprintf(line0,31,"Kalibrasi Suhu"); snprintf(line1,31,"> Offset: %.1f C  M:Edit", tempOffsetC); }
      else if (calibrationIndex == 1) { snprintf(line0,31,"Kalibrasi Flow"); snprintf(line1,31,"> Pulses/L: %.0f  M:Edit", flowPulsePerLiter); }
      else { snprintf(line0,31,"Reset Volume"); snprintf(line1,31,"M:Reset Total Volume"); }
    } else {
      if (calibrationIndex == 0) { snprintf(line0,31,"Edit Offset Suhu"); snprintf(line1,31,"Off: %.2f C +/-  M:Save", tempOffsetTmp); }
      else { snprintf(line0,31,"Edit Flow Pulses/L"); snprintf(line1,31,"PPL: %.0f +/-  M:Save", flowPPLTmp); }
    }
  }
  else if (millis() - lastSerialInputTime < SERIAL_DISPLAY_TIME && lastSerialInput[0]) { strncpy(line0,"Input Serial:",31); strncpy(line1,lastSerialInput,31); }
  else {
    const char* stateText;
    switch(state) {
      case SIAP_UJI: stateText = "SIAP UJI - READY"; break;
      case PERSIAPAN_KLIRING: stateText = "PERSIAPAN KLIRING"; break;
      case PROSES_KLIRING: stateText = "PROSES KLIRING"; break;
      case TUNGGU_PROSES: stateText = "TUNGGU PROSES"; break;
      case PERSIAPAN_KALIBRASI: stateText = "PERSIAPAN KALIBRASI"; break;
      case PROSES_KALIBRASI: stateText = "PROSES KALIBRASI"; break;
      case TUNGGU_STABIL: stateText = "TUNGGU STABIL"; break;
      default: stateText = "Unknown"; break;
    }
    strncpy(line0, stateText,31);
    char line2[32]="";
    if (inputServo==1) snprintf(line2,31,"S1:%d  S2:%d ", servoPos1, servoPos2);
    else snprintf(line2,31,"S3:%d  S2:%d ", servoPos3, servoPos2);
    unsigned long remain=0, duration=0, elapsed=0;
    switch(state) {
      case PROSES_KLIRING: elapsed=millis()-state_start_time; duration=KLIRING; remain=(duration-elapsed)/1000; snprintf(line2+strlen(line2),31-strlen(line2)," T:%lu", remain); break;
      case TUNGGU_PROSES: elapsed=millis()-state_start_time; duration=TUNGGU; remain=(duration-elapsed)/1000; snprintf(line2+strlen(line2),31-strlen(line2)," T:%lu", remain); break;
      case PROSES_KALIBRASI: elapsed=millis()-state_start_time; duration=KALIBRASI; remain=(duration-elapsed)/1000; snprintf(line2+strlen(line2),31-strlen(line2)," T:%lu", remain); break;
      case TUNGGU_STABIL: elapsed=millis()-state_start_time; duration=STABIL; remain=(duration-elapsed)/1000; snprintf(line2+strlen(line2),31-strlen(line2)," T:%lu", remain); break;
      default: break;
    }
    strncpy(line1,line2,31);
  }

  drawModernHeader("TEST BENCH PORTABLE");
  drawModernPanel(45, line0, line1);

  bool showProgress=false;
  unsigned long elapsed=0, duration=0;
  switch(state) {
    case PROSES_KLIRING: duration=KLIRING; elapsed=millis()-state_start_time; showProgress=true; break;
    case TUNGGU_PROSES: duration=TUNGGU; elapsed=millis()-state_start_time; showProgress=true; break;
    case PROSES_KALIBRASI: duration=KALIBRASI; elapsed=millis()-state_start_time; showProgress=true; break;
    case TUNGGU_STABIL: duration=STABIL; elapsed=millis()-state_start_time; showProgress=true; break;
  }
  int barX=20, barY=135, barW=tft.width()-40, barH=18;
  if (showProgress) { float prog = constrain((float)elapsed/duration,0.0,1.0); drawProgressBar(barX,barY,barW,barH,prog,COLOR_WARN,COLOR_PANEL); }
  else tft.fillRect(barX,barY,barW,barH+8,COLOR_BG);

  drawModernButtons();
}

// Virtual touch handling & menus
void handleButtons() {
  checkVirtualButton();
  bool btnA = digitalRead(BUTTON_A_PIN);
  bool btnB = digitalRead(BUTTON_B_PIN);

  // --- DATA INPUT MODE (DURATION EDIT) ---
  if (dataInputMode) {
    if (vBtnE) {
      // ensure valid
      dataInputValues[dataInputIndex] = constrain(dataInputValues[dataInputIndex], 1UL, 1000UL);
      // write back to global durations (ms)
      if (dataInputIndex == 0) KLIRING   = (unsigned long)dataInputValues[0] * 1000UL;
      else if (dataInputIndex == 1) TUNGGU    = (unsigned long)dataInputValues[1] * 1000UL;
      else if (dataInputIndex == 2) KALIBRASI = (unsigned long)dataInputValues[2] * 1000UL;
      else if (dataInputIndex == 3) STABIL    = (unsigned long)dataInputValues[3] * 1000UL;
      paramValues[0] = KLIRING; paramValues[1] = TUNGGU; paramValues[2] = KALIBRASI; paramValues[3] = STABIL;
      // save
      saveParamsToEEPROM();
      infoSavedFlag = true; infoSavedTime = millis();
      dataInputMode = false; menuLevel = DURATION_MENU;
    }
    if (vBtnF) { dataInputMode=false; menuLevel=DURATION_MENU; delay(200); }
    if (vBtnC) dataInputValues[dataInputIndex]++;
    if (vBtnD) if (dataInputValues[dataInputIndex] > 1) dataInputValues[dataInputIndex]--;
    if (btnA==LOW && btnA_last==HIGH && dataInputIndex>0) dataInputIndex--;
    if (btnB==LOW && btnB_last==HIGH && dataInputIndex<3) dataInputIndex++;
    btnA_last=btnA; btnB_last=btnB;
    vBtnC=vBtnD=vBtnE=vBtnF=false;
    return;
  }

  if (vBtnF) {
    if (menuLevel == SERVO_DEGREE_MENU) {
      if (sdStep == SD_EDIT_DEGREE) sdStep = SD_SELECT_POS;
      else if (sdStep == SD_SELECT_POS) sdStep = SD_SELECT_SERVO;
      else menuLevel = SETTINGS_MENU;
      vBtnC=vBtnD=vBtnE=vBtnF=false; return;
    }
    if (menuLevel == INPUT_SERVO_MENU) { menuLevel = SETTINGS_MENU; vBtnC=vBtnD=vBtnE=vBtnF=false; return; }
    if (menuLevel == TEST_SERVO_MENU || menuLevel == BUZZER_MENU || menuLevel == WIRELESS_MENU || menuLevel == DURATION_MENU || menuLevel == CONFIRM_RESET_MENU || menuLevel == INPUT_SERVO_SPEED_MENU || menuLevel == CONFIRM_REBOOT_MENU || menuLevel == CALIBRATION_MENU) {
      menuLevel = SETTINGS_MENU; vBtnC=vBtnD=vBtnE=vBtnF=false; return;
    }
    if (menuLevel == SETTINGS_MENU) { menuLevel = MAIN_MENU; vBtnC=vBtnD=vBtnE=vBtnF=false; return; }
    if (menuLevel == MAIN_MENU) { menuLevel = NONE_MENU; vBtnC=vBtnD=vBtnE=vBtnF=false; return; }
    if (menuLevel == NONE_MENU && !dataInputMode) {
      state = SIAP_UJI; state_start_time = millis(); led1_flash=false; led2_flash=false; digitalWrite(LED1_PIN,LOW); digitalWrite(LED2_PIN,LOW);
      setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][1]);
      vBtnC=vBtnD=vBtnE=vBtnF=false; btnA_last=btnA; btnB_last=btnB; return;
    }
    vBtnC=vBtnD=vBtnE=vBtnF=false; btnA_last=btnA; btnB_last=btnB; return;
  }

  // Wireless menu
  if (menuLevel == WIRELESS_MENU) { if (vBtnC) wirelessEnabled = !wirelessEnabled; if (vBtnE) { saveWirelessToEEPROM(); applyWirelessStatus(); infoSavedFlag=true; infoSavedTime=millis(); } vBtnC=vBtnE=vBtnF=false; return; }

  // Servo degree menu
  if (menuLevel == SERVO_DEGREE_MENU) {
    if (sdStep == SD_SELECT_SERVO) {
      if (vBtnC) { sd_servo_idx = (sd_servo_idx+1)%3; sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx]; }
      if (vBtnD) { sd_servo_idx = (sd_servo_idx+1)%3; sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx]; }
      if (vBtnE) { sdStep = SD_SELECT_POS; sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx]; }
      vBtnC=vBtnD=vBtnE=vBtnF=false; return;
    } else if (sdStep == SD_SELECT_POS) {
      if (vBtnC) { sd_pos_idx = (sd_pos_idx+1)%2; sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx]; }
      if (vBtnD) { sd_pos_idx = (sd_pos_idx+1)%2; sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx]; }
      if (vBtnE) { sdStep = SD_EDIT_DEGREE; sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx]; }
      vBtnC=vBtnD=vBtnE=vBtnF=false; return;
    } else if (sdStep == SD_EDIT_DEGREE) {
      if (vBtnC) if (sd_degree_tmp < 180) sd_degree_tmp++;
      if (vBtnD) if (sd_degree_tmp > 0) sd_degree_tmp--;
      if (vBtnE) { servoDegree[sd_servo_idx][sd_pos_idx] = sd_degree_tmp; saveServoDegreeToEEPROM(); infoSavedFlag=true; infoSavedTime=millis(); }
      vBtnC=vBtnD=vBtnE=vBtnF=false; return;
    }
    vBtnC=vBtnD=vBtnE=vBtnF=false; return;
  }

  // Input servo menu
  if (menuLevel == INPUT_SERVO_MENU) {
    if (vBtnC || vBtnD) inputServo = (inputServo==1)?3:1;
    if (vBtnE) { saveInputServoToEEPROM(); infoSavedFlag=true; infoSavedTime=millis(); }
    vBtnC=vBtnD=vBtnE=vBtnF=false; return;
  }

  // Input servo speed menu (uses global inputServoSpeedIndex & inputServoSpeedEditing)
  if (menuLevel == INPUT_SERVO_SPEED_MENU) {
    if (!inputServoSpeedEditing) {
      if (vBtnC) { inputServoSpeedIndex = (inputServoSpeedIndex - 1 + 2) % 2; menuModeStart = millis(); }
      if (vBtnD) { inputServoSpeedIndex = (inputServoSpeedIndex + 1) % 2; menuModeStart = millis(); }
      if (vBtnE) { inputServoSpeedEditing = true; }
      if (vBtnF) { menuLevel = SETTINGS_MENU; }
    } else {
      if (inputServoSpeedIndex == 0) {
        if (vBtnC) { if (inputServoSpeedCloseTmp + 100 <= 5000) inputServoSpeedCloseTmp += 100; }
        if (vBtnD) { if (inputServoSpeedCloseTmp >= 300) inputServoSpeedCloseTmp -= 100; }
        if (vBtnE) { inputServoSpeedCloseMs = constrain(inputServoSpeedCloseTmp,200,5000); saveInputServoSpeedsToEEPROM(); infoSavedFlag=true; infoSavedTime=millis(); inputServoSpeedEditing=false; }
        if (vBtnF) { inputServoSpeedCloseTmp = inputServoSpeedCloseMs; inputServoSpeedEditing=false; }
      } else {
        if (vBtnC) { if (inputServoSpeedOpenTmp + 100 <= 5000) inputServoSpeedOpenTmp += 100; }
        if (vBtnD) { if (inputServoSpeedOpenTmp >= 300) inputServoSpeedOpenTmp -= 100; }
        if (vBtnE) { inputServoSpeedOpenMs = constrain(inputServoSpeedOpenTmp,200,5000); saveInputServoSpeedsToEEPROM(); infoSavedFlag=true; infoSavedTime=millis(); inputServoSpeedEditing=false; }
        if (vBtnF) { inputServoSpeedOpenTmp = inputServoSpeedOpenMs; inputServoSpeedEditing=false; }
      }
    }
    vBtnC=vBtnD=vBtnE=vBtnF=false; return;
  }

  // Calibration menu (now 3 items: temp offset, flow pulses/L, reset volume)
  if (menuLevel == CALIBRATION_MENU) {
    if (!calibrationEditing) {
      if (vBtnC) { calibrationIndex = (calibrationIndex - 1 + 3) % 3; menuModeStart = millis(); }
      if (vBtnD) { calibrationIndex = (calibrationIndex + 1) % 3; menuModeStart = millis(); }
      if (vBtnE) {
        if (calibrationIndex == 2) { resetVolume(); }
        else { calibrationEditing = true; tempOffsetTmp = tempOffsetC; flowPPLTmp = flowPulsePerLiter; }
      }
      if (vBtnF) { menuLevel = SETTINGS_MENU; }
    } else {
      if (calibrationIndex == 0) {
        if (vBtnC) tempOffsetTmp += 0.1f;
        if (vBtnD) tempOffsetTmp -= 0.1f;
        if (vBtnE) {
          tempOffsetC = tempOffsetTmp;
          saveTempCalibrationToEEPROM(tempOffsetC);
          // broadcast SSE update so web UI updates immediately
          String payload = String("{\"tempOffset\":") + String(tempOffsetC, 4) + String(",\"flowPPL\":") + String(flowPulsePerLiter, 2) + String("}");
          events.send(payload.c_str(), "calib", millis());
          infoSavedFlag=true; infoSavedTime=millis(); calibrationEditing=false;
        }
        if (vBtnF) { tempOffsetTmp = tempOffsetC; calibrationEditing=false; }
      } else {
        if (vBtnC) flowPPLTmp += 1.0f;
        if (vBtnD) { if (flowPPLTmp > 1.0f) flowPPLTmp -= 1.0f; }
        if (vBtnE) {
          flowPulsePerLiter = (flowPPLTmp < 1.0f) ? 1.0f : flowPPLTmp;
          saveFlowCalibrationToEEPROM(flowPulsePerLiter);
          // broadcast SSE update so web UI updates immediately
          String payload = String("{\"tempOffset\":") + String(tempOffsetC, 4) + String(",\"flowPPL\":") + String(flowPulsePerLiter, 2) + String("}");
          events.send(payload.c_str(), "calib", millis());
          infoSavedFlag=true; infoSavedTime=millis(); calibrationEditing=false;
        }
        if (vBtnF) { flowPPLTmp = flowPulsePerLiter; calibrationEditing=false; }
      }
    }
    vBtnC=vBtnD=vBtnE=vBtnF=false; return;
  }

  // Buzzer menu
  if (menuLevel == BUZZER_MENU) {
    if (vBtnC) buzzerEnabled = !buzzerEnabled;
    if (vBtnE) { saveBuzzerToEEPROM(); infoSavedFlag=true; infoSavedTime=millis(); }
    vBtnC=vBtnE=vBtnF=false; return;
  }

  // Settings navigation
  if (menuLevel == SETTINGS_MENU) {
    int settingsCount = 10;
    if (vBtnC) { settingsMenuIndex = (settingsMenuIndex - 1 + settingsCount) % settingsCount; menuModeStart = millis(); }
    if (vBtnD) { settingsMenuIndex = (settingsMenuIndex + 1) % settingsCount; menuModeStart = millis(); }
    if (vBtnE) {
      if (settingsMenuIndex == 0) { menuLevel = TEST_SERVO_MENU; testServoMenuIndex=0; menuModeStart=millis(); vBtnE=false; return; }
      else if (settingsMenuIndex == 1) { menuLevel = SERVO_DEGREE_MENU; sdStep=SD_SELECT_SERVO; sd_servo_idx=0; sd_pos_idx=0; sd_degree_tmp=servoDegree[sd_servo_idx][sd_pos_idx]; menuModeStart=millis(); return; }
      else if (settingsMenuIndex == 2) { menuLevel = INPUT_SERVO_MENU; menuModeStart=millis(); return; }
      else if (settingsMenuIndex == 3) { menuLevel = INPUT_SERVO_SPEED_MENU; inputServoSpeedIndex=0; inputServoSpeedCloseTmp=inputServoSpeedCloseMs; inputServoSpeedOpenTmp=inputServoSpeedOpenMs; menuModeStart=millis(); return; }
      else if (settingsMenuIndex == 4) { menuLevel = DURATION_MENU; paramIndex=0; menuModeStart=millis(); return; }
      else if (settingsMenuIndex == 5) { menuLevel = BUZZER_MENU; menuModeStart=millis(); return; }
      else if (settingsMenuIndex == 6) { menuLevel = CALIBRATION_MENU; calibrationIndex=0; calibrationEditing=false; tempOffsetTmp=tempOffsetC; flowPPLTmp=flowPulsePerLiter; menuModeStart=millis(); return; }
      else if (settingsMenuIndex == 7) { menuLevel = WIRELESS_MENU; menuModeStart=millis(); return; }
      else if (settingsMenuIndex == 8) { menuLevel = CONFIRM_REBOOT_MENU; menuModeStart=millis(); return; }
      else if (settingsMenuIndex == 9) { menuLevel = CONFIRM_RESET_MENU; menuModeStart=millis(); return; }
    }
    vBtnC=vBtnD=vBtnE=vBtnF=false; return;
  }

  // Confirm reset
  if (menuLevel == CONFIRM_RESET_MENU) {
    if (vBtnE) { resetEEPROMToDefault(); applyWirelessStatus(); infoSavedFlag=true; infoSavedTime=millis(); menuLevel = SETTINGS_MENU; }
    if (vBtnF) { menuLevel = SETTINGS_MENU; }
    vBtnE=vBtnF=false; return;
  }

  // Confirm reboot
  if (menuLevel == CONFIRM_REBOOT_MENU) {
    if (vBtnE) { rebootRequested=true; rebootAt=millis()+200; infoSavedFlag=true; infoSavedTime=millis(); menuLevel=SETTINGS_MENU; }
    if (vBtnF) { menuLevel = SETTINGS_MENU; }
    vBtnE=vBtnF=false; return;
  }

  // Main menu & actions when SIAP_UJI
  if (state == SIAP_UJI) {
    if (menuLevel == NONE_MENU && vBtnE) { menuLevel = MAIN_MENU; mainMenuIndex=0; menuModeStart=millis(); vBtnE=false; return; }
    if (menuLevel == MAIN_MENU) {
      if (vBtnC) { mainMenuIndex = (mainMenuIndex - 1 + 2) % 2; menuModeStart=millis(); }
      if (vBtnD) { mainMenuIndex = (mainMenuIndex + 1) % 2; menuModeStart=millis(); }
      if (vBtnE) { if (mainMenuIndex==1) { menuLevel = SETTINGS_MENU; settingsMenuIndex=0; menuModeStart=millis(); } else menuLevel = NONE_MENU; }
      vBtnC=vBtnD=vBtnE=vBtnF=false; return;
    }
    if (menuLevel == DURATION_MENU) {
      if (vBtnC) { paramIndex = (paramIndex - 1 + 4) % 4; menuModeStart=millis(); }
      if (vBtnD) { paramIndex = (paramIndex + 1) % 4; menuModeStart=millis(); }
      if (vBtnE) { menuLevel = NONE_MENU; dataInputMode=true; dataInputIndex=paramIndex; dataInputValues[0] = KLIRING/1000; dataInputValues[1]=TUNGGU/1000; dataInputValues[2]=KALIBRASI/1000; dataInputValues[3]=STABIL/1000; }
      vBtnC=vBtnD=vBtnE=vBtnF=false; return;
    }
    if (menuLevel == TEST_SERVO_MENU) {
      if (vBtnC) {
        if (testServoMenuIndex==0) setPhysicalServoPos(1, servoDegree[0][0]);
        else if (testServoMenuIndex==1) setPhysicalServoPos(2, servoDegree[1][0]);
        else if (testServoMenuIndex==2) setPhysicalServoPos(3, servoDegree[2][0]);
      }
      if (vBtnD) {
        if (testServoMenuIndex==0) setPhysicalServoPos(1, servoDegree[0][1]);
        else if (testServoMenuIndex==1) setPhysicalServoPos(2, servoDegree[1][1]);
        else if (testServoMenuIndex==2) setPhysicalServoPos(3, servoDegree[2][1]);
      }
      if (vBtnE) { testServoMenuIndex = (testServoMenuIndex + 1) % 3; menuModeStart=millis(); }
      vBtnC=vBtnD=vBtnE=vBtnF=false; return;
    }
  }

  // Hardware buttons A/B for processes
  if (btnA == LOW && btnA_last == HIGH) { btnA_down_time = millis(); btnA_handled=false; }
  else if (btnA == HIGH && btnA_last == LOW) {
    unsigned long held = millis() - btnA_down_time;
    if (!btnA_handled && menuLevel==NONE_MENU && !dataInputMode) {
      if (held < LONG_PRESS) state = PERSIAPAN_KLIRING; else state = PROSES_KLIRING;
      state_start_time = millis(); btnA_handled=true;
    }
  }
  btnA_last = btnA;

  if (btnB == LOW && btnB_last == HIGH) { btnB_down_time = millis(); btnB_handled=false; }
  else if (btnB == HIGH && btnB_last == LOW) {
    unsigned long held = millis() - btnB_down_time;
    if (!btnB_handled && menuLevel==NONE_MENU && !dataInputMode) {
      if (held < LONG_PRESS) state = PERSIAPAN_KALIBRASI; else state = PROSES_KALIBRASI;
      state_start_time = millis(); btnB_handled=true;
    }
  }
  btnB_last = btnB;
}

void handlePhysicalButtons() {
  unsigned long now = millis();
  bool btnA_read = digitalRead(BUTTON_A_PIN);
  bool btnB_read = digitalRead(BUTTON_B_PIN);
  if (btnA_read != btnA_stable) btnA_lastTime = now;
  if (now - btnA_lastTime > DEBOUNCE_DELAY) {
    if (btnA_read != btnA_stable) {
      btnA_stable = btnA_read;
      if (btnA_stable == LOW) { btnA_down_time = now; btnA_handled=false; }
      else { unsigned long held = now - btnA_down_time; if (!btnA_handled && menuLevel==NONE_MENU && !dataInputMode) { if (held < LONG_PRESS) state = PERSIAPAN_KLIRING; else state = PROSES_KLIRING; state_start_time = now; btnA_handled=true; } }
    }
  }
  if (btnB_read != btnB_stable) btnB_lastTime = now;
  if (now - btnB_lastTime > DEBOUNCE_DELAY) {
    if (btnB_read != btnB_stable) {
      btnB_stable = btnB_read;
      if (btnB_stable == LOW) { btnB_down_time = now; btnB_handled=false; }
      else { unsigned long held = now - btnB_down_time; if (!btnB_handled && menuLevel==NONE_MENU && !dataInputMode) { if (held < LONG_PRESS) state = PERSIAPAN_KALIBRASI; else state = PROSES_KALIBRASI; state_start_time = now; btnB_handled=true; } }
    }
  }
}

void handleStateMachine() {
  unsigned long now = millis();
  static bool buzzerDoneLocal=false;
  if (menuLevel == TEST_SERVO_MENU) return;
  switch(state) {
    case SIAP_UJI:
      digitalWrite(LED1_PIN,LOW); digitalWrite(LED2_PIN,LOW);
      setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][1]);
      led1_flash = led2_flash = false; buzzerDoneLocal=false; break;
    case PERSIAPAN_KLIRING:
      digitalWrite(LED1_PIN,HIGH); digitalWrite(LED2_PIN,LOW);
      setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][1]); led1_flash=false; buzzerDoneLocal=false; break;
    case PROSES_KLIRING:
      led1_flash=true; digitalWrite(LED2_PIN,LOW);
      setServoPositions(servoDegree[(inputServo==1?0:2)][1], servoDegree[1][1]); buzzerDoneLocal=false;
      if (now - state_start_time >= KLIRING) { state = TUNGGU_PROSES; state_start_time = now; led1_flash=false; }
      break;
    case TUNGGU_PROSES:
      digitalWrite(LED1_PIN,HIGH); digitalWrite(LED2_PIN,LOW);
      setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][1]); buzzerDoneLocal=false;
      if (now - state_start_time >= TUNGGU) { digitalWrite(LED1_PIN,LOW); state = SIAP_UJI; }
      break;
    case PERSIAPAN_KALIBRASI:
      digitalWrite(LED1_PIN,LOW); digitalWrite(LED2_PIN,HIGH);
      setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][0]); led2_flash=false; buzzerDoneLocal=false; break;
    case PROSES_KALIBRASI:
      led2_flash=true; digitalWrite(LED1_PIN,LOW);
      setServoPositions(servoDegree[(inputServo==1?0:2)][1], servoDegree[1][0]); buzzerDoneLocal=false;
      if (now - state_start_time >= KALIBRASI) { state=TUNGGU_STABIL; state_start_time=now; led2_flash=false; buzzerDoneLocal=false; }
      break;
    case TUNGGU_STABIL:
      digitalWrite(LED1_PIN,LOW); digitalWrite(LED2_PIN,HIGH);
      setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][0]);
      if (!buzzerDoneLocal && buzzerEnabled) { startBlinkBuzzer(DEFAULT_BUZZER_TOTAL, DEFAULT_BUZZER_INTERVAL); buzzerDoneLocal=true; }
      if (now - state_start_time >= STABIL) { digitalWrite(LED2_PIN,LOW); setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][1]); state=SIAP_UJI; buzzerDoneLocal=false; }
      break;
    default: state = SIAP_UJI; buzzerDoneLocal=false; break;
  }
}

void handleFlashing() {
  unsigned long now=millis();
  if (led1_flash) { if (now - led1_flash_timer > 500) { led1_flash_timer = now; led1_on = !led1_on; digitalWrite(LED1_PIN, led1_on ? HIGH : LOW); } }
  if (led2_flash) { if (now - led2_flash_timer > 500) { led2_flash_timer = now; led2_on = !led2_on; digitalWrite(LED2_PIN, led2_on ? HIGH : LOW); } }
}

void handleSerialInput() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n'); input.trim();
    if (input.equalsIgnoreCase("INFO")) {
      Serial.println(F("==== KONFIGURASI TERKINI ===="));
      Serial.print(F("KLIRING    : ")); Serial.println(KLIRING / 1000);
      Serial.print(F("TUNGGU     : ")); Serial.println(TUNGGU / 1000);
      Serial.print(F("KALIBRASI  : ")); Serial.println(KALIBRASI / 1000);
      Serial.print(F("STABIL     : ")); Serial.println(STABIL / 1000);
      Serial.print(F("BUZZER     : ")); Serial.println(buzzerEnabled ? "ON" : "OFF");
      Serial.print(F("WIRELESS   : ")); Serial.println(wirelessEnabled ? "ON" : "OFF");
      Serial.print(F("INPUT SERVO: ")); Serial.println(inputServo);
      Serial.print(F("SPD C->O   : ")); Serial.println((float)inputServoSpeedCloseMs/1000.0);
      Serial.print(F("SPD O->C   : ")); Serial.println((float)inputServoSpeedOpenMs/1000.0);
      Serial.print(F("S1 CLOSE   : ")); Serial.println(servoDegree[0][0]);
      Serial.print(F("S1 OPEN    : ")); Serial.println(servoDegree[0][1]);
      Serial.print(F("S2 CLOSE   : ")); Serial.println(servoDegree[1][0]);
      Serial.print(F("S2 OPEN    : ")); Serial.println(servoDegree[1][1]);
      Serial.print(F("S3 CLOSE   : ")); Serial.println(servoDegree[2][0]);
      Serial.print(F("S3 OPEN    : ")); Serial.println(servoDegree[2][1]);
      Serial.print(F("TEMP OFFSET: ")); Serial.println(tempOffsetC);
      Serial.print(F("FLOW PPL   : ")); Serial.println(flowPulsePerLiter);
      Serial.print(F("VOLUME L   : ")); Serial.println(getTotalVolumeLiters());
      Serial.println(F("============================="));
      return;
    }
    if (input.equalsIgnoreCase("RESET")) { resetEEPROMToDefault(); infoSavedFlag=true; infoSavedTime=millis(); menuLevel=SETTINGS_MENU; Serial.println(F("RESET OKE — REBOOT...")); delay(200); ESP.restart(); while(1); return; }
    if (input.equalsIgnoreCase("REBOOT")) { rebootRequested=true; rebootAt=millis()+200; Serial.println(F("REBOOT DIMINTA...")); return; }
    if (input.startsWith("STATE ")) {
      String s = input.substring(6); s.trim();
      if (s.equalsIgnoreCase("S")) { state = SIAP_UJI; Serial.println(F("State diubah ke SIAP_UJI")); }
      else if (s.equalsIgnoreCase("A1")) { state = PERSIAPAN_KLIRING; Serial.println(F("State diubah ke PERSIAPAN_KLIRING")); }
      else if (s.equalsIgnoreCase("A2")) { state = PROSES_KLIRING; state_start_time=millis(); Serial.println(F("State diubah ke PROSES_KLIRING")); }
      else if (s.equalsIgnoreCase("A3")) { state = TUNGGU_PROSES; state_start_time=millis(); Serial.println(F("State diubah ke TUNGGU_PROSES")); }
      else if (s.equalsIgnoreCase("B1")) { state = PERSIAPAN_KALIBRASI; Serial.println(F("State diubah ke PERSIAPAN KALIBRASI")); }
      else if (s.equalsIgnoreCase("B2")) { state = PROSES_KALIBRASI; state_start_time=millis(); Serial.println(F("State diubah ke PROSES_KALIBRASI")); }
      else if (s.equalsIgnoreCase("B3")) { state = TUNGGU_STABIL; state_start_time=millis(); Serial.println(F("State diubah ke TUNGGU_STABIL")); }
      else { Serial.println(F("Nama state tidak dikenal.")); }
      return;
    }
    int spaceIdx = input.indexOf(' ');
    String cmd = input;
    unsigned long duration = 0;
    bool valid = false;
    if (spaceIdx != -1) { cmd = input.substring(0,spaceIdx); String durStr = input.substring(spaceIdx+1); duration = durStr.toInt(); }
    if (cmd == "KLIRING" && duration > 0) { KLIRING = duration * 1000; valid = true; }
    else if (cmd == "TUNGGU" && duration > 0) { TUNGGU = duration * 1000; valid = true; }
    else if (cmd == "KALIBRASI" && duration > 0) { KALIBRASI = duration * 1000; valid = true; }
    else if (cmd == "STABIL" && duration > 0) { STABIL = duration * 1000; valid = true; }
    paramValues[0]=KLIRING; paramValues[1]=TUNGGU; paramValues[2]=KALIBRASI; paramValues[3]=STABIL;
    if (valid) saveParamsToEEPROM();
    strncpy(lastSerialInput, input.c_str(), sizeof(lastSerialInput)-1); lastSerialInput[sizeof(lastSerialInput)-1]=0; lastSerialInputTime=millis();
    if (!valid) { invalidSerialFlag=true; invalidSerialTime=millis(); Serial.print(F("Input serial SALAH - ")); Serial.println(input); }
    else { Serial.print(F("Input serial OK: ")); Serial.println(input); }
  }
}

void setup() {
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(LED1_PIN, OUTPUT); pinMode(LED2_PIN, OUTPUT); pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowPulseISR, RISING);

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo3.attach(SERVO3_PIN);

  tft.init(); tft.setRotation(3); tft.fillScreen(COLOR_BG);
  BTN_W = tft.width() / 4;
  touchSPI.begin(TOUCH_CLK, TOUCH_DOUT, TOUCH_DIN, TOUCH_CS);
  ts.begin(touchSPI); ts.setRotation(1);

  Serial.begin(115200);

  sensors.begin();
  if (sensors.getAddress(tempDeviceAddress, 0)) sensors.setResolution(tempDeviceAddress, 11);
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  tempNextReadAt = millis() + TEMP_CONV_MS;

  EEPROM.begin(128);
  loadParamsFromEEPROM();

  if (inputServo == 1) setServoPositions(servoDegree[0][0], servoDegree[1][1]);
  else setServoPositions(servoDegree[2][0], servoDegree[1][1]);

  applyWirelessStatus();
  handleAPI();
  server.begin();

  // -------- ArduinoOTA setup --------
  // Important: ArduinoOTA uses network (mDNS + TCP). When running as SoftAP,
  // ensure your PC is connected to the ESP AP (SSID = TestBench-AP) to reach OTA.
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.onStart([]() {
    Serial.println("Start OTA");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd OTA");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  Serial.print("OTA ready. Hostname: ");
  Serial.println(ota_hostname);
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());

  lastFlowCalcTime = millis();
  lastFlowPulseSnapshot = 0;
}

void loop() {
  handleSerialInput();
  handleButtons();
  handlePhysicalButtons();
  handleStateMachine();
  handleFlashing();
  handleBlinkBuzzer();
  handleInputServoMovement();

  updateFlowMeasurement();
  updateTemperature();

  updateDisplay();

  // handle OTA in main loop
  ArduinoOTA.handle();

  if (rebootRequested && (long)(millis() - rebootAt) >= 0) { Serial.println(F("Rebooting now...")); delay(200); ESP.restart(); while(1); }
}

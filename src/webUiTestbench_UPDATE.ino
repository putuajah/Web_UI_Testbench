/*
*ssid = "TestBench-AP";
*password = "testbench123";
*/

#include <TFT_eSPI.h>
#include <ESP32Servo.h>
#include <EEPROM.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>

// -------- COLOR PALETTE --------
#define COLOR_BG         0x18E3  // Light Gray
#define COLOR_PANEL      0x2108  // Soft Gray
#define COLOR_HEADER     0x03B6  // Blue soft
#define COLOR_TEXT       0xFFFF  // White
#define COLOR_ACCENT     0x03B6  // Blue soft
#define COLOR_OK         0x03B6  // Blue soft -Green 0x07E0
#define COLOR_WARN       0xFBE0  // Orange
#define COLOR_ERR        0x03B6  // Blue soft -Red 0xF800
#define COLOR_BTN        0x03B6  // Blue soft
#define COLOR_BTN_SEL    0x07FF  // Cyan

// -------- PIN MAPPING --------
#define BUTTON_A_PIN 4
#define BUTTON_B_PIN 5
#define LED1_PIN 16
#define LED2_PIN 17
#define SERVO1_PIN 12
#define SERVO2_PIN 13
#define SERVO3_PIN 14
#define BUZZER_PIN 27

// -------- TFT_eSPI (HSPI) --------
TFT_eSPI tft = TFT_eSPI();
Servo servo1, servo2, servo3;

// -------- XPT2046 TOUCH (VSPI custom) --------
#define TOUCH_CS   35
#define TOUCH_IRQ  34
#define TOUCH_CLK  25
#define TOUCH_DOUT 26 // MISO
#define TOUCH_DIN  32 // MOSI

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// -------- EEPROM ADDRESS --------
#define EEPROM_ADDR_KLIRING     0
#define EEPROM_ADDR_TUNGGU      4
#define EEPROM_ADDR_KALIBRASI   8
#define EEPROM_ADDR_STABIL      12
#define EEPROM_ADDR_BUZZER      16
#define EEPROM_ADDR_S1_CLOSE    17
#define EEPROM_ADDR_S1_OPEN     18
#define EEPROM_ADDR_S2_CLOSE    19
#define EEPROM_ADDR_S2_OPEN     20
#define EEPROM_ADDR_WIRELESS    21    // --- WIRELESS MOD ---
#define EEPROM_ADDR_S3_CLOSE    22
#define EEPROM_ADDR_S3_OPEN     23
#define EEPROM_ADDR_INPUT_SERVO 24    // 1 = servo1, 3 = servo3

// two addresses for input servo speeds (ms) stored as unsigned long (4 bytes each)
#define EEPROM_ADDR_INPUT_SPD_CLOSE 25
#define EEPROM_ADDR_INPUT_SPD_OPEN  29

// servoDegree[servoIndex][posIndex] posIndex: 0=close,1=open
int servoDegree[3][2] = {
  {30, 120}, // servo1 close/open
  {30, 120}, // servo2 close/open
  {30, 120}  // servo3 close/open
};

int servoPos1 = 0;
int servoPos2 = 0;
int servoPos3 = 0;

// Which servo acts as the "input" servo (used where previously only servo1 was used)
// valid values: 1 or 3
int inputServo = 1;

// ----- Input servo movement speeds (ms) -----
unsigned long inputServoSpeedCloseMs = 500; // default: close -> open
unsigned long inputServoSpeedOpenMs  = 500; // default: open -> close
unsigned long inputServoSpeedCloseTmp = 500;
unsigned long inputServoSpeedOpenTmp  = 500;

// Tombol A/B
bool btnA_last = HIGH, btnB_last = HIGH;
unsigned long btnA_down_time = 0, btnB_down_time = 0;
bool btnA_handled = false, btnB_handled = false;

// -------- STATE MESIN --------
enum State {
  SIAP_UJI, PERSIAPAN_KLIRING, PROSES_KLIRING, TUNGGU_PROSES,
  PERSIAPAN_KALIBRASI, PROSES_KALIBRASI, TUNGGU_STABIL
};
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

// -------- MENU --------
const char* paramNames[] = {"KLIRING", "TUNGGU", "KALIBRASI", "STABIL"};
const int paramLength = 4;
unsigned long paramValues[] = {KLIRING, TUNGGU, KALIBRASI, STABIL};
int paramIndex = 0;

enum MenuLevel {
  NONE_MENU, MAIN_MENU, SETTINGS_MENU, DURATION_MENU, BUZZER_MENU,
  TEST_SERVO_MENU, WIRELESS_MENU, CONFIRM_RESET_MENU, CONFIRM_REBOOT_MENU, SERVO_DEGREE_MENU, INPUT_SERVO_MENU,
  INPUT_SERVO_SPEED_MENU
};
MenuLevel menuLevel = NONE_MENU;

enum ServoDegreeSettingStep {
  SD_SELECT_SERVO, SD_SELECT_POS, SD_EDIT_DEGREE
};
ServoDegreeSettingStep sdStep = SD_SELECT_SERVO;
int sd_servo_idx = 0;
int sd_pos_idx = 0;
int sd_degree_tmp = 0;

const char* mainMenuItems[] = {"Test Bench", "Pengaturan"};
int mainMenuIndex = 0;

// --------- SETTINGS MENU NAMA ---------
// Urutan baru sesuai permintaan:
const char* settingsMenuItems[] = {
  "Test Servo",                   // 0 -> enterTestServoMenu()
  "Set Posisi Servo",             // 1 -> enterServoDegreeMenu()
  "Pilih Servo Input",            // 2 -> enterInputServoMenu()
  "Set Kecepatan Servo Input",    // 3 -> enterInputServoSpeedMenu()
  "Set Durasi Pengujian",         // 4 -> enterDurationMenu()
  "Set Buzzer",                   // 5 -> enterBuzzerMenu()
  "Wireless",                     // 6 -> enterWirelessMenu()
  "Reboot",                       // 7 -> CONFIRM_REBOOT_MENU
  "Reset EEPROM"                  // 8 -> CONFIRM_RESET_MENU
};
int settingsMenuIndex = 0;

const char* testServoMenuItems[] = {"Servo1", "Servo2", "Servo3"};
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
bool wirelessEnabled = true; // --- WIRELESS MOD ---

// -------- VIRTUAL BUTTON --------
#define BTN_Y 200
#define BTN_H 40
#define BTN_W (tft.width() / 4)  // Empat tombol dalam satu baris penuh
#define BTN_GAP 0
#define BTN_C_X 0
#define BTN_D_X (BTN_W)
#define BTN_E_X (BTN_W * 2)
#define BTN_F_X (BTN_W * 3)

volatile bool vBtnC = false, vBtnD = false, vBtnE = false, vBtnF = false;

// Debounce touch
unsigned long lastTouchTime = 0;
bool lastTouch = false;
const unsigned long touchCooldown = 200; // ms

// DEBOUNCE TOMBOL FISIK & LONG PRESS
const unsigned long DEBOUNCE_DELAY = 50;  // ms
unsigned long btnA_lastTime = 0;
unsigned long btnB_lastTime = 0;
bool btnA_stable = HIGH;
bool btnB_stable = HIGH;

// -------- BUZZER NON-BLOCKING BERKEDIP (STANDARD 0.5s) --------
bool buzzerBlinkActive = false;       // proses kedipan aktif
unsigned long buzzerBlinkEndTime = 0; // kapan total durasi selesai
unsigned long buzzerToggleTime = 0;   // waktu ganti ON/OFF berikutnya
bool buzzerState = false;             // status pin buzzer saat ini
const unsigned long DEFAULT_BUZZER_TOTAL = 5000; // 5 detik total
const unsigned long DEFAULT_BUZZER_INTERVAL = 500; // 0.5s ON / 0.5s OFF

// ----------- WIFI + WEBUI + API -------------
const char *ssid = "TestBench-AP";
const char *password = "testbench123";
AsyncWebServer server(80);

// reboot control
bool rebootRequested = false;
unsigned long rebootAt = 0;

// ----------- index_html[] -----------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Test Bench Control</title>
<style>
body { font-family: Arial; background:#e9ecef; margin:0;}
.container { max-width:420px;margin:30px auto;background:#fff;padding:24px;border-radius:12px;box-shadow:0 2px 8px #0001;}
h2 {color:#0aa;}
.label{font-weight:bold;}
input[type=number]{width:70px;}
.btn{padding:10px 18px;background:#0aa;color:#fff;border:none;border-radius:6px;cursor:pointer;margin:4px;}
.btn-red{background:#d33;}
.btn-blue{background:#0277bd;color:#fff;}
.btn-orange{background:#ff6213;color:#fff}
.btn-Orchid{background:#DA70D6;color:#fff}
</style>
</head>
<body>
<div class="container">
<h2>Test Bench Control</h2>
<div>
  <div class="label">Status Mesin: <span id="state"></span></div>
  <div>
    <button class="btn btn-orange" onclick="aksi('A1')">Persiapan Kliring</button>
    <button class="btn btn-orange" onclick="aksi('A2')">Proses Kliring</button>
    <button class="btn" onclick="aksi('B1')">Persiapan Kalibrasi</button>
    <button class="btn" onclick="aksi('B2')">Proses Kalibrasi</button>
    <button class="btn btn-blue" onclick="aksi('S')">Siap Uji</button>
  </div>
</div>
<hr>
<form onsubmit="return kirimData();">
  <div class="label">Set Durasi (detik):</div>
  KLIRING <input id="kliring" type="number" min="1" value="10"><br>
  TUNGGU <input id="tunggu" type="number" min="1" value="5"><br>
  KALIBRASI <input id="kalibrasi" type="number" min="1" value="30"><br>
  STABIL <input id="stabil" type="number" min="1" value="10"><br>
  <button class="btn" type="submit">Simpan</button>
</form>
<hr>
<div>
  <div class="label">Buzzer: <span id="buzzer"></span>
    <button class="btn" onclick="toggleBuzzer()">Toggle</button>
  </div>
</div>
<hr>
<div>
  <div class="label">Wireless: <span id="wireless"></span>
    <button class="btn" onclick="toggleWireless()">Toggle</button>
  </div>
</div>
<hr>
<div>
  <div class="label">Pilih Servo Input Aktif:</div>
  <select id="inputServoSelect">
    <option value="1">Servo 1</option>
    <option value="3">Servo 3</option>
  </select>
  <button class="btn" onclick="setInputServo()">Set</button>
  <div style="margin-top:6px">Aktif: <span id="activeServo"></span></div>
</div>
<hr>
<div>
  <div class="label">Set Sudut Servo:</div>
  S1 CLOSE <input id="s1c" type="number" min="0" max="180">
  S1 OPEN <input id="s1o" type="number" min="0" max="180"><br>
  S2 CLOSE <input id="s2c" type="number" min="0" max="180">
  S2 OPEN <input id="s2o" type="number" min="0" max="180"><br>
  S3 CLOSE <input id="s3c" type="number" min="0" max="180">
  S3 OPEN <input id="s3o" type="number" min="0" max="180"><br>
  <button class="btn" onclick="setServo()">Simpan Servo</button>
</div>
<hr>
<div>
  <div class="label">Test Servo:</div>
  <!-- Web UI updated: explicit Servo1 (S1) buttons (tidak bergantung pada inputServo) -->
  <button class="btn btn-orange" onclick="testServo(1,document.getElementById('s1c').value)">S1 CLOSE</button>
  <button class="btn btn-orange" onclick="testServo(1,document.getElementById('s1o').value)">S1 OPEN</button>
  <button class="btn" onclick="testServo(2,document.getElementById('s2c').value)">S2 CLOSE</button>
  <button class="btn" onclick="testServo(2,document.getElementById('s2o').value)">S2 OPEN</button>
  <button class="btn btn-Orchid" onclick="testServo(3,document.getElementById('s3c').value)">S3 CLOSE</button>
  <button class="btn btn-Orchid" onclick="testServo(3,document.getElementById('s3o').value)">S3 OPEN</button>
  <button class="btn btn-blue" onclick="masukTestServoMenu()">Masuk Pengaturan Test Servo</button>
</div>
<hr>
<div>
  <div class="label">Kecepatan Servo Input (detik):</div>
  CLOSE -> OPEN <input id="inputSpeedClose" type="number" min="0.2" max="5" step="0.1">
  OPEN -> CLOSE <input id="inputSpeedOpen" type="number" min="0.2" max="5" step="0.1">
  <button class="btn" onclick="setInputServoSpeed()">Simpan Kecepatan</button>
</div>
<hr>
<div>
  <div class="label">Reset EEPROM:</div>
  <button class="btn btn-red" onclick="resetEEPROM()">RESET SELURUH DATA</button>
  <button class="btn btn-red" style="background:#555;margin-left:8px" onclick="rebootDevice()">REBOOT DEVICE</button>
</div>
<script>
function ambil() {
  fetch('/info').then(r=>r.json()).then(d=>{
    document.getElementById('state').innerText = d.state;
    document.getElementById('kliring').value = d.KLIRING;
    document.getElementById('tunggu').value = d.TUNGGU;
    document.getElementById('kalibrasi').value = d.KALIBRASI;
    document.getElementById('stabil').value = d.STABIL;
    document.getElementById('buzzer').innerText = d.buzzer ? 'ON':'OFF';
    document.getElementById('wireless').innerText = d.wireless ? 'ON':'OFF';
    document.getElementById('inputServoSelect').value = d.inputServo;
    document.getElementById('activeServo').innerText = d.inputServo == 1 ? 'Servo 1' : 'Servo 3';
    if (d.inputServoSpeedClose !== undefined) document.getElementById('inputSpeedClose').value = d.inputServoSpeedClose;
    if (d.inputServoSpeedOpen !== undefined) document.getElementById('inputSpeedOpen').value = d.inputServoSpeedOpen;
  });
}
function aksi(act) {
  fetch('/state?set='+act).then(ambil);
}
function kirimData() {
  let k=document.getElementById('kliring').value;
  let t=document.getElementById('tunggu').value;
  let ka=document.getElementById('kalibrasi').value;
  let s=document.getElementById('stabil').value;
  fetch(`/set?KLIRING=${k}&TUNGGU=${t}&KALIBRASI=${ka}&STABIL=${s}`).then(ambil);
  return false;
}
function toggleBuzzer() {
  fetch('/buzzer?toggle=1').then(ambil);
}
function toggleWireless() {
  fetch('/wireless?toggle=1').then(ambil);
}
function ambilServo() {
  fetch("/servo").then(r=>r.json()).then(d=>{
    document.getElementById('s1c').value = d.S1_CLOSE;
    document.getElementById('s1o').value = d.S1_OPEN;
    document.getElementById('s2c').value = d.S2_CLOSE;
    document.getElementById('s2o').value = d.S2_OPEN;
    document.getElementById('s3c').value = d.S3_CLOSE;
    document.getElementById('s3o').value = d.S3_OPEN;
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
function setInputServo() {
  let val = document.getElementById('inputServoSelect').value;
  fetch(`/inputservo?set=${val}`).then(ambil);
}
function testServo(id, pos) {
  fetch(`/testservo?id=${id}&pos=${pos}`);
}
function setInputServoSpeed() {
  let vc = parseFloat(document.getElementById('inputSpeedClose').value);
  let vo = parseFloat(document.getElementById('inputSpeedOpen').value);
  if (!isNaN(vc)) {
    if (vc < 0.2) vc = 0.2;
    if (vc > 5) vc = 5;
    fetch(`/inputservospeed?setClose=${vc}`);
  }
  if (!isNaN(vo)) {
    if (vo < 0.2) vo = 0.2;
    if (vo > 5) vo = 5;
    fetch(`/inputservospeed?setOpen=${vo}`).then(ambil);
  } else {
    ambil();
  }
}
function resetEEPROM() {
  if(confirm("Yakin reset semua data?")) fetch("/reset").then(()=>{ambil();ambilServo();});
}
function rebootDevice() {
  if(confirm("Yakin ingin me-reboot device sekarang?")) {
    fetch("/reboot").then(()=>{ alert("Perintah reboot dikirim. Perangkat akan melakukan restart."); });
  }
}
function masukTestServoMenu() {
  fetch("/testservomenu")
    .then(r => {
      if (r.status == 200) {
        alert("Masuk menu Test Servo.");
      } else {
        alert("Menu Test Servo hanya bisa diakses saat mesin SIAP UJI.");
      }
    });
}
ambil();
ambilServo();
setInterval(ambil, 3000);
</script>
</div></body></html>
)rawliteral";

// ----------- END index_html[] -----------

// -------- BUZZER --------
void startBlinkBuzzer(unsigned long totalDurasiMs = DEFAULT_BUZZER_TOTAL, unsigned long intervalMs = DEFAULT_BUZZER_INTERVAL) {
  if (!buzzerEnabled) return;
  buzzerBlinkActive = true;
  buzzerBlinkEndTime = millis() + totalDurasiMs;
  buzzerToggleTime = millis() + intervalMs;
  buzzerState = true;
  digitalWrite(BUZZER_PIN, HIGH);
}
void stopBlinkBuzzer() {
  buzzerBlinkActive = false;
  buzzerState = false;
  digitalWrite(BUZZER_PIN, LOW);
}
void handleBlinkBuzzer(unsigned long intervalMs = DEFAULT_BUZZER_INTERVAL) {
  if (!buzzerBlinkActive) return;
  unsigned long now = millis();
  if ((long)(now - buzzerBlinkEndTime) >= 0) {
    stopBlinkBuzzer();
    return;
  }
  if ((long)(now - buzzerToggleTime) >= 0) {
    buzzerState = !buzzerState;
    digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
    buzzerToggleTime = now + intervalMs;
  }
}

// -------- EEPROM HELPER --------
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
void saveWirelessToEEPROM() { EEPROM.write(EEPROM_ADDR_WIRELESS, wirelessEnabled ? 1 : 0); EEPROM.commit(); } // --- WIRELESS MOD ---
void saveInputServoToEEPROM() { EEPROM.write(EEPROM_ADDR_INPUT_SERVO, inputServo); EEPROM.commit(); }
void saveInputServoSpeedsToEEPROM() {
  EEPROM.put(EEPROM_ADDR_INPUT_SPD_CLOSE, inputServoSpeedCloseMs);
  EEPROM.put(EEPROM_ADDR_INPUT_SPD_OPEN,  inputServoSpeedOpenMs);
  EEPROM.commit();
}

void loadParamsFromEEPROM() {
  EEPROM.get(EEPROM_ADDR_KLIRING, KLIRING);
  EEPROM.get(EEPROM_ADDR_TUNGGU, TUNGGU);
  EEPROM.get(EEPROM_ADDR_KALIBRASI, KALIBRASI);
  EEPROM.get(EEPROM_ADDR_STABIL, STABIL);
  paramValues[0] = KLIRING;
  paramValues[1] = TUNGGU;
  paramValues[2] = KALIBRASI;
  paramValues[3] = STABIL;
  if (KLIRING == 0xFFFFFFFF || KLIRING == 0) KLIRING = 10000;
  if (TUNGGU == 0xFFFFFFFF || TUNGGU == 0) TUNGGU = 5000;
  if (KALIBRASI == 0xFFFFFFFF || KALIBRASI == 0) KALIBRASI = 30000;
  if (STABIL == 0xFFFFFFFF || STABIL == 0) STABIL = 10000;
  paramValues[0] = KLIRING;
  paramValues[1] = TUNGGU;
  paramValues[2] = KALIBRASI;
  paramValues[3] = STABIL;
  byte bz = EEPROM.read(EEPROM_ADDR_BUZZER);
  if (bz == 0xFF) buzzerEnabled = true;
  else buzzerEnabled = (bz == 1);
  int s1c = EEPROM.read(EEPROM_ADDR_S1_CLOSE);
  int s1o = EEPROM.read(EEPROM_ADDR_S1_OPEN);
  int s2c = EEPROM.read(EEPROM_ADDR_S2_CLOSE);
  int s2o = EEPROM.read(EEPROM_ADDR_S2_OPEN);
  int s3c = EEPROM.read(EEPROM_ADDR_S3_CLOSE);
  int s3o = EEPROM.read(EEPROM_ADDR_S3_OPEN);
  if (s1c >=0 && s1c <=180) servoDegree[0][0] = s1c;
  if (s1o >=0 && s1o <=180) servoDegree[0][1] = s1o;
  if (s2c >=0 && s2c <=180) servoDegree[1][0] = s2c;
  if (s2o >=0 && s2o <=180) servoDegree[1][1] = s2o;
  if (s3c >=0 && s3c <=180) servoDegree[2][0] = s3c;
  if (s3o >=0 && s3o <=180) servoDegree[2][1] = s3o;
  // --- WIRELESS MOD ---
  byte wl = EEPROM.read(EEPROM_ADDR_WIRELESS);
  if (wl == 0xFF) wirelessEnabled = true;
  else wirelessEnabled = (wl == 1);
  byte inp = EEPROM.read(EEPROM_ADDR_INPUT_SERVO);
  if (inp == 0xFF) inputServo = 1;
  else {
    if (inp == 1 || inp == 3) inputServo = inp;
    else inputServo = 1;
  }
  // load input servo speeds
  unsigned long spdC = 0xFFFFFFFFUL;
  unsigned long spdO = 0xFFFFFFFFUL;
  EEPROM.get(EEPROM_ADDR_INPUT_SPD_CLOSE, spdC);
  EEPROM.get(EEPROM_ADDR_INPUT_SPD_OPEN,  spdO);
  if (spdC == 0xFFFFFFFFUL || spdC == 0) inputServoSpeedCloseMs = 500;
  else if (spdC >= 200 && spdC <= 5000) inputServoSpeedCloseMs = spdC;
  else inputServoSpeedCloseMs = 500;
  if (spdO == 0xFFFFFFFFUL || spdO == 0) inputServoSpeedOpenMs = 500;
  else if (spdO >= 200 && spdO <= 5000) inputServoSpeedOpenMs = spdO;
  else inputServoSpeedOpenMs = 500;
  inputServoSpeedCloseTmp = inputServoSpeedCloseMs;
  inputServoSpeedOpenTmp  = inputServoSpeedOpenMs;
}

void resetEEPROMToDefault() {
  KLIRING = 10000;
  TUNGGU = 5000;
  KALIBRASI = 30000;
  STABIL = 10000;
  buzzerEnabled = true;
  servoDegree[0][0] = 30;
  servoDegree[0][1] = 120;
  servoDegree[1][0] = 30;
  servoDegree[1][1] = 120;
  servoDegree[2][0] = 30;
  servoDegree[2][1] = 120;
  wirelessEnabled = true; // --- WIRELESS MOD ---
  inputServo = 1;
  inputServoSpeedCloseMs = 500;
  inputServoSpeedOpenMs  = 500;
  saveParamsToEEPROM();
  saveBuzzerToEEPROM();
  saveServoDegreeToEEPROM();
  saveWirelessToEEPROM(); // --- WIRELESS MOD ---
  saveInputServoToEEPROM();
  saveInputServoSpeedsToEEPROM();
}

// --------- WIRELESS AKTIF / NON-AKTIF ---------
void applyWirelessStatus() {  // --- WIRELESS MOD ---
  if (wirelessEnabled) {
    if (WiFi.getMode() != WIFI_AP) {
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ssid, password);
    }
  } else {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
  }
}

// -------- SERVO INPUT NON-BLOCKING MOVEMENT --------
// Tracks input servo smooth movement between positions over specified durationMs
bool inputServoMoving = false;
unsigned long inputServoMoveStartTime = 0;
unsigned long inputServoMoveDuration = 0;
int inputServoMoveStartPos = 0;
int inputServoMoveTargetPos = 0;

void startInputServoMove(int targetPos, unsigned long durationMs) {
  targetPos = constrain(targetPos, 0, 180);
  durationMs = constrain(durationMs, 200, 5000);
  // determine current physical pos for input servo
  int currentPos = (inputServo == 1) ? servoPos1 : servoPos3;
  // if already at target, just write and clear
  if (currentPos == targetPos) {
    if (inputServo == 1) { servo1.write(targetPos); servoPos1 = targetPos; }
    else { servo3.write(targetPos); servoPos3 = targetPos; }
    inputServoMoving = false;
    return;
  }
  // If already moving to same target, ignore
  if (inputServoMoving && inputServoMoveTargetPos == targetPos) return;
  inputServoMoving = true;
  inputServoMoveStartTime = millis();
  inputServoMoveDuration = durationMs;
  inputServoMoveStartPos = currentPos;
  inputServoMoveTargetPos = targetPos;
}

void handleInputServoMovement() {
  if (!inputServoMoving) return;
  unsigned long now = millis();
  long elapsed = (long)(now - inputServoMoveStartTime);
  if (elapsed >= (long)inputServoMoveDuration) {
    // finish
    if (inputServo == 1) { servo1.write(inputServoMoveTargetPos); servoPos1 = inputServoMoveTargetPos; }
    else { servo3.write(inputServoMoveTargetPos); servoPos3 = inputServoMoveTargetPos; }
    inputServoMoving = false;
    return;
  }
  float t = (float)elapsed / (float)inputServoMoveDuration;
  int newPos = inputServoMoveStartPos + (int)round((inputServoMoveTargetPos - inputServoMoveStartPos) * t);
  newPos = constrain(newPos, 0, 180);
  if (inputServo == 1) { servo1.write(newPos); servoPos1 = newPos; }
  else { servo3.write(newPos); servoPos3 = newPos; }
}

// sInputPos moves whichever servo is configured as input (servo1 or servo3)
// second parameter moves servo2
void setServoPositions(int sInputPos, int s2pos) {
  sInputPos = constrain(sInputPos, 0, 180);
  s2pos = constrain(s2pos, 0, 180);
  // choose duration based on direction relative to configured close/open positions
  int idx = (inputServo == 1) ? 0 : 2;
  int closePos = servoDegree[idx][0];
  int openPos  = servoDegree[idx][1];
  int curInputPos = (inputServo == 1) ? servoPos1 : servoPos3;
  unsigned long dur = inputServoSpeedCloseMs; // default
  if (curInputPos == closePos && sInputPos == openPos) {
    dur = inputServoSpeedCloseMs; // close -> open
  } else if (curInputPos == openPos && sInputPos == closePos) {
    dur = inputServoSpeedOpenMs; // open -> close
  } else {
    // fallback: pick closer mapping by comparing target position
    if (sInputPos == openPos) dur = inputServoSpeedCloseMs;
    else if (sInputPos == closePos) dur = inputServoSpeedOpenMs;
    else dur = inputServoSpeedCloseMs;
  }
  // start non-blocking move if needed
  int currentPos = curInputPos;
  if (sInputPos != currentPos) startInputServoMove(sInputPos, dur);
  // servo2 immediate
  servo2.write(s2pos); servoPos2 = s2pos;
}

// Helper to directly set physical servo by id (1,2,3)
void setPhysicalServoPos(int id, int pos) {
  pos = constrain(pos, 0, 180);
  if (id == 1) { servo1.write(pos); servoPos1 = pos; }
  else if (id == 2) { servo2.write(pos); servoPos2 = pos; }
  else if (id == 3) { servo3.write(pos); servoPos3 = pos; }
}

// --------- HANDLER API ----------
void handleAPI() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"state\":\"";
    switch(state){
      case SIAP_UJI: json+="SIAP_UJI";break;
      case PERSIAPAN_KLIRING: json+="PERSIAPAN_KLIRING";break;
      case PROSES_KLIRING: json+="PROSES_KLIRING";break;
      case TUNGGU_PROSES: json+="TUNGGU_PROSES";break;
      case PERSIAPAN_KALIBRASI: json+="PERSIAPAN_KALIBRASI";break;
      case PROSES_KALIBRASI: json+="PROSES_KALIBRASI";break;
      case TUNGGU_STABIL: json+="TUNGGU_STABIL";break;
      default: json+="UNK";break;
    }
    json += "\",";
    json += "\"KLIRING\":" + String(KLIRING/1000) + ",";
    json += "\"TUNGGU\":" + String(TUNGGU/1000) + ",";
    json += "\"KALIBRASI\":" + String(KALIBRASI/1000) + ",";
    json += "\"STABIL\":" + String(STABIL/1000) + ",";
    json += "\"buzzer\":" + String(buzzerEnabled ? "true":"false") + ",";
    json += "\"wireless\":" + String(wirelessEnabled ? "true":"false") + ",";
    json += "\"inputServo\":" + String(inputServo) + ",";
    json += "\"inputServoSpeedClose\":" + String((float)inputServoSpeedCloseMs/1000.0) + ",";
    json += "\"inputServoSpeedOpen\":"  + String((float)inputServoSpeedOpenMs/1000.0);
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("KLIRING")) KLIRING = request->getParam("KLIRING")->value().toInt() * 1000;
    if(request->hasParam("TUNGGU")) TUNGGU = request->getParam("TUNGGU")->value().toInt() * 1000;
    if(request->hasParam("KALIBRASI")) KALIBRASI = request->getParam("KALIBRASI")->value().toInt() * 1000;
    if(request->hasParam("STABIL")) STABIL = request->getParam("STABIL")->value().toInt() * 1000;
    paramValues[0]=KLIRING;paramValues[1]=TUNGGU;paramValues[2]=KALIBRASI;paramValues[3]=STABIL;
    saveParamsToEEPROM();
    request->send(200, "text/plain", "OK");
  });

  server.on("/buzzer", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("toggle")) {
      buzzerEnabled = !buzzerEnabled;
      saveBuzzerToEEPROM();
    }
    request->send(200,"text/plain", buzzerEnabled ? "ON":"OFF");
  });

  // --- WIRELESS MOD ---
  server.on("/wireless", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("toggle")) {
      wirelessEnabled = !wirelessEnabled;
      saveWirelessToEEPROM();
      applyWirelessStatus();
    }
    request->send(200,"text/plain", wirelessEnabled ? "ON":"OFF");
  });

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("set")) {
      String v = request->getParam("set")->value();
      if(v=="S") {
        state = SIAP_UJI;
        menuLevel = NONE_MENU;
        state_start_time = millis();
        led1_flash = false;
        led2_flash = false;
        digitalWrite(LED1_PIN, LOW);
        digitalWrite(LED2_PIN, LOW);
        setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][1]);
      }
      else if(v=="A1") state = PERSIAPAN_KLIRING;
      else if(v=="A2") { state = PROSES_KLIRING; state_start_time = millis(); }
      else if(v=="A3") { state = TUNGGU_PROSES; state_start_time = millis(); }
      else if(v=="B1") state = PERSIAPAN_KALIBRASI;
      else if(v=="B2") { state = PROSES_KALIBRASI; state_start_time = millis(); }
      else if(v=="B3") { state = TUNGGU_STABIL; state_start_time = millis(); }
    }
    request->send(200,"text/plain","OK");
  });

  // --------- Servo Degree Get/Set ---------
  server.on("/servo", HTTP_GET, [](AsyncWebServerRequest *request){
    bool updated = false;
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
    request->send(200, "application/json", resp);
  });

  // --------- Test Servo Movement ---------
  // id==1 => servo1 (tidak lagi tergantung inputServo)
  server.on("/testservo", HTTP_GET, [](AsyncWebServerRequest *request){
    int id = request->hasParam("id") ? request->getParam("id")->value().toInt() : 0;
    int pos = request->hasParam("pos") ? request->getParam("pos")->value().toInt() : 0;
    pos = constrain(pos, 0, 180);
    if(id==1) { servo1.write(pos); servoPos1 = pos; }
    else if(id==2) { servo2.write(pos); servoPos2 = pos; }
    else if(id==3) { servo3.write(pos); servoPos3 = pos; }
    request->send(200,"text/plain","OK");
  });

  // --------- Enter Test Servo Menu ---------
  server.on("/testservomenu", HTTP_GET, [](AsyncWebServerRequest *request){
    if (state == SIAP_UJI) {
      enterTestServoMenu();
      request->send(200, "text/plain", "OK");
    } else {
      request->send(403, "text/plain", "Not allowed");
    }
  });

  // --------- Input Servo selection (API) ---------
  server.on("/inputservo", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("set")) {
      String v = request->getParam("set")->value();
      int val = v.toInt();
      if (val == 1 || val == 3) {
        inputServo = val;
        saveInputServoToEEPROM();
      }
    }
    request->send(200,"text/plain", String(inputServo).c_str());
  });

  // --------- Input Servo Speed (API) ---------
  // GET /inputservospeed?setClose=0.5  (seconds)
  // GET /inputservospeed?setOpen=0.6
  // GET /inputservospeed to read
  server.on("/inputservospeed", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("setClose")) {
      float v = request->getParam("setClose")->value().toFloat();
      if (v < 0.2) v = 0.2;
      if (v > 5.0) v = 5.0;
      inputServoSpeedCloseMs = (unsigned long)round(v * 1000.0);
      saveInputServoSpeedsToEEPROM();
    }
    if(request->hasParam("setOpen")) {
      float v = request->getParam("setOpen")->value().toFloat();
      if (v < 0.2) v = 0.2;
      if (v > 5.0) v = 5.0;
      inputServoSpeedOpenMs = (unsigned long)round(v * 1000.0);
      saveInputServoSpeedsToEEPROM();
    }
    // respond with JSON seconds for both
    String resp = "{";
    resp += "\"close\":" + String((float)inputServoSpeedCloseMs/1000.0) + ",";
    resp += "\"open\":"  + String((float)inputServoSpeedOpenMs/1000.0);
    resp += "}";
    request->send(200,"application/json", resp);
  });

  // --------- Reset EEPROM ---------
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    resetEEPROMToDefault();
    applyWirelessStatus(); // --- WIRELESS MOD ---
    request->send(200,"text/plain","RESET OK");
  });

  // --------- Reboot endpoint (safe) ---------
  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
    // set flag to reboot shortly in main loop to avoid blocking webserver callback
    rebootRequested = true;
    rebootAt = millis() + 200;
    request->send(200,"text/plain","REBOOTING");
  });
}

// -------- MENU NAVIGASI --------
// sub-menu control variables for INPUT_SERVO_SPEED_MENU
int inputServoSpeedIndex = 0;          // 0 = CLOSE->OPEN, 1 = OPEN->CLOSE
bool inputServoSpeedEditing = false;   // true apabila sedang mengedit nilai pada sub-menu

void enterMainMenu() { menuLevel = MAIN_MENU; mainMenuIndex = 0; menuModeStart = millis(); }
void enterSettingsMenu() { menuLevel = SETTINGS_MENU; settingsMenuIndex = 0; menuModeStart = millis(); }
void enterDurationMenu() { menuLevel = DURATION_MENU; paramIndex = 0; menuModeStart = millis(); }
void enterBuzzerMenu() { menuLevel = BUZZER_MENU; menuModeStart = millis(); }
void enterTestServoMenu() { menuLevel = TEST_SERVO_MENU; testServoMenuIndex = 0; menuModeStart = millis(); }
void enterWirelessMenu() { menuLevel = WIRELESS_MENU; menuModeStart = millis(); } // --- WIRELESS MOD ---
void enterDataInputMode() {
  dataInputMode = true; dataInputIndex = paramIndex;
  dataInputValues[0] = KLIRING / 1000;
  dataInputValues[1] = TUNGGU / 1000;
  dataInputValues[2] = KALIBRASI / 1000;
  dataInputValues[3] = STABIL / 1000;
}
void enterServoDegreeMenu() {
  menuLevel = SERVO_DEGREE_MENU;
  sdStep = SD_SELECT_SERVO;
  sd_servo_idx = 0;
  sd_pos_idx = 0;
  sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx];
  menuModeStart = millis();
}
void enterInputServoMenu() {
  menuLevel = INPUT_SERVO_MENU;
  menuModeStart = millis();
}
void enterInputServoSpeedMenu() {
  menuLevel = INPUT_SERVO_SPEED_MENU;
  inputServoSpeedIndex = 0; // mulai pada CLOSE->OPEN
  inputServoSpeedEditing = false;
  inputServoSpeedCloseTmp = inputServoSpeedCloseMs;
  inputServoSpeedOpenTmp  = inputServoSpeedOpenMs;
  menuModeStart = millis();
}
void saveDataInput() {
  KLIRING = dataInputValues[0] * 1000;
  TUNGGU = dataInputValues[1] * 1000;
  KALIBRASI = dataInputValues[2] * 1000;
  STABIL = dataInputValues[3] * 1000;
  paramValues[0] = KLIRING;
  paramValues[1] = TUNGGU;
  paramValues[2] = KALIBRASI;
  paramValues[3] = STABIL;
  saveParamsToEEPROM();
  infoSavedFlag = true;
  infoSavedTime = millis();
}

// -------- VIRTUAL BUTTON DRAW MODERN --------
void drawModernButton(int x, int y, const char *label, uint16_t btnColor, uint16_t textColor, bool selected=false) {
  tft.fillRoundRect(x, y, BTN_W, BTN_H, 8, btnColor);
  if (selected)
    tft.drawRoundRect(x, y, BTN_W, BTN_H, 8, COLOR_BTN_SEL);
  tft.setTextColor(textColor, btnColor);
  tft.setFreeFont(&FreeSansBold9pt7b); // Bold modern
  int tw = tft.textWidth(label);
  tft.setCursor(x + (BTN_W-tw)/2, y + 28);
  tft.print(label);
}

void drawModernButtons(int selectedIdx = -1) {
  uint16_t lineColor = TFT_WHITE;  // Warna pemisah seragam
  int lineThickness = 2;              // Ketebalan garis

  // Tombol C ( + )
  tft.fillRect(BTN_C_X, BTN_Y, BTN_W, BTN_H, COLOR_BTN);
  tft.setTextColor(TFT_WHITE, COLOR_BTN);
  tft.setFreeFont(&FreeMono9pt7b);
  tft.setCursor(BTN_C_X + BTN_W / 2 - tft.textWidth("+") / 2, BTN_Y + 25);
  tft.print("+");

  // Garis pemisah antara C dan D
  tft.fillRect(BTN_W - lineThickness / 2, BTN_Y, lineThickness, BTN_H, lineColor);

  // Tombol D ( - )
  tft.fillRect(BTN_D_X, BTN_Y, BTN_W, BTN_H, COLOR_BTN);
  tft.setTextColor(TFT_WHITE, COLOR_BTN);
  tft.setCursor(BTN_D_X + BTN_W / 2 - tft.textWidth("-") / 2, BTN_Y + 25);
  tft.print("-");

  // Garis pemisah antara D dan E
  tft.fillRect(BTN_W * 2 - lineThickness / 2, BTN_Y, lineThickness, BTN_H, lineColor);

  // Tombol E ( M )
  tft.fillRect(BTN_E_X, BTN_Y, BTN_W, BTN_H, COLOR_OK);
  tft.setTextColor(TFT_WHITE, COLOR_OK);
  tft.setCursor(BTN_E_X + BTN_W / 2 - tft.textWidth("M") / 2, BTN_Y + 25);
  tft.print("M");

  // Garis pemisah antara E dan F
  tft.fillRect(BTN_W * 3 - lineThickness / 2, BTN_Y, lineThickness, BTN_H, lineColor);

  // Tombol F ( B )
  tft.fillRect(BTN_F_X, BTN_Y, BTN_W, BTN_H, COLOR_ERR);
  tft.setTextColor(TFT_WHITE, COLOR_ERR);
  tft.setCursor(BTN_F_X + BTN_W / 2 - tft.textWidth("B") / 2, BTN_Y + 25);
  tft.print("B");
}

// -------- PROGRESS BAR --------
void drawProgressBar(int x, int y, int w, int h, float progress, uint16_t barColor, uint16_t bgColor) {
  tft.drawRoundRect(x, 135, w, 25, 6, COLOR_WARN);
  tft.fillRoundRect(x+2, 135+2, w-4, 25-4, 4, bgColor); int pw = (int)((w-4) * progress);
  tft.fillRoundRect(x+2, 135+2, pw, 25-4, 4, barColor);
}

// -------- TOUCH VIRTUAL BUTTON --------
void checkVirtualButton() {
  bool nowTouch = ts.touched();
  unsigned long now = millis();
  if (nowTouch && !lastTouch && now - lastTouchTime > touchCooldown) {
    lastTouchTime = now;
    TS_Point p = ts.getPoint();
    int x = map(p.x, 320, 3890, 0, tft.width());
    int y = map(p.y, 410, 3920, 0, tft.height());
    y = tft.height() - y;
    if (y > BTN_Y && y < BTN_Y + BTN_H) {
      if (x > BTN_C_X && x < BTN_C_X + BTN_W) vBtnC = true;
      else if (x > BTN_D_X && x < BTN_D_X + BTN_W) vBtnD = true;
      else if (x > BTN_E_X && x < BTN_E_X + BTN_W) vBtnE = true;
      else if (x > BTN_F_X && x < BTN_F_X + BTN_W) vBtnF = true;
    }
  }
  lastTouch = nowTouch;
}

// -------- DISPLAY MODERN --------
void drawModernHeader(const char *title) {
  static char prevHeader[32] = "";
  if (strcmp(prevHeader, title) == 0) return;
  strncpy(prevHeader, title, sizeof(prevHeader)-1);
  prevHeader[sizeof(prevHeader)-1] = '\0';
  tft.fillRect(0, 0, tft.width(), 40, COLOR_HEADER);
  tft.setTextColor(TFT_BLACK, COLOR_HEADER);
  tft.setFreeFont(&FreeMonoBold12pt7b);
  int w = tft.textWidth(title);
  int x = (tft.width() - w) / 2; // Rata tengah
  tft.setCursor(x, 28);
  tft.print(title);
}
// -------- PANEL MODERN (ANTI FLICKER) & RATA TENGAH --------
void drawModernPanel(int y, const char *line0, const char *line1) {
  static char prevLine0[32] = "";
  static char prevLine1[32] = "";
  if (strcmp(prevLine0, line0) == 0 && strcmp(prevLine1, line1) == 0) return;
  strncpy(prevLine0, line0, sizeof(prevLine0)-1);
  prevLine0[sizeof(prevLine0)-1] = '\0';
  strncpy(prevLine1, line1, sizeof(prevLine1)-1);
  prevLine1[sizeof(prevLine1)-1] = '\0';

  tft.fillRect(0, y, tft.width(), 70, COLOR_PANEL);

  tft.setTextColor(COLOR_TEXT, COLOR_PANEL);

  // line0 rata tengah
  tft.setFreeFont(&FreeMonoBold12pt7b);
  int w0 = tft.textWidth(line0);
  int x0 = (tft.width() - w0) / 2;
  tft.setCursor(x0, y + 28);
  tft.print(line0);

  // line1 rata tengah
  tft.setFreeFont(&FreeMono9pt7b);
  int w1 = tft.textWidth(line1);
  int x1 = (tft.width() - w1) / 2;
  tft.setCursor(x1, y + 57);
  tft.print(line1);
}

void updateDisplay() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 100) return;
  lastUpdate = millis();

  char line0[32] = "";
  char line1[32] = "";

  if (infoSavedFlag) {
    strncpy(line0, "Data Tersimpan!", 31);
    strncpy(line1, " ", 31);
    if (millis() - infoSavedTime > INFO_SAVED_DISPLAY) infoSavedFlag = false;
  }
  else if (invalidSerialFlag) {
    strncpy(line0, "SERIAL SALAH", 31);
    strncpy(line1, "INPUT!", 31);
    if (millis() - invalidSerialTime > INVALID_SERIAL_DISPLAY) invalidSerialFlag = false;
  }
  else if (menuLevel == CONFIRM_RESET_MENU) {
    strncpy(line0, "Yakin reset ??", 31);
    strncpy(line1, "M:Ya  B:Tidak", 31);
  }
  else if (menuLevel == CONFIRM_REBOOT_MENU) {
    strncpy(line0, "Yakin reboot ??", 31);
    strncpy(line1, "M:Ya  B:Tidak", 31);
  }
  else if (dataInputMode) {
    snprintf(line0, 31, "Input %s", paramNames[dataInputIndex]);
    snprintf(line1, 31, "%lus +/-", dataInputValues[dataInputIndex]);
  }
  else if (menuLevel == MAIN_MENU) {
    strncpy(line0, "MENU", 31);
    snprintf(line1, 31, "> %s", mainMenuItems[mainMenuIndex]);
  }
  else if (menuLevel == SETTINGS_MENU) {
    strncpy(line0, "PENGATURAN", 31);
    snprintf(line1, 31, "> %s", settingsMenuItems[settingsMenuIndex]);
  }
  else if (menuLevel == DURATION_MENU) {
    strncpy(line0, "SET DURASI:", 31);
    snprintf(line1, 31, "%s %lus", paramNames[paramIndex], paramValues[paramIndex]/1000);
  }
  else if (menuLevel == BUZZER_MENU) {
    snprintf(line0, 31, "Buzzer: %s", buzzerEnabled ? "ON " : "OFF");
    snprintf(line1, 31, "+:ON/OFF  M:Save");
  }
  else if (menuLevel == WIRELESS_MENU) { // --- WIRELESS MOD ---
    snprintf(line0, 31, "Wireless: %s", wirelessEnabled ? "ON" : "OFF");
    snprintf(line1, 31, "+:ON/OFF  M:Save");
  }
  else if (menuLevel == TEST_SERVO_MENU) {
    snprintf(line0, 31, "TEST %s", testServoMenuItems[testServoMenuIndex]);
    snprintf(line1, 31, "+:Close  -:Open");
  }
  else if (menuLevel == SERVO_DEGREE_MENU) {
    if (sdStep == SD_SELECT_SERVO) {
      snprintf(line0, 31, "Pilih Servo:");
      snprintf(line1, 31, sd_servo_idx == 0 ? "Servo1 " : (sd_servo_idx == 1 ? "Servo2 " : "Servo3 "));
    } else if (sdStep == SD_SELECT_POS) {
      snprintf(line0, 31, "Posisi Servo :");
      snprintf(line1, 31, sd_pos_idx == 0 ? "CLOSE" : "OPEN");
    } else if (sdStep == SD_EDIT_DEGREE) {
      snprintf(line0, 31, "%s%s", sd_servo_idx == 0 ? "Servo1 " : (sd_servo_idx == 1 ? "Servo2 " : "Servo3 "), sd_pos_idx == 0 ? "CLOSE" : "OPEN");
      snprintf(line1, 31, "Deg:%d +/-  M:Save", sd_degree_tmp);
    }
  }
  else if (menuLevel == INPUT_SERVO_MENU) {
    snprintf(line0, 31, "Pilih Servo Input:");
    snprintf(line1, 31, inputServo == 1 ? "Menggunakan Servo S1" : "Menggunakan Servo S3");
  }
  else if (menuLevel == INPUT_SERVO_SPEED_MENU) {
    if (!inputServoSpeedEditing) {
      if (inputServoSpeedIndex == 0) {
        snprintf(line0, 31, "Kecepatan Servo Input");
        snprintf(line1, 31, "> CLOSE->OPEN   M:Edit");
      } else {
        snprintf(line0, 31, "Kecepatan Servo Input");
        snprintf(line1, 31, "> OPEN->CLOSE   M:Edit");
      }
    } else {
      if (inputServoSpeedIndex == 0) {
        snprintf(line0, 31, "Edit CLOSE->OPEN");
        snprintf(line1, 31, "Durasi: %.1fs +/-  M:Save", (float)inputServoSpeedCloseTmp/1000.0);
      } else {
        snprintf(line0, 31, "Edit OPEN->CLOSE");
        snprintf(line1, 31, "Durasi: %.1fs +/-  M:Save", (float)inputServoSpeedOpenTmp/1000.0);
      }
    }
  }
  else if (millis() - lastSerialInputTime < SERIAL_DISPLAY_TIME && lastSerialInput[0]) {
    strncpy(line0, "Input Serial:", 31);
    strncpy(line1, lastSerialInput, 31);
  }
  else {
    const char* stateText;
    switch(state) {
      case SIAP_UJI: stateText = "SIAP UJI - READY"; break;
      case PERSIAPAN_KLIRING: stateText = "PERSIAPAN KLIRING"; break;
      case PROSES_KLIRING: stateText = "PROSES KLIRING"; break;
      case TUNGGU_PROSES: stateText = "TUNGGU PROSES"; break;
      case PERSIAPAN_KALIBRASI: stateText = "PERSIAPAN KALIBRASI"; break;
      case PROSES_KALIBRASI: stateText = "PROSES_KALIBRASI"; break;
      case TUNGGU_STABIL: stateText = "TUNGGU STABIL"; break;
      default: stateText = "Unknown"; break;
    }
    strncpy(line0, stateText, 31);
    char line2[32] = "";
    if (inputServo == 1) snprintf(line2, 31, "S1:%d  S2:%d ", servoPos1, servoPos2);
    else snprintf(line2, 31, "S3:%d  S2:%d ", servoPos3, servoPos2);
    unsigned long remain = 0, duration = 0, elapsed = 0;
    switch(state) {
      case PROSES_KLIRING:
        elapsed = millis() - state_start_time; duration = KLIRING; remain = (duration - elapsed) / 1000;
        snprintf(line2+strlen(line2), 31-strlen(line2), " T:%lu", remain); break;
      case TUNGGU_PROSES:
        elapsed = millis() - state_start_time; duration = TUNGGU; remain = (duration - elapsed) / 1000;
        snprintf(line2+strlen(line2), 31-strlen(line2), " T:%lu", remain); break;
      case PROSES_KALIBRASI:
        elapsed = millis() - state_start_time; duration = KALIBRASI; remain = (duration - elapsed) / 1000;
        snprintf(line2+strlen(line2), 31-strlen(line2), " T:%lu", remain); break;
      case TUNGGU_STABIL:
        elapsed = millis() - state_start_time; duration = STABIL; remain = (duration - elapsed) / 1000;
        snprintf(line2+strlen(line2), 31-strlen(line2), " T:%lu", remain); break;
      default: break;
    }
    strncpy(line1, line2, 31);
  }

  // ----------- NO FILL SCREEN, ONLY REDRAW PANEL/HEADER -----------
  drawModernHeader("TEST BENCH PORTABLE");
  drawModernPanel(45, line0, line1);

  // Progress bar jika proses berjalan
  bool showProgress = false;
  unsigned long elapsed = 0, duration = 0;
  switch(state) {
    case PROSES_KLIRING:  duration = KLIRING;  elapsed = millis()-state_start_time; showProgress = true; break;
    case TUNGGU_PROSES:   duration = TUNGGU;   elapsed = millis()-state_start_time; showProgress = true; break;
    case PROSES_KALIBRASI:duration = KALIBRASI;elapsed = millis()-state_start_time; showProgress = true; break;
    case TUNGGU_STABIL:   duration = STABIL;   elapsed = millis()-state_start_time; showProgress = true; break;
  }
  int barX = 20, barY = 135, barW = tft.width()-40, barH = 18;
  if (showProgress) {
    float prog = constrain((float)elapsed/duration, 0.0, 1.0);
    drawProgressBar(barX, barY, barW, barH, prog, COLOR_WARN, COLOR_PANEL);
  } else {
    // Hapus bekas progress bar setelah selesai
    tft.fillRect(barX, barY, barW, barH+8, COLOR_BG);
  }

  drawModernButtons();
}

void handleButtons() {
  checkVirtualButton();
  bool btnA = digitalRead(BUTTON_A_PIN);
  bool btnB = digitalRead(BUTTON_B_PIN);

  // Data input mode (tetap prioritas tertinggi)
  if (dataInputMode) {
    if (vBtnE) { dataInputValues[dataInputIndex] = constrain(dataInputValues[dataInputIndex], 1, 1000); saveDataInput(); }
    if (vBtnF) { dataInputMode = false; menuLevel = DURATION_MENU; delay(200); }
    if (vBtnC) dataInputValues[dataInputIndex]++;
    if (vBtnD) { if (dataInputValues[dataInputIndex] > 1) dataInputValues[dataInputIndex]--; }
    if (btnA == LOW && btnA_last == HIGH && dataInputIndex > 0) dataInputIndex--;
    if (btnB == LOW && btnB_last == HIGH && dataInputIndex < 3) dataInputIndex++;
    btnA_last = btnA; btnB_last = btnB;
    vBtnC = vBtnD = vBtnE = vBtnF = false;
    return;
  }

  // Jika tombol BACK ditekan, tangani kasus spesifik per menu/sub-step dulu
  if (vBtnF) {
    // 1) Jika sedang di pengaturan sudut servo, mundur step-by-step
    if (menuLevel == SERVO_DEGREE_MENU) {
      if (sdStep == SD_EDIT_DEGREE) {
        sdStep = SD_SELECT_POS; // mundur dari edit -> pilih posisi
      } else if (sdStep == SD_SELECT_POS) {
        sdStep = SD_SELECT_SERVO; // mundur dari pilih posisi -> pilih servo
      } else { // SD_SELECT_SERVO
        menuLevel = SETTINGS_MENU; // dari level tertinggi menu ini kembali ke Settings
      }
      vBtnC = vBtnD = vBtnE = vBtnF = false;
      return;
    }

    // 2) Jika sedang di menu pilih input servo
    if (menuLevel == INPUT_SERVO_MENU) {
      // langsung kembali ke SETTINGS_MENU
      menuLevel = SETTINGS_MENU;
      vBtnC = vBtnD = vBtnE = vBtnF = false;
      return;
    }

    // 3) Jika sedang di menu test servo, buzzer, wireless, duration, confirm reset, atau speed menu -> kembali ke SETTINGS_MENU
    if (menuLevel == TEST_SERVO_MENU || menuLevel == BUZZER_MENU || menuLevel == WIRELESS_MENU
        || menuLevel == DURATION_MENU || menuLevel == CONFIRM_RESET_MENU || menuLevel == INPUT_SERVO_SPEED_MENU || menuLevel == CONFIRM_REBOOT_MENU) {
      menuLevel = SETTINGS_MENU;
      vBtnC = vBtnD = vBtnE = vBtnF = false;
      return;
    }

    // 4) Jika sedang di SETTINGS_MENU -> kembali ke MAIN_MENU
    if (menuLevel == SETTINGS_MENU) {
      menuLevel = MAIN_MENU;
      vBtnC = vBtnD = vBtnE = vBtnF = false;
      return;
    }

    // 5) Jika sedang di MAIN_MENU -> keluar ke NONE_MENU (kembali ke tampilan utama)
    if (menuLevel == MAIN_MENU) {
      menuLevel = NONE_MENU;
      vBtnC = vBtnD = vBtnE = vBtnF = false;
      return;
    }

    // 6) Fallback untuk NONE_MENU (atau tempat lain): reset ke SIAP_UJI jika belum
    if (menuLevel == NONE_MENU && !dataInputMode) {
      state = SIAP_UJI;
      state_start_time = millis();
      led1_flash = false;
      led2_flash = false;
      digitalWrite(LED1_PIN, LOW);
      digitalWrite(LED2_PIN, LOW);
      setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][1]);
      vBtnC = vBtnD = vBtnE = vBtnF = false;
      btnA_last = btnA; btnB_last = btnB;
      return;
    }

    // Jika terjebak di menu tak terduga, clear flags dan keluar
    vBtnC = vBtnD = vBtnE = vBtnF = false;
    btnA_last = btnA; btnB_last = btnB;
    return;
  }

  // Setelah penanganan BACK, lanjutkan penanganan menu lain (aksi khusus per tombol)
  // Menu Wireless (aksi + save)
  if (menuLevel == WIRELESS_MENU) { 
    if (vBtnC) wirelessEnabled = !wirelessEnabled;
    if (vBtnE) { saveWirelessToEEPROM(); applyWirelessStatus(); infoSavedFlag = true; infoSavedTime = millis(); }
    vBtnC = vBtnE = vBtnF = false; return;
  }

  // Menu pengaturan sudut servo (aksi selain BACK)
  if (menuLevel == SERVO_DEGREE_MENU) {
    if (sdStep == SD_SELECT_SERVO) {
      if (vBtnC) { sd_servo_idx = (sd_servo_idx+1)%3; sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx]; }
      if (vBtnD) { sd_servo_idx = (sd_servo_idx+1)%3; sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx]; }
      if (vBtnE) { sdStep = SD_SELECT_POS; sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx]; }
      vBtnC = vBtnD = vBtnE = vBtnF = false; return;
    }
    else if (sdStep == SD_SELECT_POS) {
      if (vBtnC) { sd_pos_idx = (sd_pos_idx+1)%2; sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx]; }
      if (vBtnD) { sd_pos_idx = (sd_pos_idx+1)%2; sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx]; }
      if (vBtnE) { sdStep = SD_EDIT_DEGREE; sd_degree_tmp = servoDegree[sd_servo_idx][sd_pos_idx]; }
      // BACK pada level ini sudah ditangani di atas
      vBtnC = vBtnD = vBtnE = vBtnF = false; return;
    }
    else if (sdStep == SD_EDIT_DEGREE) {
      if (vBtnC) { if (sd_degree_tmp < 180) sd_degree_tmp++; }
      if (vBtnD) { if (sd_degree_tmp > 0) sd_degree_tmp--; }
      if (vBtnE) { servoDegree[sd_servo_idx][sd_pos_idx] = sd_degree_tmp; saveServoDegreeToEEPROM(); infoSavedFlag = true; infoSavedTime = millis(); }
      // BACK pada level ini sudah ditangani di atas
      vBtnC = vBtnD = vBtnE = vBtnF = false; return;
    }
    vBtnC = vBtnD = vBtnE = vBtnF = false; return;
  }

  // Menu input servo selection (aksi selain BACK)
  if (menuLevel == INPUT_SERVO_MENU) {
    if (vBtnC || vBtnD) {
      inputServo = (inputServo == 1) ? 3 : 1;
    }
    if (vBtnE) {
      saveInputServoToEEPROM();
      infoSavedFlag = true; infoSavedTime = millis();
    }
    vBtnC = vBtnD = vBtnE = vBtnF = false; return;
  }

  // Menu set kecepatan servo input (dengan sub-menu & edit mode)
  if (menuLevel == INPUT_SERVO_SPEED_MENU) {
    if (!inputServoSpeedEditing) {
      // navigasi antar sub-item
      if (vBtnC) { inputServoSpeedIndex = (inputServoSpeedIndex - 1 + 2) % 2; menuModeStart = millis(); }
      if (vBtnD) { inputServoSpeedIndex = (inputServoSpeedIndex + 1) % 2; menuModeStart = millis(); }
      if (vBtnE) {
        // masuk edit mode untuk item terpilih
        inputServoSpeedEditing = true;
      }
      if (vBtnF) {
        // kembali ke SETTINGS_MENU tanpa perubahan
        menuLevel = SETTINGS_MENU;
      }
    } else {
      // sedang di mode edit: C = +, D = -, E = save, F = cancel
      if (inputServoSpeedIndex == 0) {
        if (vBtnC) { if (inputServoSpeedCloseTmp + 100 <= 5000) inputServoSpeedCloseTmp += 100; }
        if (vBtnD) { if (inputServoSpeedCloseTmp >= 300) inputServoSpeedCloseTmp -= 100; } // akan di-constrain saat save
        if (vBtnE) {
          inputServoSpeedCloseMs = constrain(inputServoSpeedCloseTmp, 200, 5000);
          saveInputServoSpeedsToEEPROM();
          infoSavedFlag = true; infoSavedTime = millis();
          inputServoSpeedEditing = false;
        }
        if (vBtnF) {
          // batal edit, kembalikan tmp dan kembali ke pilihan
          inputServoSpeedCloseTmp = inputServoSpeedCloseMs;
          inputServoSpeedEditing = false;
        }
      } else { // index == 1 -> OPEN->CLOSE
        if (vBtnC) { if (inputServoSpeedOpenTmp + 100 <= 5000) inputServoSpeedOpenTmp += 100; }
        if (vBtnD) { if (inputServoSpeedOpenTmp >= 300) inputServoSpeedOpenTmp -= 100; }
        if (vBtnE) {
          inputServoSpeedOpenMs = constrain(inputServoSpeedOpenTmp, 200, 5000);
          saveInputServoSpeedsToEEPROM();
          infoSavedFlag = true; infoSavedTime = millis();
          inputServoSpeedEditing = false;
        }
        if (vBtnF) {
          inputServoSpeedOpenTmp = inputServoSpeedOpenMs;
          inputServoSpeedEditing = false;
        }
      }
    }
    vBtnC = vBtnD = vBtnE = vBtnF = false;
    return;
  }

  // Menu buzzer
  if (menuLevel == BUZZER_MENU) {
    if (vBtnC) buzzerEnabled = !buzzerEnabled;
    if (vBtnE) { saveBuzzerToEEPROM(); infoSavedFlag = true; infoSavedTime = millis(); }
    vBtnC = vBtnE = vBtnF = false; return;
  }

  // Menu settings (navigasi item)
  if (menuLevel == SETTINGS_MENU) {
    int settingsCount = 9; // tetap 9 item
    if (vBtnC) { settingsMenuIndex = (settingsMenuIndex - 1 + settingsCount) % settingsCount; menuModeStart = millis(); }
    if (vBtnD) { settingsMenuIndex = (settingsMenuIndex + 1) % settingsCount; menuModeStart = millis(); }
    if (vBtnE) {
      if (settingsMenuIndex == 0) { enterTestServoMenu(); vBtnE = false; return; }           // Test Servo
      else if (settingsMenuIndex == 1) { enterServoDegreeMenu(); vBtnE = false; return; }   // Set Posisi Servo
      else if (settingsMenuIndex == 2) { enterInputServoMenu(); vBtnE = false; return; }    // Pilih Servo Input
      else if (settingsMenuIndex == 3) { enterInputServoSpeedMenu(); vBtnE = false; return; } // Set Kecepatan Servo Input
      else if (settingsMenuIndex == 4) { enterDurationMenu(); vBtnE = false; return; }      // Set Durasi Pengujian
      else if (settingsMenuIndex == 5) { enterBuzzerMenu(); vBtnE = false; return; }        // Set Buzzer
      else if (settingsMenuIndex == 6) { enterWirelessMenu(); vBtnE = false; return; }      // Wireless
      else if (settingsMenuIndex == 7) { menuLevel = CONFIRM_REBOOT_MENU; vBtnE = false; return; } // Reboot
      else if (settingsMenuIndex == 8) { menuLevel = CONFIRM_RESET_MENU; vBtnE = false; return; }  // Reset EEPROM
    }
    vBtnC = vBtnD = vBtnE = vBtnF = false; return;
  }

  // Menu konfirmasi reset EEPROM
  if (menuLevel == CONFIRM_RESET_MENU) {
    if (vBtnE) { resetEEPROMToDefault(); applyWirelessStatus(); infoSavedFlag = true; infoSavedTime = millis(); menuLevel = SETTINGS_MENU; }
    if (vBtnF) { menuLevel = SETTINGS_MENU; }
    vBtnE = vBtnF = false; return;
  }

  // Menu konfirmasi reboot
  if (menuLevel == CONFIRM_REBOOT_MENU) {
    if (vBtnE) {
      // set reboot flag, will be executed in main loop shortly
      rebootRequested = true;
      rebootAt = millis() + 200;
      infoSavedFlag = true;
      infoSavedTime = millis();
      menuLevel = SETTINGS_MENU;
    }
    if (vBtnF) { menuLevel = SETTINGS_MENU; }
    vBtnE = vBtnF = false; return;
  }

  // Menu utama dan sub-menu
  if (state == SIAP_UJI) {
    if (menuLevel == NONE_MENU && vBtnE) { enterMainMenu(); vBtnE = false; return; }
    if (menuLevel == MAIN_MENU) {
      if (vBtnC) { mainMenuIndex = (mainMenuIndex - 1 + 2) % 2; menuModeStart = millis(); }
      if (vBtnD) { mainMenuIndex = (mainMenuIndex + 1) % 2; menuModeStart = millis(); }
      if (vBtnE) {
        if (mainMenuIndex == 1) { enterSettingsMenu(); vBtnE = false; return; }
        else { menuLevel = NONE_MENU; }
      }
      vBtnC = vBtnD = vBtnE = vBtnF = false; return;
    }
    if (menuLevel == DURATION_MENU) {
      if (vBtnC) { paramIndex = (paramIndex - 1 + paramLength) % paramLength; menuModeStart = millis(); }
      if (vBtnD) { paramIndex = (paramIndex + 1) % paramLength; menuModeStart = millis(); }
      if (vBtnE) { menuLevel = NONE_MENU; enterDataInputMode(); }
      vBtnC = vBtnD = vBtnE = vBtnF = false; return;
    }
    if (menuLevel == TEST_SERVO_MENU) {
      if (vBtnC) { // CLOSE
        if (testServoMenuIndex == 0) setPhysicalServoPos(1, servoDegree[0][0]); // menggunakan servo1 langsung
        else if (testServoMenuIndex == 1) setPhysicalServoPos(2, servoDegree[1][0]);
        else if (testServoMenuIndex == 2) setPhysicalServoPos(3, servoDegree[2][0]);
      }
      if (vBtnD) { // OPEN
        if (testServoMenuIndex == 0) setPhysicalServoPos(1, servoDegree[0][1]); // menggunakan servo1 langsung
        else if (testServoMenuIndex == 1) setPhysicalServoPos(2, servoDegree[1][1]);
        else if (testServoMenuIndex == 2) setPhysicalServoPos(3, servoDegree[2][1]);
      }
      if (vBtnE) { testServoMenuIndex = (testServoMenuIndex + 1) % 3; menuModeStart = millis(); }
      vBtnC = vBtnD = vBtnE = vBtnF = false; return;
    }
  }

  // Tombol A/B untuk proses KLIRING/KALIBRASI (tetap hardware)
  if (btnA == LOW && btnA_last == HIGH) {
    btnA_down_time = millis();
    btnA_handled = false;
  } else if (btnA == HIGH && btnA_last == LOW) {
    unsigned long held = millis() - btnA_down_time;
    if (!btnA_handled && menuLevel == NONE_MENU && !dataInputMode) {
      if (held < LONG_PRESS) { state = PERSIAPAN_KLIRING; state_start_time = millis(); }
      else { state = PROSES_KLIRING; state_start_time = millis(); }
      btnA_handled = true;
    }
  }
  btnA_last = btnA;

  if (btnB == LOW && btnB_last == HIGH) {
    btnB_down_time = millis();
    btnB_handled = false;
  } else if (btnB == HIGH && btnB_last == LOW) {
    unsigned long held = millis() - btnB_down_time;
    if (!btnB_handled && menuLevel == NONE_MENU && !dataInputMode) {
      if (held < LONG_PRESS) { state = PERSIAPAN_KALIBRASI; state_start_time = millis(); }
      else { state = PROSES_KALIBRASI; state_start_time = millis(); }
      btnB_handled = true;
    }
  }
  btnB_last = btnB;
}

void handlePhysicalButtons() {
    unsigned long now = millis();
    bool btnA_read = digitalRead(BUTTON_A_PIN);
    bool btnB_read = digitalRead(BUTTON_B_PIN);

    // -------- Tombol A --------
    if (btnA_read != btnA_stable) btnA_lastTime = now; // reset debounce timer

    if (now - btnA_lastTime > DEBOUNCE_DELAY) {
        if (btnA_read != btnA_stable) {
            btnA_stable = btnA_read;

            if (btnA_stable == LOW) {
                // Tombol A ditekan
                btnA_down_time = now;
                btnA_handled = false;
            } else { // tombol dilepas
                unsigned long held = now - btnA_down_time;
                if (!btnA_handled && menuLevel == NONE_MENU && !dataInputMode) {
                    if (held < LONG_PRESS) state = PERSIAPAN_KLIRING;
                    else state = PROSES_KLIRING;
                    state_start_time = now;
                    btnA_handled = true;
                }
            }
        }
    }

    // -------- Tombol B --------
    if (btnB_read != btnB_stable) btnB_lastTime = now; // reset debounce timer

    if (now - btnB_lastTime > DEBOUNCE_DELAY) {
        if (btnB_read != btnB_stable) {
            btnB_stable = btnB_read;

            if (btnB_stable == LOW) {
                // Tombol B ditekan
                btnB_down_time = now;
                btnB_handled = false;
            } else { // tombol dilepas
                unsigned long held = now - btnB_down_time;
                if (!btnB_handled && menuLevel == NONE_MENU && !dataInputMode) {
                    if (held < LONG_PRESS) state = PERSIAPAN_KALIBRASI;
                    else state = PROSES_KALIBRASI;
                    state_start_time = now;
                    btnB_handled = true;
                }
            }
        }
    }
}


void handleStateMachine() {
  unsigned long now = millis();
  static bool buzzerDoneLocal = false;
  if (menuLevel == TEST_SERVO_MENU) return;
  switch(state) {
    case SIAP_UJI:
      digitalWrite(LED1_PIN, LOW);
      digitalWrite(LED2_PIN, LOW);
      setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][1]);
      led1_flash = led2_flash = false;
      buzzerDoneLocal = false;
      break;
    case PERSIAPAN_KLIRING:
      digitalWrite(LED1_PIN, HIGH);
      digitalWrite(LED2_PIN, LOW);
      setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][1]);
      led1_flash = false;
      buzzerDoneLocal = false;
      break;
    case PROSES_KLIRING:
      led1_flash = true;
      digitalWrite(LED2_PIN, LOW);
      setServoPositions(servoDegree[(inputServo==1?0:2)][1], servoDegree[1][1]);
      buzzerDoneLocal = false;
      if (now - state_start_time >= KLIRING) { state = TUNGGU_PROSES; state_start_time = now; led1_flash = false; }
      break;
    case TUNGGU_PROSES:
      digitalWrite(LED1_PIN, HIGH);
      digitalWrite(LED2_PIN, LOW);
      setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][1]);
      buzzerDoneLocal = false;
      if (now - state_start_time >= TUNGGU) { digitalWrite(LED1_PIN, LOW); state = SIAP_UJI; }
      break;
    case PERSIAPAN_KALIBRASI:
      digitalWrite(LED1_PIN, LOW);
      digitalWrite(LED2_PIN, HIGH);
      setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][0]);
      led2_flash = false;
      buzzerDoneLocal = false;
      break;
    case PROSES_KALIBRASI:
      led2_flash = true;
      digitalWrite(LED1_PIN, LOW);
      setServoPositions(servoDegree[(inputServo==1?0:2)][1], servoDegree[1][0]);
      buzzerDoneLocal = false;
      if (now - state_start_time >= KALIBRASI) { state = TUNGGU_STABIL; state_start_time = now; led2_flash = false; buzzerDoneLocal = false; }
      break;
    case TUNGGU_STABIL:
      digitalWrite(LED1_PIN, LOW);
      digitalWrite(LED2_PIN, HIGH);
      setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][0]);
      if (!buzzerDoneLocal && buzzerEnabled) {
        startBlinkBuzzer(DEFAULT_BUZZER_TOTAL, DEFAULT_BUZZER_INTERVAL);
        buzzerDoneLocal = true;
      }
      if (now - state_start_time >= STABIL) {
        digitalWrite(LED2_PIN, LOW);
        setServoPositions(servoDegree[(inputServo==1?0:2)][0], servoDegree[1][1]);
        state = SIAP_UJI;
        buzzerDoneLocal = false;
      }
      break;
    default:
      state = SIAP_UJI;
      buzzerDoneLocal = false;
      break;
  }
}
void handleFlashing() {
  unsigned long now = millis();
  if (led1_flash) { if (now - led1_flash_timer > 500) { led1_flash_timer = now; led1_on = !led1_on; digitalWrite(LED1_PIN, led1_on ? HIGH : LOW); } }
  if (led2_flash) { if (now - led2_flash_timer > 500) { led2_flash_timer = now; led2_on = !led2_on; digitalWrite(LED2_PIN, led2_on ? HIGH : LOW); } }
}

void handleSerialInput() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.equalsIgnoreCase("INFO")) {
      Serial.println(F("==== KONFIGURASI TERKINI ===="));
      Serial.print(F("KLIRING    : ")); Serial.println(KLIRING / 1000);
      Serial.print(F("TUNGGU     : ")); Serial.println(TUNGGU / 1000);
      Serial.print(F("KALIBRASI  : ")); Serial.println(KALIBRASI / 1000);
      Serial.print(F("STABIL     : ")); Serial.println(STABIL / 1000);
      Serial.print(F("BUZZER     : ")); Serial.println(buzzerEnabled ? "ON" : "OFF");
      Serial.print(F("WIRELESS   : ")); Serial.println(wirelessEnabled ? "ON" : "OFF"); // --- WIRELESS MOD ---
      Serial.print(F("INPUT SERVO: ")); Serial.println(inputServo);
      Serial.print(F("SPD C->O   : ")); Serial.println((float)inputServoSpeedCloseMs/1000.0);
      Serial.print(F("SPD O->C   : ")); Serial.println((float)inputServoSpeedOpenMs/1000.0);
      Serial.print(F("S1 CLOSE   : ")); Serial.println(servoDegree[0][0]);
      Serial.print(F("S1 OPEN    : ")); Serial.println(servoDegree[0][1]);
      Serial.print(F("S2 CLOSE   : ")); Serial.println(servoDegree[1][0]);
      Serial.print(F("S2 OPEN    : ")); Serial.println(servoDegree[1][1]);
      Serial.print(F("S3 CLOSE   : ")); Serial.println(servoDegree[2][0]);
      Serial.print(F("S3 OPEN    : ")); Serial.println(servoDegree[2][1]);
      Serial.println(F("============================="));
      return;
    }
    if (input.equalsIgnoreCase("RESET")) {
      resetEEPROMToDefault(); infoSavedFlag = true;
      infoSavedTime = millis();
      menuLevel = SETTINGS_MENU;
      Serial.println(F("RESET OKE — REBOOT..."));
      delay(200);
      ESP.restart();
      while(1);
      return;
    }
    if (input.equalsIgnoreCase("REBOOT")) {
      rebootRequested = true;
      rebootAt = millis() + 200;
      Serial.println(F("REBOOT DIMINTA..."));
      return;
    }
    if (input.startsWith("STATE ")) {
      String stateStr = input.substring(6); stateStr.trim();
      if (stateStr.equalsIgnoreCase("S")) { state = SIAP_UJI; Serial.println(F("State diubah ke SIAP_UJI")); }
      else if (stateStr.equalsIgnoreCase("A1")) { state = PERSIAPAN_KLIRING; Serial.println(F("State diubah ke PERSIAPAN_KLIRING")); }
      else if (stateStr.equalsIgnoreCase("A2")) { state = PROSES_KLIRING; state_start_time = millis(); Serial.println(F("State diubah ke PROSES_KLIRING")); }
      else if (stateStr.equalsIgnoreCase("A3")) { state = TUNGGU_PROSES; state_start_time = millis(); Serial.println(F("State diubah ke TUNGGU_PROSES")); }
      else if (stateStr.equalsIgnoreCase("B1")) { state = PERSIAPAN_KALIBRASI; Serial.println(F("State diubah ke PERSIAPAN_KALIBRASI")); }
      else if (stateStr.equalsIgnoreCase("B2")) { state = PROSES_KALIBRASI; state_start_time = millis(); Serial.println(F("State diubah ke PROSES_KALIBRASI")); }
      else if (stateStr.equalsIgnoreCase("B3")) { state = TUNGGU_STABIL; state_start_time = millis(); Serial.println(F("State diubah ke TUNGGU_STABIL")); }
      else { Serial.println(F("Nama state tidak dikenal. Gunakan: (S)SIAP_UJI, (A1)PERSIAPAN_KLIRING, (A2)PROSES_KLIRING, (A3)TUNGGU_PROSES, (B1)PERSIAPAN_KALIBRASI, (B2)PROSES_KALIBRASI, (B3)TUNGGU_STABIL")); }
      return;
    }
    int spaceIdx = input.indexOf(' ');
    String stateStr = input;
    unsigned long duration = 0;
    bool valid = false;
    if (spaceIdx != -1) {
      stateStr = input.substring(0, spaceIdx);
      String durStr = input.substring(spaceIdx + 1);
      duration = durStr.toInt();
    }
    if (stateStr == "KLIRING" && duration > 0) { KLIRING = duration * 1000; valid = true; }
    else if (stateStr == "TUNGGU" && duration > 0) { TUNGGU = duration * 1000; valid = true; }
    else if (stateStr == "KALIBRASI" && duration > 0) { KALIBRASI = duration * 1000; valid = true; }
    else if (stateStr == "STABIL" && duration > 0) { STABIL = duration * 1000; valid = true; }
    paramValues[0] = KLIRING;
    paramValues[1] = TUNGGU;
    paramValues[2] = KALIBRASI;
    paramValues[3] = STABIL;
    if (valid) saveParamsToEEPROM();
    strncpy(lastSerialInput, input.c_str(), sizeof(lastSerialInput) - 1);
    lastSerialInput[sizeof(lastSerialInput) - 1] = '\0';
    lastSerialInputTime = millis();
    if (!valid) { invalidSerialFlag = true; invalidSerialTime = millis(); Serial.print(F("Input serial SALAH - KLIRING, TUNGGU, KALIBRASI, STABIL -")); Serial.println(input); }
    else { Serial.print(F("Input serial OK: kirim ")); Serial.println(input); }
  }

}
void setup() {
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo3.attach(SERVO3_PIN);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(COLOR_BG);
  touchSPI.begin(TOUCH_CLK, TOUCH_DOUT, TOUCH_DIN, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);
  Serial.begin(115200);
  EEPROM.begin(128);
  Serial.println(F("Kirim: [NAMA_STATE] SPASI [DURASI] --> untuk mengubah durasi (KLIRING, TUNGGU, KALIBRASI, STABIL)"));
  Serial.println(F("Kirim: [STATE] SPASI [INISIAL_STATE] --> untuk mengubah status mesin (S)SIAP_UJI, (A1)PERSIAPAN_KLIRING, (A2)PROSES_KLIRING, (A3)TUNGGU_PROSES, (B1)PERSIAPAN_KALIBRASI, (B2)PROSES_KALIBRASI, (B3)TUNGGU_STABIL"));
  Serial.println(F("Kirim: INFO --> untuk melihat konfigurasi."));
  Serial.println(F("Kirim: RESET --> untuk me-reset ke setingan awal."));
  Serial.println(F("Kirim: REBOOT --> untuk me-restart system."));
  loadParamsFromEEPROM();
  // set initial positions based on stored degrees and selected input servo
  if (inputServo == 1) {
    setServoPositions(servoDegree[0][0], servoDegree[1][1]);
  } else {
    setServoPositions(servoDegree[2][0], servoDegree[1][1]);
  }
  applyWirelessStatus(); // --- WIRELESS MOD ---
  handleAPI();
  server.begin();
}

void loop() {
  handleSerialInput();
  handleButtons();
  handlePhysicalButtons();
  handleStateMachine();
  handleFlashing();
  handleBlinkBuzzer();
  // always update input servo movement (non-blocking interpolation)
  handleInputServoMovement();
  updateDisplay();

  // handle reboot request safely in main loop
  if (rebootRequested && (long)(millis() - rebootAt) >= 0) {
    Serial.println(F("Rebooting now..."));
    delay(200);
    ESP.restart();
    while (1) ;
  }
}
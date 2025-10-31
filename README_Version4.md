# TestBench Portable - Kontrol & Web UI

TestBench Portable adalah firmware untuk ESP32 yang mengendalikan sebuah test-bench berbasis servo dengan antarmuka layar TFT + touch dan Web UI melalui Access Point (mode AP). Proyek ini memudahkan pengaturan durasi uji, posisi servo, kecepatan servo input, pengujian servo, pengaturan buzzer/wireless, dan menyediakan fungsi reboot/reset EEPROM.

## About
TestBench Portable dikembangkan sebagai solusi sederhana dan portabel untuk melakukan pengujian mekanis atau elektronis yang memerlukan siklus gerakan servo terautomasi. Tujuannya adalah menyediakan antarmuka lokal (TFT touch) dan jarak jauh (Web UI) sehingga operator dapat mengkonfigurasi dan menjalankan pengujian tanpa perlu mengubah kode tiap kali parameter perlu disesuaikan.

- Status: Working prototype
- Bahasa: C++ (Arduino framework untuk ESP32)
- Maintainer: putuajah
- Kontak: (gunakan issue/pull request di repository GitHub untuk umpan balik atau laporan bug)
- Tujuan: memudahkan debugging, kalibrasi, dan rutinitas pengujian pada perangkat mekanis sederhana yang memakai servo.

---

## Ringkasan fitur
- Tampilan modern pada TFT dengan navigasi tombol virtual (+,-,M,B) dan dua tombol fisik A/B.
- Web UI responsif untuk pengaturan cepat (durasi, sudut servo, test servo, kecepatan input, toggle buzzer/wireless, reboot, reset EEPROM).
- Gerakan servo input non-blocking dengan interpolasi (durasi configurable).
- Simpan pengaturan di EEPROM (durasi, buzzer, posisi servo, wireless, servo input, kecepatan input).
- Endpoint HTTP API untuk integrasi / automasi.
- Konfirmasi sebelum operasi destruktif (Reset EEPROM, Reboot).

---

## Perangkat keras yang digunakan
- ESP32 (board apa pun yang kompatibel, contoh: ESP32 DevKit)
- Layar TFT 2.4" (TFT_eSPI)
- Touch controller XPT2046 (touch SPI custom)
- 3 x Servo (servo1, servo2, servo3)
- 2 x LED indikator (LED1, LED2)
- Buzzer
- 2 x tombol fisik (BUTTON_A, BUTTON_B)
- Wiring sesuai pin di source (lihat bagian Wiring)

Pin default (sesuaikan di kode jika berbeda):
- BUTTON_A_PIN = GPIO4
- BUTTON_B_PIN = GPIO5
- LED1_PIN = GPIO16
- LED2_PIN = GPIO17
- SERVO1_PIN = GPIO12
- SERVO2_PIN = GPIO13
- SERVO3_PIN = GPIO14
- BUZZER_PIN = GPIO27
- TOUCH_CS = GPIO35, TOUCH_IRQ = GPIO34, TOUCH_CLK = GPIO25, TOUCH_DOUT = GPIO26 (MISO), TOUCH_DIN = GPIO32 (MOSI)

---

## Wiring Diagram (Gambar dan Penjelasan)
Di bawah ini terdapat diagram wiring sederhana yang menunjukkan koneksi utama antara ESP32, servo, layar TFT, touch controller, LED, buzzer, dan tombol. Diagram ini disediakan sebagai inline SVG sehingga akan tampil langsung di GitHub. Jika Anda ingin menyertakan file gambar terpisah (PNG/SVG) untuk distribusi atau pencetakan, tambahkan file `wiring.png` atau `wiring.svg` ke root repository dan README akan menunjukkannya.

<!-- Inline SVG wiring diagram (simple, schematic) -->
<div align="center">
<svg width="720" height="360" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 720 360" style="max-width:100%;border:1px solid #ddd;background:#fff">
  <!-- ESP32 block -->
  <rect x="30" y="40" width="220" height="280" rx="8" fill="#f7f9fc" stroke="#333"/>
  <text x="140" y="65" font-family="sans-serif" font-size="16" text-anchor="middle" fill="#111">ESP32</text>
  <!-- pins list -->
  <text x="50" y="100" font-family="monospace" font-size="12" fill="#222">GPIO4  - BUTTON_A</text>
  <text x="50" y="120" font-family="monospace" font-size="12" fill="#222">GPIO5  - BUTTON_B</text>
  <text x="50" y="140" font-family="monospace" font-size="12" fill="#222">GPIO12 - SERVO1</text>
  <text x="50" y="160" font-family="monospace" font-size="12" fill="#222">GPIO13 - SERVO2</text>
  <text x="50" y="180" font-family="monospace" font-size="12" fill="#222">GPIO14 - SERVO3</text>
  <text x="50" y="200" font-family="monospace" font-size="12" fill="#222">GPIO16 - LED1</text>
  <text x="50" y="220" font-family="monospace" font-size="12" fill="#222">GPIO17 - LED2</text>
  <text x="50" y="240" font-family="monospace" font-size="12" fill="#222">GPIO27 - BUZZER</text>
  <text x="50" y="260" font-family="monospace" font-size="12" fill="#222">GPIO35 - TOUCH_CS</text>
  <text x="50" y="280" font-family="monospace" font-size="12" fill="#222">GPIO34 - TOUCH_IRQ</text>
  <text x="50" y="300" font-family="monospace" font-size="12" fill="#222">GPIO25 - TOUCH_CLK</text>
  <text x="50" y="320" font-family="monospace" font-size="12" fill="#222">GPIO26 - TOUCH_DOUT</text>
  <text x="50" y="340" font-family="monospace" font-size="12" fill="#222">GPIO32 - TOUCH_DIN</text>

  <!-- TFT block -->
  <rect x="280" y="40" width="160" height="120" rx="6" fill="#eef6ff" stroke="#333"/>
  <text x="360" y="65" font-family="sans-serif" font-size="14" text-anchor="middle" fill="#111">TFT (TFT_eSPI)</text>
  <text x="360" y="88" font-family="monospace" font-size="12" text-anchor="middle" fill="#111">SPI (SCK/MOSI/MISO/CS)</text>

  <!-- Touch controller -->
  <rect x="280" y="180" width="160" height="60" rx="6" fill="#fff7e6" stroke="#333"/>
  <text x="360" y="202" font-family="sans-serif" font-size="14" text-anchor="middle" fill="#111">XPT2046 Touch</text>
  <text x="360" y="222" font-family="monospace" font-size="12" text-anchor="middle" fill="#111">TOUCH_CS/IRQ/CLK/DOUT/DIN</text>

  <!-- Servos -->
  <rect x="480" y="40" width="200" height="60" rx="6" fill="#f0fff4" stroke="#333"/>
  <text x="580" y="62" font-family="sans-serif" font-size="14" text-anchor="middle" fill="#111">Servos</text>
  <text x="580" y="82" font-family="monospace" font-size="12" text-anchor="middle" fill="#111">S1(GPIO12) S2(GPIO13) S3(GPIO14)</text>

  <!-- LEDs and buzzer -->
  <rect x="480" y="130" width="200" height="80" rx="6" fill="#fff0f0" stroke="#333"/>
  <text x="580" y="154" font-family="sans-serif" font-size="14" text-anchor="middle" fill="#111">LED / Buzzer / Buttons</text>
  <text x="580" y="176" font-family="monospace" font-size="12" text-anchor="middle" fill="#111">LED1(GPIO16) LED2(GPIO17)</text>
  <text x="580" y="196" font-family="monospace" font-size="12" text-anchor="middle" fill="#111">BUZZER(GPIO27)</text>
  <text x="580" y="216" font-family="monospace" font-size="12" text-anchor="middle" fill="#111">BTN_A(GPIO4) BTN_B(GPIO5)</text>

  <!-- wiring lines from ESP32 to TFT -->
  <line x1="250" y1="100" x2="280" y2="68" stroke="#888" stroke-width="2" marker-end="url(#arrow)"/>
  <line x1="250" y1="260" x2="280" y2="200" stroke="#888" stroke-width="2" marker-end="url(#arrow)"/>

  <!-- wiring lines to servos -->
  <line x1="250" y1="150" x2="480" y2="70" stroke="#1b9e77" stroke-width="2" marker-end="url(#arrow)"/>
  <line x1="250" y1="170" x2="480" y2="90" stroke="#1b9e77" stroke-width="2" marker-end="url(#arrow)"/>
  <line x1="250" y1="190" x2="480" y2="110" stroke="#1b9e77" stroke-width="2" marker-end="url(#arrow)"/>

  <!-- wiring to LEDs/Buzzer/Buttons -->
  <line x1="250" y1="200" x2="480" y2="160" stroke="#d95f02" stroke-width="2" marker-end="url(#arrow)"/>
  <line x1="250" y1="220" x2="480" y2="180" stroke="#d95f02" stroke-width="2" marker-end="url(#arrow)"/>
  <line x1="250" y1="240" x2="480" y2="200" stroke="#d95f02" stroke-width="2" marker-end="url(#arrow)"/>

  <!-- marker arrow -->
  <defs>
    <marker id="arrow" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
      <path d="M0,0 L6,3 L0,6 z" fill="#666" />
    </marker>
  </defs>

  <!-- footer note -->
  <text x="360" y="344" font-family="sans-serif" font-size="10" text-anchor="middle" fill="#666">Catatan: gambar ini bersifat skematis — periksa polaritas dan power servo (gunakan sumber 5V yang cukup)</text>
</svg>
</div>

**Keterangan penting wiring:**
- Servo: sambungkan Vcc servo (5V) ke sumber power 5V terpisah bila memungkinkan; sambungkan GND servo ke GND ESP32 bersama.
- Sinyal servo: ke GPIO12 (S1), GPIO13 (S2), GPIO14 (S3).
- Layar TFT: gunakan SPI pins (SCK, MOSI, MISO) sesuai konfigurasi TFT_eSPI Anda; pastikan pin CS/DC/RESET dikonfigurasi pada library.
- Touch XPT2046: CS ke GPIO35, IRQ ke GPIO34, SCK ke GPIO25, MISO ke GPIO26, MOSI ke GPIO32.
- Tombol: gunakan INPUT_PULLUP pada GPIO4 dan GPIO5 (tekan ke GND).
- LED dan buzzer: hubungkan melalui resistor/driver sesuai kebutuhan (jangan langsung hubungkan LED ke pin tanpa resistor).

Jika Anda ingin file gambar terpisah:
- Tambahkan file `wiring.png` (raster) atau `wiring.svg` (vektor) ke root repository.
- Untuk menampilkan file tersebut di README, gunakan sintaks berikut:
  ```markdown
  ![Wiring Diagram](./wiring.png)
  ```
  atau untuk SVG:
  ```markdown
  ![Wiring Diagram](./wiring.svg)
  ```

---

## Persiapan software / library
Pastikan menginstal library berikut di Arduino IDE atau PlatformIO:
- TFT_eSPI
- ESP32Servo
- EEPROM
- XPT2046_Touchscreen
- ESPAsyncWebServer (plus AsyncTCP/ESPAsyncTCP jika perlu)
- SPI
- WiFi (bawaan ESP32)

Build/Upload:
- Buka file `.ino` (contoh: `webUiTestbench_UPDATE.ino`) di Arduino IDE, pilih board ESP32 yang sesuai, lalu upload.
- Jika menggunakan PlatformIO, buat project baru dan copy file `.ino` ke src/ lalu build & upload.

---

## Web UI (index.html)
Web UI sudah disediakan sebagai string PROGMEM `index_html` di file .ino. Web UI menyediakan:
- Status mesin dan tombol untuk mengubah state (Persiapan/Proses Kliring & Kalibrasi, Siap Uji)
- Set Durasi (detik) — KLIRING, TUNGGU, KALIBRASI, STABIL
- Toggle Buzzer dan Wireless
- Pengaturan sudut servo (S1,S2,S3 close/open)
- Test Servo langsung (S1,S2,S3)
- Pilih servo input (Servo 1 atau Servo 3)
- Set kecepatan servo input (CLOSE->OPEN, OPEN->CLOSE) dalam detik
- Reset EEPROM (dengan konfirmasi)
- Reboot Device (dengan konfirmasi)

Jika Anda ingin menyimpan file web terpisah (index.html), Anda dapat menaruhnya di server http_static atau mengubah .ino untuk membaca file dari SPIFFS/LittleFS — saat ini web UI ter-embed di PROGMEM.

---

## HTTP API (ringkasan)
Beberapa endpoint penting:
- GET /           -> Halaman web (index_html)
- GET /info       -> JSON berisi status, durasi (detik), buzzer, wireless, inputServo, kecepatan input
- GET /set?KLIRING=X&TUNGGU=Y&KALIBRASI=Z&STABIL=W
                 -> Simpan durasi (dalam detik)
- GET /buzzer?toggle=1 -> Toggle buzzer, respon ON/OFF
- GET /wireless?toggle=1 -> Toggle wireless, respon ON/OFF
- GET /servo?s1c=&s1o=&s2c=&s2o=&s3c=&s3o= -> Set & get sudut servo
- GET /testservo?id=1|2|3&pos=NN -> Gerakkan servo langsung (pos 0-180)
- GET /testservomenu -> Masuk menu Test Servo di TFT (hanya saat SIAP_UJI)
- GET /inputservo?set=1|3 -> Pilih servo input (1 atau 3)
- GET /inputservospeed?setClose=0.5 -> set close->open speed (detik)
- GET /inputservospeed?setOpen=0.6 -> set open->close speed (detik)
- GET /reset -> Reset EEPROM ke default
- GET /reboot -> Minta reboot (safe flag, restart dilakukan di loop utama)

Catatan: beberapa endpoint mengirimkan response JSON sementara yang lain mengirim plain text.

---

## Perintah serial
Via Serial (115200) Anda bisa mengirimkan:
- INFO -> print konfigurasi saat ini
- RESET -> reset EEPROM ke default (fungsi ini juga akan me-reboot device)
- REBOOT -> minta reboot
- STATE <kode> -> ubah state, contoh: `STATE A2` -> PROSES_KLIRING
- Ubah durasi dengan `KLIRING 10` atau `TUNGGU 5` (satuan detik)

---

## Menu TFT — Urutan Pengaturan saat ini
Pada menu "Pengaturan" (index 0 = item teratas):
0. Test Servo                      -> enterTestServoMenu()
1. Set Posisi Servo                -> enterServoDegreeMenu()
2. Pilih Servo Input               -> enterInputServoMenu()
3. Set Kecepatan Servo Input       -> enterInputServoSpeedMenu()
4. Set Durasi Pengujian            -> enterDurationMenu()
5. Set Buzzer                      -> enterBuzzerMenu()
6. Wireless                        -> enterWirelessMenu()
7. Reboot                          -> menuLevel = CONFIRM_REBOOT_MENU
8. Reset EEPROM                    -> menuLevel = CONFIRM_RESET_MENU

(Detail implementasi menu ada di file .ino; urutan ini sudah disesuaikan berdasarkan permintaan.)

---

## Changelog (perubahan terakhir)
- Menambahkan menu Reboot pada Pengaturan (TFT) dengan konfirmasi.
- Menambahkan tombol "REBOOT DEVICE" di Web UI yang memanggil endpoint `/reboot`.
- Endpoint `/reboot` dibuat aman: men-set flag `rebootRequested` dan reboot dilakukan di loop utama.
- Menambahkan perintah serial `REBOOT`.
- Mengubah urutan item menu Pengaturan sesuai permintaan pengguna (lihat bagian Menu TFT).
- Menambahkan diagram wiring (inline SVG) dan instruksi untuk file wiring.png/svg terpisah.
- Web UI di-embed di PROGMEM `index_html`, dan berisi fungsi tambahan (reboot, reset, test servo, set kecepatan input).

---

## Tips & catatan
- Reboot melalui Web UI akan mengirim request ke endpoint `/reboot`. Karena device mungkin berjalan di mode AP, setelah reboot koneksi Web UI akan hilang sampai ESP32 kembali online.
- Pastikan power supply cukup untuk servo (servo dapat menarik arus besar saat bergerak). Disarankan gunakan supply terpisah untuk servo dengan ground bersama.
- Jika ingin memisahkan web UI (index.html) dari kode .ino, letakkan file pada SPIFFS/LittleFS dan ubah server handler untuk serve file statis.

---

## Lisensi
© 2025 — *Web UI Testbench Project by TAIZ - TIRTA MAJU JAYA CV*  
[github.com/putuajah] https://github.com/putuajah/testbench_portable.git

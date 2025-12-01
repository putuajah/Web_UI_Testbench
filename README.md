# TestBench Portable - Kontrol & Web UI

TestBench Portable adalah firmware untuk ESP32 yang mengendalikan sebuah test-bench berbasis servo dengan antarmuka layar TFT + touch dan Web UI melalui Access Point (mode AP). Proyek ini memudahkan pengaturan durasi uji, posisi servo, kecepatan servo input, pengujian servo, pengaturan buzzer/wireless, dan menyediakan fungsi reboot/reset EEPROM.

## About
TestBench Portable dikembangkan sebagai solusi sederhana dan portabel untuk melakukan pengujian mekanis atau elektronis yang memerlukan siklus gerakan servo terautomasi. Tujuannya adalah menyediakan antarmuka lokal (TFT touch) dan jarak jauh (Web UI) sehingga operator dapat mengkonfigurasi dan menjalankan pengujian tanpa perlu mengubah kode tiap kali parameter perlu disesuaikan.

- Status: Working prototype
- Bahasa: C++ (Arduino framework untuk ESP32)
- Maintainer: putuajah
- Kontak: (gunakan issue/pull request di repository GitHub untuk umpan balik atau laporan bug)
- Tujuan: memudahkan debugging, kalibrasi, dan rutinitas pengujian pada perangkat mekanis sederhana yang memakai servo.

## ✨ Fitur Utama
- Tampilan modern pada TFT dengan navigasi tombol virtual (+,-,M,B) dan dua tombol fisik A/B.
- Web UI responsif untuk pengaturan cepat (durasi, sudut servo, test servo, kecepatan input, toggle buzzer/wireless, reboot, reset EEPROM).
- Gerakan servo input non-blocking dengan interpolasi (durasi configurable).
- Simpan pengaturan di EEPROM (durasi, buzzer, posisi servo, wireless, servo input, kecepatan input).
- Endpoint HTTP API untuk integrasi / automasi.
- Konfirmasi sebelum operasi destruktif (Reset EEPROM, Reboot).
- Realtime kondisi suhu air.                   <-- added for DS18b2
- Realtime Flowmeter (L/jam).                  <-- added for FS300A
- Update OTA dengan arduino IDEArduinoOTA.h    <-- added for OTA

## ⚙️ Persyaratan Perangkat Keras
- ESP32 (board : ESP32 DevKit)
- Layar TFT 2.4" (TFT_eSPI)                     <-- ili9431 2.4" 240x320 + touchscreen
- Touch controller XPT2046 (touch SPI custom)   <-- ili9431 2.4" 240x320 + touchscreen
- 3 x Servo (servo1, servo2, servo3)    <-- servo tower pro MG996R METAL GEAR
- 2 x LED indikator (LED1, LED2)
- Buzzer
- 2 x tombol fisik (BUTTON_A, BUTTON_B)
- Sensor suhu DS18b20    <-- (UPDATE)
- Flowmeter FS300A       <-- (UPDATE)
- Wiring sesuai pin di source (lihat bagian Wiring)
  
  Pin default (sesuaikan di kode jika berbeda):
- BUTTON_A_PIN = GPIO4
- BUTTON_B_PIN = GPIO5
- LED1_PIN = GPIO16
- LED2_PIN = GPIO17
- SERVO1_PIN = GPIO33
- SERVO2_PIN = GPIO13
- SERVO3_PIN = GPIO14
- BUZZER_PIN = GPIO27
- TOUCH_CS = GPIO35, TOUCH_IRQ = GPIO34, TOUCH_CLK = GPIO25, TOUCH_DOUT = GPIO26 (MISO), TOUCH_DIN = GPIO32 (MOSI)
- DS18_PIN   = 22     <-- (UPDATE) tambahkan resistor 4,7K vcc ke pin sensor suhu
- FLOW_PIN   = 21     <-- (UPDATE)

## 🧩 Library yang Diperlukan
Pastikan library berikut sudah terinstal di Arduino IDE:
- TFT_eSPI
- ESP32Servo
- EEPROM
- XPT2046_Touchscreen
- ESPAsyncWebServer (plus AsyncTCP/ESPAsyncTCP jika perlu)
- SPI
- WiFi (bawaan ESP32)
- OneWire               <-- (DS18b20)
- DallasTemperature     <-- (DS18b20)
- ArduinoOTA            <-- (IDEArduinoOTA)

## 🚀 Cara Upload via usb
1. Buka file `src/webUiTestbench_UPDATE.ino` di **Arduino IDE**
2. Pilih board: **ESP32 Dev Module**
3. Hubungkan ESP32 ke komputer
4. Klik **Upload**
5. Setelah upload, akses antarmuka web sesuai IP yang muncul di Serial Monitor

## 🚀 Cara Upload via OTA (setelah upload via usb)
1. Buka file `src/webUiTestbench_UPDATE.ino` di **Arduino IDE**
2. Pilih board: **ESP32 Dev Module**
3. Pilih port: 192.168.4.1 testbench-ota
4. masukan passwor OTA : kopisusu
5. Klik **Upload**
6. Selesai

## 🧠 Kontribusi
Silakan lakukan *fork* dan kirim *pull request* untuk perbaikan atau pengembangan fitur baru.

## 📄 Lisensi
Proyek ini dilisensikan di bawah **MIT License** — bebas digunakan dan dimodifikasi selama mencantumkan kredit kepada pembuat asli.

# Web UI Testbench

Program ini digunakan untuk **mengkalibrasi meter air digital berbasis ESP32** dengan antarmuka **web** dan **layar sentuh (touchscreen)**.

## ✨ Fitur Utama
- Antarmuka web untuk konfigurasi dan monitoring
- Dukungan touchscreen (XPT2046)
- Kontrol servo dan buzzer
- Penyimpanan data di EEPROM
- Fitur reset EEPROM dan reboot otomatis

## ⚙️ Persyaratan Perangkat Keras
- ESP32 Dev Board
- Layar TFT + Touchscreen (misal: ILI9341 + XPT2046)
- Servo motor
- Buzzer

## 🧩 Library yang Diperlukan
Pastikan library berikut sudah terinstal di Arduino IDE:
- `TFT_eSPI`
- `ESP32Servo`
- `EEPROM`
- `XPT2046_Touchscreen`

## 🚀 Cara Upload
1. Buka file `src/webUiTestbench_UPDATE.ino` di **Arduino IDE**
2. Pilih board: **ESP32 Dev Module**
3. Hubungkan ESP32 ke komputer
4. Klik **Upload**
5. Setelah upload, akses antarmuka web sesuai IP yang muncul di Serial Monitor

## 🧠 Kontribusi
Silakan lakukan *fork* dan kirim *pull request* untuk perbaikan atau pengembangan fitur baru.

## 📄 Lisensi
Proyek ini dilisensikan di bawah **MIT License** — bebas digunakan dan dimodifikasi selama mencantumkan kredit kepada pembuat asli.

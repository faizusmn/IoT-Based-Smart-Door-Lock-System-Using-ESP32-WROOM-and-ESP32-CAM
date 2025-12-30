# IoT-Based-Smart-Door-Lock-System-Using-ESP32-WROOM-and-ESP32-CAM

**Project Akhir Mata Kuliah Internet of Things (IoT)**
Sistem kunci pintu berbasis IoT dengan arsitektur **multi-ESP32**, mengintegrasikan sensor fusion dan trigger kamera melalui HTTP.

---

## 1. Deskripsi Teknis

SmartDoorLock adalah sistem penguncian pintu cerdas yang terdiri dari dua node utama:

1. **ESP32_Main**

   * Bertindak sebagai *central controller* untuk seluruh alur logika.
   * Menjalankan *Sensor Fusion v1.3* sebagai baseline pemrosesan data sensor.
   * Mengontrol aktuator (servo/solenoid lock).
   * Mengirim *HTTP-trigger* ke ESP32-CAM untuk pengambilan gambar.

2. **ESP32_CAM**

   * Menjalankan *AI Thinker HTTP Capture Server*.
   * Menerima perintah GET/POST dari ESP32_Main.
   * Mengambil dan mengirimkan foto dalam format JPG.

Sistem ini dirancang untuk memberikan bukti visual ketika terjadi percobaan akses, sekaligus menggunakan data sensor sebagai sumber keputusan utama.

---

## 2. Struktur Direktori

```
SmartDoorLock/
│── ESP32/
│   └── ESP32.cpp                # Logika sistem utama + sensor fusion + HTTP trigger
│── ESP32CAM/
│   └── ESP32CAM.ino       # Server kamera ESP32-CAM (AI Thinker)
└── README.md
```

---

## 3. Arsitektur Sistem

### 3.1 Alur Kerja ESP32_Main

1. Inisialisasi WiFi, sensor, dan aktuator.
2. Menjalankan loop pembacaan sensor (via Sensor Fusion v1.3).
3. Melakukan pengambilan keputusan (decision state machine).
4. Jika kondisi tertentu terpenuhi (akses gagal, deteksi gerakan, dsb.), ESP32_Main akan:

   * Mengirim HTTP request:

     ```
     GET http://<CAM_IP>/capture
     ```
   * Menerima respon berupa **byte stream JPG**.
   * Menyimpan/forward output (opsional).
5. Mengontrol aktuator (servo/solenoid) sesuai hasil keputusan.

### 3.2 Alur Kerja ESP32_CAM

1. Mengaktifkan kamera bawaan ESP32-CAM AI Thinker.
2. Membuka endpoint HTTP, contoh:

   * `/capture` → mengambil satu foto dan mengembalikan JPG.
   * `/status` → mengecek kondisi kamera.
3. Menunggu request dari ESP32_Main.

---

## 4. Penjelasan Berkas Kode

### 4.1 `ESP32_Main/main.cpp`

Berisi:

* Konfigurasi WiFi
* Sensor Fusion v1.3 (baseline sistem)
* State machine logika akses
* Implementasi HTTP request ke ESP32-CAM
* Driver servo/solenoid untuk pengunci pintu

Fungsi inti:

* `sendCameraTrigger()` → mengirim permintaan pengambilan gambar
* `updateSensors()` → membaca data sensor
* `controlLock()` → membuka/menutup kunci

### 4.2 `ESP32_CAM/camera_server.ino`

Berisi:

* Konfigurasi modul kamera
* HTTP server (ESPAsyncWebServer/standard WebServer)
* Handler endpoint `/capture`
* Capture pipeline untuk menghasilkan JPEG

---

## 5. Cara Menjalankan Sistem

### 5.1 Setup ESP32_Main

1. Buka folder `ESP32_Main` di Arduino IDE atau PlatformIO
2. Sesuaikan:

   ```cpp
   const char* ssid = "...";
   const char* password = "...";
   const char* cam_ip = "xxx.xxx.xxx.xxx";
   ```
3. Upload dan monitor serial

### 5.2 Setup ESP32_CAM

1. Buka `camera_server.ino`
2. Pilih board: **AI Thinker ESP32-CAM**
3. Masukkan SSID dan password WiFi
4. Upload dengan mode **BOOT** (IO0 to GND)
5. Monitor serial untuk melihat IP kamera

### 5.3 Menjalankan

* Pastikan kedua perangkat terhubung ke jaringan yang sama
* ESP32_Main akan otomatis mengirim request saat sensor memicu event
* ESP32_CAM menangkap dan mengembalikan gambar

---

## 6. Kebutuhan Hardware

* ESP32 DevKit
* ESP32-CAM AI Thinker Module
* Sensor (sesuai konfigurasi sensor fusion: PIR, ultrasonik, RFID, keypad, dsb.)
* Servo/solenoid door lock
* Power 5V 2A
* Kabel jumper
* (Opsional) microSD untuk penyimpanan foto

---

## 7. Catatan Integrasi

* Keduanya harus berada pada jaringan WiFi yang sama
* Endpoint kamera harus bisa diakses langsung dari ESP32_Main
* Latensi jaringan mempengaruhi waktu respon trigger kamera
* ESP32-CAM memiliki keterbatasan RAM, gunakan resolusi **QVGA/VGA** untuk stabilitas maksimal

---

## 8. Lisensi

Proyek dibuat untuk kepentingan akademik dan pengembangan pada mata kuliah Internet of Things.

---

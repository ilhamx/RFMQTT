# 📡 ESP32 RFID UHF + MQTT Gateway

Proyek ini adalah implementasi **ESP32 sebagai gateway RFID UHF** yang membaca tag melalui UART dan mengirim data ke server MQTT secara real-time.

## 🚀 Features

- 📶 WiFi connectivity
- 📡 MQTT communication
- 🔍 Continuous RFID scanning (inventory mode)
- 🚫 Anti-duplicate tag filtering (3 detik)
- 📤 Publish EPC + RSSI ke MQTT
- 📥 Remote control via MQTT (start/stop + raw command)

---

## ⚙️ Hardware Requirement

- ESP32
- RFID UHF Module (UART, protocol 0xBB ... 0x7E)
- Antena UHF
- Power supply stabil

---

## 🔌 Pin Configuration

| Function | ESP32 Pin |
|----------|----------|
| RFID RX  | GPIO 3   |
| RFID TX  | GPIO 2   |
| RFID EN  | GPIO 1   |

---

## 📶 Configuration

Edit bagian ini di kode:

```cpp
const char *ssid = "YOUR_WIFI";
const char *password = "YOUR_PASSWORD";
const char *mqtt_server = "YOUR_MQTT_BROKER_IP";
```

---

## 📤 MQTT Publish

**Topic:** `rfid/data`

**Format JSON:**

```json
{
  "epc": "E2000017221101441890XXXX",
  "rssi": -65
}
```

---

## 📥 MQTT Subscribe

### 1. Control Scanner

**Topic:** `rfid/cmd`

| Command | Function        |
|--------|----------------|
| start  | Start scanning |
| stop   | Stop scanning  |

---

### 2. Send Raw Command 🔥

**Topic:** `rfid/send`

Contoh:

```
BB 00 27 00 03 22 FF FF XX 7E
```

atau:

```
BB0027000322FFFFXX7E
```

---

## 🧠 System Architecture

### Task RFID
- Read UART
- Parse frame
- Extract EPC & RSSI

### Task MQTT
- Handle connection
- Publish & subscribe

---

## 🧬 Frame Structure

```
[HEADER] 0xBB
[TYPE]
[CMD]
[LEN_H][LEN_L]
[DATA...]
[CRC]
[END] 0x7E
```

---

## 🛠️ Important Functions

### Send Command
```cpp
sendCommand(cmd, param, length);
```

### Start Inventory
```cpp
startInventory();
```

### Stop Inventory
```cpp
stopInventory();
```

### Send Raw Frame
```cpp
sendRawFrame(buffer, len);
```

---

## 🐞 Debugging

Enable raw debug:

```cpp
debugRawRFID();
```

---

## ⚠️ Notes

- Baudrate RFID: **115200**
- Pastikan power cukup
- Hindari publish terlalu cepat
- Gunakan antena sesuai frekuensi

---

## 🚀 Future Improvements

- Web dashboard
- OTA update
- Tag filtering (whitelist)
- Database logging
- RSSI positioning

---

## 👨‍💻 Author

Ilham — Electrical Engineering ⚡

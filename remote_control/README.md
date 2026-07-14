# Remote Control Firmware – ESP32

## Phần cứng (theo schematic `remote_control.SchDoc`)

| Thành phần | Mô tả |
|---|---|
| MCU | ESP32 WROOM 38 |
| RS-485 | MAX485 (UART2 + GPIO4 làm DE/RE) |
| Nguồn | AMS1117 3.3V |
| USB | CH340 (debug/flash qua USB-C) |

---

## Ánh xạ chân GPIO

### 8 nút mặt trước – Điều khiển 4 trục

| Trục | Tên | Nút + | GPIO + | Nút − | GPIO − |
|------|-----|-------|--------|-------|--------|
| 1 | **X** | BTN3 | IO22 | BTN4 | IO23 |
| 2 | **Y** | BTN1 | IO19 | BTN2 | IO21 |
| 3 | **Z** | BTN5 | IO27 | BTN6 | IO14 |
| 4 | **R** | BTN7 | IO25 | BTN8 | IO26 |

### 2 nút mặt sau – An toàn (Safety Interlock / Dead-man)

| Nút   | GPIO | Chức năng    |
|-------|------|--------------|
| BTN9  | IO18 | Safety trái  |
| BTN10 | IO32  | Safety phải  |

> Phải nhấn giữ **đồng thời cả BTN9 và BTN10** mới gửi được lệnh trục.
> Thả một trong hai → tất cả trục **STOP ngay lập tức**.

### RS-485

| Tín hiệu | GPIO |
|----------|------|
| TX       | IO16 |
| RX       | IO17 |
| DE+/RE−  | IO4  |

---

## Cài đặt ESP-IDF

### Linux / macOS
```bash
# Cài ESP-IDF v5.x
mkdir -p ~/esp && cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32

# Kích hoạt môi trường (phải chạy mỗi lần mở terminal mới)
. ~/esp/esp-idf/export.sh
```

### Windows
Tải và chạy ESP-IDF Windows Installer từ:
https://dl.espressif.com/dl/esp-idf/

Sau khi cài xong, mở **"ESP-IDF Command Prompt"** (tìm trong Start Menu).

---

## Build & Flash

### Linux / macOS
```bash
cd remote_control

# Cách 1 – Script tự động (khuyến nghị)
chmod +x flash.sh
./flash.sh                    # Tự tìm cổng
./flash.sh /dev/ttyUSB0       # Chỉ định cổng

# Cách 2 – Thủ công từng bước
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 -b 921600 flash monitor
```

### Windows (ESP-IDF Command Prompt)
```bat
cd remote_control

REM Cách 1 – Script tự động
flash_windows.bat             REM mặc định COM3
flash_windows.bat COM5        REM chỉ định cổng

REM Cách 2 – Thủ công
idf.py set-target esp32
idf.py build
idf.py -p COM3 -b 921600 flash monitor
```

### Tìm cổng COM
| OS | Lệnh |
|----|------|
| Linux | `ls /dev/ttyUSB*` |
| macOS | `ls /dev/cu.usbserial-*` |
| Windows | Device Manager → Ports (COM & LPT) |

---

## Nạp code thủ công (nếu không tự vào bootloader)

Một số board cần giữ nút **BOOT** trong khi cắm USB hoặc nhấn **RESET**:

1. Nhấn và **giữ** nút `BOOT` trên board
2. Nhấn nhanh nút `RESET` (hoặc cắm USB)
3. Thả nút `BOOT`
4. Chạy lệnh flash

---

## Partition Table

```
# partitions.csv
nvs       0x9000   0x6000    ← Lưu cấu hình NVS
phy_init  0xF000   0x1000    ← PHY calibration data
factory   0x10000  0x300000  ← Firmware chính (3 MB)
storage   0x310000 0xF0000   ← FAT storage dự phòng
```

---

## Giao thức RS-485

Frame 9 byte:
```
[0xAA] [DEV_ID] [CMD] [P0] [P1] [P2] [P3] [CRC_L] [CRC_H]
```

| CMD  | Mô tả | Payload |
|------|-------|---------|
| 0x01 | AXIS_COMMAND  | axis_id(1=X,2=Y,3=Z,4=R), dir(0=STOP,1=+,2=−), 0, 0 |
| 0x02 | HEARTBEAT     | safety(0=LOCKED,1=UNLOCKED), 0, 0, 0 |
| 0x03 | SAFETY_STATUS | state(0=LOCKED,1=UNLOCKED), 0, 0, 0 |

CRC: **CRC-16/ARC** (poly=0x8005, init=0x0000, refin=true, refout=true)

---

## Cấu trúc project

```
remote_control/
├── CMakeLists.txt          ← ESP-IDF root
├── partitions.csv          ← Partition table tuỳ chỉnh
├── sdkconfig.defaults      ← Cấu hình build mặc định
├── flash.sh                ← Script flash Linux/macOS
├── flash_windows.bat       ← Script flash Windows
├── README.md
└── main/
    ├── CMakeLists.txt
    ├── pin_config.h        ← GPIO map + thông số phần cứng
    ├── button.h/c          ← Debounce FSM + Safety Interlock
    ├── rs485.h/c           ← UART2 driver + CRC-16 framing
    └── main.c              ← FreeRTOS tasks + logic điều khiển
```

---

## Timing

| Tham số       | Giá trị |
|---------------|---------|
| Debounce      | 20 ms   |
| button_task   | 1 ms    |
| control_task  | 5 ms    |
| Heartbeat     | 200 ms  |
| RS-485 baud   | 9600    |
| Flash baud    | 921600  |
| FreeRTOS tick | 1 ms    |
| CPU freq      | 240 MHz |

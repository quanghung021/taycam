#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════
# flash.sh – Build, nạp code và mở monitor cho Remote Control ESP32
#
# Cách dùng:
#   chmod +x flash.sh
#   ./flash.sh             → tự tìm cổng COM, build + flash + monitor
#   ./flash.sh /dev/ttyUSB0
#   ./flash.sh COM3         (Windows dùng Git Bash)
#
# Yêu cầu: ESP-IDF đã cài và đã chạy `. $IDF_PATH/export.sh`
# ═══════════════════════════════════════════════════════════════

set -e

# ─── Màu sắc terminal ────────────────────────────────────────
RED='\033[0;31m'; GRN='\033[0;32m'; YLW='\033[1;33m'; NC='\033[0m'

# ─── Kiểm tra IDF_PATH ───────────────────────────────────────
if [ -z "$IDF_PATH" ]; then
    echo -e "${RED}[ERR] IDF_PATH chưa được set.${NC}"
    echo "      Chạy:  . \$HOME/esp/esp-idf/export.sh   rồi thử lại."
    exit 1
fi

# ─── Xác định cổng COM ───────────────────────────────────────
PORT="$1"
if [ -z "$PORT" ]; then
    # Tự động tìm cổng CH340 / CP210x / FTDI
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        PORT=$(ls /dev/ttyUSB* 2>/dev/null | head -1)
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        PORT=$(ls /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART 2>/dev/null | head -1)
    else
        # Windows Git Bash
        PORT="COM3"
    fi
fi

if [ -z "$PORT" ]; then
    echo -e "${RED}[ERR] Không tìm thấy cổng serial.${NC}"
    echo "      Cắm cáp USB hoặc truyền cổng thủ công: ./flash.sh /dev/ttyUSB0"
    exit 1
fi

echo -e "${GRN}════════════════════════════════════════${NC}"
echo -e "${GRN} Remote Control ESP32 – Build & Flash  ${NC}"
echo -e "${GRN}════════════════════════════════════════${NC}"
echo -e "  Cổng  : ${YLW}${PORT}${NC}"
echo -e "  Chip  : esp32"
echo -e "  Baud  : 921600"
echo ""

# ─── Set target ──────────────────────────────────────────────
echo -e "${YLW}[1/3] Thiết lập target esp32...${NC}"
idf.py set-target esp32

# ─── Build ───────────────────────────────────────────────────
echo -e "${YLW}[2/3] Build firmware...${NC}"
idf.py build

# ─── Flash + Monitor ─────────────────────────────────────────
echo -e "${YLW}[3/3] Nạp code lên ESP32 (${PORT}) @ 921600 baud...${NC}"
echo "      Giữ nút BOOT trên board nếu không tự động vào chế độ nạp."
echo ""
idf.py -p "$PORT" -b 921600 flash monitor


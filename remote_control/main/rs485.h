#pragma once
#include <stdint.h>

/**
 * @file rs485.h
 * @brief RS-485 half-duplex driver – UART2, frame 10 byte, CRC-16/ARC
 *
 * Cấu trúc frame:
 *   Byte 0    : 0xAA  (Start-of-Frame)
 *   Byte 1    : 0x01  (Device address)
 *   Byte 2    : CMD   (xem bên dưới)
 *   Byte 3    : Data0
 *   Byte 4    : Data1
 *   Byte 5-7  : 0x00  (reserved)
 *   Byte 8-9  : CRC-16/ARC little-endian
 *
 * CMD:
 *   0x01 = AXIS_COMMAND   (data0=axis_id, data1=direction)
 *   0x02 = SAFETY_STATUS  (data0=1/0 unlocked/locked)
 *   0x03 = HEARTBEAT      (data0=safety_state,
 *                           data1=dir trục X, data2=dir trục Y,
 *                           data3=dir trục Z, data4=dir trục R)
 *
 * Axis ID : 1=X  2=Y  3=Z  4=R
 * Direction: 0x00=STOP  0x01=+  0xFF=-
 *
 * ─── QUAN TRỌNG – yêu cầu watchdog phía SLAVE ──────────────────
 * Heartbeat (0x03) giờ mang đầy đủ trạng thái 4 trục, không chỉ
 * safety. Đây là cơ chế "state broadcast" định kỳ 200ms để bù cho
 * rủi ro mất 1 frame AXIS_COMMAND đơn lẻ trên bus (nhiễu/đường dây
 * dài). Phía SLAVE BẮT BUỘC phải cài watchdog timeout: nếu không
 * nhận được HEARTBEAT hợp lệ (CRC đúng) trong >500ms → tự động
 * STOP toàn bộ trục, KHÔNG phụ thuộc remote gửi lệnh STOP tường
 * minh. Nếu chưa có watchdog này ở firmware slave thì đây vẫn là
 * một lỗ hổng an toàn.
 */

/* ── Axis ID ─────────────────────────────────────────────────── */
#define RS485_AXIS_X    1
#define RS485_AXIS_Y    2
#define RS485_AXIS_Z    3
#define RS485_AXIS_R    4

/* ── Direction ───────────────────────────────────────────────── */
#define RS485_DIR_STOP   0x00
#define RS485_DIR_PLUS   0x01
#define RS485_DIR_MINUS  0xFF

/* ── API ─────────────────────────────────────────────────────── */

/** Khởi tạo UART2 và GPIO DE/RE. Gọi một lần trong app_main. */
void rs485_init(void);

/** Gửi lệnh hướng cho một trục. */
void rs485_send_axis(uint8_t axis_id, uint8_t direction);

/** Gửi frame trạng thái safety (unlocked=1 / locked=0). */
void rs485_send_safety(uint8_t unlocked);

/**
 * @brief Gửi frame heartbeat định kỳ, kèm trạng thái 4 trục hiện tại.
 * @param safety_unlocked  1=unlocked / 0=locked
 * @param axis_dir         Mảng 4 phần tử, thứ tự [X, Y, Z, R],
 *                          mỗi phần tử là RS485_DIR_STOP/PLUS/MINUS.
 *                          Đây là "state broadcast" giúp slave phát
 *                          hiện mất kết nối và tự STOP (xem watchdog
 *                          note ở đầu file).
 */
void rs485_send_heartbeat(uint8_t safety_unlocked, const uint8_t axis_dir[4]);
/**
 * @file rs485.c
 * @brief RS-485 half-duplex driver – UART2, CRC-16/ARC
 */

#include "rs485.h"
#include "pin_config.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "RS485";

/* ── Thông số UART ──────────────────────────────────────────── */
#define UART_PORT       UART_NUM_2
#define UART_BAUD       115200
#define UART_BUF_SIZE   256

/* ── Định nghĩa CMD ─────────────────────────────────────────── */
#define CMD_AXIS        0x01
#define CMD_SAFETY      0x02
#define CMD_HEARTBEAT   0x03

/* ── Cấu trúc frame ─────────────────────────────────────────── */
#define FRAME_LEN       10
#define FRAME_SOF       0xAA
#define DEVICE_ADDR     0x01

/* ══════════════════════════════════════════════════════════════
 * CRC-16/ARC  (reflected, poly = 0xA001, init = 0x0000)
 * ══════════════════════════════════════════════════════════════ */
static uint16_t crc16_arc(const uint8_t *data, size_t len)
{
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001u;
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* ── Đóng gói và gửi frame ──────────────────────────────────── */
static void send_frame(uint8_t cmd,
                        uint8_t d0, uint8_t d1,
                        uint8_t d2, uint8_t d3, uint8_t d4)
{
    uint8_t frame[FRAME_LEN] = {
        FRAME_SOF,
        DEVICE_ADDR,
        cmd,
        d0, d1, d2, d3, d4,
        0x00, 0x00   /* CRC placeholder */
    };

    uint16_t crc = crc16_arc(frame, 8);
    frame[8] = (uint8_t)(crc & 0xFF);
    frame[9] = (uint8_t)(crc >> 8);

    /*
     * DE/RE được điều khiển TỰ ĐỘNG bởi phần cứng UART
     * (UART_MODE_RS485_HALF_DUPLEX, xem rs485_init()) — không cần
     * gpio_set_level thủ công nữa. Cách cũ (set GPIO rồi mới
     * uart_write_bytes) có rủi ro race-condition/timing nếu task bị
     * preempt giữa 2 lệnh; driver UART xử lý DE ở mức phần cứng nên
     * đảm bảo timing chính xác hơn.
     */
    uart_write_bytes(UART_PORT, frame, FRAME_LEN);
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(10));
}

/* ══════════════════════════════════════════════════════════════
 * API công khai
 * ══════════════════════════════════════════════════════════════ */
void rs485_init(void)
{
    /* Cấu hình UART2 */
    uart_config_t cfg = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT,
                                        UART_BUF_SIZE, UART_BUF_SIZE,
                                        0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));

    /*
     * PIN_RS485_DE được gán làm chân RTS của UART2. Ở chế độ
     * UART_MODE_RS485_HALF_DUPLEX, driver tự động kéo RTS lên mức
     * cao trong lúc truyền và hạ xuống ngay sau khi truyền xong —
     * đúng timing t_ZL/t_ZH của MAX485 mà không cần gpio_set_level
     * thủ công (loại bỏ rủi ro race-condition khi task bị preempt).
     */
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT,
                                  PIN_RS485_TX, PIN_RS485_RX,
                                  PIN_RS485_DE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_mode(UART_PORT, UART_MODE_RS485_HALF_DUPLEX));

    ESP_LOGI(TAG, "RS-485 UART2 sẵn sàng @ %d baud (hardware half-duplex)",
             UART_BAUD);
}

void rs485_send_axis(uint8_t axis_id, uint8_t direction)
{
    /* d0=axis_id  d1=direction  d2-d4=0 */
    send_frame(CMD_AXIS, axis_id, direction, 0x00, 0x00, 0x00);
}

void rs485_send_safety(uint8_t unlocked)
{
    send_frame(CMD_SAFETY, unlocked, 0x00, 0x00, 0x00, 0x00);
}

void rs485_send_heartbeat(uint8_t safety_unlocked, const uint8_t axis_dir[4])
{
    /* d0=safety  d1=dir X  d2=dir Y  d3=dir Z  d4=dir R */
    send_frame(CMD_HEARTBEAT, safety_unlocked,
               axis_dir[0], axis_dir[1], axis_dir[2], axis_dir[3]);
}
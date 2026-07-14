#pragma once

/**
 * @file pin_config.h
 * @brief Ánh xạ phần cứng – đọc từ remote_control.SchDoc
 *
 * GPIO ESP32 WROOM-38 (xác định theo Y-coordinate net label):
 *   BTN1 → IO19   BTN2 → IO21   BTN3 → IO22   BTN4 → IO23
 *   BTN5 → IO27   BTN6 → IO14   BTN7 → IO25   BTN8 → IO26
 *   BTN9 → IO18   BTN10→ IO32  
 *
 * RS-485 (UART2):
 *   TX  → IO16    RX  → IO17    DE/RE → IO4
 */

/* ─── GPIO nút nhấn ─────────────────────────────────────────── */
#define PIN_BTN1    19
#define PIN_BTN2    21
#define PIN_BTN3    22
#define PIN_BTN4    23
#define PIN_BTN5    27
#define PIN_BTN6    14
#define PIN_BTN7    25
#define PIN_BTN8    26
#define PIN_BTN9    18
#define PIN_BTN10   32

/* ─── GPIO RS-485 ────────────────────────────────────────────── */
#define PIN_RS485_TX    16
#define PIN_RS485_RX    17
#define PIN_RS485_DE    4    /* DE và RE nối chung, active-HIGH = TX */

/* ─── Index 0-based vào mảng g_buttons[] ────────────────────── */
#define BTN_IDX_1   0
#define BTN_IDX_2   1
#define BTN_IDX_3   2
#define BTN_IDX_4   3
#define BTN_IDX_5   4
#define BTN_IDX_6   5
#define BTN_IDX_7   6
#define BTN_IDX_8   7
#define BTN_IDX_9   8
#define BTN_IDX_10  9

#define BTN_COUNT   10
/**
 * @file main.c
 * @brief Remote Control ESP32 – Main Application
 *
 * Kiến trúc: FreeRTOS với 2 task:
 *   1. button_task  – cập nhật debounce FSM mỗi 1 ms (1 kHz polling)
 *   2. control_task – đọc trạng thái nút, gửi RS-485, heartbeat 200 ms
 *
 * Ánh xạ trục (theo yêu cầu):
 *   Trục Y (axis_id=1): Nút+ = BTN2(IO21)  Nút- = BTN1(IO19)
 *   Trục X (axis_id=2): Nút+ = BTN3(IO22)  Nút- = BTN4(IO23)
 *   Trục Z (axis_id=3): Nút+ = BTN6(IO14)  Nút- = BTN5(IO27)
 *   Trục R (axis_id=4): Nút+ = BTN8(IO26)  Nút- = BTN7(IO25)
 *
 * Logic điều khiển:
 *   • Chỉ gửi lệnh trục khi SAFETY_UNLOCKED (BTN9 + BTN10 đều nhấn giữ)
 *   • Thay đổi trạng thái nút → gửi frame AXIS_COMMAND ngay lập tức
 *   • Heartbeat 200 ms kèm trạng thái safety
 *   • Mất safety → STOP toàn bộ trục ngay lập tức
 *   • Nhấn đồng thời cả 2 nút của một trục → STOP (ưu tiên an toàn)
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "pin_config.h"
#include "button.h"
#include "rs485.h"

static const char *TAG = "MAIN";

/* ──────────────────────────────────────────────────────────────
 * Ánh xạ 4 trục → cặp nút (BTN index 0-based)
 *
 *   Trục Y (1): Nút+ = BTN2 (idx 1)  Nút- = BTN1 (idx 0)
 *   Trục X (2): Nút+ = BTN3 (idx 2)  Nút- = BTN4 (idx 3)
 *   Trục Z (3): Nút+ = BTN6 (idx 5)  Nút- = BTN5 (idx 4)
 *   Trục R (4): Nút+ = BTN8 (idx 7)  Nút- = BTN7 (idx 6)
 * ────────────────────────────────────────────────────────────── */
typedef struct {
    uint8_t     btn_plus;    /* index vào g_buttons[] (0-based) */
    uint8_t     btn_minus;
    uint8_t     axis_id;     /* RS485_AXIS_X/Y/Z/R              */
    const char *axis_name;
} axis_map_t;

static const axis_map_t AXIS_MAP[4] = {
    { .btn_plus = BTN_IDX_2, .btn_minus = BTN_IDX_1, .axis_id = RS485_AXIS_Y, .axis_name = "Y" },
    { .btn_plus = BTN_IDX_3, .btn_minus = BTN_IDX_4, .axis_id = RS485_AXIS_X, .axis_name = "X" },
    { .btn_plus = BTN_IDX_6, .btn_minus = BTN_IDX_5, .axis_id = RS485_AXIS_Z, .axis_name = "Z" },
    { .btn_plus = BTN_IDX_8, .btn_minus = BTN_IDX_7, .axis_id = RS485_AXIS_R, .axis_name = "R" },
};

/* Trạng thái trục cuối cùng đã gửi (tránh gửi lặp) */
static uint8_t g_last_axis_dir[4] = {
    RS485_DIR_STOP, RS485_DIR_STOP, RS485_DIR_STOP, RS485_DIR_STOP
};

/* ──────────────────────────────────────────────────────────────
 * Task 1: Polling nút @ 1 ms
 * ────────────────────────────────────────────────────────────── */
static void button_task(void *arg)
{
    ESP_LOGI(TAG, "button_task bắt đầu");
    TickType_t xLastWake = xTaskGetTickCount();

    while (1) {
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        button_update(now_ms);
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(1));
    }
}

/* ──────────────────────────────────────────────────────────────
 * Task 2: Logic điều khiển + RS-485
 * ────────────────────────────────────────────────────────────── */
static void control_task(void *arg)
{
    ESP_LOGI(TAG, "control_task bắt đầu");

    safety_state_t prev_safety = SAFETY_LOCKED;
    uint32_t       hb_last_ms  = 0;
    const uint32_t HB_INTERVAL = 200;   /* ms */

    while (1) {
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

        /* ── Kiểm tra thay đổi safety ──────────────────────── */
        safety_state_t cur_safety = safety_get_state();
        if (cur_safety != prev_safety) {
            rs485_send_safety(cur_safety == SAFETY_UNLOCKED ? 1 : 0);

            if (cur_safety == SAFETY_LOCKED) {
                /* Dừng tất cả trục ngay khi mất an toàn */
                for (int i = 0; i < 4; i++) {
                    if (g_last_axis_dir[i] != RS485_DIR_STOP) {
                        rs485_send_axis(AXIS_MAP[i].axis_id, RS485_DIR_STOP);
                        g_last_axis_dir[i] = RS485_DIR_STOP;
                        ESP_LOGW(TAG, "SAFETY LOCK → Trục %s STOP",
                                 AXIS_MAP[i].axis_name);
                    }
                }
            }
            prev_safety = cur_safety;
        }

        /* ── Xử lý lệnh trục (chỉ khi safety UNLOCKED) ────── */
        if (cur_safety == SAFETY_UNLOCKED) {
            for (int i = 0; i < 4; i++) {
                bool plus  = button_is_down(AXIS_MAP[i].btn_plus);
                bool minus = button_is_down(AXIS_MAP[i].btn_minus);

                uint8_t dir;
                if (plus && !minus)
                    dir = RS485_DIR_PLUS;
                else if (minus && !plus)
                    dir = RS485_DIR_MINUS;
                else
                    dir = RS485_DIR_STOP;   /* Cả 2 cùng nhấn hoặc không nhấn */

                if (dir != g_last_axis_dir[i]) {
                    rs485_send_axis(AXIS_MAP[i].axis_id, dir);
                    g_last_axis_dir[i] = dir;
                    ESP_LOGI(TAG, "Trục %s → %s",
                             AXIS_MAP[i].axis_name,
                             dir == RS485_DIR_PLUS  ? "PLUS"  :
                             dir == RS485_DIR_MINUS ? "MINUS" : "STOP");
                }
            }
        }

        /* ── Heartbeat định kỳ (kèm trạng thái 4 trục) ─────── */
        if ((now_ms - hb_last_ms) >= HB_INTERVAL) {
            /* AXIS_MAP thứ tự [Y,X,Z,R] (idx 0..3) → chuyển sang
             * thứ tự [X,Y,Z,R] theo axis_id để khớp rs485.h */
            uint8_t hb_axis_dir[4] = {
                g_last_axis_dir[1],   /* X  (AXIS_MAP idx 1) */
                g_last_axis_dir[0],   /* Y  (AXIS_MAP idx 0) */
                g_last_axis_dir[2],   /* Z  (AXIS_MAP idx 2) */
                g_last_axis_dir[3],   /* R  (AXIS_MAP idx 3) */
            };
            rs485_send_heartbeat(cur_safety == SAFETY_UNLOCKED ? 1 : 0,
                                  hb_axis_dir);
            hb_last_ms = now_ms;
        }

        vTaskDelay(pdMS_TO_TICKS(5));   /* 200 Hz */
    }
}

/* ──────────────────────────────────────────────────────────────
 * Entry point
 * ────────────────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "═══════════════════════════════════════");
    ESP_LOGI(TAG, " Remote Control ESP32 – khởi động");
    ESP_LOGI(TAG, "═══════════════════════════════════════");

    button_init();
    rs485_init();

    /* button_task: stack 2 kB, priority 5 (cao hơn control) */
    xTaskCreate(button_task,  "btn",  2048, NULL, 5, NULL);

    /* control_task: stack 4 kB, priority 4 */
    xTaskCreate(control_task, "ctrl", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "Tất cả task đã khởi động");
}
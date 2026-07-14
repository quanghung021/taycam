#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * @file button.h
 * @brief Debounce FSM 4 trạng thái + safety interlock
 *
 * Tất cả nút dùng pull-up nội → GPIO mức LOW khi nhấn.
 * Debounce threshold: 20 ms.
 *
 * Safety: BTN9 VÀ BTN10 phải đồng thời giữ thì
 *         safety_get_state() mới trả về SAFETY_UNLOCKED.
 */

/* ── Trạng thái safety ──────────────────────────────────────── */
typedef enum {
    SAFETY_LOCKED    = 0,
    SAFETY_UNLOCKED  = 1,
} safety_state_t;

/* ── API công khai ──────────────────────────────────────────── */

/**
 * @brief Khởi tạo GPIO và cấu trúc dữ liệu debounce.
 *        Gọi một lần trong app_main trước khi tạo task.
 */
void button_init(void);

/**
 * @brief Cập nhật FSM debounce cho tất cả nút.
 *        Gọi từ button_task mỗi 1 ms.
 * @param now_ms  Thời điểm hiện tại (ms), lấy từ esp_timer_get_time()/1000.
 */
void button_update(uint32_t now_ms);

/**
 * @brief Kiểm tra nút có đang nhấn giữ (đã qua debounce).
 * @param idx  Index 0-based (dùng BTN_IDX_x từ pin_config.h).
 * @return true nếu đang nhấn.
 */
bool button_is_down(uint8_t idx);

/**
 * @brief Trả về trạng thái safety tổng hợp.
 *        SAFETY_UNLOCKED khi BTN9 VÀ BTN10 cùng nhấn giữ.
 */
safety_state_t safety_get_state(void);
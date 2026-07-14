/**
 * @file button.c
 * @brief Debounce FSM 4 trạng thái + safety interlock
 */

#include "button.h"
#include "pin_config.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "BTN";

/* ── Thông số ───────────────────────────────────────────────── */
#define DEBOUNCE_MS     20u     /* Ngưỡng ổn định tính/nhả      */

/* ── Danh sách GPIO theo thứ tự index ──────────────────────── */
static const int BTN_GPIO[BTN_COUNT] = {
    PIN_BTN1,  PIN_BTN2,  PIN_BTN3,  PIN_BTN4,  PIN_BTN5,
    PIN_BTN6,  PIN_BTN7,  PIN_BTN8,  PIN_BTN9,  PIN_BTN10,
};

/* ── FSM trạng thái từng nút ────────────────────────────────── */
typedef enum {
    S_IDLE             = 0,
    S_DEBOUNCE_PRESS,
    S_PRESSED,
    S_DEBOUNCE_RELEASE,
} btn_fsm_t;

typedef struct {
    btn_fsm_t state;
    uint32_t  ts_ms;    /* timestamp lúc bắt đầu debounce */
    bool      active;   /* trạng thái xác nhận sau debounce */
} btn_ctx_t;

/*
 * g_btns[] được ghi bởi button_task (ưu tiên 5) và đọc bởi
 * control_task (ưu tiên 4) qua button_is_down()/safety_get_state()
 * MÀ KHÔNG dùng mutex — đây là lock-free access có chủ đích. Field
 * `active` là bool (1 byte), đọc/ghi nguyên tử tự nhiên trên
 * Xtensa nên an toàn trong trường hợp này. KHÔNG thêm field lớn
 * hơn (int/struct nhiều byte) vào đường đọc chéo task này nếu
 * không có mutex đi kèm.
 */
static btn_ctx_t g_btns[BTN_COUNT];

/* ── Khởi tạo ──────────────────────────────────────────────── */
void button_init(void)
{
    gpio_config_t cfg = {
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    for (int i = 0; i < BTN_COUNT; i++) {
        cfg.pin_bit_mask = (1ULL << BTN_GPIO[i]);
        gpio_config(&cfg);

        g_btns[i].state  = S_IDLE;
        g_btns[i].ts_ms  = 0;
        g_btns[i].active = false;
    }

    ESP_LOGI(TAG, "Khởi tạo %d nút OK", BTN_COUNT);
}

/* ── Cập nhật FSM (gọi mỗi 1 ms) ───────────────────────────── */
void button_update(uint32_t now_ms)
{
    for (int i = 0; i < BTN_COUNT; i++) {
        /* Pull-up: LOW = nhấn */
        bool raw = (gpio_get_level(BTN_GPIO[i]) == 0);
        btn_ctx_t *b = &g_btns[i];

        switch (b->state) {
        case S_IDLE:
            if (raw) {
                b->state = S_DEBOUNCE_PRESS;
                b->ts_ms = now_ms;
            }
            break;

        case S_DEBOUNCE_PRESS:
            if (!raw) {
                b->state = S_IDLE;          /* Nhiễu, bỏ qua */
            } else if ((now_ms - b->ts_ms) >= DEBOUNCE_MS) {
                b->state  = S_PRESSED;
                b->active = true;
            }
            break;

        case S_PRESSED:
            if (!raw) {
                b->state = S_DEBOUNCE_RELEASE;
                b->ts_ms = now_ms;
            }
            break;

        case S_DEBOUNCE_RELEASE:
            if (raw) {
                b->state = S_PRESSED;       /* Vẫn giữ, quay lại */
            } else if ((now_ms - b->ts_ms) >= DEBOUNCE_MS) {
                b->state  = S_IDLE;
                b->active = false;
            }
            break;
        }
    }
}

/* ── API truy vấn ───────────────────────────────────────────── */
bool button_is_down(uint8_t idx)
{
    if (idx >= BTN_COUNT) return false;
    return g_btns[idx].active;
}

safety_state_t safety_get_state(void)
{
    /* Cả BTN9 (idx 8) và BTN10 (idx 9) phải cùng nhấn giữ */
    if (g_btns[BTN_IDX_9].active && g_btns[BTN_IDX_10].active)
        return SAFETY_UNLOCKED;
    return SAFETY_LOCKED;
}
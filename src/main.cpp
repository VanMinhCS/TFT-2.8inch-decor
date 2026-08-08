#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <ui/ui.h>
#include <ArduinoJson.h>
#include <time.h>
#include <HTTPClient.h>
#include <Preferences.h>

const int pinVRx = 14;
const int pinVRy = 12; 
const int pinSW = 10;

#define ADC_CENTER_X     1930  
#define ADC_CENTER_Y     1930 

#define ADC_DEADZONE     250   

#define THRES_X_MAX 3595
#define THRES_X_MIN 500 

#define THRES_Y_MAX 3595 
#define THRES_Y_MIN 500 

#define ENABLE_AUTO_REPEAT true
#define REPEAT_DELAY_MS    400 
#define REPEAT_RATE_MS     120 

static uint32_t last_key_time = 0;
static uint32_t last_active_key = 0;
static bool is_holding = false;

TFT_eSPI tft = TFT_eSPI();

static const uint32_t screenWidth  = 320;
static const uint32_t screenHeight = 240;

static uint8_t *draw_buf1;

lv_group_t * input_group;

void my_disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);

    tft.pushColors((uint16_t *)px_map, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

void joystick_init() {
  pinMode(pinVRx, INPUT);
  pinMode(pinVRy, INPUT);
  pinMode(pinSW, INPUT_PULLUP);
}

void my_joystick_read(lv_indev_t * indev_drv, lv_indev_data_t * data) {
    // 1. Đọc giá trị ADC trực tiếp (Dải 0 -> 4095 trên ESP32)
    int raw_x = analogRead(pinVRx);
    int raw_y = analogRead(pinVRy);
    bool sw_pressed = (digitalRead(pinSW) == LOW);

    uint32_t current_key = 0;

    // 2. Xử lý Deadzone & Xác định phím hướng
    // Nếu vượt qua vùng chết (Abs(Val - Center) > Deadzone)
    if (abs(raw_x - ADC_CENTER_X) > ADC_DEADZONE || abs(raw_y - ADC_CENTER_Y) > ADC_DEADZONE) {
        
        // --- Trục X ---
        if (raw_x <= THRES_X_MAX) {
            current_key = LV_KEY_UP;
        } else if (raw_x >= THRES_X_MIN) {
            current_key = LV_KEY_DOWN;
        }
        
        // --- Trục Y ---
        // (Nếu gạt đường chéo, trục Y sẽ ghi đè hoặc kết hợp tùy ưu tiên)
        if (raw_y <= THRES_Y_MAX) {
            current_key = LV_KEY_LEFT;
        } else if (raw_y >= THRES_Y_MIN) {
            current_key = LV_KEY_RIGHT;
        }
    } 
    
    // --- Nút nhấn SW ---
    if (sw_pressed) {
        current_key = LV_KEY_ENTER; 
    }

    // 3. Xử lý logic Nhấn đơn & Auto-Repeat (Kéo-Giữ tự động chạy)
    uint32_t now = millis();

    if (current_key != 0) {
        if (current_key != last_active_key) {
            // Trường hợp 1: Mới gạt/bấm phím mới lần đầu
            last_active_key = current_key;
            last_key_time = now;
            is_holding = false;

            data->key = current_key;
            data->state = LV_INDEV_STATE_PRESSED;
        } 
        else {
            // Trường hợp 2: Đang giữ nguyên vị trí cũ
#if ENABLE_AUTO_REPEAT
            uint32_t elapsed = now - last_key_time;

            if (!is_holding && elapsed >= REPEAT_DELAY_MS) {
                // Đã giữ đủ lâu -> Bắt đầu chế độ Auto-repeat
                is_holding = true;
                last_key_time = now;
                data->key = current_key;
                data->state = LV_INDEV_STATE_PRESSED;
            } 
            else if (is_holding && elapsed >= REPEAT_RATE_MS) {
                // Tự động nhả và gửi lại phím liên tục theo chu kỳ REPEAT_RATE_MS
                last_key_time = now;
                data->key = current_key;
                data->state = LV_INDEV_STATE_PRESSED;
            } 
            else {
                // Đang trong khoảng chờ giữa các lần lặp
                data->key = current_key;
                data->state = LV_INDEV_STATE_RELEASED;
            }
#else
            // Nếu TẮT Auto-repeat: Chỉ báo đang giữ (Pressed)
            data->key = current_key;
            data->state = LV_INDEV_STATE_PRESSED;
#endif
        }
    } 
    else {
        // Trường hợp 3: Thả Joystick về giữa / Không bấm nút
        if (last_active_key != 0) {
            data->key = last_active_key;
            data->state = LV_INDEV_STATE_RELEASED;
            last_active_key = 0;
            is_holding = false;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }
}
void setup() {
  Serial.begin(115200);
  Serial.println("Hello");
  joystick_init();
}

void loop() {
  lv_timer_handler();
  
}

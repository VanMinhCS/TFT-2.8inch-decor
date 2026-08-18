#include <global_help.h>

bool is_decor_mode = false;
bool is_in_widget = false;

int current_screen_index = 2;

// static bool last_is_day = false;
// static sunData cached_sun = {0, 0};
// static bool has_sun_data = false;

lv_group_t * input_group = nullptr;

TaskHandle_t getAPI_handle = NULL;
TaskHandle_t lunar_handle = NULL;
TaskHandle_t isday_handle = NULL;

volatile bool request_wifi_scan = false;
lv_group_t * joystick_group = nullptr;
lv_obj_t * main_screens[NUM_MAIN_SCREENS] = {nullptr};

static void collect_widgets_recursive(lv_obj_t * parent, uint32_t &count, lv_obj_t *&first_widget) {
    uint32_t child_cnt = lv_obj_get_child_cnt(parent);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(parent, i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;

        // Calendar: KHÔNG add chính nó, mà đệ quy xuyên qua để tìm Header (mũi tên/dropdown đổi tháng)
        // VÀ Button Matrix ngày bên trong — coi Calendar như 1 Container trong suốt
        if (lv_obj_check_type(child, &lv_calendar_class)) {
            collect_widgets_recursive(child, count, first_widget);
            continue;
        }

        bool is_clickable = lv_obj_has_flag(child, LV_OBJ_FLAG_CLICKABLE);
        if (is_clickable) {
            lv_group_add_obj(joystick_group, child);
            lv_obj_remove_event_cb(child, joystick_event_handler);
            lv_obj_add_event_cb(child, joystick_event_handler, LV_EVENT_ALL, NULL);
            if (first_widget == NULL) first_widget = child;
            count++;
        }

        // Chỉ đệ quy tiếp nếu bản thân nó KHÔNG phải widget "trọn gói" đã tự xử lý nội dung
        if (!is_clickable) {
            collect_widgets_recursive(child, count, first_widget);
        }
    }
}

void auto_load_screen_widgets(lv_obj_t * screen_obj) {
    lv_group_remove_all_objs(joystick_group);
    if (screen_obj == NULL) return;

    uint32_t count = 0;
    lv_obj_t * first_widget = NULL;
    collect_widgets_recursive(screen_obj, count, first_widget);

    if (first_widget) lv_group_focus_obj(first_widget);
    Serial.printf("[AUTO] Đã nạp %d Widget (đệ quy), focus vào %p\n", count, first_widget);
}

void navigate_to_main_screen(int new_index) {
    if (new_index < 0 || new_index >= NUM_MAIN_SCREENS) return;

    lv_scr_load_anim_t anim_type = (new_index > current_screen_index) 
                                   ? LV_SCR_LOAD_ANIM_MOVE_LEFT 
                                   : LV_SCR_LOAD_ANIM_MOVE_RIGHT;
                                   
    current_screen_index = new_index;
    lv_scr_load_anim(main_screens[current_screen_index], anim_type, 300, 0, false);
    
    // Thiết lập trạng thái chờ nhận phím cho màn hình mới
    joystick_standby_mode();
    Serial.printf("[NAV] Chuyển sang màn chính Index: %d\n", current_screen_index);
}

void joystick_event_handler(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    // =========================================================
    // 1. LONG PRESS → THOÁT MODE WIDGET
    // =========================================================
    if (code == LV_EVENT_LONG_PRESSED && is_in_widget) {
        // Nếu đang mở dropdown thì đóng trước
        lv_obj_t * focused = lv_group_get_focused(joystick_group);
        if (focused && lv_obj_check_type(focused, &lv_dropdown_class) && lv_dropdown_is_open(focused)) {
            lv_dropdown_close(focused);
        }
        joystick_standby_mode();
        Serial.println("[MODE] Thoát Widget");
        return;
    }

    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_indev_get_key(lv_indev_get_act());

    // =========================================================
    // 2. ĐANG Ở TRONG WIDGET MODE
    // =========================================================
    if (is_in_widget) {
        lv_obj_t * focused = lv_group_get_focused(joystick_group);
        if (focused == NULL) return;

        // ---------- DROPDOWN ----------
        if (lv_obj_check_type(focused, &lv_dropdown_class)) {
            if (key == LV_KEY_DOWN) { lv_group_focus_next(joystick_group); return; }
            if (key == LV_KEY_UP)   { lv_group_focus_prev(joystick_group); return; }
        }

        // ---------- SPINBOX ----------
        else if (lv_obj_check_type(focused, &lv_spinbox_class)) {
            if (key == LV_KEY_ENTER) {
                lv_group_focus_next(joystick_group);
                Serial.println("[SPINBOX] ENTER -> next");
                return;
            }
            // Các phím khác để spinbox tự xử lý
            return;
        }

        // ---------- KEYBOARD ----------
        else if (lv_obj_check_type(focused, &lv_keyboard_class)) {
            // Xử lý các phím chức năng đặc biệt khi nhấn ENTER
            if (key == LV_KEY_ENTER) {
                // 1. Lấy chỉ số của nút đang được focus trong ma trận
                uint16_t btn_idx = lv_btnmatrix_get_selected_btn(focused);
                if (btn_idx != LV_BTNMATRIX_BTN_NONE) {
                    // 2. Lấy văn bản của nút đó
                    const char * btn_txt = lv_btnmatrix_get_btn_text(focused, btn_idx);
                    
                    // 3. So sánh và gửi sự kiện tương ứng
                    if (strcmp(btn_txt, "ABC") == 0) {
                        // Bỏ qua lần nhấn ENTER này và mô phỏng sự kiện CLICK cho nút
                        // Điều này sẽ kích hoạt hành vi chuyển đổi chữ hoa/thường
                        lv_keyboard_set_mode(ui_Keyboard1, LV_KEYBOARD_MODE_TEXT_UPPER);
                        return; // Kết thúc xử lý, không cho LVGL xử lý tiếp
                    } 
                    else if (strcmp(btn_txt, "abc") == 0) {
                        lv_keyboard_set_mode(ui_Keyboard1, LV_KEYBOARD_MODE_TEXT_LOWER);
                    }
                    else if (strcmp(btn_txt, "1#") == 0) {
                        // Tương tự, mô phỏng sự kiện CLICK để chuyển sang bàn phím số
                        lv_keyboard_set_mode(ui_Keyboard1, LV_KEYBOARD_MODE_SPECIAL);
                        return;
                    }
                    else if (strcmp(btn_txt, LV_SYMBOL_OK) == 0) {
                        lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
                        return;
                    }
                    // Bạn có thể thêm xử lý cho phím OK/CLOSE tương tự ở đây
                }
            }
            
            // Nếu không phải phím chức năng (hoặc không phải ENTER), để LVGL xử lý như cũ
            return;
        }

        // ---------- BUTTONMATRIX / ROLLER ----------
        else if (lv_obj_check_type(focused, &lv_buttonmatrix_class)) {
            static lv_obj_t * last_focused_bm = nullptr;
            static uint16_t last_btn_id = LV_BTNMATRIX_BTN_NONE;

            // Reset bộ nhớ khi vừa chuyển sang 1 Button Matrix khác (vd Keyboard vs Calendar)
            if (focused != last_focused_bm) {
                last_focused_bm = focused;
                last_btn_id = LV_BTNMATRIX_BTN_NONE;
            }

            uint16_t cur_btn_id = lv_btnmatrix_get_selected_btn(focused);

            // Bấm UP mà vị trí ô chọn không đổi -> đã chạm hàng trên cùng -> thoát lên Header
            if (key == LV_KEY_UP && cur_btn_id == last_btn_id) {
                lv_group_focus_prev(joystick_group);
            }

            last_btn_id = cur_btn_id;
            return;
        }

        else if (lv_obj_check_type(focused, &lv_roller_class)) {
            return; // Roller giữ nguyên như cũ
        }

        // ---------- WIDGET THƯỜNG (btn, switch, slider, label...) ----------
        else {
            if (key == LV_KEY_UP) {
                lv_group_focus_prev(joystick_group);
                return;
            }
            if (key == LV_KEY_DOWN) {
                lv_group_focus_next(joystick_group);
                return;
            }
            // LEFT/RIGHT/ENTER để widget tự xử lý (nếu có)
        }
        return;
    }

    // =========================================================
    // 3. ĐANG ĐỨNG NGOÀI (chưa vào widget)
    // =========================================================
    if (!is_in_widget) {
        if (key == LV_KEY_ENTER) {
            is_in_widget = true;
            auto_load_screen_widgets(lv_scr_act());
            lv_indev_wait_release(lv_indev_get_act());
            Serial.println("[MODE] Vào điều khiển Widget");
            return;
        }

        if (is_decor_mode) {
            if (key == LV_KEY_RIGHT && current_screen_index < NUM_MAIN_SCREENS - 1) {
                navigate_to_main_screen(current_screen_index + 1);
            } 
            else if (key == LV_KEY_LEFT && current_screen_index > 0) {
                navigate_to_main_screen(current_screen_index - 1);
            }
        }
    }
}

void setup_joystick_navigation(lv_indev_t * indev_joystick_driver) {
    main_screens[0] = ui_Move;
    main_screens[1] = ui_CalendarDecor;
    main_screens[2] = ui_Time; 
    main_screens[3] = ui_Keqing;

    joystick_group = lv_group_create();
    lv_indev_set_group(indev_joystick_driver, joystick_group);

    // Đăng ký "Công tắc" cho ui_Time
    lv_obj_add_event_cb(ui_Time, ui_event_Clock_Loaded, LV_EVENT_SCREEN_LOADED, NULL);

    // *Lưu ý: Bạn gọi tương tự lv_obj_add_event_cb(ui_SettingX, ui_event_SubScreen_Loaded...) 
    // cho các màn hình phụ tại đây, hoặc cấu hình trực tiếp từ SquareLine Studio.

    lv_obj_add_event_cb(ui_Welcome, ui_event_SubScreen_Loaded, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(ui_WiFiSetting, ui_event_SubScreen_Loaded, LV_EVENT_SCREEN_LOADED, NULL);
    // is_decor_mode = true;
    // current_screen_index = 2; // ui_Time
    joystick_standby_mode();

}

void joystick_standby_mode() {
    is_in_widget = false;
    lv_group_remove_all_objs(joystick_group);
    
    // Lấy màn hình đang hiển thị hiện tại
    lv_obj_t * active_scr = lv_scr_act();
    
    // Ép màn hình trở thành đối tượng có thể nhận phím
    lv_obj_add_flag(active_scr, LV_OBJ_FLAG_CLICKABLE); 
    
    // Tránh đăng ký trùng lặp sự kiện
    lv_obj_remove_event_cb(active_scr, joystick_event_handler); 
    lv_obj_add_event_cb(active_scr, joystick_event_handler, LV_EVENT_ALL, NULL);
    
    // Đưa màn hình vào Group để hứng lệnh Joystick
    lv_group_add_obj(joystick_group, active_scr);
}

// 1. Gắn hàm này vào sự kiện LOADED của ui_Time
void ui_event_Clock_Loaded(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        is_decor_mode = true;
        current_screen_index = 2; // Khớp với index của ui_Time
        joystick_standby_mode();
        Serial.println("[SYSTEM] Kích hoạt chế độ DECOR (Duyệt 4 màn).");
    }
}

// 2. Gắn hàm này vào sự kiện LOADED của CÁC MÀN HÌNH PHỤ (Setting/WiFi...)
void ui_event_SubScreen_Loaded(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        is_decor_mode = false; // Đứng yên
        joystick_standby_mode();
        Serial.println("[SYSTEM] Kích hoạt chế độ SETTING (Đứng yên).");
    }
}


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
    int raw_x = analogRead(pinVRx);
    int raw_y = analogRead(pinVRy);
    bool sw_pressed = (digitalRead(pinSW) == LOW);

    uint32_t current_key = 0;

    // Xác định hướng
    if (raw_x >= THRES_X_MAX) {
        current_key = LV_KEY_DOWN;
    } else if (raw_x <= THRES_X_MIN) {
        current_key = LV_KEY_UP;
    } else if (raw_y >= THRES_Y_MAX) {
        current_key = LV_KEY_LEFT;
    } else if (raw_y <= THRES_Y_MIN) {
        current_key = LV_KEY_RIGHT;
    }

    // Nút nhấn ưu tiên cao nhất
    if (sw_pressed) {
        current_key = LV_KEY_ENTER;
    }

    uint32_t now = millis();

    if (current_key != 0) {
        if (current_key != last_active_key) {
            // Phím mới vừa được nhấn
            last_active_key = current_key;
            last_key_time = now;
            is_holding = false;

            data->key = current_key;
            data->state = LV_INDEV_STATE_PRESSED;
        }
        else {
            // Đang giữ nguyên phím → LUÔN gửi PRESSED
            // (để LVGL có thể phát hiện Long-press)
            data->key = current_key;
            data->state = LV_INDEV_STATE_PRESSED;

            // Phần auto-repeat (nếu vẫn muốn dùng)
            #if ENABLE_AUTO_REPEAT
            uint32_t elapsed = now - last_key_time;
            if (!is_holding && elapsed >= REPEAT_DELAY_MS) {
                is_holding = true;
                last_key_time = now;
                // Có thể gửi thêm 1 lần PRESSED ở đây nếu cần
            }
            else if (is_holding && elapsed >= REPEAT_RATE_MS) {
                last_key_time = now;
                // Gửi lại PRESSED để tạo hiệu ứng repeat
            }
            #endif
        }
    }
    else {
        // Thả phím thật sự
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

void getAPI(void * pvParameters){
    String url = "https://api.open-meteo.com/v1/forecast?latitude=10.80340110826354&longitude=106.71108143960556&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max&current=temperature_2m,relative_humidity_2m&timezone=Asia%2FBangkok&forecast_days=1";
    
    while (1){
        // Chờ notification hoặc timeout 30 phút
        // pdTRUE: xóa counter sau khi lấy (chỉ cần biết có notification)
        uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30 * 60 * 1000));

        // Kiểm tra WiFi và gọi API
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin(url);
            int httpResponseCode = http.GET();

            if (httpResponseCode > 0) {
                String payload = http.getString();
                JsonDocument doc;
                deserializeJson(doc, payload);
                WeatherData weatherData = {
                    .temp = doc["current"]["temperature_2m"],
                    .humid = doc["current"]["relative_humidity_2m"],
                    .maxTemp = doc["daily"]["temperature_2m_max"][0],
                    .minTemp = doc["daily"]["temperature_2m_min"][0],
                    .rain = doc["daily"]["precipitation_probability_max"][0]
                };
                xQueueSend(WeatherHandle, &weatherData, portMAX_DELAY);
                
            }
            http.end();
        }
    }
}

void lunar (void * pvParameters) {
    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        struct tm timeinfo;
        
        if(getLocalTime(&timeinfo))
        {
            LunarDate lunar = LunarCalendar::solarToLunar(timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
            static char * lunarDay = (char*)malloc(20);
            if (lunarDay) {
                snprintf(lunarDay, 20, "%02d-%02d-%d", lunar.day, lunar.month, lunar.year);
                lv_async_call(update_lunar_callback, lunarDay);
            }
        }
        
    }
}

void connectWiFi(void *pvParameters)
{
    Preferences prefs;
    WiFiData wifi;

    prefs.begin("wifi", false);

    String savedSSID = prefs.getString("ssid", "");
    String savedPass = prefs.getString("pass", "");

    strncpy(wifi.ssid, savedSSID.c_str(), sizeof(wifi.ssid) - 1);
    wifi.ssid[sizeof(wifi.ssid) - 1] = '\0';

    strncpy(wifi.pass, savedPass.c_str(), sizeof(wifi.pass) - 1);
    wifi.pass[sizeof(wifi.pass) - 1] = '\0';

    if (wifi.ssid[0] != '\0')
    {
        WiFi.begin(wifi.ssid, wifi.pass);
    }

    bool wasConnected = false;
    uint32_t lastReconnect = 0;

    while (1)
    {
        if (xQueueReceive(WiFiInfo, &wifi, 0))
        {
            Serial.println("New WiFi information");

            // Lưu WiFi mới
            prefs.putString("ssid", wifi.ssid);
            prefs.putString("pass", wifi.pass);

            // Ngắt WiFi cũ
            WiFi.disconnect();

            // Kết nối WiFi mới
            WiFi.begin(wifi.ssid, wifi.pass);

            wasConnected = false;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            if (!wasConnected)
            {
                wasConnected = true;

                Serial.println("WiFi connected!");
                Serial.print("IP: ");
                Serial.println(WiFi.localIP());

                configTime(
                    7 * 3600,
                    0,
                    "vn.pool.ntp.org",
                    "time.google.com"
                );

                Serial.println("NTP configured");

                if (getAPI_handle != NULL) xTaskNotifyGive(getAPI_handle);
                if (lunar_handle != NULL) xTaskNotifyGive(lunar_handle);
            }
        }
        else
        {
            wasConnected = false;

            // Chỉ reconnect mỗi 10 giây
            if (wifi.ssid[0] != '\0' &&
                millis() - lastReconnect >= 10000)
            {
                lastReconnect = millis();

                Serial.println("WiFi disconnected. Reconnecting...");

                WiFi.reconnect();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void scanWiFi(void * pvParameters) {
    // 🔴 1. ÉP CHUẨN STATION MODE: Chặn lỗi ESP32 tự nhớ trạng thái AP cũ
    WiFi.mode(WIFI_STA);

    while (1) {
        if (request_wifi_scan) {
            Serial.println("WiFi Scan Begin");
            request_wifi_scan = false;
            
            // 🔴 2. Xóa cache đọng lại từ các lần quét thất bại trước đó
            WiFi.scanDelete();
            
            // 🔴 3. Dừng auto-reconnect ngầm của lõi Arduino
            WiFi.enableSTA(true);

            // Quét sóng (async = false, show_hidden = false, passive = false, max_ms_per_channel = 300ms)
            int n = WiFi.scanNetworks(false, false, false, 300);
            
            WifiScanResult * result = (WifiScanResult*)malloc(sizeof(WifiScanResult));
            if (result == NULL) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            
            result->count = n;
            result->ssid_list[0] = '\0'; 

            // 🔴 4. Bắt lỗi phần cứng (Radio đang bận / Lỗi khởi tạo)
            if (n < 0) {
                Serial.printf("Lỗi phần cứng Radio (Mã lỗi: %d)\n", n);
                snprintf(result->ssid_list, sizeof(result->ssid_list), "Lỗi Radio: %d", n);
            } 
            else if (n == 0) {
                snprintf(result->ssid_list, sizeof(result->ssid_list), "Không tìm thấy Wi-Fi");
            } 
            else {
                int added_count = 0; 
                
                for (int i = 0; i < n; i++) {
                    String current_ssid = WiFi.SSID(i);
                    
                    if (current_ssid.length() == 0) continue;

                    if (strlen(result->ssid_list) + current_ssid.length() + 2 >= sizeof(result->ssid_list)) {
                        Serial.println("Cảnh báo: Đã cắt bớt danh sách Wi-Fi để chống tràn RAM!");
                        break; 
                    }

                    if (added_count > 0) {
                        strcat(result->ssid_list, "\n");
                    }
                    
                    strcat(result->ssid_list, current_ssid.c_str());
                    added_count++;
                }
                
                if (added_count == 0) {
                     snprintf(result->ssid_list, sizeof(result->ssid_list), "Không tìm thấy Wi-Fi");
                }
            }
            
            // Giải phóng bộ đệm phần cứng sau khi lấy xong dữ liệu
            WiFi.scanDelete(); 

            Serial.println("--- Kết quả Scan ---");
            Serial.println(result->ssid_list);
            Serial.println("--------------------");

            lv_async_call(updateWiFiList, result);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void resync(void * pvParameters) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (WiFi.status() == WL_CONNECTED) {
            configTime(7*3600, 0, "vn.pool.ntp.org", "time.google.com");

            if (getAPI_handle != NULL) xTaskNotifyGive(getAPI_handle);
            if (lunar_handle != NULL) xTaskNotifyGive(lunar_handle);
            if (isday_handle != NULL) xTaskNotifyGive(isday_handle);
        }
    }
}

void calculateSun(void * pvParameters) {
    SunSet sun;
    sun.setPosition(10.8033, 106.711, 7);
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        struct tm time;
        if (getLocalTime(&time)) {
            sun.setCurrentDate(time.tm_year + 1900, time.tm_mon + 1, time.tm_mday);
            int sunrise = sun.calcSunrise();
            int sunset = sun.calcSunset();
            sunData sundata = {
                .sunrise_m = sunrise, .sunset_m = sunset
            };
            xQueueSend(isdayHandle, &sundata, portMAX_DELAY);
            // continue here
        }
    }
}

void createTask() {
    xTaskCreatePinnedToCore(getAPI, "Get Weather API", 8192, NULL, 1, &getAPI_handle, 0);
    xTaskCreatePinnedToCore(lunar, "Caculate Lunar day", 4096, NULL, 1, &lunar_handle, 0);
    xTaskCreatePinnedToCore(connectWiFi, "Connect to WiFi", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(scanWiFi, "Scan WiFi to LVGL", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(resync, "Resync API manually", 8192, NULL, 2, &resync_handle, 0);
    xTaskCreatePinnedToCore(calculateSun, "Calculate sun time", 4096, NULL, 1, &isday_handle, 0);
}

void checkWiFi (lv_timer_t * timer) {
    if (WiFi.status() == WL_CONNECTED)
    {
        lv_obj_add_flag(ui_SettingOption1, LV_OBJ_FLAG_HIDDEN);
        if (lv_obj_has_flag(ui_SettingOption3, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(ui_SettingOption3, LV_OBJ_FLAG_HIDDEN);
        if (lv_obj_has_flag(ui_MoveToClock, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(ui_MoveToClock, LV_OBJ_FLAG_HIDDEN);
        String ssid = WiFi.SSID();
        lv_label_set_text_fmt(ui_WiFiStatus, "WiFi: %s", ssid.c_str());
        if (lv_obj_has_flag(ui_resync, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(ui_resync, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        if(lv_obj_has_flag(ui_SettingOption1, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(ui_SettingOption1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_SettingOption3, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_MoveToClock, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_resync, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text_fmt(ui_WiFiStatus, "WiFi: No connection");
        lv_label_set_text(ui_DailyTemp, "No info");
        lv_label_set_text(ui_TempNow, "No info");
        lv_label_set_text(ui_Humidity, "No info");
    }
    
}

void updateWiFiList(void * user_data) {
    WifiScanResult * result = (WifiScanResult*)user_data;
    if (result != NULL) {
        lv_dropdown_set_options(ui_WiFiList, result->ssid_list);
        free(result);
    }
}

void setWeatherInfo(lv_timer_t * timer) {
    WeatherData weatherData;
    if(xQueueReceive(WeatherHandle, &weatherData, 0) == pdTRUE) {
        lv_label_set_text_fmt(ui_TempNow, "%.1f°C", weatherData.temp);
        lv_label_set_text_fmt(ui_DailyTemp, "%.1f - %.1f°C", weatherData.minTemp, weatherData.maxTemp);
        lv_label_set_text_fmt(ui_Humidity, "%d%%", weatherData.humid);
        lv_label_set_text_fmt(ui_Rain, "%d%%", weatherData.rain);
    }
}

void update_clock(lv_timer_t * timer) {
    struct tm time;
    if (getLocalTime(&time, 5)) {
        lv_label_set_text_fmt(ui_Hour, "%02d:%02d", time.tm_hour, time.tm_min);
        lv_label_set_text_fmt(ui_Second, "%02d", time.tm_sec);
        lv_label_set_text_fmt(ui_SolarDay, "%02d-%02d-%04d", time.tm_mday, time.tm_mon + 1, time.tm_year + 1900);
        const char* weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Satuday"};
        lv_label_set_text_fmt(ui_DOW, "%s", weekdays[time.tm_wday]);
        lv_calendar_set_today_date(ui_CalendarD, time.tm_year+1900, time.tm_mon+1, time.tm_mday);
        lv_calendar_set_showed_date(ui_CalendarD, time.tm_year+1900, time.tm_mon+1);
        if (time.tm_hour == 0 && time.tm_min == 0 && time.tm_sec == 0) {
            if (lunar_handle != NULL) xTaskNotifyGive(lunar_handle);
            if (isday_handle != NULL) xTaskNotifyGive(isday_handle);
        }
        
    }
}

void change_theme (lv_timer_t * timer) {
    struct tm time;
    sunData new_data;
    if (xQueueReceive(isdayHandle, &new_data, 0) == pdTRUE) {
        // cached_sun = new_data;
        
        getLocalTime(&time, 5);
        int current_minute = time.tm_hour * 60 + time.tm_min;
        bool is_day = (current_minute >= new_data.sunrise_m && current_minute <= new_data.sunset_m);
        lv_image_set_src(ui_Day, is_day ? &ui_img_84851999 : &ui_img_night_png);
        ui_theme_set(is_day ? THEME_LIGHT : THEME_DARK);
        // has_sun_data = false;
    }
    
}

void update_lunar_callback (void * user_data){ 
    char * text = (char*)user_data;
    if (text) {
        lv_label_set_text(ui_LunarDay, text);
    }
}

void lvglTimerCreate() {
    lv_timer_create(checkWiFi, 500, nullptr);
    lv_timer_create(setWeatherInfo, 5000, nullptr);
    lv_timer_create(update_clock, 1000, nullptr);
    lv_timer_create(change_theme, 5000, nullptr);
}
void lvgl_task(void * pvParameters) {
    uint32_t last_tick = millis();
    while (1) {
        uint32_t now = millis();
        lv_tick_inc(now - last_tick);   // cộng đúng số ms thực tế đã trôi qua
        last_tick = now;

        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
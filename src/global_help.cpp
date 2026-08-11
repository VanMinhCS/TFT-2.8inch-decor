#include <global_help.h>

bool is_decor_mode = false;
bool is_in_widget = false;

int current_screen_index = 2;

lv_group_t * input_group = nullptr;

volatile bool request_wifi_scan = false;
lv_group_t * joystick_group = nullptr;
lv_obj_t * main_screens[NUM_MAIN_SCREENS] = {nullptr};

static void collect_widgets_recursive(lv_obj_t * parent, uint32_t &count, lv_obj_t *&first_widget) {
    uint32_t child_cnt = lv_obj_get_child_cnt(parent);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(parent, i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;

        bool is_clickable = lv_obj_has_flag(child, LV_OBJ_FLAG_CLICKABLE);
        if (is_clickable) {
            lv_group_add_obj(joystick_group, child);
            lv_obj_remove_event_cb(child, joystick_event_handler);
            lv_obj_add_event_cb(child, joystick_event_handler, LV_EVENT_ALL, NULL);
            if (first_widget == NULL) first_widget = child;
            count++;
        }
        // Chỉ đệ quy tiếp nếu bản thân nó KHÔNG phải widget clickable "trọn gói"
        // (Container/Panel thì đệ quy vào trong; Button/Dropdown/Calendar thì dừng lại)
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

    // -------------------------------------------------------------
    // 1. NHẤN GIỮ ĐỂ THOÁT WIDGET (Áp dụng cho mọi màn hình)
    // -------------------------------------------------------------
    if (code == LV_EVENT_LONG_PRESSED && is_in_widget) {
        joystick_standby_mode();
        Serial.println("[MODE] Thoát Widget. Đang chờ ngoài màn hình.");
        return;
    }
    // Điều khiển ưidget
    if (code == LV_EVENT_KEY && is_in_widget) {
        uint32_t key = lv_indev_get_key(lv_indev_get_act());
        lv_obj_t * focused = lv_group_get_focused(joystick_group);

        bool let_widget_handle = false;
        if (focused) {
            if (lv_obj_check_type(focused, &lv_dropdown_class)) {
                let_widget_handle = lv_dropdown_is_open(focused); // dropdown: chỉ nhường khi đang MỞ
            } else if (lv_obj_check_type(focused, &lv_keyboard_class)   ||
                    lv_obj_check_type(focused, &lv_buttonmatrix_class) ||
                    lv_obj_check_type(focused, &lv_calendar_class)   ||
                    lv_obj_check_type(focused, &lv_roller_class)     ||
                    lv_obj_check_type(focused, &lv_spinbox_class)) 
            {
                let_widget_handle = true; // các widget này luôn tự quản lý UP/DOWN/LEFT/RIGHT khi đang focus
            }
        }

        if (let_widget_handle) return;

        if (key == LV_KEY_DOWN) { lv_group_focus_next(joystick_group); return; }
        if (key == LV_KEY_UP)   { lv_group_focus_prev(joystick_group); return; }
    }

    // -------------------------------------------------------------
    // 2. KHI ĐANG "ĐỨNG NGOÀI" MÀN HÌNH (Chưa chui vào Widget)
    // -------------------------------------------------------------
    if (code == LV_EVENT_KEY && !is_in_widget) {
        uint32_t key = lv_indev_get_key(lv_indev_get_act());

        // A. NHẤN ENTER ĐỂ "CHUI VÀO" ĐIỀU KHIỂN (Cho mọi màn hình)
        if (key == LV_KEY_ENTER) {
            is_in_widget = true;
            // lv_group_remove_all_objs(joystick_group);
            
            // Tự động quét và nạp widget của màn hình hiện tại
            auto_load_screen_widgets(lv_scr_act());
            Serial.println("[MODE] Đã chui vào điều khiển Widget.");
            return;
        }

        // B. GẠT TRÁI/PHẢI CHUYỂN MÀN (Chỉ có tác dụng nếu đang ở DECOR)
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

// void ui_generic_setting_screen_loaded_cb(lv_event_t * e) {
//     if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
//         current_state = STATE_IN_SETTINGS;

//         // Lấy con trỏ Màn hình Setting vừa mở
//         lv_obj_t * setting_screen = (lv_obj_t *)lv_event_get_target(e);

//         // Tự động nạp toàn bộ nút bấm trong Màn hình Setting đó vào Joystick
//         lv_group_remove_all_objs(joystick_group);
//         auto_load_screen_widgets(setting_screen);

//         Serial.println("[SETTING] Đã nạp tự động Widget của Màn hình Setting!");
//     }
// }

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
    lv_obj_add_event_cb(ui_SettingTimeManually, ui_event_SubScreen_Loaded, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(ui_Calendar, ui_event_SubScreen_Loaded, LV_EVENT_SCREEN_LOADED, NULL);

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
    String url = "https://api.open-meteo.com/v1/forecast?latitude=10.823&longitude=106.6296&daily=temperature_2m_max,temperature_2m_min&models=best_match&current=relative_humidity_2m,temperature_2m,is_day&timezone=Asia%2FBangkok&forecast_days=1";
    while (1){
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
                    .isDay = doc["current"]["is_day"]
                };
                xQueueSend(WeatherHandle, &weatherData, portMAX_DELAY);
            }
        }

        vTaskDelay(30 * 60 * 1000);
    }
}

void getAPILunar(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(24 * 3600 * 1000); // 1 ngày
    
    // Lần đầu: tính thời gian đến 0:30
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    int current_seconds = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
    int target_seconds = 30 * 60; // 0:30
    
    int wait_seconds = target_seconds - current_seconds;
    if (wait_seconds <= 0) wait_seconds += 24 * 3600;
    
    // Chờ đến 0:30 lần đầu
    vTaskDelay(pdMS_TO_TICKS(wait_seconds * 1000));

    String url = "https://ngaygio.vn/api/lich-am";
    
    while(1) {
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin(url);
            int httpResponseCode = http.GET();

            if (httpResponseCode > 0) {
                String payload = http.getString();
                JsonDocument doc;
                deserializeJson(doc, payload);
                int day = doc["lunar"]["day"];
                int month = doc["lunar"]["month"];
                int year = doc["lunar"]["year"];
                char * lunar = (char*)malloc(11);
                if (lunar){
                    snprintf(lunar, sizeof(lunar), "%02d - %02d - %d", day, month, year);
                    lv_async_call(update_lunar_callback, lunar);
                }
                
            }
        }
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void settingTimeFromLVGL (void * pvParameters) {
    timeData time;
    dayData day;
    struct tm t;
    getLocalTime(&t);
    while (1)
    {
        if (xQueueReceive(TimeHandle, &time, 0))
        {
            t.tm_hour = time.hour;
            t.tm_min = time.minute;
            t.tm_sec = time.second;
            time_t timestamp = mktime(&t);
            struct timeval tv = {
                .tv_sec = timestamp,
                .tv_usec = 0
            };
            settimeofday(&tv, nullptr);
        }

        if (xQueueReceive(DayHandle, &day, 0)) {
            t.tm_mday = day.day;
            t.tm_mon = day.month - 1;
            t.tm_year = day.year - 1900;
            time_t timestamp = mktime(&t);
            struct timeval tv = {
                .tv_sec = timestamp,
                .tv_usec = 0
            };
            settimeofday(&tv, nullptr);
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
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
                Serial.println("Lỗi: Không đủ RAM để cấp phát!");
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

void createTask() {
    xTaskCreatePinnedToCore(getAPI, "Get Weather API", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(getAPILunar, "Get Lunar API", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(settingTimeFromLVGL, "Setting time from LVGL", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(connectWiFi, "Connect to WiFi", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(scanWiFi, "Scan WiFi to LVGL", 4096, NULL, 3, NULL, 0);
}

void checkWiFi (lv_timer_t * timer) {
    if (WiFi.status() == WL_CONNECTED)
    {
        lv_obj_add_flag(ui_SettingOption1, LV_OBJ_FLAG_HIDDEN);
        if (lv_obj_has_flag(ui_SettingOption3, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(ui_SettingOption3, LV_OBJ_FLAG_HIDDEN);
        if (lv_obj_has_flag(ui_MoveToClock, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(ui_MoveToClock, LV_OBJ_FLAG_HIDDEN);
        String ssid = WiFi.SSID();
        lv_label_set_text_fmt(ui_WiFiStatus, "WiFi: %s", ssid.c_str());
    }
    else {
        if(lv_obj_has_flag(ui_SettingOption1, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(ui_SettingOption1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_SettingOption3, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_MoveToClock, LV_OBJ_FLAG_HIDDEN);

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
    while(xQueueReceive(WeatherHandle, &weatherData, 0) == pdTRUE) {
        lv_label_set_text_fmt(ui_TempNow, "%.1f°C", weatherData.temp);
        lv_label_set_text_fmt(ui_DailyTemp, "%.1f - %.1f°C", weatherData.minTemp, weatherData.maxTemp);
        lv_label_set_text_fmt(ui_Humidity, "%d%%", weatherData.humid);
        if (weatherData.isDay) lv_image_set_src(ui_Day, &ui_img_84851999);
        else lv_image_set_src(ui_Day, &ui_img_night_png);
    }
}

void update_clock(lv_timer_t * timer) {
    struct tm time;
    if (getLocalTime(&time, 5)) {
        lv_label_set_text_fmt(ui_Hour, "%02d:%02d", time.tm_hour, time.tm_min);
        lv_label_set_text_fmt(ui_Second, "%02d", time.tm_sec);
        lv_label_set_text_fmt(ui_SolarDay, "%02d-%02d-%04d", time.tm_mday, time.tm_mon + 1, time.tm_year + 1900);
        const char* weekdays[] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
        lv_label_set_text_fmt(ui_DOW, "%s", weekdays[time.tm_wday]);
        lv_calendar_set_today_date(ui_CalendarD, time.tm_year+1900, time.tm_mon+1, time.tm_mday);
        lv_calendar_set_showed_date(ui_CalendarD, time.tm_year+1900, time.tm_mon+1);
    }
}

void update_lunar_callback (void * user_data){ 
    char * text = (char*)user_data;
    if (text) {
        lv_label_set_text(ui_LunarDay, text);
        free(text);
    }
}

void lvglTimerCreate() {
    lv_timer_create(checkWiFi, 500, nullptr);
    lv_timer_create(setWeatherInfo, 30*60*1000, nullptr);
    lv_timer_create(update_clock, 1000, nullptr);
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
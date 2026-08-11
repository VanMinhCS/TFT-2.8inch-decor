#include <global_help.h>

int current_screen_index = 2;

lv_group_t * input_group = nullptr;

volatile bool request_wifi_scan = false;
ControlState current_state = ControlState::STATE_NAVIGATE_SCREEN;
lv_group_t * joystick_group = nullptr;
lv_obj_t * main_screens[NUM_MAIN_SCREENS] = {nullptr};

void auto_load_screen_widgets(lv_obj_t * screen_obj) {
    // 1. Xóa sạch Widget cũ trong Group
    lv_group_remove_all_objs(joystick_group);

    if (screen_obj == NULL) return;

    // 2. Đếm số phần tử con của Màn hình này
    uint32_t child_cnt = lv_obj_get_child_cnt(screen_obj);

    // 3. Tự động thêm các Widget tương tác được vào Joystick Group
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(screen_obj, i);

        // Chỉ thêm nếu Widget đó có cờ CLICKABLE (Bấm được) 
        // và KHÔNG BỊ ẨN (LV_OBJ_FLAG_HIDDEN)
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_CLICKABLE) && 
           !lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) 
        {
            lv_group_add_obj(joystick_group, child);
        }
    }

    Serial.printf("[AUTO] Đã tự động nạp %d Widget của màn hình vào Joystick!\n", child_cnt);
}

void navigate_to_main_screen(int new_index) {
    // Chặn biên: Không vượt quá Screen A (Index 0) hoặc Screen C (Index 3)
    if (new_index < 0 || new_index >= NUM_MAIN_SCREENS) return;

    // Xác định hướng chạy hiệu ứng trượt màn hình
    lv_scr_load_anim_t anim_type = (new_index > current_screen_index) 
                                   ? LV_SCR_LOAD_ANIM_MOVE_LEFT 
                                   : LV_SCR_LOAD_ANIM_MOVE_RIGHT;

    current_screen_index = new_index;

    // Thực hiện chuyển màn hình mượt mà
    lv_scr_load_anim(main_screens[current_screen_index], anim_type, 300, 0, false);
    
    Serial.printf("[NAV] Chuyển sang Màn hình chính Index: %d\n", current_screen_index);
}

void joystick_event_handler(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_LONG_PRESSED) {
        if (current_state == STATE_CONTROL_WIDGET) {
            current_state = STATE_NAVIGATE_SCREEN;
            lv_group_remove_all_objs(joystick_group); // Xóa Focus
            Serial.println("[MODE] >>> THOÁT WIDGET! Quay lại Chế độ Duyệt Màn hình.");
            return;
        }

        if (current_state == STATE_IN_SETTINGS) {
            current_screen_index = 2; // Khôi phục về Index của Clock
            current_state = STATE_NAVIGATE_SCREEN;
            lv_group_remove_all_objs(joystick_group);
            
            lv_scr_load_anim(ui_Time, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
            Serial.println("[MODE] >>> THOÁT SETTING! Quay về Màn hình Clock gốc.");
            return;
        }
    }


    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_indev_get_key(lv_indev_get_act());

        // A. Ý 4: NHẤN CLICK JOYSTICK (ENTER) -> CHUI VÀO WIDGET MÀN HÌNH HIỆN TẠI
        if (key == LV_KEY_ENTER && current_state == STATE_NAVIGATE_SCREEN) {
            current_state = STATE_CONTROL_WIDGET;

            // Xóa sạch group và TỰ ĐỘNG NẠP WIDGET của màn hình hiện tại
            lv_group_remove_all_objs(joystick_group);
            auto_load_screen_widgets(main_screens[current_screen_index]);

            Serial.println("[MODE] >>> ĐÃ CHUI VÀO! Chuyển sang Chế độ Điều khiển Widget.");
            return;
        }

        // B. Ý 1 & 3: GẠT TRÁI / PHẢI KHI ĐANG Ở CHẾ ĐỘ DUYỆT MÀN HÌNH CHÍNH
        if (current_state == STATE_NAVIGATE_SCREEN) {
            if (key == LV_KEY_RIGHT) {
                // Gạt Phải -> Chuyển màn hình kế tiếp (A -> B -> Clock -> C)
                if (current_screen_index < NUM_MAIN_SCREENS - 1) {
                    navigate_to_main_screen(current_screen_index + 1);
                }
            } 
            else if (key == LV_KEY_LEFT) {
                // Gạt Trái -> Chuyển màn hình phía trước
                if (current_screen_index > 0) {
                    navigate_to_main_screen(current_screen_index - 1);
                }
            }
        }
    }
}

void ui_generic_setting_screen_loaded_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        current_state = STATE_IN_SETTINGS;

        // Lấy con trỏ Màn hình Setting vừa mở
        lv_obj_t * setting_screen = (lv_obj_t *)lv_event_get_target(e);

        // Tự động nạp toàn bộ nút bấm trong Màn hình Setting đó vào Joystick
        lv_group_remove_all_objs(joystick_group);
        auto_load_screen_widgets(setting_screen);

        Serial.println("[SETTING] Đã nạp tự động Widget của Màn hình Setting!");
    }
}

void setup_joystick_navigation(lv_indev_t * indev_joystick_driver) {
    // 1. Khai báo danh sách 4 màn hình chính theo thứ tự mảng
    main_screens[0] = ui_Move;
    main_screens[1] = ui_Calendar1;
    main_screens[2] = ui_Time; // Gốc
    main_screens[3] = ui_Keqing;

    // 2. Tạo Group cho Joystick
    joystick_group = lv_group_create();
    lv_indev_set_group(indev_joystick_driver, joystick_group);

    // 3. Đăng ký hàm xử lý sự kiện Joystick chính
    // Ta gắn callback vào một Dummy Object để hứng toàn bộ sự kiện phím
    lv_obj_t * dummy_obj = lv_obj_create(lv_scr_act());
    lv_obj_add_flag(dummy_obj, LV_OBJ_FLAG_HIDDEN); // Ẩn đi
    lv_group_add_obj(joystick_group, dummy_obj);
    lv_obj_add_event_cb(dummy_obj, joystick_event_handler, LV_EVENT_ALL, NULL);
    lv_group_focus_obj(dummy_obj);

    // 4. Load Màn hình Clock xuất phát đầu tiên
    current_screen_index = 2; // Index của Clock
    current_state = ControlState::STATE_NAVIGATE_SCREEN;
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
    // 1. Đọc giá trị ADC trực tiếp (Dải 0 -> 4095 trên ESP32)
    int raw_x = analogRead(pinVRx);
    int raw_y = analogRead(pinVRy);
    bool sw_pressed = (digitalRead(pinSW) == LOW);

    uint32_t current_key = 0;

    // Serial.println("READ Joystick");

    // 2. Xử lý Deadzone & Xác định phím hướng
    // Nếu vượt qua vùng chết (Abs(Val - Center) > Deadzone)
    if (abs(raw_x - ADC_CENTER_X) > ADC_DEADZONE || abs(raw_y - ADC_CENTER_Y) > ADC_DEADZONE) {
        
        // --- Trục X ---
        if (raw_x >= THRES_X_MAX) {
            current_key = LV_KEY_DOWN;
            Serial.printf("DOWN, RawX = %d\n", raw_x);
        } else if (raw_x <= THRES_X_MIN) {
            current_key = LV_KEY_UP;
            Serial.printf("UP, RawX = %d\n", raw_x);
        }
        
        // --- Trục Y ---
        // (Nếu gạt đường chéo, trục Y sẽ ghi đè hoặc kết hợp tùy ưu tiên)
        if (raw_y >= THRES_Y_MAX) {
            current_key = LV_KEY_LEFT;
            Serial.printf("LEFT, Rawy = %d\n", raw_y);
        } 
        else if (raw_y <= THRES_Y_MIN) {
            current_key = LV_KEY_RIGHT;
            Serial.printf("RIGHT, Rawy = %d\n", raw_y);
        }
    } 
    
    // --- Nút nhấn SW ---
    if (sw_pressed) {
        current_key = LV_KEY_ENTER; 
        Serial.println("PRESSED");
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
    while (1) {
        if (request_wifi_scan) {
            request_wifi_scan = false;
            int n = WiFi.scanNetworks();
            WifiScanResult * result = (WifiScanResult*)malloc(sizeof(WifiScanResult));
            result->count = n;
            result->ssid_list[0] = '\0';

            if (n == 0) snprintf(result->ssid_list, sizeof(result->ssid_list), "Không tìm thấy Wi-Fi");
            else {
                for (int i = 0; i < n; i++) {
                    strcat(result->ssid_list, WiFi.SSID(i).c_str());
                    if (i < n-1) strcat(result->ssid_list,"\n");
                }
            }
            WiFi.scanDelete();
            lv_async_call(updateWiFiList, result);
        }
        vTaskDelay(1000);
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
        lv_label_set_text(ui_DailyTemp, "No connection");
        lv_label_set_text(ui_TempNow, "No connection");
        lv_label_set_text(ui_Humidity, "No connection");
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
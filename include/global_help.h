#include <Arduino.h>
#include <ui/ui.h>
#include <ArduinoJson.h>
#include <time.h>
#include <HTTPClient.h>
#include <lvgl.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
// Joystick
const int pinVRx = 8;
const int pinVRy = 9; 
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

// Screen
extern TFT_eSPI tft;
static const uint32_t screenWidth  = 320;
static const uint32_t screenHeight = 240;

// LVGL
static uint8_t *draw_buf1;
extern lv_group_t * input_group;

extern bool is_decor_mode;
extern bool is_in_widget;

extern lv_group_t * joystick_group;
#define NUM_MAIN_SCREENS 4
extern lv_obj_t * main_screens[NUM_MAIN_SCREENS]; 
extern int current_screen_index; // Mặc định xuất phát từ Clock (Index 2)

void auto_load_screen_widgets(lv_obj_t * screen_obj);
void navigate_to_main_screen(int new_index);
void joystick_event_handler(lv_event_t * e);
// void ui_generic_setting_screen_loaded_cb(lv_event_t * e);
void setup_joystick_navigation(lv_indev_t * indev_joystick_driver);
void joystick_standby_mode();
void ui_event_Clock_Loaded(lv_event_t * e);
void ui_event_SubScreen_Loaded(lv_event_t * e);

// Global object for help, use to share between Core 0 and Core 1
extern volatile bool request_wifi_scan;
extern QueueHandle_t WiFiInfo; // use for WiFi info
extern QueueHandle_t DayHandle; // use for Day setting manually
extern QueueHandle_t TimeHandle; // use for Time setting Manually
extern QueueHandle_t WeatherHandle; // use for weather get from API
typedef struct {
    int hour, minute, second;
}timeData;

typedef struct {
    char ssid[64];
    char pass[64];
}WiFiData;

typedef struct {
    int day, month, year;
}dayData;

typedef struct {
    float temp;
    int humid;
    float maxTemp;
    float minTemp;
    bool isDay;
}WeatherData;

typedef struct {
    char ssid_list[10240];
    int count;
}WifiScanResult;

// Method
void my_disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

void joystick_init();
void my_joystick_read(lv_indev_t * indev_drv, lv_indev_data_t * data);

// Method for Core 0
void getAPI(void * pvParameters);
void getAPILunar(void * pvParameters);
void settingTimeFromLVGL(void * pvParameters); 
void connectWiFi(void * pvParameters);
void scanWiFi(void * pvParameters);
void createTask();

// Method for lvgl (Core 1)
void checkWiFi(lv_timer_t * timer);
void updateWiFiList(void * user_data);
void setWeatherInfo(lv_timer_t * timer);
void update_clock(lv_timer_t * timer);
void update_lunar_callback(void * user_data);
void lvglTimerCreate();
void lvgl_task(void *pvParameters);
#include <Arduino.h>
#include <global_help.h>

QueueHandle_t WiFiInfo = NULL;
QueueHandle_t DayHandle = NULL;
QueueHandle_t TimeHandle = NULL;
QueueHandle_t WeatherHandle = NULL;

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  Serial.println("Hello");

  joystick_init();

  lv_init();

  tft.init();
  tft.setSwapBytes(true);
  tft.invertDisplay(false);
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  static lv_color_t buf1[320 * 20];
  static lv_color_t buf2[320 * 20];

  lv_display_t * disp = lv_display_create(320, 240);
  lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_default(disp);

  TimeHandle = xQueueCreate(2, sizeof(timeData));
  WiFiInfo = xQueueCreate(2, sizeof(WiFiData));
  DayHandle = xQueueCreate(2, sizeof(dayData));
  WeatherHandle = xQueueCreate(2, sizeof(WeatherData));
  
  lv_indev_t * indev_joystick = lv_indev_create();
  lv_indev_set_type(indev_joystick, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(indev_joystick, my_joystick_read);
  
  ui_init();
  input_group = lv_group_create();
  lv_indev_set_group(indev_joystick, input_group);
  
  setup_joystick_navigation(indev_joystick);
  
  lvglTimerCreate();

  joystick_init();

  xTaskCreatePinnedToCore(lvgl_task, "LVGL_Main_Task", 4096, NULL, 2, NULL, 1);
  createTask();
  
}

void loop() {
  // lv_timer_handler();
  
}

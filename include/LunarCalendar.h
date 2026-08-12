// LunarCalendar.h
#ifndef LUNAR_CALENDAR_H
#define LUNAR_CALENDAR_H

#include <cmath>

// Cấu trúc lưu kết quả chuyển đổi âm lịch
struct LunarDate {
    int day;
    int month;
    int year;
    bool isLeap; // true nếu là tháng nhuận
};

class LunarCalendar {
public:
    // Đổi ngày Dương lịch sang Âm lịch Việt Nam
    // dd: ngày, mm: tháng, yy: năm, timeZone: múi giờ (Việt Nam = 7.0)
    static LunarDate solarToLunar(int dd, int mm, int yy, double timeZone = 7.0);

    // Đổi ngày Âm lịch Việt Nam sang Dương lịch
    // lunarDay: ngày âm, lunarMonth: tháng âm, lunarYear: năm âm
    // lunarLeap: true nếu là tháng nhuận, timeZone: múi giờ (mặc định 7.0)
    static bool lunarToSolar(int lunarDay, int lunarMonth, int lunarYear, 
                             bool lunarLeap, int& dd, int& mm, int& yy, 
                             double timeZone = 7.0);

private:
    // Hàm lấy phần nguyên (floor) cho số thực, xử lý số âm
    static int INT(double x);
    
    // Chuyển đổi ngày tháng sang số ngày Julius (Julian Day Number)
    static int jdFromDate(int dd, int mm, int yy);
    
    // Chuyển đổi số ngày Julius sang ngày tháng
    static void jdToDate(int jd, int& dd, int& mm, int& yy);
    
    // Tính ngày Sóc (New Moon) thứ k
    static int getNewMoonDay(int k, double timeZone);
    
    // Tính vị trí của mặt trời trên hoàng đạo (0-11)
    static int getSunLongitude(int jdn, double timeZone);
    
    // Tìm ngày bắt đầu tháng 11 âm lịch của một năm
    static int getLunarMonth11(int yy, double timeZone);
    
    // Xác định vị trí của tháng nhuận
    static int getLeapMonthOffset(int a11, double timeZone);
};

#endif // LUNAR_CALENDAR_H
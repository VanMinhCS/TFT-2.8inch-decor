// LunarCalendar.cpp
#include "LunarCalendar.h"
#include <cmath>

// Định nghĩa hằng số PI
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int LunarCalendar::INT(double x) {
    return (int)std::floor(x);
}

int LunarCalendar::jdFromDate(int dd, int mm, int yy) {
    int a, y, m, jd;
    a = INT((14 - mm) / 12.0);
    y = yy + 4800 - a;
    m = mm + 12 * a - 3;
    jd = dd + INT((153 * m + 2) / 5.0) + 365 * y + INT(y / 4.0) - INT(y / 100.0) + INT(y / 400.0) - 32045;
    if (jd < 2299161) {
        jd = dd + INT((153 * m + 2) / 5.0) + 365 * y + INT(y / 4.0) - 32083;
    }
    return jd;
}

void LunarCalendar::jdToDate(int jd, int& dd, int& mm, int& yy) {
    int a, b, c, d, e, m;
    if (jd > 2299160) {
        a = jd + 32044;
        b = INT((4 * a + 3) / 146097.0);
        c = a - INT((b * 146097) / 4.0);
    } else {
        b = 0;
        c = jd + 32082;
    }
    d = INT((4 * c + 3) / 1461.0);
    e = c - INT((1461 * d) / 4.0);
    m = INT((5 * e + 2) / 153.0);
    dd = e - INT((153 * m + 2) / 5.0) + 1;
    mm = m + 3 - 12 * INT(m / 10.0);
    yy = b * 100 + d - 4800 + INT(m / 10.0);
}

int LunarCalendar::getNewMoonDay(int k, double timeZone) {
    double T = k / 1236.85;
    double T2 = T * T;
    double T3 = T2 * T;
    double dr = M_PI / 180.0;
    double Jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * T2 - 0.000000155 * T3;
    Jd1 = Jd1 + 0.00033 * std::sin((166.56 + 132.87 * T - 0.009173 * T2) * dr);
    
    double M = 359.2242 + 29.10535608 * k - 0.0000333 * T2 - 0.00000347 * T3;
    double Mpr = 306.0253 + 385.81691806 * k + 0.0107306 * T2 + 0.00001236 * T3;
    double F = 21.2964 + 390.67050646 * k - 0.0016528 * T2 - 0.00000239 * T3;
    
    double C1 = (0.1734 - 0.000393 * T) * std::sin(M * dr) + 0.0021 * std::sin(2 * dr * M);
    C1 = C1 - 0.4068 * std::sin(Mpr * dr) + 0.0161 * std::sin(2 * dr * Mpr);
    C1 = C1 - 0.0004 * std::sin(3 * dr * Mpr);
    C1 = C1 + 0.0104 * std::sin(2 * dr * F) - 0.0051 * std::sin(dr * (M + Mpr));
    C1 = C1 - 0.0074 * std::sin(dr * (M - Mpr)) + 0.0004 * std::sin(dr * (2 * F + M));
    C1 = C1 - 0.0004 * std::sin(dr * (2 * F - M)) - 0.0006 * std::sin(dr * (2 * F + Mpr));
    C1 = C1 + 0.0010 * std::sin(dr * (2 * F - Mpr)) + 0.0005 * std::sin(dr * (2 * Mpr + M));
    
    double deltat;
    if (T < -11) {
        deltat = 0.001 + 0.000839 * T + 0.0002261 * T2 - 0.00000845 * T3 - 0.000000081 * T * T3;
    } else {
        deltat = -0.000278 + 0.000265 * T + 0.000262 * T2;
    }
    
    double JdNew = Jd1 + C1 - deltat;
    return INT(JdNew + 0.5 + timeZone / 24.0);
}

int LunarCalendar::getSunLongitude(int jdn, double timeZone) {
    double T = (jdn - 2451545.5 - timeZone / 24.0) / 36525.0;
    double T2 = T * T;
    double dr = M_PI / 180.0;
    double M = 357.52910 + 35999.05030 * T - 0.0001559 * T2 - 0.00000048 * T * T2;
    double L0 = 280.46645 + 36000.76983 * T + 0.0003032 * T2;
    double DL = (1.914600 - 0.004817 * T - 0.000014 * T2) * std::sin(dr * M);
    DL = DL + (0.019993 - 0.000101 * T) * std::sin(dr * 2 * M) + 0.000290 * std::sin(dr * 3 * M);
    double L = L0 + DL;
    L = L * dr;
    L = L - 2 * M_PI * (INT(L / (2 * M_PI)));
    return INT(L / M_PI * 6);
}

int LunarCalendar::getLunarMonth11(int yy, double timeZone) {
    int off = jdFromDate(31, 12, yy) - 2415021;
    int k = INT(off / 29.530588853);
    int nm = getNewMoonDay(k, timeZone);
    int sunLong = getSunLongitude(nm, timeZone);
    if (sunLong >= 9) {
        nm = getNewMoonDay(k - 1, timeZone);
    }
    return nm;
}

int LunarCalendar::getLeapMonthOffset(int a11, double timeZone) {
    int k = INT((a11 - 2415021.076998695) / 29.530588853 + 0.5);
    int last = 0;
    int i = 1;
    int arc = getSunLongitude(getNewMoonDay(k + i, timeZone), timeZone);
    do {
        last = arc;
        i++;
        arc = getSunLongitude(getNewMoonDay(k + i, timeZone), timeZone);
    } while (arc != last && i < 14);
    return i - 1;
}

LunarDate LunarCalendar::solarToLunar(int dd, int mm, int yy, double timeZone) {
    LunarDate result = {0, 0, 0, false};
    int dayNumber = jdFromDate(dd, mm, yy);
    int k = INT((dayNumber - 2415021.076998695) / 29.530588853);
    int monthStart = getNewMoonDay(k + 1, timeZone);
    if (monthStart > dayNumber) {
        monthStart = getNewMoonDay(k, timeZone);
    }
    
    int a11 = getLunarMonth11(yy, timeZone);
    int b11 = a11;
    int lunarYear;
    if (a11 >= monthStart) {
        lunarYear = yy;
        a11 = getLunarMonth11(yy - 1, timeZone);
    } else {
        lunarYear = yy + 1;
        b11 = getLunarMonth11(yy + 1, timeZone);
    }
    
    result.day = dayNumber - monthStart + 1;
    int diff = INT((monthStart - a11) / 29.0);
    int lunarLeap = 0;
    int lunarMonth = diff + 11;
    
    if (b11 - a11 > 365) {
        int leapMonthDiff = getLeapMonthOffset(a11, timeZone);
        if (diff >= leapMonthDiff) {
            lunarMonth = diff + 10;
            if (diff == leapMonthDiff) {
                lunarLeap = 1;
            }
        }
    }
    
    if (lunarMonth > 12) {
        lunarMonth = lunarMonth - 12;
    }
    if (lunarMonth >= 11 && diff < 4) {
        lunarYear -= 1;
    }
    
    result.month = lunarMonth;
    result.year = lunarYear;
    result.isLeap = (lunarLeap == 1);
    return result;
}

bool LunarCalendar::lunarToSolar(int lunarDay, int lunarMonth, int lunarYear, 
                                 bool lunarLeap, int& dd, int& mm, int& yy, 
                                 double timeZone) {
    int a11, b11;
    if (lunarMonth < 11) {
        a11 = getLunarMonth11(lunarYear - 1, timeZone);
        b11 = getLunarMonth11(lunarYear, timeZone);
    } else {
        a11 = getLunarMonth11(lunarYear, timeZone);
        b11 = getLunarMonth11(lunarYear + 1, timeZone);
    }
    
    int off = lunarMonth - 11;
    if (off < 0) {
        off += 12;
    }
    
    if (b11 - a11 > 365) {
        int leapOff = getLeapMonthOffset(a11, timeZone);
        int leapMonth = leapOff - 2;
        if (leapMonth < 0) {
            leapMonth += 12;
        }
        if (lunarLeap && lunarMonth != leapMonth) {
            return false; // Tháng nhuận không hợp lệ
        } else if (lunarLeap || off >= leapOff) {
            off += 1;
        }
    }
    
    int k = INT(0.5 + (a11 - 2415021.076998695) / 29.530588853);
    int monthStart = getNewMoonDay(k + off, timeZone);
    jdToDate(monthStart + lunarDay - 1, dd, mm, yy);
    return true;
}
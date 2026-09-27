#pragma once

struct RTCTime {
    unsigned char second;
    unsigned char minute;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned int  year;
};

void rtc_read(RTCTime *t);

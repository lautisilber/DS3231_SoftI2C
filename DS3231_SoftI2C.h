/*
 * DS3231_SoftI2C.h
 * 
 * Arduino Library for the DS3231 Real-Time Clock chip with software I2C
 * 
 * (c) Lautaro Silbergleit
 * 31/10/2023
 * released into the public domain. If you use this, please let me know
 * (just out of pure curiosity!) by sending me an email:
 * lautisilbergleit@gmail.com
 * 
 * The I2C software emulation library used is SoftWire (https://github.com/stevemarple/SoftWire)
 * 
 * Inspired by https://github.com/NorthernWidget/DS3231 and https://github.com/adafruit/TinyRTCLib/tree/master
 *
 */

#ifndef DS3231_SoftI2C_H
#define DS3231_SoftI2C_H

#include <Arduino.h>

#include <SoftWire.h>

class DateTime {
    // https://github.com/adafruit/TinyRTCLib/blob/master/TinyRTClib.h
    // Code by JeeLabs http://news.jeelabs.org/code/
    // Released to the public domain! Enjoy!
    // No changes from RTClib.h

    // Minor changes made by me!
public:
    DateTime(uint32_t t = 0);
    DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour = 0,
           uint8_t min = 0, uint8_t sec = 0);
    DateTime(const char *date, const char *time);
    uint16_t year() const { return 2000 + yOff; }
    uint8_t decade() const { return yOff; }
    uint8_t month() const { return m; }
    uint8_t day() const { return d; }
    uint8_t hour() const { return hh; }
    uint8_t minute() const { return mm; }
    uint8_t second() const { return ss; }
    uint8_t dayOfWeek() const;
    
    // 32-bit times as seconds since 1/1/2000
    uint32_t secondstime() const;
    // 32-bit times as seconds since 1/1/1970
    uint32_t epoch() const; // same as unix time

    void print() const;
    void println() const;
    void print(Stream &s) const;
    void println(Stream &s) const;
    String toString() const;

    bool operator==(const DateTime& that) const;
    bool operator!=(const DateTime& that) const;
    bool operator<(const DateTime& that) const;
    bool operator>(const DateTime& that) const;
    bool operator<=(const DateTime& that) const;
    bool operator>=(const DateTime& that) const;
protected:
    uint8_t yOff, m, d, hh, mm, ss;
};

class DS3231_SoftI2C
{
protected:
    char _swTxBuffer[16];
    char _swRxBuffer[16];
    SoftWire &_sw;

public:
    DS3231_SoftI2C(SoftWire &softWire);

    void begin();
    void end();

    // intended to be called once in setup. this will get the time from the
    // compiler (computer) and compare it with the time from the DS3231.
    // if the compiler time is more recent, the configure the DS3231 with
    // that time
    void matchCompileTime();

    uint32_t getEpoch(); // same as unix time (seconds since 1/1/1970)
    DateTime getDateTime();
    DateTime now(); // same as getDateTime()

    void setEpoch(uint32_t epoch); // same as unix time
    void setDateTime(const DateTime &dateTime);

    // not recommended to use these, really inefficient.
    // it's better to use DateTime and epoch to prevent unnecessary register lookups
    byte getYear();
    byte getMonth();
    byte getDay();
    byte getHours();
    byte getMinutes();
    byte getSeconds();

    // these can be used, though it's still recommended to use DateTime and epoch
    void setYear(byte year);
    void setMonth(byte month);
    void setDay(byte day);
    void setHours(byte hour);
    void setMinutes(byte minute);
    void setSeconds(byte second);
};

#endif

#include "DS3231_SoftI2C.h"

#include <Arduino.h>
//#include <SofWire.h>

#define DS3231_ADDR 0x68
#define SECONDS_FROM_1970_TO_2000 946684800


/******* helpers *******/
const uint8_t daysInMonth[] PROGMEM = {31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30, 31};

static bool isleapYear(const uint16_t y) {
    if(y&3)//check if divisible by 4
        return false;
    //only check other, when first failed
    return (y % 100 || y % 400 == 0);
}

// number of days since 2000/01/01, valid for 2001..2099
static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
    if (y >= 2000)
        y -= 2000;
    uint16_t days = d;
    for (uint8_t i = 1; i < m; ++i)
        days += pgm_read_byte(daysInMonth + i - 1);
    if (m > 2 && y % 4 == 0)
        ++days;
    return days + 365 * y + (y + 3) / 4 - 1;
}

static long time2long(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
    return ((days * 24L + h) * 60 + m) * 60 + s;
}

static uint8_t conv2d(const char *p) {
    uint8_t v = 0;
    if ('0' <= *p && *p <= '9')
        v = *p - '0';
    return 10 * v + *++p - '0';
}

static void getDateTimeStr(DateTime *dt, char *str, uint8_t strLen)
{
    if (strLen < 20) return;
    snprintf(str, strLen, "%04d-%02d-%02d %02d:%02d:%02d", dt->year(), dt->month(), dt->day(), dt->hour(), dt->minute(), dt->second());
}
/***********************/

/******* DateTime *******/
DateTime::DateTime (uint32_t t)
{
    t -= SECONDS_FROM_1970_TO_2000;    // bring to 2000 timestamp from 1970

    ss = t % 60;
    t /= 60;
    mm = t % 60;
    t /= 60;
    hh = t % 24;
    uint16_t days = t / 24;
    uint8_t leap;
    for (yOff = 0; ; ++yOff) {
        leap = isleapYear((uint16_t) yOff);
        if (days < (uint16_t)(365 + leap))
            break;
        days -= (365 + leap);
    }
    for (m = 1; ; ++m) {
        uint8_t daysPerMonth = pgm_read_byte(daysInMonth + m - 1);
        if (leap && m == 2)
            ++daysPerMonth;
        if (days < daysPerMonth)
            break;
        days -= daysPerMonth;
    }
    d = days + 1;
}

DateTime::DateTime (uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)
{
    if (year >= 2000)
        year -= 2000;
    yOff = year;
    m = month;
    d = day;
    hh = hour;
    mm = min;
    ss = sec;
}

// A convenient constructor for using "the compiler's time":
//   DateTime now (__DATE__, __TIME__);
// NOTE: using PSTR would further reduce the RAM footprint
DateTime::DateTime(const char *date, const char *time) {
  // sample input: date = "Dec 26 2009", time = "12:34:56"
  yOff = conv2d(date + 9);
  // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
  switch (date[0]) {
  case 'J':
    m = date[1] == 'a' ? 1 : m = date[2] == 'n' ? 6 : 7;
    break;
  case 'F':
    m = 2;
    break;
  case 'A':
    m = date[2] == 'r' ? 4 : 8;
    break;
  case 'M':
    m = date[2] == 'r' ? 3 : 5;
    break;
  case 'S':
    m = 9;
    break;
  case 'O':
    m = 10;
    break;
  case 'N':
    m = 11;
    break;
  case 'D':
    m = 12;
    break;
  }
  d = conv2d(date + 4);
  hh = conv2d(time);
  mm = conv2d(time + 3);
  ss = conv2d(time + 6);
}

uint8_t DateTime::dayOfWeek() const {
    uint16_t day = date2days(yOff, m, d);
    return (day + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
}

uint32_t DateTime::secondstime() const 
{
    uint32_t t;
    uint16_t days = date2days(yOff, m, d);
    t = time2long(days, hh, mm, ss);
    return t;
}

// unix time only correct if in UTC timezone
uint32_t DateTime::epoch() const
{
    uint32_t t = secondstime();
    t += SECONDS_FROM_1970_TO_2000;
    return t;
}

void DateTime::print() const
{
    print(Serial);
}

void DateTime::println() const
{
    println(Serial);
}

void DateTime::print(Stream &s) const
{
    const uint8_t strLen = 20;
    char str[strLen] = {0};
    getDateTimeStr(this, str, strLen);
    s.print(str);
}

void DateTime::println(Stream &s) const
{
    const uint8_t strLen = 20;
    char str[strLen] = {0};
    getDateTimeStr(this, str, strLen);
    s.println(str);
}

bool DateTime::operator==(const DateTime& that) const
{
    return secondstime() == that.secondstime();
}

bool DateTime::operator!=(const DateTime& that) const
{
    return !operator==(that);
}

bool DateTime::operator<(const DateTime& that) const
{
    return secondstime() < that.secondstime();
}
bool DateTime::operator>(const DateTime& that) const
{
    return !operator<(that);
}
bool DateTime::operator<=(const DateTime& that) const
{
    return operator<(that) || operator==(that);
}
bool DateTime::operator>=(const DateTime& that) const
{
    return !operator<=(that);   
}
/************************/

/****** helpers ******/
static inline byte decToBcd(byte val) {
    // Convert normal decimal numbers to binary coded decimal
    return ( (val/10*16) + (val%10) );
}

static byte readControlByte(SoftWire &_sw, bool which) {
    // Read selected control byte
    // first byte (0) is 0x0e, second (1) is 0x0f
    _sw.beginTransmission(DS3231_ADDR);
    if (which) {
        // second control byte
        _sw.write(0x0f);
    } else {
        // first control byte
        _sw.write(0x0e);
    }
    _sw.endTransmission();
    _sw.requestFrom(DS3231_ADDR, 1);
    return _sw.read();
}

static void writeControlByte(SoftWire &_sw, byte control, bool which) {
    // Write the selected control byte.
    // which=false -> 0x0e, true->0x0f.
    _sw.beginTransmission(DS3231_ADDR);
    if (which) {
        _sw.write(0x0f);
    } else {
        _sw.write(0x0e);
    }
    _sw.write(control);
    _sw.endTransmission();
}

static inline byte yearFromRegisters(uint8_t registers[7])
{
    int16_t tenYear = (registers[6] & 0xf0) >> 4;
    int16_t unitYear = registers[6] & 0x0f;
    return (byte)((10 * tenYear) + unitYear);
}

static inline byte monthFromRegisters(uint8_t registers[7])
{
    int16_t tenMonth = (registers[5] & 0x10) >> 4;
    int16_t unitMonth = registers[5] & 0x0f;
    return (byte)((10 * tenMonth) + unitMonth);
}

static inline byte dayFromRegisters(uint8_t registers[7])
{
    int16_t tenDateOfMonth = (registers[4] & 0x30) >> 4;
    int16_t unitDateOfMonth = registers[4] & 0x0f;
    return (byte)((10 * tenDateOfMonth) + unitDateOfMonth);
}

static byte hourFromRegisters(uint8_t registers[7])
{
    bool twelveHour = registers[2] & 0x40;
    bool pm = false;
    int16_t unitHour;
    int16_t tenHour;
    if (twelveHour) {
        pm = registers[2] & 0x20;
        tenHour = (registers[2] & 0x10) >> 4;
    } else {
        tenHour = (registers[2] & 0x30) >> 4;
    }
    unitHour = registers[2] & 0x0f;
    byte hour = (byte)((10 * tenHour) + unitHour);
    if (twelveHour) {
        // 12h clock? Convert to 24h.
        hour += 12;
    }
    return hour;
}

static inline byte minuteFromRegisters(uint8_t registers[7])
{
    int16_t tenMinute = (registers[1] & 0xf0) >> 4;
    int16_t unitMinute = registers[1] & 0x0f;
    return (byte)((10 * tenMinute) + unitMinute);
}

static inline byte secondFromRegisters(uint8_t registers[7])
{
    int16_t tenSecond = (registers[0] & 0xf0) >> 4;
    int16_t unitSecond = registers[0] & 0x0f;
    return (byte)((10 * tenSecond) + unitSecond);
}
/*********************/

/**** DS3231_SoftI2C ****/
DS3231_SoftI2C::DS3231_SoftI2C(SoftWire &softWire) : _sw(softWire) {}

void DS3231_SoftI2C::begin()
{
    _sw.setTxBuffer(_swTxBuffer, sizeof(_swTxBuffer)/sizeof(_swTxBuffer[0]));
    _sw.setRxBuffer(_swRxBuffer, sizeof(_swRxBuffer)/sizeof(_swRxBuffer[0]));
    _sw.setDelay_us(5);
    _sw.setTimeout(1000);
    _sw.begin();
}

void DS3231_SoftI2C::end()
{
    _sw.end();
}

void DS3231_SoftI2C::matchCompileTime()
{
    const DateTime compileTime(__DATE__, __TIME__);
    if (compileTime > now())
        setDateTime(compileTime);
}

DateTime DS3231_SoftI2C::getDateTime()
{
    // Ensure register address is valid
    _sw.beginTransmission(DS3231_ADDR);
    _sw.write(uint8_t(0)); // Access the first register
    _sw.endTransmission();

    uint8_t registers[7]; // There are 7 registers we need to read from to get the date and time.
    uint8_t numBytes = _sw.requestFrom(DS3231_ADDR, (uint8_t)7);
    for (uint8_t i = 0; i < numBytes; ++i)
    {
        registers[i] = _sw.read();
    }
    if (numBytes != 7)
    {
        Serial.println("Couldn't read time");
        return DateTime();
    }

    byte year = yearFromRegisters(registers);
    byte month = monthFromRegisters(registers);
    byte day = dayFromRegisters(registers);
    byte hour = hourFromRegisters(registers);
    byte minute = minuteFromRegisters(registers);
    byte second = secondFromRegisters(registers);
    DateTime now(year, month, day, hour, minute, second);
    return now;
}

DateTime DS3231_SoftI2C::now()
{
    return getDateTime();
}

uint32_t DS3231_SoftI2C::getEpoch()
{
    DateTime dateTime = getDateTime();
    return dateTime.epoch();
}

byte DS3231_SoftI2C::getYear()
{
    DateTime dateTime = getDateTime();
    return dateTime.decade();
}

byte DS3231_SoftI2C::getMonth()
{
    DateTime dateTime = getDateTime();
    return dateTime.month();
}

byte DS3231_SoftI2C::getDay()
{
    DateTime dateTime = getDateTime();
    return dateTime.day();
}

byte DS3231_SoftI2C::getHours()
{
    DateTime dateTime = getDateTime();
    return dateTime.hour();
}

byte DS3231_SoftI2C::getMinutes()
{
    DateTime dateTime = getDateTime();
    return dateTime.minute();
}

byte DS3231_SoftI2C::getSeconds()
{
    DateTime dateTime = getDateTime();
    return dateTime.second();
}

void DS3231_SoftI2C::setYear(byte year)
{
    _sw.beginTransmission(DS3231_ADDR);
    _sw.write(0x06);
    _sw.write(decToBcd(year));
    _sw.endTransmission();
}

void DS3231_SoftI2C::setMonth(byte month)
{
    _sw.beginTransmission(DS3231_ADDR);
    _sw.write(0x05);
    _sw.write(decToBcd(month));
    _sw.endTransmission();
}

void DS3231_SoftI2C::setDay(byte day)
{
    _sw.beginTransmission(DS3231_ADDR);
    _sw.write(0x04);
    _sw.write(decToBcd(day));
    _sw.endTransmission();
}

void DS3231_SoftI2C::setHours(byte hour)
{
    // Sets the hour, without changing 12/24h mode.
    // The hour must be in 24h format.

    bool h12;
    byte temp_hour;

    // Start by figuring out what the 12/24 mode is
    _sw.beginTransmission(DS3231_ADDR);
    _sw.write(0x02);
    _sw.endTransmission();
    _sw.requestFrom(DS3231_ADDR, 1);
    h12 = (_sw.read() & 0b01000000);
    // if h12 is true, it's 12h mode; false is 24h.

    if (h12) {
        // 12 hour
        bool am_pm = (hour > 11);
        temp_hour = hour;
        if (temp_hour > 11) {
            temp_hour = temp_hour - 12;
        }
        if (temp_hour == 0) {
            temp_hour = 12;
        }
        temp_hour = decToBcd(temp_hour) | (am_pm << 5) | 0b01000000;
    } else {
        // 24 hour
        temp_hour = decToBcd(hour) & 0b10111111;
    }

    _sw.beginTransmission(DS3231_ADDR);
    _sw.write(0x02);
    _sw.write(temp_hour);
    _sw.endTransmission();
}

void DS3231_SoftI2C::setMinutes(byte minute)
{
    _sw.beginTransmission(DS3231_ADDR);
    _sw.write(0x01);
    _sw.write(decToBcd(minute));
    _sw.endTransmission();
}

void DS3231_SoftI2C::setSeconds(byte second)
{
    _sw.beginTransmission(DS3231_ADDR);
    _sw.write(0x00);
    _sw.write(decToBcd(second));
    _sw.endTransmission();

    // clear OSF flag
    byte temp_buffer = readControlByte(_sw, 1);
    writeControlByte(_sw, (temp_buffer & 0b01111111), 1);
}

void DS3231_SoftI2C::setDateTime(const DateTime &dateTime)
{
    setYear(dateTime.decade());
    setMonth(dateTime.month());
    setDay(dateTime.day());
    setHours(dateTime.hour());
    setMinutes(dateTime.minute());
    setSeconds(dateTime.second());
}

void DS3231_SoftI2C::setEpoch(uint32_t epoch)
{
    DateTime dateTime(epoch);
    setDateTime(dateTime);
}
/*************************/

// this file is from project:  https://github.com/zxhio/time_rfc3339.git
// update to support parse rfc3339 string, and support timespec conversion

//===- time_rfc3339.h - Time library for RFC3339 ----------------*- C++ -*-===//
//
/// \file
/// Date and time library for RFC3339 with C++11 implementation.
//
// Author:  zxh
// Date:    2021/10/11 21:26:25
//===----------------------------------------------------------------------===//

#pragma once

#include <time.h>

#include <chrono>
#include <stdexcept>
#include <string>

namespace time_rfc3339 {

// The digits table is used to look up for number within 100.
// Each two character corresponds to one digit and ten digits.
static constexpr char DigitsTable[200] = {
    '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0',
    '7', '0', '8', '0', '9', '1', '0', '1', '1', '1', '2', '1', '3', '1', '4',
    '1', '5', '1', '6', '1', '7', '1', '8', '1', '9', '2', '0', '2', '1', '2',
    '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
    '3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3',
    '7', '3', '8', '3', '9', '4', '0', '4', '1', '4', '2', '4', '3', '4', '4',
    '4', '5', '4', '6', '4', '7', '4', '8', '4', '9', '5', '0', '5', '1', '5',
    '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
    '6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6',
    '7', '6', '8', '6', '9', '7', '0', '7', '1', '7', '2', '7', '3', '7', '4',
    '7', '5', '7', '6', '7', '7', '7', '8', '7', '9', '8', '0', '8', '1', '8',
    '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
    '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9',
    '7', '9', '8', '9', '9'};

template <typename T,
          typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
static inline size_t formatUIntInternal(T v, char to[]) {
  char *p = to;

  while (v >= 100) {
    const unsigned idx = (v % 100) << 1;
    v /= 100;
    *p++ = DigitsTable[idx + 1];
    *p++ = DigitsTable[idx];
  }

  if (v < 10) {
    *p++ = v + '0';
  } else {
    const unsigned idx = v << 1;
    *p++ = DigitsTable[idx + 1];
    *p++ = DigitsTable[idx];
  }

  return p - to;
}

template <typename T,
          typename std::enable_if<std::is_integral<T>::value, T>::type = 0>
static inline size_t formatUIntWidth(T v, char *to, size_t fmtLen) {
  char buf[sizeof(v) * 4];
  size_t len = formatUIntInternal(v, buf);
  char *p = buf + len;

  for (size_t i = len; i < fmtLen; i++)
    *to++ = '0';

  size_t minLen = std::min(len, fmtLen);
  for (size_t i = 0; i < minLen; ++i)
    *to++ = *--p;

  return fmtLen;
}

static inline size_t formatChar(char *to, char c) {
  *to = c;
  return sizeof(char);
}

enum TimeFieldLen : size_t {
  Year = 4,
  Month = 2,
  Day = 2,
  Hour = 2,
  Minute = 2,
  Second = 2,
};

enum SecFracLen : size_t { Sec = 0, Milli = 3, Macro = 6, Nano = 9 };

class Time {
public:
  using TimePoint = std::chrono::time_point<std::chrono::system_clock,
                                            std::chrono::nanoseconds>;
  Time() = delete;
  Time(const Time &) = default;
  Time &operator=(const Time &) = default;

  explicit Time(const time_t second)
      : Time(TimePoint(std::chrono::seconds(second))) {}
  explicit Time(const TimePoint &tp) : tp_(tp) {}

  explicit Time(const timespec ts) : tp_(timespecToTimePoint(ts)) {}

  /// Current time.
  static Time now() { return Time(std::chrono::system_clock::now()); }

  std::time_t to_time_t() const { return std::chrono::system_clock::to_time_t(tp_); }
  timespec to_timespec() const { return timepointToTimespec(tp_); }
  TimePoint to_timepoint() const { return tp_; }

  /// Year (4 digits, e.g. 1996).
  int year() const { return toTm().tm_year + 1900; }

  /// Month of then year, in the range [1, 12].
  int month() const { return toTm().tm_mon + 1; }

  /// Day of the month, in the range [1, 28/29/30/31].
  int day() const { return toTm().tm_mday; }

  /// Day of the week, in the range [1, 7].
  int weekday() const { return toTm().tm_wday; }

  /// Hour within day, in the range [0, 23].
  int hour() const { return toTm().tm_hour; }

  /// Minute offset within the hour, in the range [0, 59].
  int minute() const { return toTm().tm_min; }

  /// Second offset within the minute, in the range [0, 59].
  int second() const { return toTm().tm_sec; }

  /// Nanosecond offset within the second, in the range [0, 999999999].
  int nanosecond() const { return static_cast<int>(count() % std::nano::den); }

  /// Count of nanosecond elapsed since 1970-01-01T00:00:00Z .
  int64_t count() const { return tp_.time_since_epoch().count(); }

  /// Timezone name and offset in seconds east of UTC.
  std::pair<long int, std::string> timezone() const {
    static thread_local long int t_off = std::numeric_limits<long int>::min();
    static thread_local char t_zone[8];

    if (t_off == std::numeric_limits<long int>::min()) {
      struct tm t;
      time_t c = std::chrono::system_clock::to_time_t(tp_);
      localtime_r(&c, &t);
      t_off = t.tm_gmtoff;
      std::copy(t.tm_zone,
                t.tm_zone + std::char_traits<char>::length(t.tm_zone), t_zone);
    }

    return std::make_pair(t_off, t_zone);
  }

  /// Standard date-time full format using RFC3339 specification.
  /// e.g.
  ///   2021-10-10T13:46:58Z
  ///   2021-10-10T05:46:58+08:00
  std::string format() const { return formatInternal(SecFracLen::Sec); }

  /// Standard date-time format with millisecond using RFC3339 specification.
  /// e.g.
  ///   2021-10-10T13:46:58.123Z
  ///   2021-10-10T05:46:58.123+08:00
  std::string formatMilli() const { return formatInternal(SecFracLen::Milli); }

  /// Standard date-time format with macrosecond using RFC3339 specification.
  /// e.g.
  ///   2021-10-10T13:46:58.123456Z
  ///   2021-10-10T05:46:58.123456+08:00
  std::string formatMacro() const { return formatInternal(SecFracLen::Macro); }

  /// Standard date-time format with nanosecond using RFC3339 specification.
  /// e.g.
  ///   2021-10-10T13:46:58.123456789Z
  ///   2021-10-10T05:46:58.123456789+08:00
  std::string formatNano() const { return formatInternal(SecFracLen::Nano); }

  /// Parse an RFC3339 timestamp with an optional nanosecond fraction.
  /// Throws std::invalid_argument when the string is malformed or out of range.
  static Time parseNano(const std::string &value) {
    return parseInternal(value, SecFracLen::Nano, false);
  }

  /// Parse an RFC3339 timestamp with an optional millisecond fraction.
  /// Throws std::invalid_argument when the string is malformed or out of range.
  static Time parseMilli(const std::string &value) {
    return parseInternal(value, SecFracLen::Milli, true);
  }

  /// Parse an RFC3339 timestamp with an optional microsecond fraction.
  /// Throws std::invalid_argument when the string is malformed or out of range.
  static Time parseMacro(const std::string &value) {
    return parseInternal(value, SecFracLen::Macro, true);
  }

  static constexpr TimePoint timespecToTimePoint(const timespec &ts)
  {
    return TimePoint{duration_cast<std::chrono::system_clock::duration>(timespecToDuration(ts))};
  }

  static constexpr timespec timepointToTimespec(const TimePoint &tp)
  {
    auto secs = time_point_cast<std::chrono::seconds>(tp);
    auto ns = time_point_cast<std::chrono::nanoseconds>(tp) -
             time_point_cast<std::chrono::nanoseconds>(secs);

    return timespec{secs.time_since_epoch().count(), (long)ns.count()};
  }

private:

  static constexpr std::chrono::nanoseconds timespecToDuration(const timespec &ts)
  {
    auto duration = std::chrono::seconds{ts.tv_sec} + std::chrono::nanoseconds{ts.tv_nsec};
    return duration_cast<std::chrono::nanoseconds>(duration);
  }

  static constexpr timespec durationToTimespec(std::chrono::nanoseconds dur)
  {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur);
    dur -= secs;
    return timespec{secs.count(), (long)dur.count()};
  }

  static constexpr size_t MIN = sizeof("2006-01-02T15:04:05Z")-1;
  static constexpr size_t MAX = sizeof("2006-01-02T15:04:05.123456789+06:00")-1;
  static Time parseInternal(const std::string &value, size_t fracLen,
                            bool exactFracLen) {
    const size_t size = value.size();
    if (size<MIN || size>MAX)
        throw std::invalid_argument("invalid RFC3339 timestamp");
    size_t pos = 0;

    auto expect = [&](char c) {
      if (pos >= size || value[pos] != c)
        throw std::invalid_argument("invalid RFC3339 timestamp");
      ++pos;
    };
    auto readDigits = [&](size_t count) {
      if (pos + count > size)
        throw std::invalid_argument("invalid RFC3339 timestamp");
      int result = 0;
      for (size_t i = 0; i < count; ++i) {
        const char c = value[pos++];
        if (c < '0' || c > '9')
          throw std::invalid_argument("invalid RFC3339 timestamp");
        result = result * 10 + c - '0';
      }
      return result;
    };

    const int year = readDigits(4);
    expect('-');
    const int month = readDigits(2);
    expect('-');
    const int day = readDigits(2);
    if (month < 1 || month > 12 || day < 1 ||
        day > daysInMonth(year, month))
      throw std::invalid_argument("invalid RFC3339 timestamp");

    if (pos >= size || (value[pos] != 'T' && value[pos] != 't'))
      throw std::invalid_argument("invalid RFC3339 timestamp");
    ++pos;
    const int hour = readDigits(2);
    expect(':');
    const int minute = readDigits(2);
    expect(':');
    const int second = readDigits(2);
    if (hour > 23 || minute > 59 || second > 59)
      throw std::invalid_argument("invalid RFC3339 timestamp");

    int nanosecond = 0;
    if (pos < size && value[pos] == '.') {
      ++pos;
      const size_t fractionStart = pos;
      while (pos < size && value[pos] >= '0' && value[pos] <= '9') {
        if (pos - fractionStart == Nano)
          throw std::invalid_argument("invalid RFC3339 timestamp");
        nanosecond = nanosecond * 10 + value[pos++] - '0';
      }
      const size_t fractionLength = pos - fractionStart;
      if (fractionLength == 0 ||
          (exactFracLen && fractionLength != fracLen))
        throw std::invalid_argument("invalid RFC3339 timestamp");
      for (size_t i = fractionLength; i < Nano; ++i)
        nanosecond *= 10;
    }

    int offsetSeconds = 0;
    if (pos < size && (value[pos] == 'Z' || value[pos] == 'z')) {
      ++pos;
    } else {
      if (pos >= size || (value[pos] != '+' && value[pos] != '-'))
        throw std::invalid_argument("invalid RFC3339 timestamp");
      const bool negative = value[pos++] == '-';
      const int offsetHour = readDigits(2);
      expect(':');
      const int offsetMinute = readDigits(2);
      if (offsetHour > 23 || offsetMinute > 59)
        throw std::invalid_argument("invalid RFC3339 timestamp");
      offsetSeconds = offsetHour * 3600 + offsetMinute * 60;
      if (negative)
        offsetSeconds = -offsetSeconds;
    }
    if (pos != size)
      throw std::invalid_argument("invalid RFC3339 timestamp");

    const int64_t seconds = daysFromCivil(year, month, day) * 86400 +
                            hour * 3600 + minute * 60 + second - offsetSeconds;
    return Time(TimePoint(std::chrono::seconds(seconds) +
                          std::chrono::nanoseconds(nanosecond)));
  }

private:
  static bool isLeapYear(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  }

  static int daysInMonth(int year, int month) {
    static constexpr int days[] = {31, 28, 31, 30, 31, 30,
                                   31, 31, 30, 31, 30, 31};
    return days[month - 1] + (month == 2 && isLeapYear(year));
  }

  // ref: https://howardhinnant.github.io/date_algorithms.html
  static int64_t daysFromCivil(int year, int month, int day) {
    year -= month <= 2;
    const int64_t era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 +
                               day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 -
                              yearOfEra / 100 + dayOfYear;
    return era * 146097 + static_cast<int64_t>(dayOfEra) - 719468;
  }

  struct tm toTm() const {
    struct tm t;
    time_t c = std::chrono::system_clock::to_time_t(tp_);

    // gmtime_r() is 150% faster than localtime_r() because it doesn't need to
    // calculate the time zone. For a more faster implementation refer to
    // musl-libc, which is 10% faster than the glibc gmtime_r().
    // ref:
    // https://git.musl-libc.org/cgit/musl/tree/src/time/gmtime_r.c?h=v1.2.2#n4
    // gmtime_r(&c, &t);
    localtime_r(&c, &t);

    return t;
  }

  std::string formatInternal(size_t fracLen) const {
    char datetime[40];
    char *p = datetime;
    struct tm t = toTm();

    p += formatDate(p, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    p += formatChar(p, 'T');
    p += formatTime(p, t.tm_hour, t.tm_min, t.tm_sec, fracLen);

    return std::string(datetime, p - datetime);
  }

  size_t formatDate(char *to, int year, int mon, int mday) const {
    char *p = to;
    p += formatUIntWidth(year, p, TimeFieldLen::Year);
    p += formatChar(p, '-');
    p += formatUIntWidth(mon, p, TimeFieldLen::Month);
    p += formatChar(p, '-');
    p += formatUIntWidth(mday, p, TimeFieldLen::Day);
    return p - to;
  }

  size_t formatTime(char *to, int hour, int min, int sec,
                    size_t fracLen) const {
    char *p = to;
    p += formatPartialTime(p, hour, min, sec, fracLen);
    p += formatTimeOff(p);
    return p - to;
  }

  size_t formatPartialTime(char *to, int hour, int min, int sec,
                           size_t fracLen) const {
    char *p = to;
    p += formatUIntWidth(hour, p, TimeFieldLen::Hour);
    p += formatChar(p, ':');
    p += formatUIntWidth(min, p, TimeFieldLen::Minute);
    p += formatChar(p, ':');
    p += formatUIntWidth(sec, p, TimeFieldLen::Second);
    p += formatSecFrac(p, nanosecond(), fracLen);
    return p - to;
  }

  size_t formatSecFrac(char *to, int frac, size_t fracLen) const {
    if (fracLen == 0 || frac == 0)
      return 0;

    char *p = to;
    p += formatChar(p, '.');
    p += formatUIntWidth(frac, p, fracLen);
    return p - to;
  }

  size_t formatTimeOff(char *to) const {
    long int off = timezone().first;
    char *p = to;

    if (off == 0) {
      p += formatChar(p, 'Z');
    } else {
      p += formatChar(p, off < 0 ? '-' : '+');
      p += formatUIntWidth(off / 3600, p, TimeFieldLen::Hour);
      p += formatChar(p, ':');
      p += formatUIntWidth(off % 3600, p, TimeFieldLen::Minute);
    }

    return p - to;
  }

  TimePoint tp_;
};

} // namespace time_rfc3339

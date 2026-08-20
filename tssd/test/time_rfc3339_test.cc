#include "gtest/gtest.h"

#include "time_rfc3339.h"

using time_rfc3339::Time;

TEST(TimeRFC3339, ParseNano) {
  const Time time = Time::parseNano("2021-10-10T05:46:58.123456789+08:00");
  const Time utc = Time::parseNano("2021-10-09T21:46:58.123456789Z");

  EXPECT_EQ(time.count(), utc.count());
}

TEST(TimeRFC3339, ParseMilliAndMacro) {
  const Time milli = Time::parseMilli("2021-10-10T05:46:58.123+08:00");
  const Time macro = Time::parseMacro("2021-10-10T05:46:58.123456+08:00");

  EXPECT_EQ(milli.nanosecond(), 123000000);
  EXPECT_EQ(macro.nanosecond(), 123456000);
  EXPECT_EQ(milli.count() + 456000, macro.count());
}

TEST(TimeRFC3339, ParseNanoRejectsInvalidInput) {
  EXPECT_THROW(Time::parseNano("2021-02-29T00:00:00Z"), std::invalid_argument);
  EXPECT_THROW(Time::parseNano("2021-10-10T00:00:00.1234567890Z"),
               std::invalid_argument);
  EXPECT_THROW(Time::parseNano("2021-10-10T00:00:00"), std::invalid_argument);
  EXPECT_THROW(Time::parseMilli("2021-10-10T00:00:00.12Z"),
               std::invalid_argument);
  EXPECT_THROW(Time::parseMacro("2021-10-10T00:00:00.123Z"),
               std::invalid_argument);
}

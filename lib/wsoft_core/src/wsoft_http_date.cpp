#include "wsoft_http_date.h"

#include <stdio.h>
#include <string.h>

namespace {

int month_index(const char *m) {
  static const char *const names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  for (int i = 0; i < 12; i++) {
    if (strncmp(m, names[i], 3) == 0) return i;
  }
  return -1;
}

// Days from 1970-01-01 to y-m-d (proleptic Gregorian), no timegm() needed.
long days_from_civil(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

}  // namespace

bool wsoft_parse_http_date(const char *text, time_t *out) {
  if (!text || !out) return false;
  char wday[4], mon[4], zone[4];
  int day = 0, year = 0, hh = 0, mm = 0, ss = 0;
  if (sscanf(text, "%3s, %d %3s %d %d:%d:%d %3s", wday, &day, mon, &year, &hh, &mm, &ss, zone) != 8) return false;
  if (strcmp(zone, "GMT") != 0) return false;
  int m = month_index(mon);
  if (m < 0 || day < 1 || day > 31 || year < 2020 || year > 2200 || hh < 0 || hh > 23 || mm < 0 || mm > 59 ||
      ss < 0 || ss > 60) {
    return false;
  }
  long days = days_from_civil(year, m + 1, day);
  *out = (time_t)(days * 86400L + hh * 3600L + mm * 60L + ss);
  return true;
}

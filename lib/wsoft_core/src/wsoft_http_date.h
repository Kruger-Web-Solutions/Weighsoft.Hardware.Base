#pragma once

// Reads the HTTP Date header the Hub sends with every reply, so a board on a network with no
// internet time can still set its clock (the Hub checks each signature against its own clock,
// 300 s window). Ported from the 31 Oct 2026 Waveshare branch (c49a592, hub_parse_http_date),
// Benjamin 18 Nov 2026. Pure C++, host-tested in test/test_wsoft_http_date.

#include <time.h>

// Parses an RFC 7231 IMF-fixdate ("Sat, 31 Oct 2026 08:05:56 GMT") into Unix seconds (UTC).
// Returns false for any other format, a zone other than GMT, or a year before 2020.
bool wsoft_parse_http_date(const char *text, time_t *out);

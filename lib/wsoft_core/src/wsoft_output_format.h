#pragma once

// Weight formatter for the Writer (Rhyno, 12 Nov 2026): how the weight looks on the line the
// Writer sends out of its port, so the receiving device (printer, big display, PC) gets the
// format it expects. Runs on the Writer, the board with the spare room. Pure C++, host-tested.
//
// The template is plain text with these fields, each replaced by what the weigh board sent:
//   {line}    the whole line as received (the default template, so nothing changes by itself)
//   {weight}  the weight number, e.g. 12.345
//   {unit}    kg, g or t (empty when the weigh board sent none)
//   {stable}  ST when steady, US when moving (empty when the weigh board sent no stable flag)
// Any other text, including an unknown {field}, is copied as it is.

#include <stddef.h>

static const size_t WSOFT_OUTPUT_FORMAT_MAX = 64;
static const char *const WSOFT_OUTPUT_FORMAT_DEFAULT = "{line}";

struct WsoftOutputFields {
  const char *line = "";
  const char *weight = "";
  const char *unit = "";
  bool has_stable = false;
  bool stable = false;
};

// Writes the formatted line to out (always 0-terminated when size > 0). An empty or null
// template means {line}. Returns false when out was too small (the text is cut) or when the
// template needs a weight and none was received: the caller must then not send the line,
// because a missing weight is never sent as a weight.
bool wsoft_format_output(const char *tmpl, const WsoftOutputFields &fields, char *out, size_t size);

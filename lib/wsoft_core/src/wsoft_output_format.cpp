#include "wsoft_output_format.h"

#include <string.h>

namespace {

// Appends text at *used; false when it did not all fit.
bool append(char *out, size_t size, size_t *used, const char *text) {
  size_t length = strlen(text);
  size_t room = size - 1 - *used;
  size_t take = length < room ? length : room;
  memcpy(out + *used, text, take);
  *used += take;
  out[*used] = '\0';
  return take == length;
}

// The value for a {field} name, or nullptr when the name is not a field.
const char *field_value(const char *name, size_t length, const WsoftOutputFields &f, bool *needs_weight) {
  if (length == 4 && strncmp(name, "line", 4) == 0) return f.line ? f.line : "";
  if (length == 6 && strncmp(name, "weight", 6) == 0) {
    *needs_weight = true;
    return f.weight ? f.weight : "";
  }
  if (length == 4 && strncmp(name, "unit", 4) == 0) return f.unit ? f.unit : "";
  if (length == 6 && strncmp(name, "stable", 6) == 0) return !f.has_stable ? "" : f.stable ? "ST" : "US";
  return nullptr;
}

}  // namespace

bool wsoft_format_output(const char *tmpl, const WsoftOutputFields &fields, char *out, size_t size) {
  if (out == nullptr || size == 0) return false;
  out[0] = '\0';
  if (tmpl == nullptr || tmpl[0] == '\0') tmpl = WSOFT_OUTPUT_FORMAT_DEFAULT;
  size_t used = 0;
  bool fits = true;
  bool needs_weight = false;
  for (const char *p = tmpl; *p != '\0';) {
    const char *close = *p == '{' ? strchr(p + 1, '}') : nullptr;
    const char *value = close ? field_value(p + 1, (size_t)(close - p - 1), fields, &needs_weight) : nullptr;
    if (value != nullptr) {
      fits = append(out, size, &used, value) && fits;
      p = close + 1;
    } else {
      char one[2] = {*p, '\0'};
      fits = append(out, size, &used, one) && fits;
      p++;
    }
  }
  if (needs_weight && (fields.weight == nullptr || fields.weight[0] == '\0')) return false;
  return fits;
}

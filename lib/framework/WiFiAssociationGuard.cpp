#include "WiFiAssociationGuard.h"

namespace WiFiAssociationGuard {

static unsigned long s_lastStaBeginMs = 0;

void markStaBeginAttempt() {
  s_lastStaBeginMs = millis();
}

unsigned long lastStaBeginAttemptMs() {
  return s_lastStaBeginMs;
}

void clearStaBeginAttempt() {
  s_lastStaBeginMs = 0;
}

}  // namespace WiFiAssociationGuard

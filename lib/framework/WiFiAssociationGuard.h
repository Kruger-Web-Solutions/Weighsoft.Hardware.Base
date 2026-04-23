#pragma once

#include <Arduino.h>

// Tracks last STA association attempt so AP provisioning (AP_STA) can defer until
// the STA handshake has time to finish — starting soft-AP too early causes AUTH_FAIL.

namespace WiFiAssociationGuard {

void markStaBeginAttempt();
unsigned long lastStaBeginAttemptMs();
void clearStaBeginAttempt();

}  // namespace WiFiAssociationGuard

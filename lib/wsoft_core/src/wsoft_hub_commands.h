#pragma once

// Board side of the Hub switch commands (contract B1/B2 in
// docs/superpowers/plans/2026-11-07-wsoft-switches-scale-setup.md). Pure C++ for host tests
// (test/test_wsoft_hub_commands).
//
// Every 10 s the board posts {"applied_through":N,"results":[...]} to
// POST /api/v1/ingest/commands, signed like every ingest request. The Hub answers with the
// pending commands, signed under its own prefix with the request's key id and nonce.
// The board drops any answer whose key id, nonce or signature does not match.

#include <stddef.h>
#include <stdint.h>

#include "wsoft_switch.h"

static const char *const WSOFT_HUB_COMMAND_PREFIX = "scale-models-hub-command-v1";
static const size_t WSOFT_CMD_RESULTS_MAX = 20;
static const size_t WSOFT_CMD_PER_PULL = 10;
static const uint64_t WSOFT_CMD_ID_MAX = 9007199254740991ULL;  // 2^53 - 1
static const size_t WSOFT_CMD_ERROR_MAX = 32;
static const size_t WSOFT_CMD_BY_MAX = 64;
// Room for the largest request: 20 results with 16-digit ids and 32-character codes.
static const size_t WSOFT_CMD_REQUEST_MAX = 48 + WSOFT_CMD_RESULTS_MAX * 80;
static const size_t WSOFT_CMD_SIGNATURE_SIZE = 7 + 64 + 1;  // "sha256=" + hex + end byte

struct WsoftCmdResult {
  uint64_t id = 0;
  bool ok = false;
  char error[WSOFT_CMD_ERROR_MAX + 1] = "";  // [A-Z_]{1,32} when not ok
};

struct WsoftHubCommand {
  uint64_t id = 0;
  WsoftSwitchWhich which = WSOFT_SWITCH_APPROVAL;
  bool on = false;
  char by[WSOFT_CMD_BY_MAX + 1] = "";
  // Kept one character past the 80 limit, so the switch rules answer REASON_TOO_LONG.
  char reason[WSOFT_SWITCH_REASON_MAX + 2] = "";
};

// The B1 request body. An error code that is not [A-Z_]{1,32} is sent as "REFUSED".
bool wsoft_hub_commands_request(uint64_t applied_through, const WsoftCmdResult *results, size_t count, char *out,
                                size_t size);

// The B1 answer, strictly: {"commands":[...]} with 0 to 10 items, each with exactly id
// (1..2^53-1), switch ("approval" or "auto_save"), on (bool), by and reason (strings).
// False for anything else; then nothing in it may be used.
bool wsoft_hub_commands_parse(const char *body, size_t length, WsoftHubCommand out[WSOFT_CMD_PER_PULL],
                              size_t *count);

// "sha256=" + hex HMAC-SHA256(key, prefix\nkey_id\ntimestamp\nnonce\nbody).
void wsoft_hub_signature(const uint8_t key[32], const char *prefix, const char *key_id, const char *timestamp,
                         const char *nonce, const char *body, size_t length, char out[WSOFT_CMD_SIGNATURE_SIZE]);

// True only when the answer's key id and nonce are the ones the board sent, the timestamp
// is digits, and the signature under the given response prefix matches. Constant-time compare.
bool wsoft_hub_verify_answer(const uint8_t key[32], const char *prefix, const char *sent_key_id,
                             const char *sent_nonce, const char *key_id, const char *timestamp, const char *nonce,
                             const char *signature, const char *body, size_t length);

// wsoft_hub_verify_answer with the command prefix.
bool wsoft_hub_commands_verify(const uint8_t key[32], const char *sent_key_id, const char *sent_nonce,
                               const char *key_id, const char *timestamp, const char *nonce, const char *signature,
                               const char *body, size_t length);

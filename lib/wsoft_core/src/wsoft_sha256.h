#pragma once

// A small SHA-256 (FIPS 180-4) and HMAC-SHA256 (RFC 2104). Pure C++, so the same code
// runs on the board and in host tests (NIST and RFC 4231 vectors). Used for the screen
// PIN hash and to check the Hub's signed command answers.

#include <stddef.h>
#include <stdint.h>

struct WsoftSha256 {
  uint32_t state[8];
  uint64_t length;  // bytes added so far
  uint8_t block[64];
  size_t used;      // bytes waiting in block
};

void wsoft_sha256_begin(WsoftSha256 *ctx);
void wsoft_sha256_add(WsoftSha256 *ctx, const uint8_t *data, size_t length);
void wsoft_sha256_end(WsoftSha256 *ctx, uint8_t out[32]);
void wsoft_sha256(const uint8_t *data, size_t length, uint8_t out[32]);

// HMAC-SHA256 over two parts (head then body), so a caller need not join them first.
void wsoft_hmac_sha256(const uint8_t *key, size_t key_length, const uint8_t *head, size_t head_length,
                       const uint8_t *body, size_t body_length, uint8_t out[32]);

// Compares in time that does not depend on where the bytes differ.
bool wsoft_same_bytes(const uint8_t *a, const uint8_t *b, size_t length);

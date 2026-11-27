// Host tests for W-Soft packer strip, 3-4 digit PIN pad (45 s lock), hard stop at 0 bags,
// Floor Manager ISSUE/HANDIN, and Sprint 2 Hub signed fixtures (lib/wsoft_core/src/wsoft_packers.cpp).
// Run: pio test -e native -f test_wsoft_packers

#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "wsoft_packers.h"
#include "wsoft_sha256.h"
#include "wsoft_weighing.h"

void setUp() {}
void tearDown() {}

static const char *hex(const uint8_t *data, size_t n) {
  static char out[200];
  for (size_t i = 0; i < n; i++) snprintf(out + i * 2, 3, "%02x", data[i]);
  out[n * 2] = 0;
  return out;
}

// Frozen Sprint 2 golden vectors from Weighsoft.Indicator.Hub/server/test/fixtures/hub-packers-v1-fixtures.json
void test_frozen_fixture_pin_hash_and_all_three_signed_exchanges() {
  const char *secret = "wsoft-packers-fixture-secret";
  uint8_t key[32];
  wsoft_sha256((const uint8_t *)secret, strlen(secret), key);
  const char *key_id = "k-packers-fixture";
  const char *nonce = "0123456789abcdef0123456789abcdef";

  // 1. Offline verifier hash for PIN "1234" + sample_salt_hex.
  uint8_t salt[WSOFT_PACKER_PIN_SALT_SIZE];
  TEST_ASSERT_TRUE(wsoft_packer_hex_decode("00112233445566778899aabbccddeeff", salt, sizeof salt));
  uint8_t pin_hash[32];
  wsoft_packer_pin_hash(salt, "1234", pin_hash);
  TEST_ASSERT_EQUAL_STRING("8c8fb15e510c23192b9ecceb9cc163a6062c5c05faa10a68d900fe73cb0af2f3", hex(pin_hash, 32));
  TEST_ASSERT_TRUE(wsoft_packer_pin_matches(salt, pin_hash, "1234"));
  TEST_ASSERT_FALSE(wsoft_packer_pin_matches(salt, pin_hash, "1235"));

  // 2. /api/v1/ingest/packers request & signed response.
  char req[WSOFT_PACKERS_REQUEST_MAX];
  TEST_ASSERT_TRUE(wsoft_packers_request((const char *const *)nullptr, 0, req, sizeof req));
  TEST_ASSERT_EQUAL_STRING("{\"signed_in_codes\":[]}", req);

  char sig[WSOFT_CMD_SIGNATURE_SIZE];
  wsoft_hub_signature(key, "scale-models-hub-v1", key_id, "1795780800", nonce, req, strlen(req), sig);
  TEST_ASSERT_EQUAL_STRING("sha256=c5b39b1f03d7f4b730967f54c49f9fff438e253476e6531cdea98e39e96bb943", sig);

  const char *packers_resp =
      "{\"require_packer_bags\":true,\"pack_unit_label\":\"bags\",\"packers\":[{\"packer_id\":\"p-koos-1\","
      "\"code\":\"1\",\"name\":\"Koos\",\"floor_manager\":false,\"pin_salt\":\"00112233445566778899aabbccddeeff\","
      "\"pin_sha256\":\"8c8fb15e510c23192b9ecceb9cc163a6062c5c05faa10a68d900fe73cb0af2f3\","
      "\"batch_id\":\"b-koos-1\",\"issued\":100,\"remaining\":100,\"signed_in_here\":false}],\"evicted_codes\":[]}";
  TEST_ASSERT_TRUE(wsoft_hub_packers_verify(
      key, key_id, nonce, key_id, "1795780805", nonce,
      "sha256=7a53086bcfb203e43373b316af49530ee3ee5c2a09cfb657a47b8252c2ed294f", packers_resp, strlen(packers_resp)));
  // Wrong prefix must fail.
  TEST_ASSERT_FALSE(wsoft_hub_packer_auth_verify(
      key, key_id, nonce, key_id, "1795780805", nonce,
      "sha256=7a53086bcfb203e43373b316af49530ee3ee5c2a09cfb657a47b8252c2ed294f", packers_resp, strlen(packers_resp)));

  WsoftPackerRoster roster;
  TEST_ASSERT_TRUE(wsoft_packers_parse(packers_resp, strlen(packers_resp), &roster));
  TEST_ASSERT_TRUE(roster.require_packer_bags);
  TEST_ASSERT_EQUAL_STRING("bags", roster.pack_unit_label);
  TEST_ASSERT_EQUAL(1, (int)roster.count);
  TEST_ASSERT_EQUAL_STRING("p-koos-1", roster.packers[0].packer_id);
  TEST_ASSERT_EQUAL_STRING("1", roster.packers[0].code);
  TEST_ASSERT_EQUAL_STRING("Koos", roster.packers[0].name);
  TEST_ASSERT_FALSE(roster.packers[0].floor_manager);
  TEST_ASSERT_TRUE(roster.packers[0].has_verifier);
  TEST_ASSERT_TRUE(wsoft_packer_pin_matches(roster.packers[0].pin_salt, roster.packers[0].pin_sha256, "1234"));
  TEST_ASSERT_EQUAL_STRING("b-koos-1", roster.packers[0].batch_id);
  TEST_ASSERT_EQUAL(100, (int)roster.packers[0].issued);
  TEST_ASSERT_EQUAL(100, (int)roster.packers[0].remaining);
  TEST_ASSERT_FALSE(roster.packers[0].signed_in_here);
  TEST_ASSERT_EQUAL(0, (int)roster.evicted_count);

  // 3. /api/v1/ingest/packer-auth request & signed response.
  char auth_req[WSOFT_PACKER_AUTH_REQUEST_MAX];
  TEST_ASSERT_TRUE(wsoft_packer_auth_sign_in_request("1", "1234", auth_req, sizeof auth_req));
  TEST_ASSERT_EQUAL_STRING("{\"action\":\"sign_in\",\"code\":\"1\",\"pin\":\"1234\"}", auth_req);
  wsoft_hub_signature(key, "scale-models-hub-v1", key_id, "1795780800", nonce, auth_req, strlen(auth_req), sig);
  TEST_ASSERT_EQUAL_STRING("sha256=bb1a96e21b3394c812e99892fb9be507659c00854ed1ab37db3e2a42128fcecf", sig);

  const char *auth_resp =
      "{\"ok\":true,\"action\":\"sign_in\",\"packer_id\":\"p-koos-1\",\"code\":\"1\",\"name\":\"Koos\","
      "\"floor_manager\":false,\"batch_id\":\"b-koos-1\",\"issued\":100,\"remaining\":100,"
      "\"transferred_from_device_id\":null}";
  TEST_ASSERT_TRUE(wsoft_hub_packer_auth_verify(
      key, key_id, nonce, key_id, "1795780805", nonce,
      "sha256=dc936b4e01336497e9d475fbd752e21a5df5943e042d09553f580130d6c604f9", auth_resp, strlen(auth_resp)));
  WsoftPackerAuthReply auth_reply;
  TEST_ASSERT_TRUE(wsoft_packer_auth_parse(auth_resp, strlen(auth_resp), &auth_reply));
  TEST_ASSERT_TRUE(auth_reply.ok);
  TEST_ASSERT_EQUAL_STRING("sign_in", auth_reply.action);
  TEST_ASSERT_EQUAL_STRING("1", auth_reply.code);
  TEST_ASSERT_EQUAL(100, (int)auth_reply.remaining);

  // 4. /api/v1/ingest/bag-events request & signed response.
  WsoftBagEvent ev;
  strcpy(ev.event_id, "ev-fix-1");
  ev.type = WSOFT_BAG_ISSUE;
  strcpy(ev.packer_code, "1");
  strcpy(ev.actor_packer_code, "9");
  ev.quantity = 50;
  char bag_req[512];
  TEST_ASSERT_TRUE(wsoft_bag_events_request(&ev, 1, bag_req, sizeof bag_req));
  TEST_ASSERT_EQUAL_STRING(
      "{\"events\":[{\"event_id\":\"ev-fix-1\",\"event_type\":\"ISSUE\",\"packer_code\":\"1\","
      "\"actor_packer_code\":\"9\",\"quantity\":50}]}",
      bag_req);
  wsoft_hub_signature(key, "scale-models-hub-v1", key_id, "1795780800", nonce, bag_req, strlen(bag_req), sig);
  TEST_ASSERT_EQUAL_STRING("sha256=4080e97be086d862626216de8f9aa279edef72fb8c8777329d770a35cac4bd15", sig);

  const char *bag_resp =
      "{\"results\":[{\"event_id\":\"ev-fix-1\",\"status\":\"accepted\",\"batch_id\":\"b-koos-1\",\"remaining\":150}]}";
  TEST_ASSERT_TRUE(wsoft_hub_bag_events_verify(
      key, key_id, nonce, key_id, "1795780805", nonce,
      "sha256=3f85f5472005e885e0e0b43bb8e13ccaf2784f232441b4a5ba6dde2f7be7f792", bag_resp, strlen(bag_resp)));
  WsoftBagEventResult bag_results[4];
  size_t res_count = 0;
  TEST_ASSERT_TRUE(wsoft_bag_events_parse(bag_resp, strlen(bag_resp), bag_results, 4, &res_count));
  TEST_ASSERT_EQUAL(1, (int)res_count);
  TEST_ASSERT_EQUAL_STRING("ev-fix-1", bag_results[0].event_id);
  TEST_ASSERT_EQUAL(WSOFT_BAG_RES_ACCEPTED, bag_results[0].status);
  TEST_ASSERT_EQUAL_STRING("b-koos-1", bag_results[0].batch_id);
  TEST_ASSERT_TRUE(bag_results[0].has_remaining);
  TEST_ASSERT_EQUAL(150, (int)bag_results[0].remaining);
}

void test_packer_pin_format_is_3_to_4_digits_and_45s_lock() {
  TEST_ASSERT_TRUE(wsoft_packer_pin_format_ok("123"));
  TEST_ASSERT_TRUE(wsoft_packer_pin_format_ok("007"));
  TEST_ASSERT_TRUE(wsoft_packer_pin_format_ok("1234"));
  TEST_ASSERT_FALSE(wsoft_packer_pin_format_ok("12"));
  TEST_ASSERT_FALSE(wsoft_packer_pin_format_ok("12345"));
  TEST_ASSERT_FALSE(wsoft_packer_pin_format_ok("12a"));
  TEST_ASSERT_FALSE(wsoft_packer_pin_format_ok(""));
  TEST_ASSERT_FALSE(wsoft_packer_pin_format_ok(nullptr));

  uint8_t salt[WSOFT_PACKER_PIN_SALT_SIZE] = {1, 2, 3, 4};
  uint8_t hash[32];
  wsoft_packer_pin_hash(salt, "456", hash);
  TEST_ASSERT_TRUE(wsoft_packer_pin_matches(salt, hash, "456"));

  WsoftPackerPinGuard guard;
  uint32_t t = 10000;
  for (int i = 1; i <= 4; i++) {
    TEST_ASSERT_EQUAL(WSOFT_PACKER_PIN_WRONG, wsoft_packer_pin_try(&guard, true, salt, hash, "999", t));
    TEST_ASSERT_EQUAL(5 - i, guard.tries_left());
  }
  TEST_ASSERT_EQUAL(WSOFT_PACKER_PIN_LOCKED, wsoft_packer_pin_try(&guard, true, salt, hash, "999", t));
  TEST_ASSERT_EQUAL(45, (int)guard.locked_s(t));
  TEST_ASSERT_EQUAL(WSOFT_PACKER_PIN_LOCKED, wsoft_packer_pin_try(&guard, true, salt, hash, "456", t + 1000));
  TEST_ASSERT_EQUAL(44, (int)guard.locked_s(t + 1000));
  TEST_ASSERT_EQUAL(1, (int)guard.locked_s(t + 44999));
  TEST_ASSERT_EQUAL(0, (int)guard.locked_s(t + 45000));
  TEST_ASSERT_EQUAL(WSOFT_PACKER_PIN_OK, wsoft_packer_pin_try(&guard, true, salt, hash, "456", t + 45000));
  TEST_ASSERT_EQUAL(5, guard.tries_left());
}

void test_strip_8_slots_select_idle_timeout_and_eviction() {
  WsoftPackerStrip strip;
  for (int i = 1; i <= 8; i++) {
    char code[8];
    snprintf(code, sizeof code, "P%d", i);
    TEST_ASSERT_TRUE(strip.sign_in(code, (uint32_t)(i * 1000)));
  }
  TEST_ASSERT_EQUAL(8, (int)strip.count());
  TEST_ASSERT_EQUAL_STRING("P8", strip.active());
  TEST_ASSERT_FALSE(strip.sign_in("P9", 9000));  // 8 slots max

  // 1-tap select refreshes timestamp and selects active.
  TEST_ASSERT_TRUE(strip.select_code("p1", 10000));  // case-insensitive match
  TEST_ASSERT_EQUAL_STRING("P1", strip.active());

  // Idle timeout expires older slots while keeping recently active ones.
  // P2..P8 last activity was 2000..8000; at 305000 with 300000 ms timeout, P2..P5 (<=5000) expire.
  TEST_ASSERT_EQUAL(4, (int)strip.expire_idle(305000, 300000));
  TEST_ASSERT_EQUAL(4, (int)strip.count());  // P1, P6, P7, P8 remain
  TEST_ASSERT_EQUAL_STRING("P1", strip.active());

  // Eviction via evicted_codes removes from strip and clears active if evicted.
  char evicted[2][WSOFT_PACKER_CODE_MAX + 1] = {"P1", "P7"};
  TEST_ASSERT_EQUAL(2, (int)strip.apply_evicted(evicted, 2));
  TEST_ASSERT_EQUAL(2, (int)strip.count());
  TEST_ASSERT_EQUAL_STRING("", strip.active());
  TEST_ASSERT_FALSE(strip.contains("P1"));
  TEST_ASSERT_FALSE(strip.contains("P7"));
  TEST_ASSERT_TRUE(strip.contains("P6"));
  TEST_ASSERT_TRUE(strip.contains("P8"));
}

void test_hard_stop_count_rules_and_void_credit() {
  WsoftPackerRoster roster;
  TEST_ASSERT_FALSE(roster.require_packer_bags);  // OFF by default!
  WsoftPackerStrip strip;
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NONE, wsoft_packer_save_hold(roster, strip));

  // Enable packer bag enforcement with 1 packer having 2 bags.
  roster.require_packer_bags = true;
  roster.count = 1;
  strcpy(roster.packers[0].packer_id, "p-1");
  strcpy(roster.packers[0].code, "1");
  strcpy(roster.packers[0].name, "Koos");
  strcpy(roster.packers[0].batch_id, "b-1");
  roster.packers[0].has_batch = true;
  roster.packers[0].issued = 2;
  roster.packers[0].remaining = 2;

  // No active packer on strip -> NEED_PACKER blocks both manual and auto-save.
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NEED_PACKER, wsoft_packer_save_hold(roster, strip));
  WsoftWeighingSaver saver{1};
  TradeWeight issued;
  uint32_t t = 0;
  int64_t now = 1762520400;
  for (int i = 0; i < 4; i++, t += 250) {
    saver.update({true, true, true, false, 0, 0, 1, t, now}, true, false, false, &issued,
                 wsoft_packer_save_hold(roster, strip));
  }
  for (int i = 0; i < 20; i++, t += 250) {
    TEST_ASSERT_EQUAL(WSOFT_SAVE_NONE,
                      saver.update({true, true, true, false, 5000, 1, 1, t, now}, true, false, false, &issued,
                                   wsoft_packer_save_hold(roster, strip)));
  }
  t += 250;
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NEED_PACKER,
                    saver.update({true, true, true, false, 5000, 1, 1, t, now}, false, true, false, &issued,
                                 wsoft_packer_save_hold(roster, strip)));
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NEED_PACKER, saver.waiting());

  // Sign in Koos -> plain save (no Hi-Lo) counts 1 bag.
  TEST_ASSERT_TRUE(strip.sign_in("1", t));
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NONE, wsoft_packer_save_hold(roster, strip));
  t += 250;
  TEST_ASSERT_EQUAL(WSOFT_SAVE_SAVED,
                    saver.update({true, true, true, false, 5000, 1, 1, t, now}, false, true, false, &issued,
                                 wsoft_packer_save_hold(roster, strip)));
  TEST_ASSERT_TRUE(wsoft_packer_record_save(&roster, "1", nullptr));
  TEST_ASSERT_EQUAL(1, (int)roster.packers[0].remaining);

  // Second save with Hi-Lo PASS (or operator_override) counts 1 bag -> remaining hits 0.
  WsoftHiloStamp hilo_pass;
  hilo_pass.on = true;
  hilo_pass.state = WSOFT_HILO_PASS;
  TEST_ASSERT_TRUE(wsoft_packer_record_save(&roster, "1", &hilo_pass));
  TEST_ASSERT_EQUAL(0, (int)roster.packers[0].remaining);

  // At 0 remaining -> NO_BAGS blocks both manual SAVE and Auto-Save.
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NO_BAGS, wsoft_packer_save_hold(roster, strip));
  for (int i = 0; i < 4; i++, t += 250) {
    saver.update({true, true, true, false, 0, 0, 1, t, now}, true, false, false, &issued,
                 wsoft_packer_save_hold(roster, strip));
  }
  for (int i = 0; i < 20; i++, t += 250) {
    TEST_ASSERT_EQUAL(WSOFT_SAVE_NONE,
                      saver.update({true, true, true, false, 5000, 1, 1, t, now}, true, false, false, &issued,
                                   wsoft_packer_save_hold(roster, strip)));
  }
  t += 250;
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NO_BAGS,
                    saver.update({true, true, true, false, 5000, 1, 1, t, now}, false, true, false, &issued,
                                 wsoft_packer_save_hold(roster, strip)));
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NO_BAGS, saver.waiting());

  // Void returns +1 bag and clears hard stop.
  TEST_ASSERT_TRUE(wsoft_packer_record_void(&roster, "1", "b-1"));
  TEST_ASSERT_EQUAL(1, (int)roster.packers[0].remaining);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NONE, wsoft_packer_save_hold(roster, strip));

  // operator_override also counts 1 bag.
  WsoftHiloStamp hilo_ov;
  hilo_ov.on = true;
  hilo_ov.state = WSOFT_HILO_HIGH;
  hilo_ov.overridden = true;
  TEST_ASSERT_TRUE(wsoft_packer_record_save(&roster, "1", &hilo_ov));
  TEST_ASSERT_EQUAL(0, (int)roster.packers[0].remaining);
}

void test_floor_manager_issue_and_handin() {
  WsoftPackerRoster roster;
  roster.require_packer_bags = true;
  roster.count = 2;
  strcpy(roster.packers[0].packer_id, "p-1");
  strcpy(roster.packers[0].code, "1");
  strcpy(roster.packers[0].name, "Koos");
  roster.packers[0].floor_manager = false;
  strcpy(roster.packers[1].packer_id, "p-9");
  strcpy(roster.packers[1].code, "9");
  strcpy(roster.packers[1].name, "Sipho");
  roster.packers[1].floor_manager = true;

  WsoftBagEventOutbox outbox;

  // Non-floor-manager cannot issue or handin.
  TEST_ASSERT_EQUAL(WSOFT_BAG_ERR_NOT_FLOOR_MANAGER,
                    wsoft_packer_floor_issue(&roster, &outbox, "ev-1", "1", "1", 50));
  TEST_ASSERT_EQUAL(0, (int)outbox.size());

  // Floor manager issues 50 bags.
  TEST_ASSERT_EQUAL(WSOFT_BAG_OK,
                    wsoft_packer_floor_issue(&roster, &outbox, "ev-1", "9", "1", 50, "morning issue"));
  TEST_ASSERT_EQUAL(1, (int)outbox.size());
  TEST_ASSERT_EQUAL(50, (int)roster.packers[0].issued);
  TEST_ASSERT_EQUAL(50, (int)roster.packers[0].remaining);

  // Lost > 0 requires reason_code; exceeds remaining rejected.
  TEST_ASSERT_EQUAL(WSOFT_BAG_ERR_REASON_REQUIRED,
                    wsoft_packer_floor_handin(&roster, &outbox, "ev-2", "9", "1", 5, 10, nullptr));
  TEST_ASSERT_EQUAL(WSOFT_BAG_ERR_EXCEEDS_REMAINING,
                    wsoft_packer_floor_handin(&roster, &outbox, "ev-2", "9", "1", 10, 45, "RIPPED"));

  // Valid handin closes batch.
  TEST_ASSERT_EQUAL(WSOFT_BAG_OK,
                    wsoft_packer_floor_handin(&roster, &outbox, "ev-2", "9", "1", 2, 8, "RIPPED", "torn", true));
  TEST_ASSERT_EQUAL(2, (int)outbox.size());
  TEST_ASSERT_EQUAL(0, (int)roster.packers[0].remaining);
  TEST_ASSERT_FALSE(roster.packers[0].has_batch);

  char req[WSOFT_BAG_EVENTS_REQUEST_MAX];
  WsoftBagEvent batch[2] = {outbox.at(0), outbox.at(1)};
  TEST_ASSERT_TRUE(wsoft_bag_events_request(batch, 2, req, sizeof req));
  TEST_ASSERT_TRUE(strstr(req, "\"event_id\":\"ev-1\"") != nullptr);
  TEST_ASSERT_TRUE(strstr(req, "\"event_type\":\"HANDIN\"") != nullptr);
  TEST_ASSERT_TRUE(strstr(req, "\"reason_code\":\"RIPPED\"") != nullptr);
  TEST_ASSERT_TRUE(strstr(req, "\"close_batch\":true") != nullptr);

  // Ack removes events from outbox and updates roster.
  WsoftBagEventResult res[2];
  strcpy(res[0].event_id, "ev-1");
  res[0].status = WSOFT_BAG_RES_ACCEPTED;
  res[0].has_batch_id = true;
  strcpy(res[0].batch_id, "b-new");
  res[0].has_remaining = true;
  res[0].remaining = 50;
  strcpy(res[1].event_id, "ev-2");
  res[1].status = WSOFT_BAG_RES_DUPLICATE;
  res[1].has_remaining = true;
  res[1].remaining = 0;
  TEST_ASSERT_EQUAL(2, (int)outbox.apply_results(res, 2, &roster));
  TEST_ASSERT_EQUAL(0, (int)outbox.size());
  TEST_ASSERT_EQUAL_STRING("b-new", roster.packers[0].batch_id);
  TEST_ASSERT_EQUAL(0, (int)roster.packers[0].remaining);
}

void test_weighing_json_unchanged_when_packer_fields_null_or_empty() {
  TradeWeight w;
  w.valid = true;
  w.seq = 7;
  w.value = 3120;
  w.decimals = 0;
  w.unit = 1;
  w.at = 1762520400;

  WsoftWeighingMeta meta_null = {"6CC8404560F4", "rs232", "wsoft-cyd28", false};
  char out_null[512];
  TEST_ASSERT_TRUE(wsoft_weighing_json(w, meta_null, out_null, sizeof out_null));
  TEST_ASSERT_EQUAL_STRING(
      "{\"weighing_id\":\"6CC8404560F4-1762520400-7\",\"committed_at\":\"2025-11-07T13:00:00Z\","
      "\"gross_kg\":3120,\"tare_kg\":0,\"net_kg\":3120,\"basis\":\"GS\","
      "\"payload\":{\"seq\":7,\"reading\":\"3120\",\"unit\":\"kg\",\"source\":\"rs232\","
      "\"station_model\":\"wsoft-cyd28\",\"for_trade\":false}}",
      out_null);

  WsoftWeighingMeta meta_empty = meta_null;
  meta_empty.packer_id = "";
  meta_empty.packer_name = "";
  meta_empty.bag_batch_id = "";
  char out_empty[512];
  TEST_ASSERT_TRUE(wsoft_weighing_json(w, meta_empty, out_empty, sizeof out_empty));
  TEST_ASSERT_EQUAL_STRING(out_null, out_empty);

  WsoftWeighingMeta meta_set = meta_null;
  meta_set.packer_id = "1";
  meta_set.packer_name = "Koos";
  meta_set.bag_batch_id = "b-koos-1";
  char out_set[640];
  TEST_ASSERT_TRUE(wsoft_weighing_json(w, meta_set, out_set, sizeof out_set));
  TEST_ASSERT_TRUE(strstr(out_set, ",\"packer_id\":\"1\",\"packer_name\":\"Koos\",\"bag_batch_id\":\"b-koos-1\"}") !=
                   nullptr);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_frozen_fixture_pin_hash_and_all_three_signed_exchanges);
  RUN_TEST(test_packer_pin_format_is_3_to_4_digits_and_45s_lock);
  RUN_TEST(test_strip_8_slots_select_idle_timeout_and_eviction);
  RUN_TEST(test_hard_stop_count_rules_and_void_credit);
  RUN_TEST(test_floor_manager_issue_and_handin);
  RUN_TEST(test_weighing_json_unchanged_when_packer_fields_null_or_empty);
  return UNITY_END();
}

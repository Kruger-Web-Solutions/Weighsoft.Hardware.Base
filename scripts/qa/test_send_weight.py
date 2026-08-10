#!/usr/bin/env python3
"""Unit tests for scripts/send-weight.py. No board or network required.

Run:  python scripts/qa/test_send_weight.py
      python -m unittest discover -s scripts/qa -p "test_*.py"

Both bugs these cover were found by hand on real hardware, not in review:
  - the last reading was silently dropped by the rate limiter
  - "did not answer" and "answered but is not our board" had inverted results
Anything that can regress those two gets a test here.
"""

from __future__ import annotations

import importlib.util
import io
import pathlib
import sys
import unittest
from unittest import mock

_HERE = pathlib.Path(__file__).resolve()
_SENDER = _HERE.parent.parent / "send-weight.py"

_spec = importlib.util.spec_from_file_location("send_weight", _SENDER)
assert _spec and _spec.loader, f"cannot load {_SENDER}"
sw = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sw)


class ParseWeight(unittest.TestCase):
    """Scale protocols vary wildly. Last number on the line wins."""

    def test_common_scale_formats(self):
        cases = {
            "ST,GS,   12.34kg": 12.34,
            "+00014.75 kg": 14.75,
            "ST,US,   16.25kg": 16.25,
            "12.34": 12.34,
            "  7 ": 7.0,
            "-3.5 kg": -3.5,
            "0.00": 0.0,
        }
        for line, expected in cases.items():
            with self.subTest(line=line):
                self.assertEqual(sw.parse_weight(line), expected)

    def test_last_number_wins_over_leading_fields(self):
        # Station id / sequence numbers come first on many protocols.
        self.assertEqual(sw.parse_weight("02,GS,1,  45.60"), 45.60)

    def test_lines_without_numbers_are_rejected(self):
        for line in ("garbage no numbers", "", "   ", "ERROR", "kg"):
            with self.subTest(line=line):
                self.assertIsNone(sw.parse_weight(line))

    def test_negative_and_zero_are_real_readings_not_falsy_misses(self):
        # 0.0 is falsy in Python; a truthiness check here would drop a real
        # zero reading, which is exactly what a tared scale reports.
        self.assertIsNotNone(sw.parse_weight("0"))
        self.assertIsNotNone(sw.parse_weight("-1.5"))


class Identify(unittest.TestCase):
    """Reachable is not the same as correct. A stranger can hold the address."""

    def test_right_board_is_accepted(self):
        with mock.patch.object(sw, "_get_json", return_value={"id": "97cbc0", "ip": "10.0.0.5"}):
            info, reason = sw._identify("http://board", "97cbc0")
        self.assertIsNotNone(info)
        self.assertIsNone(reason)

    def test_wrong_board_is_wrong_device_not_not_found(self):
        with mock.patch.object(sw, "_get_json", return_value={"id": "deadbe"}):
            info, reason = sw._identify("http://board", "97cbc0")
        self.assertIsNone(info)
        self.assertEqual(reason, sw.WRONG_DEVICE)

    def test_silent_host_is_not_found(self):
        with mock.patch.object(sw, "_get_json", side_effect=OSError("timeout")):
            info, reason = sw._identify("http://board", "97cbc0")
        self.assertIsNone(info)
        self.assertEqual(reason, sw.NOT_FOUND)

    def test_empty_expect_id_skips_the_check(self):
        with mock.patch.object(sw, "_get_json", return_value={"id": "anything"}):
            info, reason = sw._identify("http://board", "")
        self.assertIsNotNone(info)
        self.assertIsNone(reason)


class FindBoard(unittest.TestCase):
    def test_wrong_device_does_not_fall_through_to_udp_hunting(self):
        """A wrong device is an answer. Hunting on would hide it."""
        with mock.patch.object(sw, "_get_json", return_value={"id": "deadbe"}), mock.patch.object(
            sw, "discover_via_udp"
        ) as udp:
            base, info, reason = sw.find_board("", 1.0, "97cbc0")
        self.assertIsNone(info)
        self.assertEqual(reason, sw.WRONG_DEVICE)
        udp.assert_not_called()

    def test_explicit_host_is_final_and_does_not_hunt(self):
        with mock.patch.object(sw, "_get_json", side_effect=OSError("timeout")), mock.patch.object(
            sw, "discover_via_udp"
        ) as udp:
            base, info, reason = sw.find_board("10.0.0.9", 1.0, "97cbc0")
        self.assertIsNone(info)
        self.assertEqual(reason, sw.NOT_FOUND)
        udp.assert_not_called()

    def test_falls_back_to_udp_when_the_name_is_silent(self):
        answers = [OSError("no mdns"), {"id": "97cbc0", "ip": "10.0.0.7"}]

        def fake_get(url, timeout=8.0):
            nxt = answers.pop(0)
            if isinstance(nxt, Exception):
                raise nxt
            return nxt

        with mock.patch.object(sw, "_get_json", side_effect=fake_get), mock.patch.object(
            sw, "discover_via_udp", return_value="10.0.0.7"
        ):
            base, info, reason = sw.find_board("", 1.0, "97cbc0")
        self.assertEqual(base, "http://10.0.0.7")
        self.assertEqual(info["id"], "97cbc0")
        self.assertIsNone(reason)


class StdinCoalescing(unittest.TestCase):
    """The settled reading must always arrive. This is the bug that shipped."""

    def _run_stdin(self, lines, min_interval="0.2"):
        sent: list[float] = []

        def fake_send(base, weight, last_line, count):
            sent.append(weight)
            return 200, ""

        argv = ["send-weight.py", "--stdin", "--min-interval", min_interval, "--host", "10.0.0.7"]
        with mock.patch.object(sw, "send_weight", side_effect=fake_send), mock.patch.object(
            sw, "find_board", return_value=("http://10.0.0.7", {"id": "97cbc0", "ip": "10.0.0.7"}, None)
        ), mock.patch.object(sys, "argv", argv), mock.patch.object(
            sys, "stdin", io.StringIO("\n".join(lines) + "\n")
        ):
            rc = sw.main()
        return rc, sent

    def test_last_reading_always_lands_even_when_rate_limited(self):
        rc, sent = self._run_stdin(
            ["ST,GS,   12.34kg", "ST,GS,   13.50kg", "+00014.75 kg", "ST,US,   16.25kg"],
            min_interval="9999",  # nothing may pass on the interval; only the flush
        )
        self.assertEqual(rc, 0)
        self.assertTrue(sent, "nothing was sent - the settled reading was dropped")
        self.assertEqual(sent[-1], 16.25, "the last reading must be the one the board ends on")

    def test_unparsable_lines_do_not_supersede_a_good_reading(self):
        rc, sent = self._run_stdin(["ST,GS,   5.00kg", "ERROR", "garbage"], min_interval="9999")
        self.assertEqual(rc, 0)
        self.assertEqual(sent[-1], 5.00)

    def test_bom_from_a_piped_stream_is_stripped(self):
        rc, sent = self._run_stdin(["﻿ST,GS,   8.80kg"], min_interval="9999")
        self.assertEqual(rc, 0)
        self.assertEqual(sent[-1], 8.80)

    def test_zero_reading_is_sent_not_swallowed(self):
        rc, sent = self._run_stdin(["ST,GS,    0.00kg"], min_interval="9999")
        self.assertEqual(rc, 0)
        self.assertEqual(sent[-1], 0.0)

    def test_failed_send_reports_nonzero(self):
        with mock.patch.object(sw, "send_weight", return_value=(500, "boom")), mock.patch.object(
            sw, "find_board", return_value=("http://10.0.0.7", {"id": "97cbc0"}, None)
        ), mock.patch.object(sys, "argv", ["send-weight.py", "--stdin", "--host", "10.0.0.7"]), mock.patch.object(
            sys, "stdin", io.StringIO("ST,GS,   1.00kg\n")
        ):
            rc = sw.main()
        self.assertEqual(rc, 4)


class ExitCodes(unittest.TestCase):
    """Documented codes must match behaviour - they were inverted once."""

    def _run_single(self, find_result):
        with mock.patch.object(sw, "find_board", return_value=find_result), mock.patch.object(
            sys, "argv", ["send-weight.py", "--weight", "1.0"]
        ):
            return sw.main()

    def test_not_found_is_2(self):
        self.assertEqual(self._run_single((None, None, sw.NOT_FOUND)), 2)

    def test_wrong_device_is_3(self):
        self.assertEqual(self._run_single((None, None, sw.WRONG_DEVICE)), 3)

    def test_send_failure_is_4(self):
        with mock.patch.object(sw, "send_weight", return_value=(500, "boom")):
            rc = self._run_single(("http://b", {"id": "97cbc0"}, None))
        self.assertEqual(rc, 4)

    def test_success_is_0(self):
        with mock.patch.object(sw, "send_weight", return_value=(200, "")):
            rc = self._run_single(("http://b", {"id": "97cbc0"}, None))
        self.assertEqual(rc, 0)

    def test_dry_run_contacts_nothing(self):
        with mock.patch.object(sw, "find_board") as find, mock.patch.object(sw, "send_weight") as send, mock.patch.object(
            sys, "argv", ["send-weight.py", "--weight", "1.0", "--dry-run"]
        ):
            rc = sw.main()
        self.assertEqual(rc, 0)
        find.assert_not_called()
        send.assert_not_called()


if __name__ == "__main__":
    unittest.main(verbosity=2)

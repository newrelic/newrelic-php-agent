#!/usr/bin/env python3

#
# Copyright 2026 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

"""Tests of release-data/validate.py: run `python3 -m unittest discover -s release-data/tests`.

Only test_committed_files_are_valid reads the real data files. Every other test builds its files from the fixtures
below, so a key rotation or a new release in the real files cannot turn a test red."""

import contextlib
import copy
import io
import json
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.dirname(HERE)
sys.path.insert(0, DATA)

import validate  # noqa: E402

HASH = "a" * 64


def release(version, platforms=("linux", "linux-musl"), date="2026-11-07"):
    libc = {"linux": "glibc", "linux-musl": "musl"}
    files = []
    for platform in platforms:
        item = {"filename": "newrelic-php5-%s-%s.tar.gz" % (version, platform), "platform": platform, "sha256": HASH}
        if platform in libc:
            item["libc"] = libc[platform]
        files.append(item)
    return {"date": date, "version": version, "files": files}


def image(version):
    return {"version": version, "digest": "sha256:" + "b" * 64, "pushed": "2026-11-07T12:00:00Z",
            "platforms": {"linux/amd64": "sha256:" + "c" * 64}}


def key(purpose, fingerprint, created="2026-01-01"):
    return {"purpose": purpose, "name": "%s key" % purpose, "fingerprint": fingerprint, "created": created, "expires": "never",
            "public_key_url": "https://download.newrelic.com/NEWRELIC_%s_%s.public" % (purpose.upper(), fingerprint[-8:])}


# fixed keys: the real ones are not used, so a rotation of the real keys changes no test
APT = "1111111111111111111111111111111111112222"
RPM = "3333333333333333333333333333333333334444"
TARBALL = "5555555555555555555555555555555555556666"
LEGACY = "B60A3EC9BC013B9C23790EC8B31B29E5548C16BF"


def dump(data):
    return json.dumps(data, indent=2, sort_keys=True) + "\n"


class ValidateTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir)
        shutil.copytree(os.path.join(DATA, "schema"), os.path.join(self.dir, "schema"))
        self.keys = {"first_signed_version": "12.13.0", "keys": [key("apt", APT), key("rpm", RPM), key("tarball", TARBALL)]}
        self.hashes = {"releases": [release("12.13.0.320"), release("12.10.0.1"), release("12.9.0.1")]}
        self.images = {"first_image_version": "9.2.0", "images": [image("12.12.0"), image("12.9.0")]}
        self.write()

    def write(self, text=None):
        """Write the three files from the attributes, or replace one of them by text: write((name, text))."""
        files = {"tarball-hashes.json": dump(self.hashes), "daemon-image-digests.json": dump(self.images),
                 "signing-key-fingerprints.json": dump(self.keys)}
        if text:
            files[text[0]] = text[1]
        for name, content in files.items():
            with open(os.path.join(self.dir, name), "w", newline="") as handle:
                handle.write(content)

    def problems(self):
        return validate.check_tree(self.dir)

    def assertProblem(self, *fragments):
        """One problem line has all the fragments."""
        found = self.problems()
        self.assertTrue(any(all(f in p for f in fragments) for p in found), "%r not in %r" % (fragments, found))

    # the real files
    def test_committed_files_are_valid(self):
        self.assertEqual(validate.check_tree(DATA), [])

    def test_sample_is_valid(self):
        self.assertEqual(self.problems(), [])

    # tarball-hashes.json
    def test_hash_of_63_characters(self):
        self.hashes["releases"][0]["files"][0]["sha256"] = "a" * 63
        self.write()
        self.assertProblem("tarball-hashes.json", "files/0/sha256", "does not match")

    def test_hash_with_trailing_newline(self):
        self.hashes["releases"][0]["files"][0]["sha256"] = "a" * 64 + "\n"
        self.write()
        self.assertProblem("files/0/sha256", "does not match")

    def test_entries_out_of_order(self):
        self.hashes["releases"].reverse()
        self.write()
        self.assertProblem("not newest first")

    def test_version_numbers_are_compared_as_numbers(self):
        # 12.9 is older than 12.10: the order of the sample is the correct one
        self.assertEqual(self.problems(), [])
        self.hashes["releases"] = [release("12.9.0.1"), release("12.10.0.1")]
        self.write()
        self.assertProblem("not newest first")

    def test_duplicate_version(self):
        self.hashes["releases"].append(copy.deepcopy(self.hashes["releases"][-1]))
        self.write()
        self.assertProblem("listed twice")

    def test_file_name_of_another_version(self):
        self.hashes["releases"][0]["files"][0]["filename"] = "newrelic-php5-12.13.0.321-linux.tar.gz"
        self.write()
        self.assertProblem("should be named 'newrelic-php5-12.13.0.320-linux.tar.gz'")

    def test_platform_twice_in_a_release(self):
        files = self.hashes["releases"][0]["files"]
        files.append(copy.deepcopy(files[0]))
        self.write()
        self.assertProblem("the platform 'linux' is listed twice in '12.13.0.320'")

    def test_libc_must_match_the_platform(self):
        cases = [
            ("musl on the glibc platform", lambda f: f.__setitem__("libc", "musl"), "has libc 'musl', expected 'glibc'"),
            ("no libc on a linux platform", lambda f: f.pop("libc"), "has libc None, expected 'glibc'"),
        ]
        for name, change, message in cases:
            with self.subTest(name):
                self.hashes = {"releases": [release("12.13.0.320")]}
                change(self.hashes["releases"][0]["files"][0])
                self.write()
                self.assertProblem(message)
        with self.subTest("libc on osx"):
            self.hashes = {"releases": [release("12.13.0.320", platforms=("osx",))]}
            self.hashes["releases"][0]["files"][0]["libc"] = "glibc"
            self.write()
            self.assertProblem("has libc 'glibc', expected None")

    def test_osx_freebsd_solaris_need_no_libc(self):
        self.hashes = {"releases": [release("12.13.0.320", platforms=("linux", "linux-musl", "osx", "freebsd", "solaris"))]}
        self.write()
        self.assertEqual(self.problems(), [])

    # the written format
    def test_not_the_written_format(self):
        self.write(("tarball-hashes.json", json.dumps(self.hashes, indent=4, sort_keys=True) + "\n"))
        self.assertProblem("not in the format")

    def test_keys_not_sorted(self):
        self.write(("tarball-hashes.json", json.dumps(self.hashes, indent=2) + "\n"))
        self.assertProblem("not in the format")

    def test_no_final_newline(self):
        self.write(("tarball-hashes.json", dump(self.hashes).rstrip("\n")))
        self.assertProblem("not in the format")

    def test_windows_line_endings(self):
        self.write(("tarball-hashes.json", dump(self.hashes).replace("\n", "\r\n")))
        self.assertProblem("not in the format")

    # dates and times
    def test_date_that_does_not_exist(self):
        self.hashes["releases"][0]["date"] = "2026-02-30"
        self.write()
        self.assertProblem("is not a 'date'")

    def test_expires_never_and_a_date_are_valid(self):
        self.keys["keys"][0]["expires"] = "2030-01-31"
        self.write()
        self.assertEqual(self.problems(), [])

    def test_expires_that_does_not_exist(self):
        self.keys["keys"][0]["expires"] = "2030-13-01"
        self.write()
        self.assertProblem("is not a 'date'")

    def test_time_that_does_not_exist(self):
        self.images["images"][0]["pushed"] = "2026-11-07T25:00:00Z"
        self.write()
        self.assertProblem("is not a 'date-time'")

    # signing-key-fingerprints.json
    def test_fingerprint_in_lower_case_with_spaces(self):
        self.keys["keys"][0]["fingerprint"] = "1111 1111 1111 1111 1111 1111 1111 1111 1111 2222".lower()
        self.write()
        self.assertProblem("keys/0/fingerprint", "does not match '^[0-9A-F]{40}$'")

    def test_key_url_on_another_host(self):
        self.keys["keys"][0]["public_key_url"] = "https://example.com/NEWRELIC_APT_11112222.public"
        self.write()
        self.assertProblem("keys/0/public_key_url", "does not match")

    def test_key_url_with_http(self):
        self.keys["keys"][0]["public_key_url"] = "http://download.newrelic.com/NEWRELIC_APT_11112222.public"
        self.write()
        self.assertProblem("keys/0/public_key_url", "does not match")

    def test_key_url_with_a_line_break_or_a_tab(self):
        for bad in ("https://download.newrelic.com/NEWRELIC_APT_1111\n2222.public",
                    "https://download.newrelic.com/NEWRELIC_APT_1111\t2222.public",
                    "https://download.newrelic.com/NEWRELIC_APT_11112222.public\n",
                    "https://down\nload.newrelic.com/NEWRELIC_APT_11112222.public",
                    "https://download.newrelic.com/NEWRELIC_APT_11112222.public?x=1"):
            with self.subTest(repr(bad)):
                self.keys["keys"][0]["public_key_url"] = bad
                self.write()
                self.assertProblem("keys/0/public_key_url", "does not match")

    def test_key_url_named_for_another_key(self):
        self.keys["keys"][0]["public_key_url"] = "https://download.newrelic.com/NEWRELIC_APT_DEADBEEF.public"
        self.write()
        self.assertProblem("should end in 'NEWRELIC_APT_11112222.public'")

    def test_key_url_with_another_purpose(self):
        self.keys["keys"][0]["public_key_url"] = "https://download.newrelic.com/NEWRELIC_RPM_11112222.public"
        self.write()
        self.assertProblem("should end in 'NEWRELIC_APT_11112222.public'")

    def test_legacy_key_is_listed_under_its_short_key_id(self):
        legacy = key("legacy", LEGACY, created="2010-06-29")
        legacy["public_key_url"] = "https://download.newrelic.com/548C16BF.gpg"
        legacy["status"] = "retired"
        self.keys["keys"].append(legacy)
        self.write()
        self.assertEqual(self.problems(), [])

    def test_legacy_key_with_a_public_file_name(self):
        legacy = key("legacy", LEGACY, created="2010-06-29")
        legacy["status"] = "retired"
        self.keys["keys"].append(legacy)
        self.write()
        self.assertProblem("should end in '548C16BF.gpg'")

    def test_legacy_key_must_be_retired(self):
        for status in (None, "active"):
            with self.subTest(status):
                legacy = key("legacy", LEGACY, created="2010-06-29")
                legacy["public_key_url"] = "https://download.newrelic.com/548C16BF.gpg"
                if status:
                    legacy["status"] = status
                self.keys["keys"] = self.keys["keys"][:3] + [legacy]
                self.write()
                self.assertProblem("the legacy key", "must have the status retired")

    def test_status_must_be_active_or_retired(self):
        self.keys["keys"][0]["status"] = "old"
        self.write()
        self.assertProblem("keys/0/status")

    def test_rotation_the_old_key_retired_and_the_new_one_active(self):
        old = self.keys["keys"][2]
        old["status"] = "retired"
        new = key("tarball", "7777777777777777777777777777777777778888", created="2027-01-01")
        new["status"] = "active"
        self.keys["keys"].append(new)
        self.write()
        self.assertEqual(self.problems(), [])

    def test_two_active_tarball_keys(self):
        self.keys["keys"].append(key("tarball", "7777777777777777777777777777777777778888", created="2027-01-01"))
        self.write()
        self.assertTrue(self.problems())

    def test_two_active_apt_keys(self):
        self.keys["keys"].append(key("apt", "7777777777777777777777777777777777778888", created="2027-01-01"))
        self.write()
        self.assertProblem("exactly one active key with the purpose apt, found 2")

    def test_no_active_rpm_key(self):
        self.keys["keys"][1]["status"] = "retired"
        self.write()
        self.assertProblem("exactly one active key with the purpose rpm, found 0")

    def test_no_active_tarball_key(self):
        self.keys["keys"][2]["status"] = "retired"
        self.write()
        self.assertTrue(self.problems())

    def test_gpg_file_name_is_only_for_the_legacy_key(self):
        self.keys["keys"][0]["public_key_url"] = "https://download.newrelic.com/11112222.gpg"
        self.write()
        self.assertProblem("should end in 'NEWRELIC_APT_11112222.public'")

    def test_key_url_in_a_directory_is_valid(self):
        self.keys["keys"][2]["public_key_url"] = "https://download.newrelic.com/php_agent/NEWRELIC_TARBALL_55556666.public"
        self.write()
        self.assertEqual(self.problems(), [])

    def test_fingerprint_twice(self):
        self.keys["keys"].append(copy.deepcopy(self.keys["keys"][0]))
        self.write()
        self.assertProblem("listed twice")

    # daemon-image-digests.json
    def test_image_below_first_image_version(self):
        self.images["images"].append(image("9.1.0"))
        self.write()
        self.assertProblem("the tag '9.1.0' is below first_image_version '9.2.0'")

    def test_images_out_of_order(self):
        self.images["images"].reverse()
        self.write()
        self.assertProblem("not newest first")

    def test_digest_with_upper_case(self):
        self.images["images"][0]["digest"] = "sha256:" + "B" * 64
        self.write()
        self.assertProblem("images/0/digest", "does not match")

    # unreadable input
    def test_file_that_is_not_json(self):
        self.write(("tarball-hashes.json", "{"))
        self.assertProblem("cannot be read as JSON")

    def test_missing_file(self):
        os.remove(os.path.join(self.dir, "tarball-hashes.json"))
        self.assertProblem("cannot be read as JSON")

    def test_every_problem_is_reported(self):
        self.hashes["releases"][0]["files"][0]["sha256"] = "a" * 63
        self.images["images"].reverse()
        self.write()
        self.assertGreaterEqual(len(self.problems()), 2)

    def test_a_schema_that_is_not_valid(self):
        path = os.path.join(self.dir, "schema", "tarball-hashes.schema.json")
        with open(path, "w") as handle:
            json.dump({"type": 5}, handle)
        self.assertProblem("tarball-hashes.schema.json", "not a usable schema")

    def test_a_schema_that_is_missing_or_not_json(self):
        os.remove(os.path.join(self.dir, "schema", "daemon-image-digests.schema.json"))
        with open(os.path.join(self.dir, "schema", "tarball-hashes.schema.json"), "w") as handle:
            handle.write("{")
        self.assertProblem("daemon-image-digests.schema.json", "not a usable schema")
        self.assertProblem("tarball-hashes.schema.json", "not a usable schema")

    # main: the exit status and the lines the runner reads
    def run_main(self, *args):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
            status = validate.main(["validate.py"] + list(args))
        return status, out.getvalue()

    def test_main_exit_status_and_output_of_a_valid_tree(self):
        status, out = self.run_main(self.dir)
        self.assertEqual((status, out), (0, "release-data is valid\n"))

    def test_main_exit_status_and_error_lines_of_an_invalid_tree(self):
        self.hashes["releases"][0]["files"][0]["sha256"] = "a" * 63
        self.images["images"].reverse()
        self.write()
        status, out = self.run_main(self.dir)
        self.assertEqual(status, 1)
        lines = out.splitlines()
        self.assertGreaterEqual(len(lines), 2)
        self.assertTrue(all(line.startswith("::error::") for line in lines), lines)

    def test_main_prints_a_value_with_a_line_break_on_one_line(self):
        original = validate.check_tree
        validate.check_tree = lambda directory: ["file: 'a\nb::warning::injected' is not valid\r::notice::x"]
        self.addCleanup(setattr, validate, "check_tree", original)
        status, out = self.run_main(self.dir)
        self.assertEqual(status, 1)
        self.assertEqual(len(out.splitlines()), 1)
        self.assertTrue(out.startswith("::error::"))

    def test_main_with_too_many_arguments(self):
        status, _ = self.run_main(self.dir, "extra")
        self.assertEqual(status, 2)

    def test_the_messages_show_values_with_repr(self):
        # a file name with a line break cannot pass the schema, but the messages must still not carry one if it did
        problems = validate.check_tarball_hashes({"releases": [{"version": "1.2.3.4", "files": [
            {"filename": "x\nbad", "platform": "linux", "sha256": HASH, "libc": "glibc"}]}]}, "file")
        self.assertTrue(problems)
        self.assertTrue(all("\n" not in p for p in problems), problems)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3

#
# Copyright 2026 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

"""Tests of release-data/validate.py: run `python3 -m unittest discover -s release-data/tests`."""

import copy
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
    return {
        "date": date,
        "version": version,
        "files": [{"filename": "newrelic-php5-%s-%s.tar.gz" % (version, p), "platform": p, "sha256": HASH} for p in platforms],
    }


def image(version):
    return {"version": version, "digest": "sha256:" + "b" * 64, "pushed": "2026-11-07T12:00:00Z",
            "platforms": {"linux/amd64": "sha256:" + "c" * 64}}


def dump(data):
    return json.dumps(data, indent=2, sort_keys=True) + "\n"


class ValidateTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir)
        shutil.copytree(os.path.join(DATA, "schema"), os.path.join(self.dir, "schema"))
        with open(os.path.join(DATA, "signing-key-fingerprints.json")) as handle:
            self.keys = json.load(handle)
        self.hashes = {"releases": [release("12.13.0.320"), release("12.9.0.1"), release("12.10.0.1")][:1]
                       + [release("12.10.0.1"), release("12.9.0.1")]}
        self.images = {"first_image_version": "9.2.0", "images": [image("12.12.0"), image("12.9.0")]}
        self.write()

    def write(self, text=None):
        """Write the three files from the attributes, or replace one of them by text: write((name, text))."""
        files = {"tarball-hashes.json": dump(self.hashes), "daemon-image-digests.json": dump(self.images),
                 "signing-key-fingerprints.json": dump(self.keys)}
        if text:
            files[text[0]] = text[1]
        for name, content in files.items():
            with open(os.path.join(self.dir, name), "w") as handle:
                handle.write(content)

    def problems(self):
        return validate.check_tree(self.dir)

    def assertProblem(self, fragment):
        found = self.problems()
        self.assertTrue(any(fragment in p for p in found), "%r not in %r" % (fragment, found))

    def test_committed_files_are_valid(self):
        self.assertEqual(validate.check_tree(DATA), [])

    def test_sample_is_valid(self):
        self.assertEqual(self.problems(), [])

    def test_hash_of_63_characters(self):
        self.hashes["releases"][0]["files"][0]["sha256"] = "a" * 63
        self.write()
        self.assertProblem("tarball-hashes.json")

    def test_hash_with_trailing_newline(self):
        self.hashes["releases"][0]["files"][0]["sha256"] = "a" * 64 + "\n"
        self.write()
        self.assertProblem("does not match")

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
        self.assertProblem("should be named newrelic-php5-12.13.0.320-linux.tar.gz")

    def test_not_the_written_format(self):
        self.write(("tarball-hashes.json", json.dumps(self.hashes, indent=4, sort_keys=True) + "\n"))
        self.assertProblem("not in the format")

    def test_keys_not_sorted(self):
        self.write(("tarball-hashes.json", json.dumps(self.hashes, indent=2) + "\n"))
        self.assertProblem("not in the format")

    def test_no_final_newline(self):
        self.write(("tarball-hashes.json", dump(self.hashes).rstrip("\n")))
        self.assertProblem("not in the format")

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

    def test_fingerprint_in_lower_case_with_spaces(self):
        self.keys["keys"][0]["fingerprint"] = "2ea7 db77 38a2 2d92 945e 8ec7 32fa 4c17 2dad 550e"
        self.write()
        self.assertProblem("signing-key-fingerprints.json")

    def test_key_url_on_another_host(self):
        self.keys["keys"][0]["public_key_url"] = "https://example.com/NEWRELIC_APT_2DAD550E.public"
        self.write()
        self.assertProblem("not an https URL on download.newrelic.com")

    def test_key_url_with_http(self):
        self.keys["keys"][0]["public_key_url"] = "http://download.newrelic.com/NEWRELIC_APT_2DAD550E.public"
        self.write()
        self.assertProblem("does not match '^https://'")

    def test_key_url_named_for_another_key(self):
        self.keys["keys"][0]["public_key_url"] = "https://download.newrelic.com/NEWRELIC_APT_DEADBEEF.public"
        self.write()
        self.assertProblem("should end in NEWRELIC_APT_2DAD550E.public")

    def test_key_url_with_another_purpose(self):
        self.keys["keys"][0]["public_key_url"] = "https://download.newrelic.com/NEWRELIC_RPM_2DAD550E.public"
        self.write()
        self.assertProblem("should end in NEWRELIC_APT_2DAD550E.public")

    def test_fingerprint_twice(self):
        self.keys["keys"].append(copy.deepcopy(self.keys["keys"][0]))
        self.write()
        self.assertProblem("listed twice")

    def test_image_below_first_image_version(self):
        self.images["images"].append(image("9.1.0"))
        self.write()
        self.assertProblem("below first_image_version")

    def test_images_out_of_order(self):
        self.images["images"].reverse()
        self.write()
        self.assertProblem("not newest first")

    def test_digest_with_upper_case(self):
        self.images["images"][0]["digest"] = "sha256:" + "B" * 64
        self.write()
        self.assertProblem("daemon-image-digests.json")

    def test_time_that_does_not_exist(self):
        self.images["images"][0]["pushed"] = "2026-11-07T25:00:00Z"
        self.write()
        self.assertProblem("is not a 'date-time'")

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


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3

#
# Copyright 2026 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

"""Check the files in release-data/ against their schemas and against the format that the scripts of the
builder and daemon repositories write.

usage: validate.py [<release-data-dir>]      (default: the directory of this script)

Every problem is printed (one line each, as a GitHub Actions ::error:: line, with line breaks inside a value written as
\n) and the exit status is 1 if there was one. What it checks:

  * each schema is a valid JSON Schema, and each data file matches its schema with the formats date and
    date-time asserted and with "$" in a pattern read as ECMA-262 reads it (it does not match before a
    trailing newline; Python's re.search does);
  * the file is in the format that insert-release-data.py writes: sorted keys, two-space indent, one final
    newline;
  * tarball-hashes.json: releases newest first by version number (12.10 is newer than 12.9), no version twice,
    every file named newrelic-php5-<version>-<platform>.tar.gz, no platform twice in a release, and `libc` set
    exactly for the linux platforms (glibc for linux, musl for linux-musl);
  * daemon-image-digests.json: images newest first, no tag twice, no tag below first_image_version;
  * signing-key-fingerprints.json: no fingerprint twice, and each public_key_url is https on download.newrelic.com
    with the file name NEWRELIC_<PURPOSE>_<last 8 characters of the fingerprint>.public. The fingerprint is not
    compared with the key at that address (the key is not fetched).

Needs the Python package jsonschema.
"""

import datetime
import json
import os
import re
import sys
from urllib.parse import urlsplit

import jsonschema
from jsonschema import exceptions, validators

DATE_TIME = re.compile(r"^(\d{4}-\d{2}-\d{2})T(\d{2}:\d{2}:\d{2})(\.\d+)?(Z|[+-](\d{2}):(\d{2}))$", re.ASCII)

FILES = {
    "tarball-hashes.json": "tarball-hashes.schema.json",
    "daemon-image-digests.json": "daemon-image-digests.schema.json",
    "signing-key-fingerprints.json": "signing-key-fingerprints.schema.json",
}


def pattern(validator, regex, instance, schema):
    """The keyword pattern. jsonschema searches with re.search, where "$" also matches before a trailing
    newline; in ECMA-262, which JSON Schema uses, it does not."""
    if validator.is_type(instance, "string") and (instance.endswith("\n") or not re.search(regex, instance)):
        yield exceptions.ValidationError("%r does not match %r" % (instance, regex))


Validator = validators.extend(jsonschema.Draft202012Validator, {"pattern": pattern})
FORMATS = jsonschema.FormatChecker()


@FORMATS.checks("date-time")
def is_date_time(value):
    """A time as RFC 3339 writes it, with a real date and time (jsonschema needs another package for this)."""
    match = DATE_TIME.fullmatch(value) if isinstance(value, str) else None
    if not match:
        return False
    try:
        datetime.datetime.strptime(match.group(1) + "T" + match.group(2), "%Y-%m-%dT%H:%M:%S")
    except ValueError:
        return False
    return match.group(5) is None or (int(match.group(5)) < 24 and int(match.group(6)) < 60)


def version_key(version):
    return tuple(int(part) for part in version.split("."))


def check_order(versions, what, where):
    """The versions are newest first, and no version is there twice."""
    keys = []
    for version in versions:
        try:
            keys.append(version_key(version))
        except ValueError:
            return ["%s: the %s %r is not made of numbers separated by dots" % (where, what, version)]
    problems = []
    if len(set(keys)) != len(keys):
        problems.append("%s: a %s is listed twice" % (where, what))
    elif keys != sorted(keys, reverse=True):
        problems.append("%s: the %ss are not newest first" % (where, what))
    return problems


LIBC = {"linux": "glibc", "linux-musl": "musl"}


def check_tarball_hashes(data, where):
    problems = check_order([r["version"] for r in data["releases"]], "version", where)
    for release in data["releases"]:
        platforms = set()
        for item in release["files"]:
            expected = "newrelic-php5-%s-%s.tar.gz" % (release["version"], item["platform"])
            if item["filename"] != expected:
                problems.append("%s: the file %r of %r should be named %r" % (where, item["filename"], release["version"], expected))
            if item["platform"] in platforms:
                problems.append("%s: the platform %r is listed twice in %r" % (where, item["platform"], release["version"]))
            platforms.add(item["platform"])
            if item.get("libc") != LIBC.get(item["platform"]):
                problems.append("%s: the file %r of %r has libc %r, expected %r (libc is set for the linux platforms only: glibc for linux, musl for linux-musl)"
                                % (where, item["filename"], release["version"], item.get("libc"), LIBC.get(item["platform"])))
    return problems


def check_daemon_digests(data, where):
    problems = check_order([i["version"] for i in data["images"]], "tag", where)
    try:
        first = version_key(data["first_image_version"])
        for image in data["images"]:
            if version_key(image["version"]) < first:
                problems.append("%s: the tag %r is below first_image_version %r" % (where, image["version"], data["first_image_version"]))
    except ValueError:
        problems.append("%s: first_image_version is not made of numbers separated by dots" % where)
    return problems


def check_fingerprints(data, where):
    problems = []
    seen = set()
    for key in data["keys"]:
        fingerprint = key["fingerprint"]
        # a key that is replaced is marked retired, not removed; the legacy key is retired
        if key["purpose"] == "legacy" and key.get("status", "active") != "retired":
            problems.append("%s: the legacy key %r must have the status retired" % (where, fingerprint))
        if fingerprint in seen:
            problems.append("%s: the fingerprint %r is listed twice" % (where, fingerprint))
        seen.add(fingerprint)
        url = urlsplit(key["public_key_url"])
        # the legacy key was published under the name of its short key id, before the NEWRELIC_ names
        if key["purpose"] == "legacy":
            expected = "%s.gpg" % fingerprint[-8:]
        else:
            expected = "NEWRELIC_%s_%s.public" % (key["purpose"].upper(), fingerprint[-8:])
        if url.scheme != "https" or url.netloc != "download.newrelic.com" or url.query or url.fragment:
            problems.append("%s: the public_key_url %r of %r is not an https URL on download.newrelic.com" % (where, key["public_key_url"], fingerprint))
        elif url.path.rsplit("/", 1)[-1] != expected:
            problems.append("%s: the public_key_url %r should end in %r" % (where, key["public_key_url"], expected))
    for purpose in ("apt", "rpm", "tarball"):
        active = [k for k in data["keys"] if k["purpose"] == purpose and k.get("status", "active") == "active"]
        if len(active) != 1:
            problems.append("%s: there must be exactly one active key with the purpose %s, found %d" % (where, purpose, len(active)))
    return problems


CHECKS = {
    "tarball-hashes.json": check_tarball_hashes,
    "daemon-image-digests.json": check_daemon_digests,
    "signing-key-fingerprints.json": check_fingerprints,
}


def check_tree(directory):
    """Return the list of problems of the three data files below directory, using the schemas in directory/schema."""
    problems = []
    for name, schema_name in FILES.items():
        path = os.path.join(directory, name)
        schema_path = os.path.join(directory, "schema", schema_name)
        try:
            with open(schema_path) as handle:
                schema = json.load(handle)
            Validator.check_schema(schema)
        except (OSError, ValueError, exceptions.SchemaError) as error:
            problems.append("%s: not a usable schema: %s" % (schema_path, str(error).splitlines()[0]))
            continue
        try:
            with open(path, newline="") as handle:
                text = handle.read()
            data = json.loads(text)
        except (OSError, ValueError) as error:
            problems.append("%s: cannot be read as JSON: %s" % (path, error))
            continue
        errors = sorted(Validator(schema, format_checker=FORMATS).iter_errors(data), key=lambda e: list(e.absolute_path))
        for error in errors:
            problems.append("%s: %s: %s" % (path, "/".join(str(part) for part in error.absolute_path) or "(file)", error.message))
        if errors:
            continue  # the checks below assume a file that matches its schema
        if text != json.dumps(data, indent=2, sort_keys=True) + "\n":
            problems.append("%s: not in the format that insert-release-data.py writes (sorted keys, two-space indent, one final newline)" % path)
        problems.extend(CHECKS[name](data, path))
    return problems


def main(argv):
    directory = argv[1] if len(argv) > 1 else os.path.dirname(os.path.abspath(__file__))
    if len(argv) > 2 or directory in ("-h", "--help"):
        print(__doc__, file=sys.stderr)
        return 2
    problems = check_tree(directory)
    for problem in problems:
        # a value from a file must not start another line, which the runner would read as a workflow command
        print("::error::" + problem.replace("\r", "\\r").replace("\n", "\\n"))
    if not problems:
        print("release-data is valid")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

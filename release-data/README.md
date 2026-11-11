# Release data

Published facts about PHP agent releases that a customer or a workflow can check a download against. The files are
written only by workflows that open a pull request against `main`; each pull request needs an approval from the PHP
agent team.

| File | Content |
|---|---|
| [`tarball-hashes.json`](tarball-hashes.json) | SHA-256 hash of every tarball of each release, newest release first |
| [`daemon-image-digests.json`](daemon-image-digests.json) | Content digest of each `newrelic/php-daemon` image, newest first |
| [`signing-key-fingerprints.json`](signing-key-fingerprints.json) | Fingerprints and public key locations of the keys that sign PHP agent packages |

The format of each file is described by the JSON Schema with the same name in [`schema/`](schema/).

A validator has to check the `format` keywords (`date`, `date-time`) to reject a date that does not exist, and it has to follow ECMA-262 for `pattern`, where `$` does not match before a trailing newline. Python's `jsonschema` does neither by default; the scripts that write these files check both.

## Read the files

Everything below was tried with `jq` on sample files in the same shape. The examples are shell commands; replace the version and
the file name.

### `tarball-hashes.json`

```
{"releases": [ {"version": "12.13.0.320", "date": "2026-11-20", "files": [
    {"filename": "newrelic-php5-12.13.0.320-linux.tar.gz", "platform": "linux", "libc": "glibc", "sha256": "<64 hex digits>"},
    ... ]}, ... ]}
```

* `releases` is ordered newest version first, by version number (12.10 is newer than 12.9). `.releases[0]` is the newest.
* `version` is four numbers (`12.13.0.320`). `date` is the UTC date the hashes were recorded: the release date for a release that was
  recorded when it was promoted, the date of the backfill for a release that was signed later.
* A file has `filename`, `platform`, `sha256`, and `libc` (`glibc` or `musl`) for the linux platforms only. The `filename` is always
  `newrelic-php5-<version>-<platform>.tar.gz`, and `platform` is the part between the version and `.tar.gz`: `linux`, `linux-musl`,
  `osx`, `freebsd` or `solaris`.
* **There is no architecture and no URL in an entry, on purpose.** A linux tarball holds the builds for both architectures
  (`daemon/newrelic-daemon.x64` and `daemon/newrelic-daemon.aarch64`, `agent/x64/` and `agent/aarch64/`), so the architecture is not a
  property of the file. The URL is a function of two values that the entry has, so storing it would only add a value that can
  disagree with them.

**The URL of a file** is `https://download.newrelic.com/php_agent/archive/<version>/<filename>`. The newest release is also copied to
`https://download.newrelic.com/php_agent/release/<filename>`; use the `archive` URL, which works for every version. The signature is the
same URL with `.asc` added.

```
# the SHA-256 of one file
jq -r --arg v 12.13.0.320 --arg f newrelic-php5-12.13.0.320-linux.tar.gz \
  '.releases[] | select(.version == $v) | .files[] | select(.filename == $f) | .sha256' tarball-hashes.json

# the URL and the hash of the file of one platform
jq -r --arg v 12.13.0.320 --arg p linux '.releases[] | select(.version == $v) | . as $r | .files[] | select(.platform == $p)
  | "https://download.newrelic.com/php_agent/archive/\($r.version)/\(.filename)  \(.sha256)"' tarball-hashes.json

# every URL of a release
jq -r --arg v 12.13.0.320 '.releases[] | select(.version == $v) | . as $r | .files[]
  | "https://download.newrelic.com/php_agent/archive/\($r.version)/\(.filename)"' tarball-hashes.json

# the version of the newest release, and the musl file names of a release
jq -r '.releases[0].version' tarball-hashes.json
jq -r --arg v 12.13.0.320 '.releases[] | select(.version == $v) | .files[] | select(.libc == "musl") | .filename' tarball-hashes.json

# check downloaded files against the list (run in the directory of the files; prints OK or FAILED for each)
jq -r --arg v 12.13.0.320 '.releases[] | select(.version == $v) | .files[] | "\(.sha256)  \(.filename)"' tarball-hashes.json | sha256sum -c --ignore-missing -
```

If a version is not in the list, no hash is recorded for it: it is a snapshot, a beta or a release candidate (never listed), a release whose pull request has not merged yet, an older release that the backfill has not reached, or a version that was removed from the download site.

### `daemon-image-digests.json`

`{"first_image_version": "9.2.0", "images": [{"version": "12.12.0", "digest": "sha256:...", "platforms": {"linux/amd64": "sha256:..."},
"pushed": "<UTC time>"}, ...]}`, newest first. `version` is the image tag, which is the agent version without the build number.
`digest` is the digest of the tag (the index over all platforms) and `platforms` has the digest of the image of each platform. A tag
below `first_image_version` has no official image.

```
jq -r --arg t 12.12.0 '.images[] | select(.version == $t) | .digest' daemon-image-digests.json
jq -r --arg t 12.12.0 '.images[] | select(.version == $t) | .platforms["linux/amd64"]' daemon-image-digests.json
docker pull newrelic/php-daemon@sha256:<the digest>
```

### `signing-key-fingerprints.json`

`{"first_signed_version": "12.13.0", "keys": [{"purpose": "apt" | "rpm" | "tarball", "name", "fingerprint", "created", "expires",
"public_key_url"}, ...]}`. `fingerprint` is 40 upper-case hex digits without spaces. `expires` is a date or `never`.
`first_signed_version` is the first version whose tarballs were signed when they were built: the numbers at the start of a snapshot
version (`12.13.0.320SNAPSHOT`) are compared with it. Older versions that are in `tarball-hashes.json` were signed later.

Exactly one key has the purpose `tarball` (the schema enforces it). The tools that read this file take that key, so a
key rotation lifts the limit in the schema in the same pull request that changes them.

```
jq -r '.keys[] | select(.purpose == "tarball") | .public_key_url' signing-key-fingerprints.json
jq -r '.keys[] | select(.purpose == "tarball") | .fingerprint | [scan(".{4}")] | join(" ")' signing-key-fingerprints.json   # as gpg prints it
jq -r '.first_signed_version' signing-key-fingerprints.json
```

## Checks on a change to these files

The other pull request workflows ignore `release-data/**`, so the workflow
[`validate-release-data`](../.github/workflows/validate-release-data.yml) is the check that runs on a change to these files
(on every pull request, and on every push to `main` that changes them). It runs [`validate.py`](validate.py), which checks:

* each file against its schema, with the `date` and `date-time` formats asserted and `$` read as ECMA-262 reads it;
* the format that the writer scripts of the builder and daemon repositories write: sorted keys, two-space indent, one final newline;
* `tarball-hashes.json`: releases newest first by version number, no version twice, file names that match the version and platform, no platform
  twice in a release, and `libc` set exactly for the linux platforms (`glibc` for `linux`, `musl` for `linux-musl`);
* `daemon-image-digests.json`: images newest first, no tag twice, no tag below `first_image_version`;
* `signing-key-fingerprints.json`: no fingerprint twice; every `public_key_url` is an https URL on `download.newrelic.com` with the
  file name `NEWRELIC_<PURPOSE>_<last 8 characters of the fingerprint>.public`. The key at that address is not fetched, so a
  wrong but well-formed fingerprint is still the approver's check.

The tests of the validator are in [`tests/`](tests/): `python3 -m unittest discover -s release-data/tests` (needs `jsonschema`).
The workflow is a required check on `main`. It installs `jsonschema` and its dependencies from `requirements.txt`, which pins them with hashes
(made by `pip-compile` from `requirements.in`; regenerate it with the command in that file).

## Verify a tarball

Each tarball on the download site has a detached signature next to it, named like the tarball with `.asc` added.

1. Download the tarball, its `.asc` file and the public key listed for the `tarball` purpose in
   `signing-key-fingerprints.json`.
2. Compare the fingerprint of the key file with the one in `signing-key-fingerprints.json`, then import the key. Get that
   file from this repository, not from the download site, so that the fingerprint does not come from the same place as the key.
   The fingerprint printed under the `pub` line must equal the listed one, written in groups of four (see the `jq` line above):

   ```
   gpg --show-keys --with-fingerprint NEWRELIC_TARBALL_<last 8 of the fingerprint>.public
   gpg --import NEWRELIC_TARBALL_<last 8 of the fingerprint>.public
   ```
3. Verify the signature:

   ```
   gpg --verify newrelic-php5-<version>-linux.tar.gz.asc newrelic-php5-<version>-linux.tar.gz
   ```

   The output must say `Good signature`, and its `Primary key fingerprint:` line (printed after the warning below) must equal
   the listed fingerprint. Do not rely on the name in the `Good signature from "..."` line: anyone can make a key with that
   name. gpg also prints a warning that the key is not certified with a trusted signature; the fingerprint comparison is
   what establishes trust in the key.

4. If the version is listed in `tarball-hashes.json`, compare the SHA-256 hash of the file with its entry. The signature shows
   that New Relic's key signed these bytes; it does not say which version or build they are, so a signed tarball of
   another build that was renamed would pass step 3. The hash is recorded for this version and file name:

   ```
   sha256sum newrelic-php5-<version>-linux.tar.gz
   ```

   Only official releases are listed, and a release only after the pull request that records it has merged. Snapshots, betas and
   release candidates are signed but never listed, and a new release has no entry until then. If the version is not listed,
   steps 3 and 5 are the checks that apply.
5. Check that the first entry of the tarball is the folder named like the file, `newrelic-php5-<version>-linux/`. That
   name is inside the signed bytes, so this check works with or without an entry and ties the signed bytes to the version
   when there is no hash:

   ```
   tar -tzf newrelic-php5-<version>-linux.tar.gz | head -n 1
   ```

Tarballs built before signing started are listed in `tarball-hashes.json` once they have been signed. Versions
at or above `first_signed_version` in `signing-key-fingerprints.json` were signed when they were built. The value `12.13.0` covers every build
of 12.13.0 and later: snapshots, betas, release candidates and releases.

## Pin a daemon image

`daemon-image-digests.json` lists the digest of each tag of the daemon image. Pulling by digest always gives the
same image:

```
docker pull newrelic/php-daemon@sha256:<digest>
```

`first_image_version` is the first tag that was published (`9.2.0`). A tag below it has no official image, so an
image that claims such a tag is not ours. A tag at or above it that is not listed in `images` is missing from the list:
do not trust an image for it without asking the PHP agent team.

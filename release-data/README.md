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

## Verify a tarball

Each tarball on the download site has a detached signature next to it, named like the tarball with `.asc` added.

1. Download the tarball, its `.asc` file and the public key listed for the `tarball` purpose in
   `signing-key-fingerprints.json`.
2. Import the key and compare its fingerprint with the one in `signing-key-fingerprints.json`. Get that file from
   this repository, not from the download site, so that the fingerprint does not come from the same place as the key:

   ```
   gpg --import NEWRELIC_TARBALL_<last 8 of the fingerprint>.public
   gpg --fingerprint "New Relic PHP Agent Tarball Signing Key"
   ```
3. Verify the signature:

   ```
   gpg --verify newrelic-php5-<version>-linux.tar.gz.asc newrelic-php5-<version>-linux.tar.gz
   ```

   The line `Good signature from "New Relic PHP Agent Tarball Signing Key (2026)"` means the file is the one that was
   signed. gpg also prints a warning that the key is not certified with a trusted signature; the fingerprint
   comparison in the previous step is what establishes trust in the key.

4. Compare the SHA-256 hash of the file with the entry for the version in `tarball-hashes.json`. The signature shows
   that New Relic's key signed these bytes; it does not say which version or build they are, so a signed tarball of
   another build that was renamed would pass step 3. The hash is recorded for this version and file name:

   ```
   sha256sum newrelic-php5-<version>-linux.tar.gz
   ```
5. Check that the first entry of the tarball is the folder named like the file, `newrelic-php5-<version>-linux/`. That
   name is inside the signed bytes:

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

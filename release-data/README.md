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

To check only that the download is intact, compare its SHA-256 hash with the entry for the version in
`tarball-hashes.json`:

```
sha256sum newrelic-php5-<version>-linux.tar.gz
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

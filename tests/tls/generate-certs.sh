#!/bin/sh
#
# Copyright 2026 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Generate the throwaway certificates used by the TLS integration tests: a
# private CA, a server certificate for the MySQL TLS service, a client
# certificate signed by the CA, and a second CA that the server does not use
# (for tests that expect a certificate check to fail). Nothing is generated if
# the certificates already exist. The certificates are written to
# tests/tls/certs, which is not tracked.
#
set -e

dir="$(cd "$(dirname "$0")" && pwd)/certs"
host="${MYSQL_TLS_HOST:-mysqltls}"

if [ -f "$dir/ca.pem" ] && [ -f "$dir/server-cert.pem" ] && [ -f "$dir/client-cert.pem" ]; then
  exit 0
fi

mkdir -p "$dir"
cd "$dir"

openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
  -subj "/CN=Test CA" -keyout ca-key.pem -out ca.pem 2> /dev/null

# The server certificate name has to match the host name the tests connect to.
openssl req -newkey rsa:2048 -nodes -subj "/CN=$host" \
  -keyout server-key.pem -out server.csr 2> /dev/null
printf "subjectAltName=DNS:%s,DNS:localhost\n" "$host" > san.cnf
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca-key.pem -CAcreateserial \
  -days 3650 -extfile san.cnf -out server-cert.pem 2> /dev/null

openssl req -newkey rsa:2048 -nodes -subj "/CN=nr_tls_client" \
  -keyout client-key.pem -out client.csr 2> /dev/null
openssl x509 -req -in client.csr -CA ca.pem -CAkey ca-key.pem -CAcreateserial \
  -days 3650 -out client-cert.pem 2> /dev/null

openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
  -subj "/CN=Other CA" -keyout other-ca-key.pem -out other-ca.pem 2> /dev/null

rm -f server.csr client.csr san.cnf ca.srl

# The MySQL server runs as another user inside its container.
chmod 644 ./*.pem

#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

thumb="${CERTUM_CERT_SHA1:?CERTUM_CERT_SHA1 is required}"
thumb="${thumb//[[:space:]]/}"
if [[ ! "$thumb" =~ ^[[:xdigit:]]{40}$ ]] || (( $# == 0 )); then
    echo 'Usage: CERTUM_CERT_SHA1=<thumbprint> bash verify-certum.sh <exe> [...]' >&2
    exit 1
fi

ca_file="${SSL_CERT_FILE:-/etc/ssl/certs/ca-certificates.crt}"
for file in "$@"; do
    echo "Verifying $file"
    if ! output=$(osslsigncode verify -CAfile "$ca_file" -TSA-CAfile "$ca_file" \
        -require-leaf-hash "sha1:$thumb" -in "$file" 2>&1); then
        printf '%s\n' "$output" >&2
        exit 1
    fi
    printf '%s\n' "$output"
    # A valid signature without a timestamp also returns success from osslsigncode.
    if ! grep -qx 'Timestamp Server Signature verification: ok' <<< "$output"; then
        echo "Missing verified timestamp: $file" >&2
        exit 1
    fi
done

#!/bin/bash
# Install Certum SimplySign Desktop on the (ephemeral) Windows runner.
# SimplySign Desktop is what mounts the cloud signing certificate as a virtual
# smart card into the Windows certificate store; signtool then selects it by
# thumbprint. The version + checksum are pinned for reproducibility.

set -euo pipefail

VERSION="9.3.4.72"
URL="https://files.certum.eu/software/SimplySignDesktop/Windows/${VERSION}/SimplySignDesktop-${VERSION}-64-bit-en.msi"
EXPECTED_SHA256="bd51ebbaaac20fc7d59ab7103b5ed532b7800586df4e31f6999d03a394f9c515"
INSTALLER="SimplySignDesktop.msi"
INSTALL_DIR="/c/Program Files/Certum/SimplySign Desktop"

echo "=== Installing Certum SimplySign Desktop ${VERSION} ==="

# Idempotent: a cached/pre-provisioned image may already have it.
if [ -d "$INSTALL_DIR" ]; then
  echo "Already installed at: $INSTALL_DIR"
  exit 0
fi

echo "Downloading installer..."
curl -L "$URL" -o "$INSTALLER" --fail --max-time 600

ACTUAL_SHA256=$(sha256sum "$INSTALLER" | awk '{print $1}')
if [ "$EXPECTED_SHA256" != "$ACTUAL_SHA256" ]; then
  echo "ERROR: checksum mismatch"
  echo "  expected: $EXPECTED_SHA256"
  echo "  actual:   $ACTUAL_SHA256"
  exit 1
fi
echo "Checksum verified."

echo "Running msiexec (silent)..."
# Start-Process inherits this CWD, so the relative installer path resolves here.
powershell -Command "Start-Process msiexec.exe -ArgumentList '/i','${INSTALLER}','/quiet','/norestart','/l*v','install.log','ALLUSERS=1','REBOOT=ReallySuppress' -Wait -NoNewWindow"

if [ ! -d "$INSTALL_DIR" ]; then
  echo "ERROR: installation directory not found after install"
  echo "Last 20 lines of install.log:"
  tail -20 install.log 2>/dev/null || echo "(no install.log)"
  exit 1
fi

echo "SimplySign Desktop installed."

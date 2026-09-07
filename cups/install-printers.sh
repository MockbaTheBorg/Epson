#!/bin/bash
# Registers the Epson-LX and IBM-1403 virtual CUPS printers using custom backends
# (System V interface scripts are no longer supported by modern CUPS).
# Re-run safely; existing queues/backends are removed and re-added.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKEND_DIR="/usr/lib/cups/backend"

sudo mkdir -p /var/spool/printer-emu/epson /var/spool/printer-emu/1403
# Sticky bit: shared write access (CUPS backends run as root) without cross-user deletes
sudo chmod 1777 /var/spool/printer-emu /var/spool/printer-emu/epson /var/spool/printer-emu/1403

# Install backends: CUPS requires them to be owned by root and not writable by group/other
sudo install -o root -g root -m 0700 "$SCRIPT_DIR/backend-epsonpdf" "$BACKEND_DIR/epsonpdf"
sudo install -o root -g root -m 0700 "$SCRIPT_DIR/backend-virt1403" "$BACKEND_DIR/virt1403"

lpadmin -x Epson-LX 2>/dev/null || true
lpadmin -x IBM-1403 2>/dev/null || true

# No PPD/model given => raw queue: CUPS sends job data to the backend unfiltered
sudo lpadmin -p Epson-LX -E -v epsonpdf:/ -D "Epson LX PDF Printer"
sudo lpadmin -p IBM-1403 -E -v virt1403:/ -D "IBM 1403 PDF Printer"

echo "Installed: Epson-LX -> /var/spool/printer-emu/epson/"
echo "Installed: IBM-1403 -> /var/spool/printer-emu/1403/"

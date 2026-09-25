#!/bin/bash
#
# Mockba the Borg - Uninstall printer configuration for CUPS
# https://github.com/MockbaTheBorg/Epson
# Removes the Epson-LX and IBM-1403 virtual CUPS printers and their backends
# (spool output under /var/spool/printer-emu is left in place).
sudo lpadmin -x Epson-LX 2>/dev/null || true
sudo lpadmin -x IBM-1403 2>/dev/null || true
sudo rm -f /usr/lib/cups/backend/epsonpdf /usr/lib/cups/backend/virt1403
echo "Removed Epson-LX and IBM-1403 CUPS queues and backends."

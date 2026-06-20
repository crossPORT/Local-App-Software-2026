#!/usr/bin/env bash
set -euo pipefail
echo "Installing wxWidgets development packages (GTK3)..."
sudo apt-get update
sudo apt-get install -y libwxgtk3.2-dev

Optional sample identity configs for two-port trade-show setups.
Not loaded automatically — pass explicitly at launch:

  ./build/apps/wx/RocketBox --port 0 --config demo-config/port0.conf
  ./build/apps/wx/RocketBox --port 1 --config demo-config/port1.conf

Or both instances can share demo-config/shared.conf (uses [port0]/[port1] sections when --port is set).

Without --port, a second instance with two cables connected shows the Connect USB picker instead.

PWA: use in-app Settings (stored in the browser; no config file).

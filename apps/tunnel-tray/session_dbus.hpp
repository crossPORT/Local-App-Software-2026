#pragma once

/** If DBUS_SESSION_BUS_ADDRESS is unset, point it at the user session bus. */
void adopt_session_dbus_env();

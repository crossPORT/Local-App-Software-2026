#include "demo_styles.h"

#include <gtk/gtk.h>

namespace {

const char* kDemoCss = R"(
window.ces-window {
  background-color: #0f1419;
}

.ces-panel {
  background-color: #1a2332;
  border-radius: 10px;
  padding: 14px;
}

.ces-header-title {
  font-size: 22px;
  font-weight: 700;
  color: #f0f4f8;
}

.ces-header-sub {
  font-size: 11px;
  color: #8899aa;
  letter-spacing: 0.5px;
}

.ces-port-badge {
  font-size: 13px;
  font-weight: 700;
  color: #0f1419;
  background-color: #00d4aa;
  border-radius: 6px;
  padding: 4px 10px;
}

.ces-role-receiver {
  color: #5eb3ff;
  font-weight: 700;
  font-size: 12px;
}

.ces-role-sender {
  color: #ffb347;
  font-weight: 700;
  font-size: 12px;
}

.ces-role-primary {
  color: #00d4aa;
  font-weight: 700;
  font-size: 12px;
}

.ces-throughput-label {
  font-size: 10px;
  font-weight: 600;
  color: #667788;
  letter-spacing: 1.2px;
}

.ces-throughput-value {
  font-size: 42px;
  font-weight: 800;
  color: #00d4aa;
}

.ces-throughput-unit {
  font-size: 16px;
  color: #8899aa;
}

.ces-detail {
  font-size: 12px;
  color: #8899aa;
}

.ces-status {
  font-size: 13px;
  color: #c8d4e0;
}

.ces-status-success {
  font-size: 14px;
  font-weight: 600;
  color: #00d4aa;
}

.ces-port-live {
  color: #00d4aa;
  font-weight: 700;
  font-size: 12px;
}

.ces-port-idle {
  color: #556677;
  font-size: 12px;
}

.ces-demo-hint {
  font-size: 11px;
  color: #778899;
}

#error-label {
  color: #ff6b6b;
  font-size: 12px;
}

button.ces-btn {
  min-height: 44px;
  font-weight: 600;
  border-radius: 8px;
  border: none;
  color: #f0f4f8;
}

button.ces-btn-send {
  background: linear-gradient(to bottom, #2a6fdb, #1e5099);
}

button.ces-btn-send:hover {
  background: linear-gradient(to bottom, #3a7feb, #2a60a9);
}

button.ces-btn-receive {
  background: linear-gradient(to bottom, #1a9a6c, #127a54);
}

button.ces-btn-receive:hover {
  background: linear-gradient(to bottom, #2aaa7c, #228a64);
}

button.ces-btn-accent {
  background: linear-gradient(to bottom, #4a5568, #2d3748);
}

button.ces-btn-accent:hover {
  background: linear-gradient(to bottom, #5a6578, #3d4758);
}

button.ces-btn-loopback {
  background: linear-gradient(to bottom, #6b46c1, #553c9a);
}

button.ces-btn-loopback:hover {
  background: linear-gradient(to bottom, #7b56d1, #654caa);
}

button.ces-btn:disabled {
  opacity: 0.45;
}

progressbar.ces-progress trough {
  background-color: #252f3d;
  border-radius: 6px;
  min-height: 14px;
}

progressbar.ces-progress progress {
  background-color: #00d4aa;
  border-radius: 6px;
}

.ces-drop-zone {
  background-color: #141c28;
  border: 2px dashed #3a4a5c;
  border-radius: 12px;
  min-height: 88px;
  padding: 16px;
}

.ces-drop-zone-active {
  background-color: #1a2838;
  border-color: #00d4aa;
}

.ces-drop-title {
  font-size: 15px;
  font-weight: 700;
  color: #e8eef4;
}

.ces-drop-sub {
  font-size: 11px;
  color: #8899aa;
}
)";

}  // namespace

void apply_demo_theme() {
    GtkCssProvider* provider = gtk_css_provider_new();
    GError* error = nullptr;
    if (!gtk_css_provider_load_from_data(provider, kDemoCss, -1, &error)) {
        g_warning("Demo theme failed: %s", error ? error->message : "unknown");
        g_clear_error(&error);
        g_object_unref(provider);
        return;
    }

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

#include "demo_config.h"
#include "demo_styles.h"
#include "main_window.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <iostream>
#include <memory>
#include <string>

namespace {

int port_index = 0;
gchar* config_path = nullptr;
gchar* send_on_start = nullptr;

const GOptionEntry kOptions[] = {
    {"port",
     'p',
     0,
     G_OPTION_ARG_INT,
     &port_index,
     "Fabric port index for this instance (0 = first 1772:0006 device)",
     "N"},
    {"config",
     'c',
     0,
     G_OPTION_ARG_FILENAME,
     &config_path,
     "Demo config file (source_dir / target_dir). "
     "Default: CES_DEMO_CONFIG, ./ces-demo.conf, or ~/.config/sls-fabric/demo.conf",
     "PATH"},
    {"send-on-start",
     0,
     0,
     G_OPTION_ARG_FILENAME,
     &send_on_start,
     "Send a file immediately after the window opens (debug)",
     "PATH"},
    {},
};

}  // namespace

int main(int argc, char* argv[]) {
    GOptionContext* context = g_option_context_new("- USB fabric transfer demo");
    g_option_context_add_main_entries(context, kOptions, nullptr);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));

    GError* error = nullptr;
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        std::cerr << error->message << '\n';
        g_error_free(error);
        g_option_context_free(context);
        return 1;
    }
    g_option_context_free(context);

    if (port_index < 0) {
        std::cerr << "Port index must be >= 0\n";
        return 1;
    }

    gtk_init(&argc, &argv);
    apply_demo_theme();

    DemoConfig config{};
    if (load_demo_config(port_index, config_path ? config_path : "", config)) {
        std::cout << "Loaded demo config: " << config.loaded_from << '\n';
    }

    auto app = std::make_unique<MainWindow>(port_index, config);
    gtk_widget_show_all(app->widget());
    if (send_on_start && *send_on_start != '\0') {
        app->send_path_on_startup(send_on_start);
    }
    gtk_main();
    app.reset();
    return 0;
}

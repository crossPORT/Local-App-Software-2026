#include "main_window.h"

#include "fabric_meta_file.h"

#include <glib.h>

#include <iomanip>
#include <sstream>
#include <vector>

namespace {

struct UiUpdatePayload {
    MainWindow* window;
    TransferUiState state;
};

void free_ui_update_payload(gpointer data) {
    delete static_cast<UiUpdatePayload*>(data);
}

std::string format_gib(uint64_t bytes) {
    const double gib = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << gib << " GiB";
    return out.str();
}

std::string format_mbps(double mbps) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << mbps;
    return out.str();
}

GtkWidget* styled_label(const char* text, const char* css_class, gdouble xalign) {
    GtkWidget* label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), xalign);
    if (css_class) {
        gtk_style_context_add_class(gtk_widget_get_style_context(label), css_class);
    }
    return label;
}

std::string uri_to_local_path(const char* uri) {
    if (!uri || *uri == '\0') {
        return {};
    }
    gchar* path = g_filename_from_uri(uri, nullptr, nullptr);
    if (!path) {
        return {};
    }
    std::string local(path);
    g_free(path);
    return local;
}

std::vector<std::string> parse_uri_list(const guchar* data, gsize length) {
    std::vector<std::string> paths;
    if (!data || length == 0) {
        return paths;
    }

    std::string text(reinterpret_cast<const char*>(data), length);
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        const std::string path = uri_to_local_path(line.c_str());
        if (!path.empty()) {
            paths.push_back(path);
        }
    }
    return paths;
}

}  // namespace

struct DropIdlePayload {
    GtkWidget* window = nullptr;
    std::vector<std::string> paths;
};

gboolean MainWindow::on_drop_idle(gpointer data) {
    auto* payload = static_cast<DropIdlePayload*>(data);
    if (payload->window) {
        auto* self = static_cast<MainWindow*>(
            g_object_get_data(G_OBJECT(payload->window), "main-window"));
        if (self && !self->destroyed_) {
            self->handle_dropped_paths(payload->paths);
        }
    }
    delete payload;
    return G_SOURCE_REMOVE;
}

MainWindow::MainWindow(int port_index, const DemoConfig& config)
    : port_index_(port_index)
    , session_role_(config.role.empty()
                        ? default_session_role_for_port(port_index)
                        : session_role_from_string(config.role))
    , source_dir_(config.source_dir)
    , target_dir_(config.target_dir) {
    window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_), "SLS Fabric — Live USB Demo");
    gtk_window_set_default_size(GTK_WINDOW(window_), 480, 680);
    gtk_widget_set_name(window_, "ces-window");
    gtk_style_context_add_class(gtk_widget_get_style_context(window_), "ces-window");
    gtk_window_move(GTK_WINDOW(window_), port_index_ == 0 ? 60 : 560, 60);
    g_object_set_data(G_OBJECT(window_), "main-window", this);
    g_signal_connect(window_, "destroy", G_CALLBACK(on_destroy), this);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(root), 16);
    gtk_container_add(GTK_CONTAINER(window_), root);

    build_header(root);
    build_connection_panel(root);
    build_locations_panel(root);
    build_drop_panel(root);
    build_throughput_panel(root);

    progress_bar_ = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress_bar_), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(progress_bar_), "ces-progress");
    gtk_box_pack_start(GTK_BOX(root), progress_bar_, FALSE, FALSE, 0);

    build_controls(root);
    build_footer(root);

    g_signal_connect(send_button_, "clicked", G_CALLBACK(on_send_clicked), this);
    g_signal_connect(receive_button_, "clicked", G_CALLBACK(on_receive_clicked), this);
    g_signal_connect(source_dir_button_, "clicked", G_CALLBACK(on_source_dir_clicked), this);
    g_signal_connect(target_dir_button_, "clicked", G_CALLBACK(on_target_dir_clicked), this);
    g_signal_connect(loopback_button_, "clicked", G_CALLBACK(on_loopback_clicked), this);
    g_signal_connect(role_combo_, "changed", G_CALLBACK(on_role_changed), this);

    controller_ = std::make_unique<TransferController>(
        port_index_,
        [this](const TransferUiState& state) { schedule_ui_update(state); });

    refresh_connection_panel();
    update_location_labels();
    update_role_presentation();
    refresh_timer_id_ = g_timeout_add_seconds(1, on_refresh_timer, this);

    setup_drag_dest(window_);
}

MainWindow::~MainWindow() {
    if (refresh_timer_id_ != 0) {
        g_source_remove(refresh_timer_id_);
    }
}

GtkWidget* MainWindow::make_panel(GtkWidget* parent, const char* css_class) {
    GtkWidget* frame = gtk_frame_new(nullptr);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    gtk_container_add(GTK_CONTAINER(frame), box);
    gtk_style_context_add_class(gtk_widget_get_style_context(frame), "ces-panel");
    if (css_class) {
        gtk_style_context_add_class(gtk_widget_get_style_context(frame), css_class);
    }
    gtk_box_pack_start(GTK_BOX(parent), frame, FALSE, FALSE, 0);
    return box;
}

void MainWindow::build_header(GtkWidget* root) {
    GtkWidget* panel = make_panel(root, nullptr);

    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(panel), row, FALSE, FALSE, 0);

    GtkWidget* titles = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(row), titles, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(titles),
                       styled_label("SLS Fabric", "ces-header-title", 0.0),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(titles),
                       styled_label("HIGH-SPEED USB DATA PATH", "ces-header-sub", 0.0),
                       FALSE, FALSE, 0);

    port_badge_label_ = styled_label(
        ("PORT " + std::to_string(port_index_)).c_str(), "ces-port-badge", 1.0);
    gtk_box_pack_start(GTK_BOX(row), port_badge_label_, FALSE, FALSE, 0);

    GtkWidget* role_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(panel), role_row, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(role_row),
                       styled_label("Session role", "ces-header-sub", 0.0),
                       FALSE, FALSE, 0);

    role_combo_ = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(role_combo_), "Receiver");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(role_combo_), "Sender");
    gtk_combo_box_set_active(GTK_COMBO_BOX(role_combo_),
                             session_role_ == SessionRole::Sender ? 1 : 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(role_combo_), "ces-role-combo");
    gtk_box_pack_start(GTK_BOX(role_row), role_combo_, FALSE, FALSE, 0);

    role_label_ = styled_label("", "ces-role-receiver", 0.0);
    gtk_box_pack_start(GTK_BOX(panel), role_label_, FALSE, FALSE, 4);
}

void MainWindow::build_connection_panel(GtkWidget* root) {
    GtkWidget* panel = make_panel(root, nullptr);
    gtk_box_pack_start(GTK_BOX(panel),
                       styled_label("FABRIC LINK", "ces-throughput-label", 0.0),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel),
                       styled_label(
                           "Each row tracks one cable by USB address (not libusb index)",
                           "ces-demo-hint", 0.0),
                       FALSE, FALSE, 0);

    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    port0_status_label_ = styled_label("○ link A", "ces-port-idle", 0.0);
    port1_status_label_ = styled_label("○ link B", "ces-port-idle", 0.0);
    gtk_box_pack_start(GTK_BOX(row), port0_status_label_, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), port1_status_label_, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(panel), row, FALSE, FALSE, 0);
}

void MainWindow::build_locations_panel(GtkWidget* root) {
    GtkWidget* panel = make_panel(root, nullptr);
    gtk_box_pack_start(GTK_BOX(panel),
                       styled_label("FOLDERS", "ces-throughput-label", 0.0),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel),
                       styled_label(
                           "Meta names the file; payload is raw bytes (one read, no temp archive)",
                           "ces-demo-hint", 0.0),
                       FALSE, FALSE, 0);

    source_dir_label_ = styled_label("Source folder: (not set)", "ces-detail", 0.0);
    target_dir_label_ = styled_label("Target folder: (not set)", "ces-detail", 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(source_dir_label_), TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(target_dir_label_), TRUE);
    gtk_box_pack_start(GTK_BOX(panel), source_dir_label_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel), target_dir_label_, FALSE, FALSE, 0);

    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    source_dir_button_ = gtk_button_new_with_label("Set source folder");
    target_dir_button_ = gtk_button_new_with_label("Set target folder");
    gtk_style_context_add_class(gtk_widget_get_style_context(source_dir_button_), "ces-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(target_dir_button_), "ces-btn");
    gtk_box_pack_start(GTK_BOX(row), source_dir_button_, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), target_dir_button_, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(panel), row, FALSE, FALSE, 0);
}

void MainWindow::build_drop_panel(GtkWidget* root) {
    GtkWidget* panel = make_panel(root, nullptr);
    drop_zone_ = gtk_event_box_new();
    gtk_widget_set_can_default(drop_zone_, FALSE);
    GtkStyleContext* drop_ctx = gtk_widget_get_style_context(drop_zone_);
    gtk_style_context_add_class(drop_ctx, "ces-drop-zone");

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(drop_zone_), box);

    drop_zone_label_ = styled_label("", "ces-drop-title", 0.5);
    gtk_box_pack_start(GTK_BOX(box), drop_zone_label_, FALSE, FALSE, 0);
    drop_zone_sub_label_ = styled_label("", "ces-drop-sub", 0.5);
    gtk_box_pack_start(GTK_BOX(box), drop_zone_sub_label_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(panel), drop_zone_, TRUE, TRUE, 0);
}

void MainWindow::setup_drag_dest(GtkWidget* widget) {
    GtkTargetEntry target{};
    target.target = const_cast<gchar*>("text/uri-list");
    target.flags = 0;
    target.info = 0;

    gtk_drag_dest_set(widget,
                      GTK_DEST_DEFAULT_ALL,
                      &target,
                      1,
                      GDK_ACTION_COPY);
    g_signal_connect(widget, "drag-motion", G_CALLBACK(on_drag_motion), this);
    g_signal_connect(widget, "drag-leave", G_CALLBACK(on_drag_leave), this);
    g_signal_connect(widget, "drag-data-received", G_CALLBACK(on_drag_data_received), this);
}

void MainWindow::set_drop_highlight(bool active) {
    if (!drop_zone_) {
        return;
    }
    GtkStyleContext* ctx = gtk_widget_get_style_context(drop_zone_);
    if (active) {
        gtk_style_context_add_class(ctx, "ces-drop-zone-active");
    } else {
        gtk_style_context_remove_class(ctx, "ces-drop-zone-active");
    }
}

gboolean MainWindow::on_drag_motion(GtkWidget*,
                                    GdkDragContext* context,
                                    gint,
                                    gint,
                                    guint time,
                                    gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    if (self->destroyed_) {
        return FALSE;
    }
    self->set_drop_highlight(true);
    gdk_drag_status(context, GDK_ACTION_COPY, time);
    return TRUE;
}

void MainWindow::on_drag_leave(GtkWidget*, GdkDragContext*, gpointer user_data) {
    static_cast<MainWindow*>(user_data)->set_drop_highlight(false);
}

void MainWindow::on_drag_data_received(GtkWidget*,
                                       GdkDragContext* context,
                                       gint,
                                       gint,
                                       GtkSelectionData* data,
                                       guint,
                                       guint time,
                                       gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    if (self->destroyed_) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }
    self->set_drop_highlight(false);

    if (!data || gtk_selection_data_get_length(data) <= 0) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    const std::vector<std::string> paths =
        parse_uri_list(gtk_selection_data_get_data(data),
                       static_cast<gsize>(gtk_selection_data_get_length(data)));
    if (paths.empty()) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    auto* payload = new DropIdlePayload{self->window_, std::move(paths)};
    g_idle_add(on_drop_idle, payload);
    gtk_drag_finish(context, TRUE, FALSE, time);
}

void MainWindow::handle_dropped_paths(const std::vector<std::string>& paths) {
    if (destroyed_ || paths.empty()) {
        return;
    }

    if (session_role_ == SessionRole::Sender) {
        for (const std::string& path : paths) {
            if (g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR)) {
                start_send_from_path(path);
                return;
            }
        }
        for (const std::string& path : paths) {
            if (g_file_test(path.c_str(), G_FILE_TEST_IS_DIR)) {
                source_dir_ = path;
                update_location_labels();
                gtk_label_set_text(GTK_LABEL(error_label_), "");
                return;
            }
        }
        gtk_label_set_text(GTK_LABEL(error_label_), "Drop a file to send.");
        return;
    }

    // Receiver role: folder sets target; file uses parent directory.
    for (const std::string& path : paths) {
        if (g_file_test(path.c_str(), G_FILE_TEST_IS_DIR)) {
            target_dir_ = path;
            update_location_labels();
            gtk_label_set_text(GTK_LABEL(error_label_), "");
            return;
        }
    }
    for (const std::string& path : paths) {
        if (g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR)) {
            const std::size_t slash = path.find_last_of('/');
            if (slash != std::string::npos) {
                target_dir_ = path.substr(0, slash);
                update_location_labels();
                gtk_label_set_text(GTK_LABEL(error_label_),
                                   "Target set to file's folder — click Receive.");
                return;
            }
        }
    }
    gtk_label_set_text(GTK_LABEL(error_label_), "Drop a folder to set the target directory.");
}

void MainWindow::start_send_from_path(const std::string& path) {
    if (destroyed_ || !controller_) {
        return;
    }
        gtk_label_set_text(GTK_LABEL(error_label_), "Busy — wait for the current transfer.");
    if (controller_->is_busy()) {
        gtk_label_set_text(GTK_LABEL(error_label_), "Not a regular file.");
        return;
    }

    const std::string relative_name = relative_name_for_send(path);
    if (relative_name.empty()) {
        gtk_label_set_text(GTK_LABEL(error_label_), "Invalid file path for send.");
        return;
    }

    FabricSendMeta meta{};
    meta.type = "file";
    meta.relative_name = relative_name;
    gtk_label_set_text(GTK_LABEL(error_label_), "");
    controller_->send_transfer(path, meta, source_dir_);
}

void MainWindow::build_throughput_panel(GtkWidget* root) {
    GtkWidget* panel = make_panel(root, nullptr);
    gtk_box_pack_start(GTK_BOX(panel),
                       styled_label("LIVE THROUGHPUT", "ces-throughput-label", 0.5),
                       FALSE, FALSE, 0);

    throughput_value_label_ = styled_label("—", "ces-throughput-value", 0.5);
    gtk_box_pack_start(GTK_BOX(panel), throughput_value_label_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel),
                       styled_label("MiB/s", "ces-throughput-unit", 0.5),
                       FALSE, FALSE, 0);

    throughput_detail_label_ = styled_label("Ready", "ces-detail", 0.5);
    gtk_box_pack_start(GTK_BOX(panel), throughput_detail_label_, FALSE, FALSE, 4);
}

void MainWindow::build_controls(GtkWidget* root) {
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_box_pack_start(GTK_BOX(root), grid, FALSE, FALSE, 4);

    receive_button_ = gtk_button_new_with_label("▼  Receive File");
    send_button_ = gtk_button_new_with_label("▶  Send");
    loopback_button_ = gtk_button_new_with_label("↻  Loopback (0→1)");

    gtk_style_context_add_class(gtk_widget_get_style_context(receive_button_), "ces-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(receive_button_), "ces-btn-receive");
    gtk_style_context_add_class(gtk_widget_get_style_context(send_button_), "ces-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(send_button_), "ces-btn-send");
    gtk_style_context_add_class(gtk_widget_get_style_context(loopback_button_), "ces-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(loopback_button_), "ces-btn-loopback");

    gtk_grid_attach(GTK_GRID(grid), receive_button_, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), send_button_, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), loopback_button_, 0, 1, 2, 1);

    if (port_index_ != 0) {
        gtk_widget_hide(loopback_button_);
    }
}

void MainWindow::build_footer(GtkWidget* root) {
    status_label_ = styled_label("Ready", "ces-status", 0.0);
    gtk_box_pack_start(GTK_BOX(root), status_label_, FALSE, FALSE, 0);

    demo_hint_label_ = styled_label("", "ces-demo-hint", 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(demo_hint_label_), TRUE);
    gtk_box_pack_start(GTK_BOX(root), demo_hint_label_, FALSE, FALSE, 0);

    error_label_ = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(error_label_), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(error_label_), TRUE);
    gtk_widget_set_name(error_label_, "error-label");
    gtk_box_pack_start(GTK_BOX(root), error_label_, FALSE, FALSE, 0);
}

void MainWindow::on_destroy(GtkWidget* widget, gpointer user_data) {
    g_object_set_data(G_OBJECT(widget), "main-window", nullptr);
    delete static_cast<MainWindow*>(user_data);
    gtk_main_quit();
}

gboolean MainWindow::on_refresh_timer(gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    if (!self->controller_->is_busy()) {
        self->refresh_connection_panel();
    }
    return G_SOURCE_CONTINUE;
}

void MainWindow::on_send_clicked(GtkButton*, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    if (self->controller_->is_busy()) {
        return;
    }
    const std::string path = self->pick_open_file();
    if (!path.empty()) {
        self->start_send_from_path(path);
    }
}

void MainWindow::on_receive_clicked(GtkButton*, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    if (self->controller_->is_busy()) {
        return;
    }
    if (self->target_dir_.empty()) {
        gtk_label_set_text(GTK_LABEL(self->error_label_),
                           "Set a target folder before receiving.");
        return;
    }
    self->controller_->receive_to_directory(self->target_dir_);
}

void MainWindow::on_source_dir_clicked(GtkButton*, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    const std::string dir = self->pick_folder("Select source folder");
    if (!dir.empty()) {
        self->source_dir_ = dir;
        self->update_location_labels();
    }
}

void MainWindow::on_target_dir_clicked(GtkButton*, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    const std::string dir = self->pick_folder("Select target folder");
    if (!dir.empty()) {
        self->target_dir_ = dir;
        self->update_location_labels();
    }
}

void MainWindow::on_role_changed(GtkComboBox* combo, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    const int active = gtk_combo_box_get_active(combo);
    self->session_role_ = active == 1 ? SessionRole::Sender : SessionRole::Receiver;
    self->update_role_presentation();
}

void MainWindow::on_loopback_clicked(GtkButton*, gpointer user_data) {
    auto* self = static_cast<MainWindow*>(user_data);
    if (self->controller_->is_busy()) {
        return;
    }
    const std::string path = self->pick_open_file();
    if (!path.empty()) {
        self->controller_->loopback_test(path);
    }
}

void MainWindow::schedule_ui_update(const TransferUiState& state) {
    auto* payload = new UiUpdatePayload{this, state};
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                    [](gpointer data) -> gboolean {
                        auto* update = static_cast<UiUpdatePayload*>(data);
                        update->window->apply_ui_state(update->state);
                        return G_SOURCE_REMOVE;
                    },
                    payload,
                    free_ui_update_payload);
}

void MainWindow::apply_ui_state(const TransferUiState& state) {
    set_controls_enabled(!state.busy);

    if (state.waiting_for_sender) {
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress_bar_));
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar_), "Waiting for sender…");
        gtk_label_set_text(GTK_LABEL(throughput_value_label_), "…");
        gtk_label_set_text(GTK_LABEL(throughput_detail_label_), "Listening on EP1");
    } else if (state.bytes_total > 0) {
        const double fraction = static_cast<double>(state.bytes_done)
            / static_cast<double>(state.bytes_total);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar_), fraction);
        const int pct = static_cast<int>((state.bytes_done * 100) / state.bytes_total);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar_),
                                 (std::to_string(pct) + "%").c_str());
        gtk_label_set_text(GTK_LABEL(throughput_value_label_),
                           format_mbps(state.live_mbps).c_str());
        std::ostringstream detail;
        detail << format_gib(state.bytes_done) << " / " << format_gib(state.bytes_total);
        if (state.elapsed_secs > 0.0) {
            detail << "  ·  " << std::fixed << std::setprecision(1)
                   << state.elapsed_secs << " s";
        }
        gtk_label_set_text(GTK_LABEL(throughput_detail_label_), detail.str().c_str());
    } else if (!state.busy) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar_), 0.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar_), "");
        if (state.last_success && state.result_mbps > 0.0) {
            gtk_label_set_text(GTK_LABEL(throughput_value_label_),
                               format_mbps(state.result_mbps).c_str());
            gtk_label_set_text(GTK_LABEL(throughput_detail_label_),
                               format_gib(state.bytes_done).c_str());
        } else if (!state.last_success) {
            gtk_label_set_text(GTK_LABEL(throughput_value_label_), "—");
            gtk_label_set_text(GTK_LABEL(throughput_detail_label_), "Ready");
        }
    }

    if (state.last_success && !state.busy && state.error_message.empty()) {
        gtk_style_context_add_class(gtk_widget_get_style_context(status_label_),
                                    "ces-status-success");
    } else {
        gtk_style_context_remove_class(gtk_widget_get_style_context(status_label_),
                                       "ces-status-success");
    }
    gtk_label_set_text(GTK_LABEL(status_label_), state.status_message.c_str());

    gtk_label_set_text(GTK_LABEL(error_label_),
                       state.error_message.empty() ? "" : state.error_message.c_str());

    if (!state.busy) {
        refresh_connection_panel();
    }
}

void MainWindow::set_controls_enabled(bool enabled) {
    gtk_widget_set_sensitive(receive_button_, enabled);
    gtk_widget_set_sensitive(send_button_, enabled);
    gtk_widget_set_sensitive(role_combo_, enabled);
    gtk_widget_set_sensitive(source_dir_button_, enabled);
    gtk_widget_set_sensitive(target_dir_button_, enabled);
    gtk_widget_set_sensitive(loopback_button_, enabled);
}

void MainWindow::refresh_connection_panel() {
    const std::vector<FabricUsbDevice> present = controller_->list_fabric_devices();

    auto is_pinned_elsewhere = [this](uint8_t bus, uint8_t addr, int skip_slot) {
        for (int slot = 0; slot < 2; ++slot) {
            if (slot == skip_slot || !pinned_links_[slot].pinned) {
                continue;
            }
            if (pinned_links_[slot].bus == bus && pinned_links_[slot].addr == addr) {
                return true;
            }
        }
        return false;
    };

    for (const FabricUsbDevice& dev : present) {
        for (int slot = 0; slot < 2; ++slot) {
            if (pinned_links_[slot].pinned) {
                continue;
            }
            if (is_pinned_elsewhere(dev.bus, dev.addr, slot)) {
                continue;
            }
            pinned_links_[slot].bus = dev.bus;
            pinned_links_[slot].addr = dev.addr;
            pinned_links_[slot].pinned = true;
            break;
        }
    }

    auto is_present = [&present](uint8_t bus, uint8_t addr) {
        for (const FabricUsbDevice& dev : present) {
            if (dev.bus == bus && dev.addr == addr) {
                return true;
            }
        }
        return false;
    };

    // Replug gives a new USB address — re-pin offline slots to unseen devices.
    std::vector<FabricUsbDevice> unmatched;
    for (const FabricUsbDevice& dev : present) {
        bool claimed = false;
        for (int slot = 0; slot < 2; ++slot) {
            const PinnedLink& link = pinned_links_[slot];
            if (link.pinned && link.bus == dev.bus && link.addr == dev.addr) {
                claimed = true;
                break;
            }
        }
        if (!claimed) {
            unmatched.push_back(dev);
        }
    }

    for (int slot = 0; slot < 2 && !unmatched.empty(); ++slot) {
        PinnedLink& link = pinned_links_[slot];
        if (!link.pinned || is_present(link.bus, link.addr)) {
            continue;
        }
        link.bus = unmatched.front().bus;
        link.addr = unmatched.front().addr;
        unmatched.erase(unmatched.begin());
    }

    auto set_link = [&](GtkWidget* label, int slot, const char* link_name) {
        std::ostringstream text;
        const PinnedLink& link = pinned_links_[slot];
        const bool online = link.pinned && is_present(link.bus, link.addr);

        if (!link.pinned) {
            text << "○ " << link_name << "  (waiting for cable)";
        } else if (online) {
            text << "● " << link_name << "  online  (bus "
                 << static_cast<int>(link.bus) << " · addr "
                 << static_cast<int>(link.addr) << ')';
        } else {
            text << "○ " << link_name << "  offline  (bus "
                 << static_cast<int>(link.bus) << " · addr "
                 << static_cast<int>(link.addr) << ')';
        }

        gtk_label_set_text(GTK_LABEL(label), text.str().c_str());
        GtkStyleContext* ctx = gtk_widget_get_style_context(label);
        if (online) {
            gtk_style_context_remove_class(ctx, "ces-port-idle");
            gtk_style_context_add_class(ctx, "ces-port-live");
        } else {
            gtk_style_context_remove_class(ctx, "ces-port-live");
            gtk_style_context_add_class(ctx, "ces-port-idle");
        }
    };

    set_link(port0_status_label_, 0, "Link A");
    set_link(port1_status_label_, 1, "Link B");
}

void MainWindow::update_role_presentation() {
    const bool sender = session_role_ == SessionRole::Sender;
    const char* role_class = sender ? "ces-role-sender" : "ces-role-receiver";
    const char* role_text = sender
        ? "● SENDER — Send after the receiver is waiting"
        : "● RECEIVER — Start Receive first in a two-window demo";

    gtk_label_set_text(GTK_LABEL(role_label_), role_text);
    GtkStyleContext* ctx = gtk_widget_get_style_context(role_label_);
    gtk_style_context_remove_class(ctx, "ces-role-receiver");
    gtk_style_context_remove_class(ctx, "ces-role-sender");
    gtk_style_context_remove_class(ctx, "ces-role-primary");
    gtk_style_context_add_class(ctx, role_class);

    if (drop_zone_label_) {
        gtk_label_set_text(GTK_LABEL(drop_zone_label_),
                           sender ? "Drop file here to send"
                                  : "Drop folder here (target dir)");
    }
    if (drop_zone_sub_label_) {
        gtk_label_set_text(GTK_LABEL(drop_zone_sub_label_),
                           sender ? "Or click Send — receiver must be waiting"
                                  : "Then click Receive — or drop a file to set its parent folder");
    }

    update_demo_hint();
    update_location_labels();
}

void MainWindow::update_demo_hint() {
    std::ostringstream hint;
    hint << "CES demo · port " << port_index_ << " · ";
    if (session_role_ == SessionRole::Receiver) {
        hint << "Set target folder, click Receive first. Partner sends from their window.";
    } else {
        hint << "Drop a file (or Send) after the receiver is waiting.";
    }
    gtk_label_set_text(GTK_LABEL(demo_hint_label_), hint.str().c_str());
}

std::string MainWindow::pick_open_file() {
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Select file to send",
        GTK_WINDOW(window_),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        nullptr);

    if (!source_dir_.empty()) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), source_dir_.c_str());
    }

    if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(dialog);
        return {};
    }

    gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    gtk_widget_destroy(dialog);
    if (!filename) {
        return {};
    }
    std::string path(filename);
    g_free(filename);
    return path;
}

std::string MainWindow::pick_folder(const char* title) {
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        title,
        GTK_WINDOW(window_),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Select", GTK_RESPONSE_ACCEPT,
        nullptr);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(dialog);
        return {};
    }

    gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    gtk_widget_destroy(dialog);
    if (!filename) {
        return {};
    }
    std::string path(filename);
    g_free(filename);
    return path;
}

std::string MainWindow::relative_name_for_send(const std::string& file_path) const {
    if (file_path.empty()) {
        return {};
    }

    if (!source_dir_.empty()) {
        std::string base = source_dir_;
        if (base.back() != '/') {
            base += '/';
        }
        if (file_path.compare(0, base.size(), base) == 0) {
            return file_path.substr(base.size());
        }
    }

    const std::size_t slash = file_path.find_last_of('/');
    if (slash == std::string::npos) {
        return file_path;
    }
    return file_path.substr(slash + 1);
}

void MainWindow::update_location_labels() {
    const std::string prefix = "Port " + std::to_string(port_index_) + " · ";
    const std::string source_text = source_dir_.empty()
        ? prefix + "Source folder: (not set)"
        : prefix + "Source: " + source_dir_;
    const std::string target_text = target_dir_.empty()
        ? prefix + "Target folder: (required to receive)"
        : prefix + "Target: " + target_dir_;
    gtk_label_set_text(GTK_LABEL(source_dir_label_), source_text.c_str());
    gtk_label_set_text(GTK_LABEL(target_dir_label_), target_text.c_str());
}


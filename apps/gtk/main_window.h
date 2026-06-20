#pragma once

#include "transfer_controller.h"
#include "demo_config.h"
#include "session_role.h"

#include <gtk/gtk.h>
#include <memory>
#include <string>
#include <vector>

class MainWindow {
public:
    explicit MainWindow(int port_index, const DemoConfig& config = DemoConfig{});
    ~MainWindow();

    GtkWidget* widget() const { return window_; }

private:
    static void on_destroy(GtkWidget* widget, gpointer user_data);
    static void on_send_clicked(GtkButton* button, gpointer user_data);
    static void on_receive_clicked(GtkButton* button, gpointer user_data);
    static void on_source_dir_clicked(GtkButton* button, gpointer user_data);
    static void on_target_dir_clicked(GtkButton* button, gpointer user_data);
    static void on_loopback_clicked(GtkButton* button, gpointer user_data);
    static void on_role_changed(GtkComboBox* combo, gpointer user_data);
    static gboolean on_refresh_timer(gpointer user_data);
    static gboolean on_drag_motion(GtkWidget* widget,
                                   GdkDragContext* context,
                                   gint x,
                                   gint y,
                                   guint time,
                                   gpointer user_data);
    static void on_drag_leave(GtkWidget* widget, GdkDragContext* context, gpointer user_data);
    static void on_drag_data_received(GtkWidget* widget,
                                      GdkDragContext* context,
                                      gint x,
                                      gint y,
                                      GtkSelectionData* data,
                                      guint info,
                                      guint time,
                                      gpointer user_data);

    static gboolean on_drop_idle(gpointer user_data);

    void build_drop_panel(GtkWidget* root);
    void setup_drag_dest(GtkWidget* widget);
    void set_drop_highlight(bool active);
    void handle_dropped_paths(const std::vector<std::string>& paths);
    void start_send_from_path(const std::string& path);

    GtkWidget* make_panel(GtkWidget* parent, const char* css_class);
    void build_header(GtkWidget* root);
    void build_connection_panel(GtkWidget* root);
    void build_locations_panel(GtkWidget* root);
    void build_throughput_panel(GtkWidget* root);
    void build_controls(GtkWidget* root);
    void build_footer(GtkWidget* root);

    void apply_ui_state(const TransferUiState& state);
    void set_controls_enabled(bool enabled);
    void refresh_connection_panel();
    void update_role_presentation();
    void update_demo_hint();
    std::string pick_open_file();
    std::string pick_folder(const char* title);
    std::string relative_name_for_send(const std::string& file_path) const;
    void update_location_labels();
    void schedule_ui_update(const TransferUiState& state);
    void send_path_on_startup(const std::string& path);

    GtkWidget* window_ = nullptr;
    GtkWidget* role_label_ = nullptr;
    GtkWidget* role_combo_ = nullptr;
    GtkWidget* port_badge_label_ = nullptr;
    GtkWidget* port0_status_label_ = nullptr;
    GtkWidget* port1_status_label_ = nullptr;
    GtkWidget* throughput_value_label_ = nullptr;
    GtkWidget* throughput_detail_label_ = nullptr;
    GtkWidget* send_button_ = nullptr;
    GtkWidget* receive_button_ = nullptr;
    GtkWidget* source_dir_button_ = nullptr;
    GtkWidget* target_dir_button_ = nullptr;
    GtkWidget* source_dir_label_ = nullptr;
    GtkWidget* target_dir_label_ = nullptr;
    GtkWidget* loopback_button_ = nullptr;
    GtkWidget* progress_bar_ = nullptr;
    GtkWidget* status_label_ = nullptr;
    GtkWidget* demo_hint_label_ = nullptr;
    GtkWidget* error_label_ = nullptr;
    GtkWidget* drop_zone_ = nullptr;
    GtkWidget* drop_zone_label_ = nullptr;
    GtkWidget* drop_zone_sub_label_ = nullptr;

    int port_index_ = 0;
    SessionRole session_role_ = SessionRole::Receiver;
    guint refresh_timer_id_ = 0;
    std::unique_ptr<TransferController> controller_;

    struct PinnedLink {
        uint8_t bus = 0;
        uint8_t addr = 0;
        bool pinned = false;
    };
    PinnedLink pinned_links_[2];
    std::string source_dir_;
    std::string target_dir_;
    bool destroyed_ = false;
};

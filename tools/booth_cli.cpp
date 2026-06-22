// Command-line booth peer — mirrors wx GUI session flow with file logging.
//
//   booth-cli listen --port 1
//   booth-cli send --port 0 --to Peer-Name --file /path
//   booth-cli probe  --port 0

#include "booth_log.h"
#include "identity_profile.h"
#include "transfer_orchestrator.h"
#include "usb_protocol.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) {
    g_stop.store(true);
}

struct Args {
    std::string mode;
    int port = 0;
    std::string config;
    std::string peer;
    std::string file;
    std::string note;
    bool wiring_probe = true;
    bool auto_accept = false;
};

void usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " listen --port N --config FILE [--open] [--no-wiring-probe]\n"
        << "  " << argv0 << " send   --port N --config FILE --to PEER --file PATH [--note TEXT]\n"
        << "  " << argv0 << " probe  --port N [--no-wiring-probe]\n"
        << "\nLog file: " << booth_log_path() << " (override: SLSFABRIC_LOG=/path)\n";
}

bool next_value(int argc, char* argv[], int& i, std::string* out) {
    if (i + 1 >= argc) {
        return false;
    }
    *out = argv[++i];
    return true;
}

bool parse_args(int argc, char* argv[], Args& out) {
    if (argc < 2) {
        return false;
    }
    out.mode = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string key = argv[i];
        std::string value;
        if (key == "--port" && next_value(argc, argv, i, &value)) {
            out.port = std::atoi(value.c_str());
        } else if (key == "--config" && next_value(argc, argv, i, &value)) {
            out.config = value;
        } else if (key == "--to" && next_value(argc, argv, i, &value)) {
            out.peer = value;
        } else if (key == "--file" && next_value(argc, argv, i, &value)) {
            out.file = value;
        } else if (key == "--note" && next_value(argc, argv, i, &value)) {
            out.note = value;
        } else if (key == "--no-wiring-probe") {
            out.wiring_probe = false;
        } else if (key == "--open") {
            out.auto_accept = true;
        } else {
            std::cerr << "Unknown argument: " << key << '\n';
            return false;
        }
    }
    return true;
}

std::string state_line(const OrchestratorUiState& state) {
    std::ostringstream out;
    out << "busy=" << state.busy
        << " waiting=" << state.waiting_for_partner
        << " status=\"" << state.status_message << '"';
    if (!state.error_message.empty()) {
        out << " err=\"" << state.error_message << '"';
    }
    if (state.bytes_total > 0) {
        out << " bytes=" << state.bytes_done << '/' << state.bytes_total
            << " mbps=" << state.live_mbps;
    }
    if (!state.notification.empty()) {
        out << " notification=\"" << state.notification << '"';
    }
    if (!state.dev_log.empty()) {
        out << " dev=\"" << state.dev_log << '"';
    }
    return out.str();
}

int run_listen(const Args& args) {
    IdentityProfile identity{};
    if (!load_identity_profile(args.port, args.config, identity)) {
        std::cerr << "Could not load config: " << args.config << '\n';
        return 1;
    }
    if (args.auto_accept) {
        identity.receive_status = ReceiveStatus::Open;
    }

    booth_log(args.port,
              "listen_start",
              "name=" + identity.display_name + " config=" + args.config
                  + " receive=" + receive_status_to_string(identity.receive_status)
                  + " folder=" + identity.receive_folder);

    std::string last_status;
    TransferOrchestrator orchestrator(
        args.port,
        identity,
        [&](const OrchestratorUiState& state) {
            const std::string line = state_line(state);
            if (line != last_status) {
                last_status = line;
                booth_log(args.port, "state", line);
            }
            if (!state.pending_offer && state.notification.empty()) {
                return;
            }
            if (state.pending_offer) {
                booth_log(args.port,
                          "pending_offer",
                          "from=" + state.pending_offer->message.from_name
                              + " file=" + state.pending_offer->message.payload_name);
            }
        });

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    orchestrator.start(args.wiring_probe);

    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    booth_log(args.port, "listen_stop", "");
    orchestrator.stop();
    return 0;
}

int run_send(const Args& args) {
    if (args.peer.empty() || args.file.empty()) {
        std::cerr << "send requires --to and --file\n";
        return 1;
    }

    IdentityProfile identity{};
    if (!load_identity_profile(args.port, args.config, identity)) {
        std::cerr << "Could not load config: " << args.config << '\n';
        return 1;
    }

    booth_log(args.port,
              "send_start",
              "name=" + identity.display_name + " to=" + args.peer + " file=" + args.file);

    bool finished = false;
    bool success = false;
    bool send_started = false;
    std::string last_status;

    TransferOrchestrator orchestrator(
        args.port,
        identity,
        [&](const OrchestratorUiState& state) {
            const std::string line = state_line(state);
            if (line != last_status) {
                last_status = line;
                booth_log(args.port, "state", line);
            }
            if (state.busy || state.waiting_for_partner) {
                send_started = true;
            }
            if (send_started && !state.busy && !state.waiting_for_partner) {
                finished = true;
                if (!state.error_message.empty()) {
                    success = false;
                } else if (state.status_message.find("complete") != std::string::npos) {
                    success = true;
                } else {
                    success = false;
                }
            }
        });

    orchestrator.start(args.wiring_probe);

    const int settle_ms = args.wiring_probe ? (args.port > 0 ? 2500 : 1500) : 400;
    std::this_thread::sleep_for(std::chrono::milliseconds(settle_ms));

    if (!orchestrator.send_to_peer(args.peer, {args.file}, args.note)) {
        booth_log(args.port, "send_rejected", "orchestrator busy or peer missing");
        orchestrator.stop();
        return 1;
    }

    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds(usb_protocol::kHandshakeTimeoutSec + 10);
    while (std::chrono::steady_clock::now() < deadline && !finished) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (!finished) {
        booth_log(args.port,
                  "send_timeout",
                  "no terminal state within "
                      + std::to_string(usb_protocol::kHandshakeTimeoutSec + 10) + "s");
        orchestrator.stop();
        return 2;
    }

    booth_log(args.port, success ? "send_ok" : "send_fail", last_status);
    orchestrator.stop();
    return success ? 0 : 3;
}

int run_probe(const Args& args) {
    booth_log_clear();
    booth_log(args.port, "probe_start", "");

    IdentityProfile identity{};
    identity.display_name = "probe";
    identity.receive_status = ReceiveStatus::Open;

    TransferOrchestrator orchestrator(args.port, identity, [](const OrchestratorUiState& state) {
        if (!state.dev_log.empty()) {
            booth_log(-1, "probe_dev", state.dev_log);
        }
    });

    orchestrator.start(args.wiring_probe);
    const int settle_ms = args.wiring_probe ? 3000 : 500;
    std::this_thread::sleep_for(std::chrono::milliseconds(settle_ms));
    orchestrator.stop();

    booth_log(args.port, "probe_done", "");
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    Args args;
    if (!parse_args(argc, argv, args)) {
        usage(argv[0]);
        return 1;
    }

    if (args.mode == "listen") {
        if (args.config.empty()) {
            std::cerr << "listen requires --config\n";
            return 1;
        }
        booth_log_clear();
        return run_listen(args);
    }
    if (args.mode == "send") {
        if (args.config.empty()) {
            std::cerr << "send requires --config\n";
            return 1;
        }
        return run_send(args);
    }
    if (args.mode == "probe") {
        return run_probe(args);
    }

    usage(argv[0]);
    return 1;
}

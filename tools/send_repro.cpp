// Headless repro for drop->send path (no wx drag-and-drop UI).
#include <rocketbox/sdk.h>
#include "usb_protocol.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
    const int port = (argc >= 2) ? std::atoi(argv[1]) : 1;
    const char* path = (argc >= 3) ? argv[2] : "/tmp/rocketbox-drop-test.dat";

    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "rocketbox drop repro\n";
    }

    auto transport = rocketbox::create_rocketbox_transport(rocketbox::TransportMode::Usb, port + 1);
    transport->connect();
    std::cout << "send_file port=" << port << " path=" << path << '\n';
    auto result = transport->send_file_on_port(port, path, nullptr, 0,
                                               usb_protocol::kFrameKindPayload);
    std::cout << "ok=" << result.ok << " err=" << result.error_message << '\n';
    transport->disconnect();
    return result.ok ? 0 : 2;
}

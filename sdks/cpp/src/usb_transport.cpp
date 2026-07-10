#include "transport.hpp"
#include "usb_endpoints.hpp"

#include <libusb-1.0/libusb.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace rocketbox {
namespace detail {

class UsbTransport final : public ITransport {
public:
    ~UsbTransport() override { stop(); }

    void start(ControlHandler on_ctrl, DataHandler on_data, DetachHandler on_detach) override {
        on_ctrl_ = std::move(on_ctrl);
        on_data_ = std::move(on_data);
        on_detach_ = std::move(on_detach);

        if (libusb_init(&ctx_) != 0) throw std::runtime_error("libusb_init failed");
        handle_ = libusb_open_device_with_vid_pid(ctx_, USB_VID, USB_PID);
        if (!handle_) {
            libusb_exit(ctx_);
            ctx_ = nullptr;
            throw std::runtime_error("RocketBox USB device not found");
        }
        if (libusb_kernel_driver_active(handle_, USB_INTERFACE) == 1) {
            libusb_detach_kernel_driver(handle_, USB_INTERFACE);
        }
        if (libusb_claim_interface(handle_, USB_INTERFACE) != 0) {
            close_device();
            throw std::runtime_error("Failed to claim USB interface");
        }
        eps_ = discover_usb_endpoints(handle_);
        libusb_clear_halt(handle_, eps_.ep1_out);
        libusb_clear_halt(handle_, eps_.ep2_in);
        libusb_clear_halt(handle_, eps_.ep3_in);
        libusb_clear_halt(handle_, eps_.ep4_out);

        stop_ = false;
        ep2_thread_ = std::thread([this] { read_ep2(); });
        ep3_thread_ = std::thread([this] { read_ep3(); });
    }

    void stop() override {
        stop_ = true;
        close_device();
        if (ep2_thread_.joinable()) ep2_thread_.join();
        if (ep3_thread_.joinable()) ep3_thread_.join();
    }

    void write_ep1(const std::vector<uint8_t>& data) override {
        std::vector<uint8_t> packet(4 + data.size());
        write_u32_be(packet.data(), static_cast<uint32_t>(data.size()));
        if (!data.empty()) std::memcpy(packet.data() + 4, data.data(), data.size());
        bulk_out(eps_.ep1_out, packet.data(), packet.size());
    }

    void write_ep4(const std::vector<uint8_t>& packet) override {
        const uint8_t* wire = packet.data();
        size_t len = packet.size();
        if (len > 0 && packet[0] == 0x04) {
            ++wire;
            --len;
        }
        bulk_out(eps_.ep4_out, wire, len);
    }

private:
    void close_device() {
        if (handle_) {
            libusb_release_interface(handle_, USB_INTERFACE);
            libusb_close(handle_);
            handle_ = nullptr;
        }
        if (ctx_) {
            libusb_exit(ctx_);
            ctx_ = nullptr;
        }
    }

    void bulk_out(uint8_t ep, const uint8_t* data, size_t len) {
        std::lock_guard<std::mutex> lock(out_mu_);
        if (!handle_) throw std::runtime_error("USB detached");
        int transferred = 0;
        int rc = libusb_bulk_transfer(handle_, ep, const_cast<uint8_t*>(data),
                                      static_cast<int>(len), &transferred, USB_TIMEOUT_MS);
        if (rc != 0) throw std::runtime_error(libusb_strerror(static_cast<libusb_error>(rc)));
    }

    bool bulk_in(uint8_t ep, uint8_t* buf, int capacity, int* transferred) {
        if (!handle_) return false;
        int rc = libusb_bulk_transfer(handle_, ep, buf, capacity, transferred, USB_TIMEOUT_MS);
        if (rc == LIBUSB_ERROR_TIMEOUT) {
            *transferred = 0;
            return true;
        }
        return rc == 0;
    }

    void read_ep2() {
        std::vector<uint8_t> acc;
        uint8_t buf[USB_READ_SIZE];
        while (!stop_) {
            int n = 0;
            if (!bulk_in(eps_.ep2_in, buf, USB_READ_SIZE, &n)) {
                if (on_detach_) on_detach_();
                break;
            }
            if (n <= 0) continue;
            append_bytes(acc, buf, n);
            drain_ep2(acc);
        }
    }

    void read_ep3() {
        std::vector<uint8_t> acc;
        uint8_t buf[USB_READ_SIZE];
        while (!stop_) {
            int n = 0;
            if (!bulk_in(eps_.ep3_in, buf, USB_READ_SIZE, &n)) {
                if (on_detach_) on_detach_();
                break;
            }
            if (n <= 0) continue;
            append_bytes(acc, buf, n);
            drain_ep3(acc);
        }
    }

    void drain_ep2(std::vector<uint8_t>& acc) {
        while (acc.size() >= 4) {
            uint32_t len = read_u32_be(acc.data());
            if (acc.size() < 4 + len) return;
            std::vector<uint8_t> payload(acc.begin() + 4,
                                         acc.begin() + 4 + static_cast<std::ptrdiff_t>(len));
            acc.erase(acc.begin(), acc.begin() + 4 + static_cast<std::ptrdiff_t>(len));
            if (on_data_) on_data_(payload);
        }
    }

    void drain_ep3(std::vector<uint8_t>& acc) {
        while (acc.size() >= 12) {
            auto h = parse_control_header(acc.data());
            if (acc.size() < 12 + h.len) return;
            std::vector<uint8_t> payload(acc.begin() + 12,
                                         acc.begin() + 12 + static_cast<std::ptrdiff_t>(h.len));
            acc.erase(acc.begin(), acc.begin() + 12 + static_cast<std::ptrdiff_t>(h.len));
            if (on_ctrl_) on_ctrl_(h, payload);
        }
    }

    libusb_context* ctx_ = nullptr;
    libusb_device_handle* handle_ = nullptr;
    UsbEndpoints eps_;
    std::atomic<bool> stop_{false};
    std::thread ep2_thread_;
    std::thread ep3_thread_;
    std::mutex out_mu_;
    ControlHandler on_ctrl_;
    DataHandler on_data_;
    DetachHandler on_detach_;
};

std::unique_ptr<ITransport> make_usb_transport() {
    return std::make_unique<UsbTransport>();
}

}  // namespace detail
}  // namespace rocketbox

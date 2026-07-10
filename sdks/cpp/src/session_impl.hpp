#pragma once

#include "connection_impl.hpp"
#include "rocketbox/sdk.h"
#include "transport.hpp"

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace rocketbox {
namespace detail {

class SessionImpl : public Session {
public:
    SessionImpl(TransportType transport, int port);
    ~SessionImpl() override;

    void init();

    std::string GetSystemId() override { return system_id_; }
    std::vector<SystemInfo> ListSystems() override;
    std::shared_ptr<Connection> Connect(const std::string& targetSystemId) override;

    void OnSystemsChanged(std::function<void(const std::vector<SystemInfo>&)> cb) override;
    void OnIncomingCircuit(std::function<void(std::shared_ptr<Connection>)> cb) override;
    void OnDetached(std::function<void()> cb) override;

private:
    struct TxnWait {
        bool resolved = false;
        ControlHeader header;
        std::vector<uint8_t> payload;
    };

    void handle_control(const ControlHeader& h, const std::vector<uint8_t>& payload);
    void handle_data(const std::vector<uint8_t>& payload);
    void handle_detach();
    void send_attach();
    void disconnect_circuit();
    std::shared_ptr<TxnWait> send_control(uint8_t type, uint32_t arg,
                                          const std::vector<uint8_t>& payload);
    std::vector<SystemInfo> parse_systems(const std::vector<uint8_t>& payload);

    TransportType transport_type_;
    int port_;
    std::unique_ptr<ITransport> transport_;
    std::string system_id_;

    std::mutex mu_;
    std::condition_variable cv_;
    uint16_t next_txn_ = 1;
    std::map<uint16_t, std::shared_ptr<TxnWait>> txn_waits_;
    std::shared_ptr<ConnectionImpl> active_connection_;

    std::function<void(const std::vector<SystemInfo>&)> on_systems_changed_;
    std::function<void(std::shared_ptr<Connection>)> on_incoming_circuit_;
    std::function<void()> on_detached_;
};

}  // namespace detail
}  // namespace rocketbox

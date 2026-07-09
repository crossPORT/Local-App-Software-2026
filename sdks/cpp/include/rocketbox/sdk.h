#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace rocketbox {

struct SystemInfo {
    std::string id;
    std::string name;
    std::string status; // "reachable", "busy", "offline"
};

class Connection {
public:
    virtual ~Connection() = default;
    virtual std::string GetPeerSystemId() = 0;
    virtual std::string GetState() = 0; // "open", "closing", "closed"
    virtual void Send(const std::vector<uint8_t>& bytes) = 0;
    virtual void SendMessage(const std::vector<uint8_t>& message) = 0;
    virtual void Close() = 0;

    // Callbacks
    virtual void OnReceived(std::function<void(const std::vector<uint8_t>&)> cb) = 0;
    virtual void OnMessageReceived(std::function<void(const std::vector<uint8_t>&)> cb) = 0;
    virtual void OnClosed(std::function<void(const std::string&)> cb) = 0;
};

class Session {
public:
    virtual ~Session() = default;
    virtual std::string GetSystemId() = 0;
    virtual std::vector<SystemInfo> ListSystems() = 0;
    virtual std::shared_ptr<Connection> Connect(const std::string& targetSystemId) = 0;
    
    // Callbacks
    virtual void OnSystemsChanged(std::function<void(const std::vector<SystemInfo>&)> cb) = 0;
    virtual void OnIncomingCircuit(std::function<void(std::shared_ptr<Connection>)> cb) = 0;
    virtual void OnDetached(std::function<void()> cb) = 0;
};

enum class TransportType {
    Usb,
    Sim
};

std::unique_ptr<Session> Attach(TransportType transport, int port = 1);

} // namespace rocketbox

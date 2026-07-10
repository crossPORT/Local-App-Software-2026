#include "dialer.hpp"
#include "peer_map.hpp"

#include <iostream>
#include <thread>

CircuitDialer::CircuitDialer(rocketbox::Session& session, int local_port)
    : session_(session), local_port_(local_port) {
  last_activity_ = std::chrono::steady_clock::now();
  session_.OnIncomingCircuit([this](std::shared_ptr<rocketbox::Connection> c) {
    if (stop_ || !c) {
      return;
    }
    int peer = 0;
    const std::string id = c->GetPeerSystemId();
    if (id.rfind("sys-port-", 0) == 0) {
      try {
        peer = std::stoi(id.substr(9));
      } catch (...) {
        peer = 0;
      }
    }
    std::cerr << "[rocketbox-tunnel] incoming from " << id << std::endl;
    bind_conn(std::move(c), peer);
  });
}

CircuitDialer::~CircuitDialer() { shutdown(); }

void CircuitDialer::shutdown() {
  stop_ = true;
  std::shared_ptr<rocketbox::Connection> c;
  {
    std::lock_guard<std::mutex> lock(mu_);
    c = std::move(conn_);
    active_peer_port_ = 0;
  }
  if (c) {
    try {
      c->Close();
    } catch (...) {
    }
  }
}

void CircuitDialer::on_connection(ConnHandler handler) {
  std::shared_ptr<rocketbox::Connection> cur;
  ConnHandler h;
  {
    std::lock_guard<std::mutex> lock(mu_);
    on_conn_ = std::move(handler);
    h = on_conn_;
    cur = conn_;
  }
  if (cur && h) {
    h(cur);
  }
}

void CircuitDialer::note_activity() {
  std::lock_guard<std::mutex> lock(mu_);
  last_activity_ = std::chrono::steady_clock::now();
}

void CircuitDialer::tick_idle() {
  std::shared_ptr<rocketbox::Connection> c;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!conn_ || stop_) {
      return;
    }
    const auto idle = std::chrono::steady_clock::now() - last_activity_;
    if (idle < std::chrono::seconds(fabric_lan::kIdleDisconnectSec)) {
      return;
    }
    std::cerr << "[rocketbox-tunnel] idle release peer port " << active_peer_port_ << std::endl;
    c = std::move(conn_);
    active_peer_port_ = 0;
  }
  if (c) {
    try {
      c->Close();
    } catch (...) {
    }
  }
}

void CircuitDialer::clear_conn(const std::string& reason) {
  std::cerr << "[rocketbox-tunnel] circuit closed: " << reason << std::endl;
  std::lock_guard<std::mutex> lock(mu_);
  conn_.reset();
  active_peer_port_ = 0;
}

void CircuitDialer::bind_conn(std::shared_ptr<rocketbox::Connection> conn, int peer_port) {
  std::shared_ptr<rocketbox::Connection> old;
  ConnHandler notify;
  std::shared_ptr<rocketbox::Connection> active;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (conn_ && conn_ != conn) {
      old = std::move(conn_);
    }
    conn_ = std::move(conn);
    active = conn_;
    active_peer_port_ = peer_port;
    last_activity_ = std::chrono::steady_clock::now();
    notify = on_conn_;
  }
  if (old) {
    try {
      old->Close();
    } catch (...) {
    }
  }
  if (!active) {
    return;
  }
  active->OnClosed([this](const std::string& reason) { clear_conn(reason); });
  if (notify) {
    notify(active);
  }
}

std::shared_ptr<rocketbox::Connection> CircuitDialer::ensure(int dest_port) {
  if (dest_port < 1 || dest_port > 4 || dest_port == local_port_ || stop_) {
    return nullptr;
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    if (conn_ && active_peer_port_ == dest_port && conn_->GetState() == "open") {
      last_activity_ = std::chrono::steady_clock::now();
      return conn_;
    }
  }

  std::shared_ptr<rocketbox::Connection> old;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (conn_) {
      old = std::move(conn_);
      active_peer_port_ = 0;
    }
  }
  if (old) {
    std::cerr << "[rocketbox-tunnel] switching peer" << std::endl;
    try {
      old->Close();
    } catch (...) {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  const std::string target = fabric_lan::system_id_for_port(dest_port);
  std::cerr << "[rocketbox-tunnel] connect " << target << std::endl;
  try {
    auto conn = session_.Connect(target);
    bind_conn(conn, dest_port);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    return conn;
  } catch (const std::exception& e) {
    std::cerr << "[rocketbox-tunnel] connect failed: " << e.what() << std::endl;
    return nullptr;
  }
}

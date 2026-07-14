#include "host_gateway_wfp.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <fwpmu.h>
#include <ws2tcpip.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "rpcrt4.lib")

namespace {

// {A7C5E8B1-4D2F-4A91-9C3E-1B2A3C4D5E6F}
const GUID kSublayer = {0xa7c5e8b1, 0x4d2f, 0x4a91, {0x9c, 0x3e, 0x1b, 0x2a, 0x3c, 0x4d, 0x5e, 0x6f}};

UINT32 fabric_host(int port) {
  return (10u << 24) | (64u << 16) | (0u << 8) | static_cast<UINT32>(port);
}

void throw_fw(const char* what, DWORD err) {
  throw std::runtime_error(std::string(what) + ": " + std::to_string(err));
}

struct CondBuf {
  FWP_V4_ADDR_AND_MASK addr{};
  std::array<FWPM_FILTER_CONDITION0, 3> conds{};
};

UINT64 add_filter(HANDLE engine, const GUID& layer, FWP_ACTION_TYPE action, UINT8 weight,
                  CondBuf& buf, UINT32 ncond, const wchar_t* name) {
  FWPM_FILTER0 f{};
  f.layerKey = layer;
  f.subLayerKey = kSublayer;
  f.displayData.name = const_cast<wchar_t*>(name);
  f.action.type = action;
  f.weight.type = FWP_UINT8;
  f.weight.uint8 = weight;
  f.numFilterConditions = ncond;
  f.filterCondition = buf.conds.data();
  UINT64 id = 0;
  const DWORD e = FwpmFilterAdd0(engine, &f, nullptr, &id);
  if (e != ERROR_SUCCESS) throw_fw("FwpmFilterAdd0", e);
  return id;
}

std::unique_ptr<CondBuf> make_local(int port) {
  auto b = std::make_unique<CondBuf>();
  b->addr.addr = fabric_host(port);
  b->addr.mask = 0xffffffffu;
  b->conds[0].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
  b->conds[0].matchType = FWP_MATCH_EQUAL;
  b->conds[0].conditionValue.type = FWP_V4_ADDR_MASK;
  b->conds[0].conditionValue.v4AddrMask = &b->addr;
  return b;
}

std::unique_ptr<CondBuf> make_remote_fabric() {
  auto b = std::make_unique<CondBuf>();
  b->addr.addr = (10u << 24) | (64u << 16);  // 10.64.0.0
  b->addr.mask = 0xffffff00u;                // /24
  b->conds[0].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
  b->conds[0].matchType = FWP_MATCH_EQUAL;
  b->conds[0].conditionValue.type = FWP_V4_ADDR_MASK;
  b->conds[0].conditionValue.v4AddrMask = &b->addr;
  return b;
}

}  // namespace

struct WfpExposeSession {
  HANDLE engine = nullptr;
  int port = 0;
  std::vector<UINT64> ids;
  // Keep condition storage alive for the life of each added filter.
  std::vector<std::unique_ptr<CondBuf>> cond_bufs;
};

WfpExposeSession* wfp_expose_open(int local_port) {
  auto* s = new WfpExposeSession();
  s->port = local_port;
  DWORD e = FwpmEngineOpen0(nullptr, RPC_C_AUTHN_WINNT, nullptr, nullptr, &s->engine);
  if (e != ERROR_SUCCESS) {
    delete s;
    throw_fw("FwpmEngineOpen0", e);
  }
  FWPM_SUBLAYER0 sub{};
  sub.subLayerKey = kSublayer;
  sub.displayData.name = const_cast<wchar_t*>(L"RocketBox expose");
  // High weight so ICMP permits beat default Windows Firewall on Wintun.
  sub.weight = 0xffff;
  (void)FwpmSubLayerDeleteByKey0(s->engine, &kSublayer);
  e = FwpmSubLayerAdd0(s->engine, &sub, nullptr);
  if (e != ERROR_SUCCESS && e != FWP_E_ALREADY_EXISTS) {
    FwpmEngineClose0(s->engine);
    delete s;
    throw_fw("FwpmSubLayerAdd0", e);
  }
  return s;
}

void wfp_expose_close(WfpExposeSession* s) {
  if (!s) return;
  if (s->engine) {
    for (UINT64 id : s->ids) FwpmFilterDeleteById0(s->engine, id);
    s->ids.clear();
    s->cond_bufs.clear();
    FwpmEngineClose0(s->engine);
    s->engine = nullptr;
  }
  delete s;
}

void wfp_expose_apply(WfpExposeSession* s, const std::vector<ExposeRule>& expose) {
  if (!s || !s->engine) return;
  const DWORD te = FwpmTransactionBegin0(s->engine, 0);
  if (te != ERROR_SUCCESS) throw_fw("FwpmTransactionBegin0", te);
  try {
    for (UINT64 id : s->ids) FwpmFilterDeleteById0(s->engine, id);
    s->ids.clear();
    s->cond_bufs.clear();

    auto add_owned = [&](const GUID& layer, FWP_ACTION_TYPE action, UINT8 weight,
                         std::unique_ptr<CondBuf> buf, UINT32 ncond, const wchar_t* name) {
      s->ids.push_back(add_filter(s->engine, layer, action, weight, *buf, ncond, name));
      s->cond_bufs.push_back(std::move(buf));
    };

    auto add_icmp = [&](const GUID& layer, std::unique_ptr<CondBuf> b, const wchar_t* name) {
      b->conds[1].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
      b->conds[1].matchType = FWP_MATCH_EQUAL;
      b->conds[1].conditionValue.type = FWP_UINT8;
      b->conds[1].conditionValue.uint8 = IPPROTO_ICMP;
      add_owned(layer, FWP_ACTION_PERMIT, 15, std::move(b), 2, name);
    };
    // ALE + transport so kernel ICMP on Wintun is not dropped by Firewall.
    add_icmp(FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, make_local(s->port), L"RocketBox ICMP in");
    add_icmp(FWPM_LAYER_INBOUND_TRANSPORT_V4, make_local(s->port), L"RocketBox ICMP in xport");
    add_icmp(FWPM_LAYER_ALE_AUTH_CONNECT_V4, make_remote_fabric(), L"RocketBox ICMP out");
    add_icmp(FWPM_LAYER_OUTBOUND_TRANSPORT_V4, make_remote_fabric(), L"RocketBox ICMP out xport");

    for (const ExposeRule& r : expose) {
      if (r.port <= 0 || r.port > 65535) continue;
      auto add_port = [&](UINT8 proto, bool on) {
        if (!on) return;
        auto b = make_local(s->port);
        b->conds[1].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
        b->conds[1].matchType = FWP_MATCH_EQUAL;
        b->conds[1].conditionValue.type = FWP_UINT8;
        b->conds[1].conditionValue.uint8 = proto;
        b->conds[2].fieldKey = FWPM_CONDITION_IP_LOCAL_PORT;
        b->conds[2].matchType = FWP_MATCH_EQUAL;
        b->conds[2].conditionValue.type = FWP_UINT16;
        b->conds[2].conditionValue.uint16 = static_cast<UINT16>(r.port);
        add_owned(FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, FWP_ACTION_PERMIT, 14, std::move(b), 3,
                  L"RocketBox expose port");
      };
      add_port(IPPROTO_TCP, r.tcp);
      add_port(IPPROTO_UDP, r.udp);
    }

    for (UINT8 proto : {static_cast<UINT8>(IPPROTO_TCP), static_cast<UINT8>(IPPROTO_UDP)}) {
      auto b = make_local(s->port);
      b->conds[1].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
      b->conds[1].matchType = FWP_MATCH_EQUAL;
      b->conds[1].conditionValue.type = FWP_UINT8;
      b->conds[1].conditionValue.uint8 = proto;
      add_owned(FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, FWP_ACTION_BLOCK, 8, std::move(b), 2,
                L"RocketBox block");
    }

    const DWORD ce = FwpmTransactionCommit0(s->engine);
    if (ce != ERROR_SUCCESS) throw_fw("FwpmTransactionCommit0", ce);
  } catch (...) {
    FwpmTransactionAbort0(s->engine);
    throw;
  }
}

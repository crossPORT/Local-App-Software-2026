#include "host_gateway_wfp.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <fwpmu.h>
#include <ws2tcpip.h>

#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "rpcrt4.lib")

namespace {

// {A7C5E8B1-4D2F-4A91-9C3E-1B2A3C4D5E6F}
const GUID kSublayer = {0xa7c5e8b1, 0x4d2f, 0x4a91, {0x9c, 0x3e, 0x1b, 0x2a, 0x3c, 0x4d, 0x5e, 0x6f}};

UINT32 fabric_addr(int port) {
  return (10u << 24) | (64u << 16) | (0u << 8) | static_cast<UINT32>(port);
}

void throw_fw(const char* what, DWORD err) {
  throw std::runtime_error(std::string(what) + ": " + std::to_string(err));
}

UINT64 add_filter(HANDLE engine, const GUID& layer, FWP_ACTION_TYPE action, UINT8 weight,
                  const FWPM_FILTER_CONDITION0* conds, UINT32 ncond, const wchar_t* name) {
  FWPM_FILTER0 f{};
  f.layerKey = layer;
  f.subLayerKey = kSublayer;
  f.displayData.name = const_cast<wchar_t*>(name);
  f.action.type = action;
  f.weight.type = FWP_UINT8;
  f.weight.uint8 = weight;
  f.numFilterConditions = ncond;
  f.filterCondition = const_cast<FWPM_FILTER_CONDITION0*>(conds);
  UINT64 id = 0;
  const DWORD e = FwpmFilterAdd0(engine, &f, nullptr, &id);
  if (e != ERROR_SUCCESS) throw_fw("FwpmFilterAdd0", e);
  return id;
}

}  // namespace

struct WfpExposeSession {
  HANDLE engine = nullptr;
  int port = 0;
  std::vector<UINT64> ids;
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
  sub.weight = 0x100;
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

    FWP_V4_ADDR_AND_MASK addr{};
    addr.addr = fabric_addr(s->port);
    addr.mask = 0xffffffff;

    FWPM_FILTER_CONDITION0 conds[3]{};
    conds[0].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
    conds[0].matchType = FWP_MATCH_EQUAL;
    conds[0].conditionValue.type = FWP_V4_ADDR_MASK;
    conds[0].conditionValue.v4AddrMask = &addr;

    // ICMP echo to fabric IP always permitted.
    conds[1].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
    conds[1].matchType = FWP_MATCH_EQUAL;
    conds[1].conditionValue.type = FWP_UINT8;
    conds[1].conditionValue.uint8 = IPPROTO_ICMP;
    s->ids.push_back(add_filter(s->engine, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, FWP_ACTION_PERMIT, 15,
                                conds, 2, L"RocketBox ICMP"));

    for (const ExposeRule& r : expose) {
      if (r.port <= 0 || r.port > 65535) continue;
      auto add_port = [&](UINT8 proto, bool on) {
        if (!on) return;
        conds[1].conditionValue.uint8 = proto;
        conds[2].fieldKey = FWPM_CONDITION_IP_LOCAL_PORT;
        conds[2].matchType = FWP_MATCH_EQUAL;
        conds[2].conditionValue.type = FWP_UINT16;
        conds[2].conditionValue.uint16 = static_cast<UINT16>(r.port);
        s->ids.push_back(add_filter(s->engine, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, FWP_ACTION_PERMIT,
                                    14, conds, 3, L"RocketBox expose port"));
      };
      add_port(IPPROTO_TCP, r.tcp);
      add_port(IPPROTO_UDP, r.udp);
    }

    // Block remaining TCP/UDP to fabric IP (empty allowlist => host services closed).
    for (UINT8 proto : {static_cast<UINT8>(IPPROTO_TCP), static_cast<UINT8>(IPPROTO_UDP)}) {
      conds[1].conditionValue.uint8 = proto;
      s->ids.push_back(add_filter(s->engine, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, FWP_ACTION_BLOCK, 8,
                                  conds, 2, L"RocketBox block"));
    }

    const DWORD ce = FwpmTransactionCommit0(s->engine);
    if (ce != ERROR_SUCCESS) throw_fw("FwpmTransactionCommit0", ce);
  } catch (...) {
    FwpmTransactionAbort0(s->engine);
    throw;
  }
}

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>

#include "log.h"
#include "RIP.h"

constexpr int RIP::UDP_PORT = 520;
constexpr int RIP::ADDRESS_FAMILY = 2;
constexpr int RIP::METRIC_INF = 16;

RIP::RIP(UDP &udp_, NetworkLayer &network_, NetBase &netBase_)
    : udp(udp_), network(network_), netBase(netBase_), updateCycle(30),
      isUp(false), expireCycle(180), cleanCycle(120), udpHandler(*this),
      loopHandler(*this) {}

int RIP::setup() {
  if (isUp) {
    ERRLOG("RIP is already running.\n");
    return 1;
  }
  isUp = true;
  udp.addRecvCallback(&udpHandler);
  netBase.addLoopCallback(&loopHandler);
  return 0;
}

IP::Routing::HopInfo RIP::match(const Addr &addr) {
  if (table.count(addr)) {
    auto &&e = table[addr];
    return HopInfo{device : e.device, dstMAC : e.dstMAC};
  }
  return HopInfo{device : nullptr, dstMAC : {}};
}

int RIP::sendUpdate() {
  time_t curTime = time(nullptr);

  UDP::NetworkLayer::Addr srcAddr;
  if (udp.network.getAnyAddr(nullptr, srcAddr) < 0) {
    ERRLOG("No IP address on the host.\n");
    return -1;
  }

  const auto &netAddrs = network.getAddrs();
  for (auto &&e : netAddrs)
    table[e.addr] = TabEntry{
      device : e.device,
      dstMAC : e.device->addr,
      metric : 0,
      expireTime : curTime + updateCycle * 10
    };

  int dataLen = sizeof(Header) + sizeof(DataEntry) * (int)table.size();
  void *buf = malloc(dataLen);
  if (!buf) {
    ERRLOG("malloc error: %s\n", strerror(errno));
    return -1;
  }

  Header &header = *(Header *)buf;
  header = {command : 2, version : 0, zero : 0};
  DataEntry *p = (DataEntry *)(&header + 1);
  for (auto &&e : table) {
    *p++ = DataEntry{
      addressFamily : htons(ADDRESS_FAMILY),
      zero0 : 0,
      address : e.first,
      zero1 : 0,
      zero2 : 0,
      metric : htonl(e.second.metric)
    };
  }

  int rc = udp.sendSegment(buf, dataLen, srcAddr, UDP_PORT,
                           NetworkLayer::Addr::BROADCAST, UDP_PORT);
  free(buf);
  return rc;
}

const RIP::Table &RIP::getTable() {
  return table;
}

RIP::UDPHandler::UDPHandler(RIP &rip_)
    : rip(rip_), UDP::RecvCallback(UDP_PORT) {}

int RIP::UDPHandler::handle(const void *buf, int len, const Info &info) {
  const auto &udpHeader = *(const UDP::Header *)buf;
  const void *payload = &udpHeader + 1;
  Header requriedHeader{command : 2, version : 0, zero : 0};
  if (memcmp(payload, &requriedHeader, sizeof(Header)) != 0) {
    fprintf(stderr, "Invalid RIP header.\n");
    return 1;
  }
  const DataEntry *recvEnts = (const DataEntry *)((const Header *)payload + 1);
  int entriesLen =
      ntohs(udpHeader.length) - sizeof(UDP::Header) - sizeof(Header);
  int nEntries = entriesLen / sizeof(DataEntry);

  bool realUpdated = false;

  time_t curTime = time(nullptr);
  for (int i = 0; i < nEntries; i++) {
    const auto &e = recvEnts[i];
    if (ntohs(e.addressFamily) != ADDRESS_FAMILY)
      continue;

    int metric = ntohl(e.metric) + 1;
    if (metric > METRIC_INF)
      continue;

    // Ignore the local host.
    if (rip.network.findDeviceByAddr(e.address))
      continue;

    bool updateEnt = false;
    if (!rip.table.count(e.address)) {
      updateEnt = true;
      realUpdated = true;
    } else {
      auto &&r = rip.table[e.address];
      if ((r.metric != 0 && r.device == info.linkDevice) ||
          metric <= r.metric) {
        updateEnt = true;
        if (metric != r.metric)
          realUpdated = true;
      }
    }

    if (updateEnt)
      rip.table[e.address] = TabEntry{
        device : info.linkDevice,
        dstMAC : info.linkHeader->src,
        metric : metric,
        expireTime : curTime + rip.expireCycle
      };
  }

  // Triggered updates
  if (realUpdated)
    rip.sendUpdate();

  return 0;
}

RIP::LoopHandler::LoopHandler(RIP &rip_) : rip(rip_), updateTime(0) {}

int RIP::LoopHandler::handle() {
  time_t curTime = time(nullptr);
  for (auto it = rip.table.begin(); it != rip.table.end(); ) {
    auto &e = *it;
    if (curTime > e.second.expireTime)
      e.second.metric = METRIC_INF;
    if (curTime - e.second.expireTime > rip.cleanCycle)
      it = rip.table.erase(it);
    else
      it++;
  }
  if (curTime >= updateTime) {
    updateTime = curTime + rip.updateCycle;
    rip.sendUpdate();
  }
  return 0;
}

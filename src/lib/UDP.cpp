#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <arpa/inet.h>

#include "utils.h"
#include "log.h"

#include "UDP.h"

constexpr int UDP::PROTOCOL_ID = 17;

UDP::UDP(L3 &l3_) : l3(l3_) {}

int UDP::sendSegment(const void *data, int dataLen, L3::Addr srcAddr,
                     int srcPort, L3::Addr dstAddr, int dstPort) {
  int segLen = sizeof(Header) + dataLen;

  if (dataLen < 0 || (segLen >> 16) != 0) {
    ERRLOG("Invalid UDP data length: %d\n", dataLen);
    return -1;
  }
  if ((srcPort >> 16) != 0 || (dstPort >> 16) != 0) {
    ERRLOG("Invalid UDP port: %d - %d\n", srcPort, dstPort);
    return -1;
  }

  void *seg = malloc(segLen);
  if (!seg) {
    ERRLOG("malloc error: %s\n", strerror(errno));
    return -1;
  }
  PseudoL3Header pseudo{
    srcAddr : srcAddr,
    dstAddr : dstAddr,
    zero : 0,
    protocol : PROTOCOL_ID,
    udpLength : htons(segLen)
  };
  auto &header = *(Header *)seg;
  header = Header{
    srcPort : htons(srcPort),
    dstPort : htons(dstPort),
    length : htons(segLen),
    checksum : 0
  };
  memcpy(&header + 1, data, dataLen);
  header.checksum = csum16(seg, segLen, ~csum16(&pseudo, sizeof(pseudo)));
#ifdef NETSTACK_DEBUG
  assert(csum16(seg, segLen, ~csum16(&pseudo, sizeof(pseudo))) == 0);
#endif

  return l3.send(seg, segLen, srcAddr, dstAddr, PROTOCOL_ID,
                 {.autoRetry = true, .freeBuf = true});
}

void UDP::addOnRecv(RecvHandler handler, uint16_t port) {
  onRecv.insert({port, handler});
}

int UDP::setup() {
  l3.addOnRecv(
      [this](auto &&...args) -> int {
        handleRecv(args...);
        return 0;
      },
      PROTOCOL_ID);
  return 0;
}

static uint16_t calcUdpChecksum(const UDP::PseudoL3Header &pseudoHeader,
                                const void *data, int len) {
  const uint8_t *d0 = (const uint8_t *)&pseudoHeader;
  const uint8_t *d1 = (const uint8_t *)data;
  uint32_t sum = 0;
  for (int i = 0; i + 1 < sizeof(UDP::PseudoL3Header); i += 2) {
    uint16_t x = ((uint16_t)d0[i] << 8 | d0[i + 1]);
    sum += x;
    sum = (sum + (sum >> 16)) & 0xFFFF;
  }
  for (int i = 0; i + 1 < len; i += 2) {
    uint16_t x = ((uint16_t)d1[i] << 8 | d1[i + 1]);
    sum += x;
    sum = (sum + (sum >> 16)) & 0xFFFF;
  }
  if (len % 2 != 0) {
    sum += (uint16_t)d1[len - 1] << 8;
    sum = (sum + (sum >> 16)) & 0xFFFF;
  }
  return htons(~sum);
}

void UDP::handleRecv(const void *seg, size_t segLen, const L3::RecvInfo &info) {
  if (segLen < sizeof(Header)) {
    LOG_INFO("Truncated UDP header: %lu/%lu", segLen, sizeof(Header));
    return;
  }
  const Header &header = *(const Header *)seg;
  if (segLen != ntohs(header.length)) {
    LOG_INFO("Invalid UDP packet length: %lu/%hu", segLen, header.length);
    return;
  }
  PseudoL3Header pseudoHeader{.srcAddr = info.header->src,
                              .dstAddr = info.header->dst,
                              .zero = 0,
                              .protocol = info.header->protocol,
                              .udpLength = header.length};
  if (header.checksum != 0 && calcUdpChecksum(pseudoHeader, seg, segLen) != 0) {
    LOG_INFO("UDP Checksum error");
    return;
  }

  const void *data = &header + 1;
  size_t dataLen = segLen - sizeof(Header);
  UDP::RecvInfo newInfo{.l3 = info, .udpHeader = &header};

  auto r = onRecv.equal_range(ntohs(header.dstPort));
  for (auto it = r.first; it != r.second;) {
    if (it->second(data, dataLen, newInfo) == 1)
      it = onRecv.erase(it);
    else
      it++;
  }
}

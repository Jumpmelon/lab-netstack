#include <cstring>
#include <cstdlib>
#include <cerrno>

#include <pcap/pcap.h>

#include <linux/if_arp.h>
#include <linux/if_packet.h>

#include "log.h"

#include "Ethernet.h"

constexpr int Ethernet::LINK_TYPE = DLT_EN10MB;

constexpr Ethernet::Addr Ethernet::Addr::BROADCAST{0xFF, 0xFF, 0xFF,
                                                   0xFF, 0xFF, 0xFF};

Ethernet::Ethernet(NetBase &netBase_)
    : netBase(netBase_), netBaseHandler(*this) {}

Ethernet::Device::Device(pcap_t *p_, const char *name_, const Addr &addr_)
    : NetBase::Device(p_, name_, LINK_TYPE), addr(addr_) {}

Ethernet::Device *Ethernet::addDeviceByName(const char *name) {
  if (netBase.findDeviceByName(name)) {
    ERRLOG("Duplicated device: %s\n", name);
    return nullptr;
  }

  int rc;
  char errbuf[PCAP_ERRBUF_SIZE];
  bool found = false;
  Addr addr;

  pcap_if_t *alldevs;
  rc = pcap_findalldevs(&alldevs, errbuf);
  if (rc != 0) {
    ERRLOG("pcap_findalldevs error: %s\n", errbuf);
    return nullptr;
  }
  for (auto *d = alldevs; d; d = d->next)
    if (strcmp(d->name, name) == 0) {
      for (auto *a = d->addresses; a; a = a->next)
        if (a->addr && a->addr->sa_family == AF_PACKET) {
          sockaddr_ll *s = (sockaddr_ll *)a->addr;
          if (s->sll_hatype == ARPHRD_ETHER) {
            memcpy(&addr, s->sll_addr, sizeof(addr));
            found = true;
            break;
          }
        }
      if (!found)
        ERRLOG("No Ethernet address: %s\n", name);
      if (found)
        break;
    }
  pcap_freealldevs(alldevs);

  if (!found) {
    ERRLOG("No such Ethernet device: %s\n", name);
    return nullptr;
  }

  pcap_t *p = pcap_create(name, errbuf);
  Ethernet::Device *d;
  if (!p) {
    ERRLOG("pcap_create(device %s) error: %s\n", name, errbuf);
    return nullptr;
  }
  rc = pcap_set_immediate_mode(p, 1);
  if (rc != 0) {
    ERRLOG("pcap_set_immediate(device %s) error: %s\n", name, pcap_geterr(p));
    goto CLOSE;
  }
  rc = pcap_setnonblock(p, 1, errbuf);
  if (rc != 0) {
    ERRLOG("pcap_setnonblock(device %s) error: %s\n", name, errbuf);
    goto CLOSE;
  }
  rc = pcap_activate(p);
  if (rc != 0) {
    ERRLOG("pcap_activate(device %s) error: %s\n", name, pcap_geterr(p));
    goto CLOSE;
  }
  rc = pcap_setdirection(p, PCAP_D_IN);
  if (rc != 0) {
    ERRLOG("pcap_setdirection(device %s) error: %s\n", name, pcap_geterr(p));
    goto CLOSE;
  }

  d = new Device(p, name, addr);
  netBase.addDevice(d);
  return d;

CLOSE:
  pcap_close(p);
  return nullptr;
}

Ethernet::Device *Ethernet::findDeviceByName(const char *name) {
  return dynamic_cast<Device *>(netBase.findDeviceByName(name));
}

int Ethernet::send(const void *data, size_t dataLen, Addr dst,
                   uint16_t etherType, Device *dev) {
  if (dataLen > SIZE_MAX - sizeof(Header)) {
    LOG_ERR("Ethernet data length too large: %lu", dataLen);
    return -1;
  }

  size_t frameLen = sizeof(Header) + dataLen;
  void *frame = malloc(frameLen);
  if (!frame) {
    LOG_ERR_POSIX("malloc");
    return -1;
  }

  Header &header = *(Header *)frame;
  header = Header{.dst = dst, .src = dev->addr, .etherType = htons(etherType)};
  memcpy(&header + 1, data, dataLen);

  int rc = netBase.send(frame, frameLen, dev);
  free(frame);
  return rc;
}

Ethernet::RecvCallback::RecvCallback(int etherType_) : etherType(etherType_) {}

void Ethernet::addRecvCallback(RecvCallback *callback) {
  callbacks.push_back(callback);
}

int Ethernet::setup() {
  netBase.addRecvCallback(&netBaseHandler);
  return 0;
}

Ethernet::NetBaseHandler::NetBaseHandler(Ethernet &ethernet_)
    : NetBase::RecvCallback(LINK_TYPE), ethernet(ethernet_) {}

int Ethernet::NetBaseHandler::handle(const void *frame, int frameLen,
                                     NetBase::Device *baseDevice,
                                     const Info &info) {
  auto *device = dynamic_cast<Device *>(baseDevice);
  if (!device) {
    ERRLOG("Unconfigured Ethernet device: %s\n", baseDevice->name);
    return -1;
  }

  if (frameLen < sizeof(Header)) {
    ERRLOG("Truncated Ethernet header (device %s): %d/%d\n", device->name,
           frameLen, (int)sizeof(Header));
    return -1;
  }

  const Header &header = *(const Header *)frame;
  Ethernet::RecvCallback::Info newInfo(info);
  newInfo.linkHeader = &header;
  newInfo.linkDevice = device;

  int etherType = ntohs(header.etherType);
  int rc = 0;
  for (auto *c : ethernet.callbacks)
    if (c->etherType == -1 || c->etherType == etherType)
      if (c->handle(&header + 1, frameLen - sizeof(Header), newInfo) != 0)
        rc = -1;
  return rc;
}

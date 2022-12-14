#include "ARP.h"

#include "common.h"
#include "commands.h"

class CmdIPAddrAdd : public Command {
public:
  CmdIPAddrAdd() : Command("ip-addr-add") {}

  int main(int argc, char **argv) override {
    IP::Addr addr, mask;
    if (argc != 4 ||
        sscanf(argv[1], IP_ADDR_FMT_STRING, IP_ADDR_FMT_ARGS(&addr)) !=
            IP_ADDR_FMT_NUM ||
        sscanf(argv[2], IP_ADDR_FMT_STRING, IP_ADDR_FMT_ARGS(&mask)) !=
            IP_ADDR_FMT_NUM) {
      fprintf(stderr, "Usage: %s <ip> <mask> <device>\n", argv[0]);
      return 1;
    }

    auto *d = findDeviceByName(argv[3]);
    if (!d)
      return 1;

    IP::DevAddr entry{device : d, addr : addr, mask : mask};

    INVOKE({ ns.ip.addAddr(entry); })
    return 0;
  }
};

class CmdRouteAdd : public Command {
public:
  CmdRouteAdd() : Command("route-add") {}

  int main(int argc, char **argv) override {
    LpmRouting::Entry entry;
    if (argc < 5 ||
        sscanf(argv[1], IP_ADDR_FMT_STRING, IP_ADDR_FMT_ARGS(&entry.addr)) !=
            IP_ADDR_FMT_NUM ||
        sscanf(argv[2], IP_ADDR_FMT_STRING, IP_ADDR_FMT_ARGS(&entry.mask)) !=
            IP_ADDR_FMT_NUM ||
        sscanf(argv[4], IP_ADDR_FMT_STRING, IP_ADDR_FMT_ARGS(&entry.gateway)) !=
            IP_ADDR_FMT_NUM) {
      fprintf(stderr, "usage: %s <ip> <mask> <device> <gateway> [<metric>]\n",
              argv[0]);
      return 1;
    }

    entry.device = findDeviceByName(argv[3]);
    if (!entry.device)
      return 1;

    int rc;
    IP::Routing *routing;
    INVOKE({
      routing = ns.ip.getRouting();
      if (!routing)
        ns.configStaticRouting();
      routing = ns.ip.getRouting();
    })
    if (auto *r = dynamic_cast<LpmRouting *>(routing)) {
      INVOKE({ rc = r->setEntry(entry); })
    } else if (auto *r = dynamic_cast<RIP *>(routing)) {
      RIP::TabEntry rentry{.device = entry.device,
                           .gateway = entry.gateway,
                           .metric = 1,
                           .expire = nullptr};
      if (argc >= 6 && sscanf(argv[5], "%d", &rentry.metric) != 1) {
        fprintf(stderr, "Invalid metric.\n");
        return 1;
      }
      INVOKE({ rc = r->setEntry(entry.addr, entry.mask, rentry); })
    }
    if (rc != 0) {
      fprintf(stderr, "Error setting routing entry.\n");
      return 1;
    }
    return 0;
  }
};

class CmdRouteRip : public Command {
public:
  CmdRouteRip() : Command("route-rip") {}

  int main(int argc, char **argv) override {
    int rc;
    INVOKE({ rc = ns.configRIP(); })
    if (rc != 0) {
      fprintf(stderr, "Error setting up RIP routing.\n");
      return 1;
    }
    return 0;
  }
};

class CmdRouteInfo : public Command {
public:
  CmdRouteInfo() : Command("route-info") {}

  int main(int argc, char **argv) override {
    INVOKE({
      auto *r = dynamic_cast<LpmRouting *>(ns.ip.getRouting());
      if (r) {
        const auto &table = r->getTable();
        printf("IP | Netmask | Device | Gateway\n");
        time_t curTime = time(nullptr);
        for (auto &&e : table) {
          printf(IP_ADDR_FMT_STRING " | " IP_ADDR_FMT_STRING
                                    " | %s | " IP_ADDR_FMT_STRING "\n",
                 IP_ADDR_FMT_ARGS(e.addr), IP_ADDR_FMT_ARGS(e.mask),
                 e.device->name, IP_ADDR_FMT_ARGS(e.gateway));
        }
      } else {
        printf("No static routing table\n");
      }
    })
    return 0;
  }
};

class CmdRouteRipInfo : public Command {
public:
  CmdRouteRipInfo() : Command("route-rip-info") {}

  int main(int argc, char **argv) override {
    INVOKE({
      auto *r = dynamic_cast<RIP *>(ns.ip.getRouting());
      if (r) {
        const auto &table = r->getTable();
        printf("IP | Mask | Device | Gateway | Metric | Exipre\n");
        time_t curTime = time(nullptr);
        for (auto &&e : table) {
          printf(IP_ADDR_FMT_STRING " | " IP_ADDR_FMT_STRING
                                    " | %s | " IP_ADDR_FMT_STRING
                                    " | %d | %+ld\n",
                 IP_ADDR_FMT_ARGS(e.first.addr), IP_ADDR_FMT_ARGS(e.first.mask),
                 e.second.device->name, IP_ADDR_FMT_ARGS(e.second.gateway),
                 e.second.metric,
                 e.second.expire
                     ? (e.second.expire->expireTime - Timer::Clock::now()) / 1s
                     : 0xffffffffL);
        }
      } else {
        printf("No RIP routing\n");
      }
    })
    return 0;
  }
};

class CmdArpInfo : public Command {
public:
  CmdArpInfo() : Command("arp-a") {}

  int main(int argc, char **argv) override {
    INVOKE({
      const auto &table = ns.ip.arp.getTable();
      printf("IP Address | MAC Address | Expire Time \n");
      time_t curTime = time(nullptr);
      for (auto &&e : table) {
        printf(IP_ADDR_FMT_STRING " | " ETHERNET_ADDR_FMT_STRING " | %+ld\n",
               IP_ADDR_FMT_ARGS(e.first),
               ETHERNET_ADDR_FMT_ARGS(e.second.linkAddr),
               e.second.expire
                   ? (e.second.expire->expireTime - Timer::Clock::now()) / 1s
                   : 0xffffffffL);
      }
    });
    return 0;
  }
};

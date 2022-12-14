#include "common.h"
#include "commands.h"

#include <random>
#include <thread>

class CmdEthTest : public Command {
public:
  CmdEthTest() : Command("eth-test") {}

  static constexpr int NUM_LEN = 128;

  struct LongNum {
    u_char v[NUM_LEN];
    LongNum() : v{} {}

    LongNum &operator^=(const LongNum &a) {
      for (int i = 0; i < NUM_LEN; i++)
        v[i] ^= a.v[i];
      return *this;
    }

    uint64_t checksum() const {
      uint64_t r = 0;
      for (int i = 0; i < NUM_LEN; i++)
        r ^= (uint64_t)v[i] << ((i % 8) * 8);
      return r;
    }
  };

  static LongNum randLongNum(std::mt19937 &rnd) {
    LongNum ret;
    for (int i = 0; i < NUM_LEN; i++)
      ret.v[i] = rnd() & 255;
    return ret;
  }

  static constexpr uint16_t ETHER_TYPE_CTRL = 0x88B5;
  static constexpr uint16_t ETHER_TYPE_DATA = 0x88B6;

  static constexpr int MSG_LEN = 75;
  static constexpr char HELO[MSG_LEN] =
      "8C7D8C49-C942-4158-BF63-A582CF8425E7 "
      "99372B57-257E-47E1-9972-593D5D83805C\n";
  static constexpr char RESP[MSG_LEN] =
      "DEB260F8-2FA3-4D78-A1C6-8993C3F96C9A "
      "8000077C-7322-4493-8009-77862ABC34DC\n";
  static constexpr char STOP[MSG_LEN] =
      "C62ABF3A-0C70-4587-8349-14489DE7DD9C "
      "CC76130C-4609-4E24-B5A5-D27221F22F1B\n";

  class CtrlHandler;
  class DataHandler;

  struct Link {
    Ethernet::Device *device;
    Ethernet::Addr dst;
    int recvNum;
    LongNum sent, recv;
    std::timed_mutex start, stop;
    CtrlHandler *ctrlHandler;
    DataHandler *dataHandler;
    std::thread *sendThread;
  };

  class CtrlHandler {
    Link &link;

  public:
    CtrlHandler(Link &link_) : link(link_) {}

    int handle(const void *data, int dataLen, const Ethernet::RecvInfo &info) {
      if (info.device != link.device || dataLen != MSG_LEN)
        return 0;

      bool start = false;
      if (memcmp(data, HELO, MSG_LEN) == 0) {
        fprintf(stderr, "device %s received HELO\n", link.device->name);
        ns.ethernet.send(RESP, MSG_LEN, info.header->src, ETHER_TYPE_CTRL,
                      link.device);
        start = true;
      } else if (memcmp(data, RESP, MSG_LEN) == 0) {
        fprintf(stderr, "device %s received RESP\n", link.device->name);
        start = true;
      } else if (memcmp(data, STOP, MSG_LEN) == 0) {
        fprintf(stderr, "device %s received STOP\n", link.device->name);
        link.stop.unlock();
      }

      if (start && !link.start.try_lock()) {
        link.dst = info.header->src;
        link.start.unlock();
      }

      return 0;
    }
  };

  class DataHandler {
    Link &link;

  public:
    DataHandler(Link &link_) : link(link_) {}

    int handle(const void *data, int dataLen, const Ethernet::RecvInfo &info) {
      if (info.device != link.device || dataLen != sizeof(LongNum))
        return 0;

      const LongNum &v = *(const LongNum *)data;
      link.recvNum++;
      link.recv ^= v;
      return 0;
    }
  };

  int main(int argc, char **argv) override {
    if (argc < 2) {
      fprintf(stderr, "usage: %s <device> [<device>]...\n", argv[0]);
      return 1;
    }

    std::vector<Link *> links;
    for (int i = 1; i < argc; i++) {
      Ethernet::Device *d;

      INVOKE({ d = ns.ethernet.addDeviceByName(argv[i]); })

      if (!d)
        return 1;
      auto *l = new Link();
      l->device = d;
      l->recvNum = 0;
      l->ctrlHandler = new CtrlHandler(*l);
      l->dataHandler = new DataHandler(*l);
      l->dst = Ethernet::BROADCAST;
      l->start.lock();
      l->stop.lock();
      links.push_back(l);
    }

    static constexpr int FRAMES = 10000;
    static constexpr int BATCH = 1000;
    std::random_device rndDev;

    for (auto *l : links) {
      int seed = rndDev();
      l->sendThread = new std::thread([l, seed]() {
        using namespace std::chrono_literals;

        std::mt19937 rnd(seed);

        INVOKE({
          ns.ethernet.addOnRecv(
              [l](auto &&...args) -> int {
                l->ctrlHandler->handle(args...);
                return 0;
              },
              ETHER_TYPE_CTRL);
          ns.ethernet.addOnRecv(
              [l](auto &&...args) -> int {
                l->dataHandler->handle(args...);
                return 0;
              },
              ETHER_TYPE_DATA);
        });

        while (!l->start.try_lock_for(1s)) {
          fprintf(stderr, "device %s waiting\n", l->device->name);
          INVOKE({
            ns.ethernet.send(HELO, MSG_LEN, l->dst, ETHER_TYPE_CTRL, l->device);
          })
        }

        for (int i = 0; i < FRAMES; i++) {
          if (i % BATCH == 0)
            fprintf(stderr, "device %s: sent %d/%d\n", l->device->name, i,
                    FRAMES);
          LongNum v = randLongNum(rnd);
          l->sent ^= v;
          INVOKE({
            ns.ethernet.send(&v, sizeof(LongNum), l->dst, ETHER_TYPE_DATA,
                          l->device);
          })
          std::this_thread::sleep_for(1ms);
        }

        INVOKE({
          ns.ethernet.send(STOP, MSG_LEN, l->dst, ETHER_TYPE_CTRL, l->device);
        })

        l->stop.lock();
      });
    }

    for (auto *l : links) {
      l->sendThread->join();
      printf("Device %s: sent (%d) %0lX, recv (%d) %0lX\n", l->device->name,
             FRAMES, l->sent.checksum(), l->recvNum, l->recv.checksum());
    }

    return 0;
  }
};

constexpr char CmdEthTest::HELO[];
constexpr char CmdEthTest::RESP[];
constexpr char CmdEthTest::STOP[];

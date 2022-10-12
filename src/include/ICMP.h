#ifndef NETSTACK_ICMP_H
#define NETSTACK_ICMP_H

#include "IP.h"

class ICMP {
public:
  static const int PROTOCOL_ID;

  struct Header {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t seqNumber;
  };

  IP &ip;

  ICMP(IP &ip_);
  ICMP(const ICMP &) = delete;

  /**
   * @brief Send Time Exceeded Message back for an IP packet.
   *
   * @param orig Pointer to the original packet.
   * @param origLen Length of the original packet.
   * @param IP The IP layer service object.
   * @param info Other information of the received packet.
   *
   * @return 0 on success, negative on error.
   */
  int sendTimeExceeded(const void *orig, int origLen,
                       const IP::RecvCallback::Info &info);

  /**
   * @brief Send Echo or Reply Message.
   *
   * @param src IP address of the source host.
   * @param dst IP address of the destination host.
   * @param type 8 for echo message; 0 for echo reply message.
   * @param identifier Identifier field for matching.
   * @param seqNumber Sequence number.
   * @param data Pointer to the data.
   * @param dataLen Length of the data.
   * @param timeToLive The `time to live` field for IP transmission.
   * 
   * @return 0 on success, negative on error.
   */
  int sendEchoOrReply(const IP::Addr &src, const IP::Addr &dst, int type,
                      int identifier, int seqNumber, const void *data,
                      int dataLen, int timeToLive = 64);

  class RecvCallback {
  public:
    int type; // The matching `type` field, -1 for any;

    struct Info : IP::RecvCallback::Info {
      const Header *icmpHeader;

      Info(const IP::RecvCallback::Info &info_)
          : IP::RecvCallback::Info(info_) {}
    };

    RecvCallback(int type_);

    virtual int handle(const void *data, int dataLen, const Info &info) = 0;
  };

  void addRecvCallback(RecvCallback *callback);
  int removeRecvCallback(RecvCallback *callback);

  int setup();

private:
  Vector<RecvCallback *> callbacks;

  class IPHandler : public IP::RecvCallback {
    ICMP &icmp;

  public:
    IPHandler(ICMP &icmp_);

    int handle(const void *msg, int msgLen, const Info &info) override;
  } ipHandler;
};

#endif
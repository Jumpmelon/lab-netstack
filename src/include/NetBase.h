#ifndef NETSTACK_NET_STACK_H
#define NETSTACK_NET_STACK_H

#include "utils.h"
#include "LoopDispatcher.h"

/**
 * @brief Base of the netstack, handling sending & receiving of pcap devices.
 * May build services for devices of specific linkType above it.
 */
class NetBase {
public:
  NetBase() = default;
  NetBase(const NetBase &) = delete;

  class Device {
    struct pcap *p;

  public:
    int id;             // The added ID of the device, assigned from 0.
    char *const name;   // Name of the device
    const int linkType; // Type of its link layer.

    Device(struct pcap *p_, const char *name_, int linkType_);
    Device(const Device &) = delete;
    virtual ~Device();

    /**
     * @brief Send a frame through the device.
     *
     * @param buf Pointer to the frame.
     * @param len Length of the frame.
     * @return 0 on success, negative on error.
     */
    int sendFrame(const void *buf, int len);

    friend class NetBase;
  };

  /**
   * @brief Add a device to the netstack.
   *
   * @param device Pointer to the `Device` object.
   * @return Non-negative ID of the added device.
   */
  int addDevice(Device *device);

  /**
   * @brief Find an added device by its name.
   *
   * @param name Name of the device.
   * @return Pointer to the `Device` object, `nullptr` if not found.
   */
  Device *findDeviceByName(const char *name);

  class RecvCallback {
  public:
    int linkType; // The matching linkType, -1 for any.

    /**
     * @brief Construct a new RecvCallback object.
     *
     * @param linkType_ The matching linkType, -1 for any.
     */
    RecvCallback(int linkType_);

    struct Info {
      timeval ts;
    };

    /**
     * @brief Handle a received frame.
     *
     * @param frame Pointer to the frame.
     * @param frameLen Length of the frame.
     * @param device The receiving device.
     * @param info Other information of the received frame.
     * @return 0 on success, negative on error.
     */
    virtual int handle(const void *frame, int frameLen, Device *device,
                       const Info &info) = 0;
  };

  /**
   * @brief Register a callback on receiving frames.
   *
   * @param callback Pointer to a `RecvCallback` object (which need to be
   * persistent).
   */
  void addRecvCallback(RecvCallback *callback);

  /**
   * @brief Handle a receiving frame; dispatch it to registered callbacks.
   *
   * @param frame Pointer to the frame.
   * @param frameLen Length of the frame.
   * @param device The device receiving the frame.
   * @param info Other information of the received frame.
   * @return 0 on success, negative on error.
   */
  int handleFrame(const void *frame, int frameLen, Device *device,
                  RecvCallback::Info info);

  /**
   * @brief Setup the netstack base.
   *
   * @return 0 on success, negative on error.
   */
  int setup();

  /**
   * @brief Register a callback in `loop`.
   *
   * @param callback Pointer to an `Action` object to be invoked (which need to
   * be persistent).
   */
  void addLoopCallback(LoopCallback *callback);

  /**
   * @brief Start to loop for receiving.
   *
   * @return 0 if breaked by callback, negative on error.
   */
  int loop();

private:
  Vector<Device *> devices;
  Vector<RecvCallback *> callbacks;
  Vector<LoopCallback *> loopCallbacks;
};

#endif
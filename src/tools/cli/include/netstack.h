#ifndef TOOLS_NETSTACK_H
#define TOOLS_NETSTACK_H

#include <thread>

#include "LoopDispatcher.h"

#include "NetBase.h"
#include "Ethernet.h"
#include "IP.h"
#include "ICMP.h"
#include "IPForward.h"
#include "ARP.h"
#include "LpmRouting.h"
#include "UDP.h"
#include "RIP.h"

extern NetBase netBase;
extern Ethernet ethernet;
extern IP ip;
extern ARP arp;
extern LpmRouting staticRouting;
extern UDP udp;
extern RIP ripRouting;

extern IPForward ipForward;

extern std::thread *netThread;
extern LoopDispatcher loopDispatcher;

/**
 * @brief Initialize the netstack.
 * 
 * @return 0 on success, negative on error.
 */
int initNetStack();

/**
 * @brief Start the netstack main loop.
 * 
 * @return 0 on success, negative on error.
 */
int startLoop();

/**
 * @brief Stop the netstack.
 */
void stopLoop();

#endif

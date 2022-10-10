#include <cstring>
#include <cstdlib>

#include "commands.h"

Command::Command(const char *name_) : name(strdup(name_)) {}

Command::~Command() {
  free(name);
}

#include "commands/Devices.hpp"
#include "commands/Control.hpp"
#include "commands/FrameIO.hpp"
#include "commands/PacketIO.hpp"
#include "commands/IPConfig.hpp"
#include "commands/IPForward.hpp"
#include "commands/UdpIO.hpp"
#include "commands/AutoConfig.hpp"

std::vector<Command *> allCommands = {
  new CmdAddDevice(),
  new CmdFindDevice(),
  new CmdStartLoop(),
  new CmdSendFrame(),
  new CmdCaptureFrames(),
  new CmdCapturePackets(),
  new CmdIPAddrAdd(),
  new CmdRouteAdd(),
  new CmdRouteRip(),
  new CmdRouteRipInfo(),
  new CmdIPForward(),
  new CmdNcUdpListen(),
  new CmdNcUdp(),
  new CmdAutoConfig()
};

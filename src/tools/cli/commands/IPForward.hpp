#include "common.h"
#include "commands.h"

class CmdIPForward : public Command {
public:
  CmdIPForward() : Command("ip-forward") {}

  int main(int argc, char **argv) override {
    int rc;
    INVOKE({ rc = ns.enableForward(); })
    if (rc != 0)
      fprintf(stderr, "Error setting up IP forwarding.\n");
    return rc;
  }
};

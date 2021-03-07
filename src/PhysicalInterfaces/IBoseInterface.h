#ifndef IBoseINTERFACE_H_
#define IBoseINTERFACE_H_

#include <homegear-base/BaseLib.h>
#include "../BosePacket.h"

namespace Bose {

class IBoseInterface : public BaseLib::Systems::IPhysicalInterface
{
public:
	IBoseInterface(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings);
	virtual ~IBoseInterface();

	virtual std::string listenAddress() { return "::1"; }
	virtual int32_t listenPort() { return 7373; }
	virtual std::string ttsProgram() { return ""; }
	virtual std::string dataPath() { return ""; }
    virtual void sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet) {}
protected:
	BaseLib::Output _out;
};

}

#endif

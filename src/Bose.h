#ifndef Bose_H_
#define Bose_H_

#include <homegear-base/BaseLib.h>

namespace Bose
{
class BoseCentral;

using namespace BaseLib;

class Bose : public BaseLib::Systems::DeviceFamily
{
public:
	Bose(BaseLib::SharedObjects* bl, BaseLib::Systems::IFamilyEventSink* eventHandler);
	virtual ~Bose();
	virtual void dispose();

	virtual bool hasPhysicalInterface() { return true; }
	virtual PVariable getPairingInfo();
protected:
	virtual void createCentral();
	virtual std::shared_ptr<BaseLib::Systems::ICentral> initializeCentral(uint32_t deviceId, int32_t address, std::string serialNumber);
};

}

#endif

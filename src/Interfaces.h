#ifndef INTERFACES_H_
#define INTERFACES_H_

#include <homegear-base/BaseLib.h>

namespace Bose
{

using namespace BaseLib;

class Interfaces : public BaseLib::Systems::PhysicalInterfaces
{
public:
	Interfaces(BaseLib::SharedObjects* bl, std::map<std::string, Systems::PPhysicalInterfaceSettings> physicalInterfaceSettings);
	virtual ~Interfaces();

protected:
	virtual void create();
};

}

#endif

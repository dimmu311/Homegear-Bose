#include "Interfaces.h"
#include "GD.h"
#include "PhysicalInterfaces/EventServer.h"

namespace Bose
{

Interfaces::Interfaces(BaseLib::SharedObjects* bl, std::map<std::string, Systems::PPhysicalInterfaceSettings> physicalInterfaceSettings) : Systems::PhysicalInterfaces(bl, GD::family->getFamily(), physicalInterfaceSettings)
{
	create();
}

Interfaces::~Interfaces()
{
}

void Interfaces::create()
{
	try
	{

		for(std::map<std::string, Systems::PPhysicalInterfaceSettings>::iterator i = _physicalInterfaceSettings.begin(); i != _physicalInterfaceSettings.end(); ++i)
		{
			std::shared_ptr<IBoseInterface> device;
			if(!i->second) continue;
			GD::out.printDebug("Debug: Creating physical device. Type defined in Bose.conf is: " + i->second->type);
			if(i->second->type == "eventserver") device.reset(new EventServer((i->second)));
			else GD::out.printError("Error: Unsupported physical device type: " + i->second->type);
			if(device)
			{
				if(_physicalInterfaces.find(i->second->id) != _physicalInterfaces.end()) GD::out.printError("Error: id used for two devices: " + i->second->id);
				_physicalInterfaces[i->second->id] = device;
				GD::physicalInterface = device;
			}
		}
		if(!GD::physicalInterface) GD::physicalInterface = std::make_shared<IBoseInterface>(std::make_shared<BaseLib::Systems::PhysicalInterfaceSettings>());
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

}

#include "IBoseInterface.h"
#include "../GD.h"

namespace Bose
{

IBoseInterface::IBoseInterface(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IPhysicalInterface(GD::bl, GD::family->getFamily(), settings)
{
	_maxPacketProcessingTime = 15000;
	if(settings->listenThreadPriority == -1)
	{
		settings->listenThreadPriority = 0;
		settings->listenThreadPolicy = SCHED_OTHER;
	}
}

IBoseInterface::~IBoseInterface()
{

}

}

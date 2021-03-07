#include "Factory.h"
#include "../config.h"
#include "GD.h"

BaseLib::Systems::DeviceFamily* BoseFactory::createDeviceFamily(BaseLib::SharedObjects* bl, BaseLib::Systems::IFamilyEventSink* eventHandler)
{
	return new Bose::Bose(bl, eventHandler);
}

std::string getVersion()
{
	return VERSION;
}

int32_t getFamilyId()
{
	return Bose_FAMILY_ID;
}

std::string getFamilyName()
{
	return Bose_FAMILY_NAME;
}

BaseLib::Systems::SystemFactory* getFactory()
{
	return (BaseLib::Systems::SystemFactory*)(new BoseFactory);
}

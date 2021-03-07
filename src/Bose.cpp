#include "Bose.h"
#include "BoseCentral.h"
#include "Interfaces.h"
#include "GD.h"

#include <iomanip>

namespace Bose
{

Bose::Bose(BaseLib::SharedObjects* bl, BaseLib::Systems::IFamilyEventSink* eventHandler) : BaseLib::Systems::DeviceFamily(bl, eventHandler, Bose_FAMILY_ID, Bose_FAMILY_NAME)
{
	GD::bl = bl;
	GD::family = this;
	GD::dataPath = _settings->getString("datapath");
	if(!GD::dataPath.empty() && GD::dataPath.back() != '/') GD::dataPath.push_back('/');
	GD::out.init(bl);
	GD::out.setPrefix("Module Bose: ");
	GD::out.printDebug("Debug: Loading module...");
	_physicalInterfaces.reset(new Interfaces(bl, _settings->getPhysicalInterfaceSettings()));
}

Bose::~Bose()
{

}

void Bose::dispose()
{
	if(_disposed) return;
	DeviceFamily::dispose();
}

std::shared_ptr<BaseLib::Systems::ICentral> Bose::initializeCentral(uint32_t deviceId, int32_t address, std::string serialNumber)
{
	return std::shared_ptr<BoseCentral>(new BoseCentral(deviceId, serialNumber, this));
}

void Bose::createCentral()
{
	try
	{
		if(_central) return;

		int32_t seedNumber = BaseLib::HelperFunctions::getRandomNumber(1, 9999999);
		std::ostringstream stringstream;
		stringstream << "VSC" << std::setw(7) << std::setfill('0') << std::dec << seedNumber;
		std::string serialNumber(stringstream.str());

		_central.reset(new BoseCentral(0, serialNumber, this));
		GD::out.printMessage("Created Bose central with id " + std::to_string(_central->getId()) + " and serial number " + serialNumber);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

PVariable Bose::getPairingInfo()
{
	try
	{
		if(!_central) return std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		PVariable info = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

		//{{{ General
		info->structValue->emplace("searchInterfaces", std::make_shared<BaseLib::Variable>(false));
		//}}}

		//{{{ Family settings
		PVariable familySettings = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		info->structValue->emplace("familySettings", familySettings);
		//}}}

		//{{{ Pairing methods
		PVariable pairingMethods = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		pairingMethods->structValue->emplace("searchDevices", std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct));
		info->structValue->emplace("pairingMethods", pairingMethods);
		//}}}

		//{{{ interfaces
		auto interfaces = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

		//{{{ Event server
		auto interface = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		interface->structValue->emplace("name", std::make_shared<BaseLib::Variable>(std::string("Event Server")));
		interface->structValue->emplace("ipDevice", std::make_shared<BaseLib::Variable>(false));
        interface->structValue->emplace("predefined", std::make_shared<BaseLib::Variable>(true));

		auto field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(0));
		field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.common.id")));
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		interface->structValue->emplace("id", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(2));
		field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.common.listenip")));
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		field->structValue->emplace("required", std::make_shared<BaseLib::Variable>(false));
		interface->structValue->emplace("host", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(3));
		field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.common.listenport")));
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		field->structValue->emplace("default", std::make_shared<BaseLib::Variable>(std::string("7373")));
		field->structValue->emplace("required", std::make_shared<BaseLib::Variable>(true));
		interface->structValue->emplace("port", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(4));
		field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.common.ttsprogram")));
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		field->structValue->emplace("required", std::make_shared<BaseLib::Variable>(false));
		interface->structValue->emplace("ttsProgram", field);

		interfaces->structValue->emplace("eventserver", interface);
		//}}}

		info->structValue->emplace("interfaces", interfaces);
		//}}}

		return info;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}
}

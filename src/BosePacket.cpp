#include "BosePacket.h"
#include "GD.h"

namespace Bose
{
BosePacket::BosePacket()
{
	_values.reset(new std::unordered_map<std::string, std::string>());
	_valuesToSet.reset(new std::vector<std::pair<std::string, std::string>>());
}

BosePacket::BosePacket(std::string& soap, int64_t timeReceived)
{
	try
	{
		BaseLib::HelperFunctions::trim(soap);
		_values.reset(new std::unordered_map<std::string, std::string>());
		_valuesToSet.reset(new std::vector<std::pair<std::string, std::string>>());
		_timeReceived = timeReceived;
		if(soap.empty()) return;
		xml_document doc;
		doc.parse<parse_no_entity_translation>(&soap.at(0));

        xml_node* updates = doc.first_node("updates");
		if(updates) //Updatdes packet
        {
            if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type updates");
            if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

            if(!setDeviceId(updates)) return;

            xml_node* presetsUpdated = updates->first_node("presetsUpdated");
            if(presetsUpdated){
                xml_node* presets = presetsUpdated->first_node("presets");
                if(presets) { //presets packet
                    if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type presets");
                    if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

                    parsePresets(presets);
                    return;
                }
            }

            xml_node* nowPlayingUpdated = updates->first_node("nowPlayingUpdated");
            if(nowPlayingUpdated){
                xml_node* nowPlaying = nowPlayingUpdated->first_node("nowPlaying");
                if(nowPlaying) { //nowPlaying packet
                    if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type nowPlaying");
                    if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

                    parseNowPlaying(nowPlaying);
                    return;
                }
            }

            //the nowSelectionUpdated looks like a smaller on of the presetsUpdated. Because of that wo can call parsePreset() to parse this response
            xml_node* nowSelectionUpdated = updates->first_node("nowSelectionUpdated");
            if(nowSelectionUpdated){
                xml_node* preset = nowSelectionUpdated->first_node("preset");
                if(preset) { //nowSelection packet
                    if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type nowSelection");
                    if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

                    _functionName = "presets";

                    parsePreset(preset);
                    return;
                }
            }

            xml_node* volumeUpdated = updates->first_node("volumeUpdated");
            if(volumeUpdated){
                xml_node* volume = volumeUpdated->first_node("volume");
                if(volume) { //volume packet
                    if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type volume");
                    if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

                    parseVolume(volume);
                    return;
                }
            }

            xml_node* bassUpdated = updates->first_node("bassUpdated");
            if(bassUpdated){
                xml_node* bass = bassUpdated->first_node("bass");
                if(bass) { //bass packet
                    if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type bass");
                    if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

                    parseBass(bass);
                    return;
                }
            }

            xml_node* zoneUpdated = updates->first_node("zoneUpdated");
            if(zoneUpdated){
                xml_node* zone = zoneUpdated->first_node("zone");
                if(zone) { //zone packet
                    if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type zone");
                    if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

                    parseGetZone(zone);
                    return;
                }
            }

            xml_node* infoUpdated = updates->first_node("infoUpdated");
            if(infoUpdated){
                xml_node* info = infoUpdated->first_node("info");
                if(info) { //info packet
                    if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type info");
                    if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

                    parseInfo(info);
                    return;
                }
            }

            xml_node* nameUpdated = updates->first_node("nameUpdated");
            if(nameUpdated){
                if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type name");
                if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

                _functionName = "name";
                _values->operator[](nameUpdated->name()) = nameUpdated->value();
                return;
            }

            xml_node* errorUpdate = updates->first_node("errorUpdate");
            if(errorUpdate){
                xml_node* error = errorUpdate->first_node("error");
                if(error) { //error packet
                    if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type error");
                    if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

                    parseError(error);
                    return;
                }
            }

            xml_node* groupUpdated = updates->first_node("groupUpdated");
            if(groupUpdated){
                xml_node* group = infoUpdated->first_node("group");
                if(group) { //group packet
                    if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type group");
                    if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

                    parseGetGroup(group);
                    return;
                }
            }
        }
		xml_node* info = doc.first_node("info");
		if(info) //Info packet
		{
		    if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type info");
            if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

			if(!setDeviceId(info)) return;
            parseInfo(info);
            return;
		}

        xml_node* nowPlaying = doc.first_node("nowPlaying");
        if(nowPlaying) //nowPlaying packet
        {
            if(GD::bl->debugLevel>=4) GD::out.printInfo("Packet is of type nowPlaying");
            if(GD::bl->debugLevel>=5) GD::out.printInfo(soap);

            if(!setDeviceId(nowPlaying)) return;

            parseNowPlaying(nowPlaying);
            return;
        }

        xml_node* volume = doc.first_node("volume");
        if(volume) //volume packet
        {
            if (GD::bl->debugLevel >= 4) GD::out.printInfo("Packet is of type volume");
            if (GD::bl->debugLevel >= 5) GD::out.printInfo(soap);

            if(!setDeviceId(volume)) return;

            parseVolume(volume);
            return;
        }

        xml_node* sources = doc.first_node("sources");
        if(sources) //sources packet
        {
            if (GD::bl->debugLevel >= 4) GD::out.printInfo("Packet is of type sources");
            if (GD::bl->debugLevel >= 5) GD::out.printInfo(soap);

            if (!setDeviceId(sources)) return;
            parseSources(sources);
            return;
        }

        xml_node* zone = doc.first_node("zone");
        if(zone) //zone packet
        {
            if (GD::bl->debugLevel >= 4) GD::out.printInfo("Packet is of type zone");
            if (GD::bl->debugLevel >= 5) GD::out.printInfo(soap);

            if (!setDeviceId(zone)) return;
            parseGetZone(zone);
            return;
        }

        xml_node* bassCapabilities = doc.first_node("bassCapabilities");
        if(bassCapabilities) //bassCapabilities packet
        {
            if (GD::bl->debugLevel >= 4) GD::out.printInfo("Packet is of type bassCapabilities");
            if (GD::bl->debugLevel >= 5) GD::out.printInfo(soap);

            if (!setDeviceId(bassCapabilities)) return;
            parseBassCapabilities(bassCapabilities);
            return;
        }

        xml_node* bass = doc.first_node("bass");
        if(bass) //bass packet
        {
            if (GD::bl->debugLevel >= 4) GD::out.printInfo("Packet is of type bass");
            if (GD::bl->debugLevel >= 5) GD::out.printInfo(soap);

            if (!setDeviceId(bass)) return;
            parseBass(bass);
            return;
        }

        xml_node* presets = doc.first_node("presets");
        if(presets) //presets packet
        {
            if (GD::bl->debugLevel >= 4) GD::out.printInfo("Packet is of type presets");
            if (GD::bl->debugLevel >= 5) GD::out.printInfo(soap);

            parsePresets(presets);
            return;
        }

        xml_node* group = doc.first_node("group");
        if(group) //group packet
        {
            if (GD::bl->debugLevel >= 4) GD::out.printInfo("Packet is of type group");
            if (GD::bl->debugLevel >= 5) GD::out.printInfo(soap);

            parseGetGroup(group);
            return;
        }

        GD::out.printWarning("Warning: Tried to parse invalid packet.");
        if(GD::bl->debugLevel>=5) GD::out.printInfo("packet was: "+ soap);
	}
	catch(const std::exception& ex)
    {
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(ex.what()) + " Packet was: " + soap);
    }
}

BosePacket::BosePacket(std::string& soap, std::string serialNumber, int64_t timeReceived) : BosePacket(soap, timeReceived)
{
	_serialNumber = serialNumber;
}

BosePacket::BosePacket(xml_node* node, int64_t timeReceived )
{
	try
	{
		if(!node) return;
		_values.reset(new std::unordered_map<std::string, std::string>());
		_valuesToSet.reset(new std::vector<std::pair<std::string, std::string>>());
		_timeReceived = timeReceived;
		_functionName = "InfoBroadcast2";
		for(xml_node* subNode = node->first_node(); subNode; subNode = subNode->next_sibling())
		{
			_values->operator [](std::string(subNode->name())) = std::string(subNode->value());
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

BosePacket::BosePacket(xml_node* node, std::string serialNumber, int64_t timeReceived) : BosePacket(node, timeReceived)
{
	_serialNumber = serialNumber;
}

BosePacket::BosePacket(std::string& functionName, std::shared_ptr<std::vector<std::pair<std::string, std::string>>> valuesToSet)
{
	_functionName = functionName;
	_valuesToSet = valuesToSet;
	if(!_valuesToSet) _valuesToSet.reset(new std::vector<std::pair<std::string, std::string>>());
	_values.reset(new std::unordered_map<std::string, std::string>());
}

BosePacket::~BosePacket()
{
}

bool BosePacket::setDeviceId(xml_node* node)
{
    try {
        if(!node) return false;

        xml_attribute* attr = node->first_attribute("deviceID");
        if(!attr)
        {
            GD::out.printWarning("Warning: Tried to parse element without attribute: deviceID");
            return false;
        }
        _deviceId = attr->value();
        return true;
    }
    catch (const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
        return false;
    }
}

void BosePacket::getSoapRequest(std::string& request)
{
    if (GD::bl->debugLevel >= 4) GD::out.printInfo("Create request");
    try
	{
	    if(_functionName.empty())return;
	    if(_valuesToSet->empty())return;

	    xml_document doc;

	    xml_node* rootNode = new xml_node(node_element);
        if(!rootNode) return;
        doc.append_node(rootNode);
        rootNode->name(_functionName.data());

        if(_functionName == "play_info")
        {
            for(auto i = _valuesToSet->begin(); i < _valuesToSet->end(); ++i)
            {
                if(i->second.empty()) continue;

                xml_node* node = new xml_node(node_element);
                if(!node) return;
                rootNode->append_node(node);
                node->name(i->first.data());
                node->value(i->second.data());
            }
        }
        else if(_functionName == "key")
        {
            for(auto i = _valuesToSet->begin(); i < _valuesToSet->end(); ++i)
            {
                if(i->first == "key")
                {
                    rootNode->value(i->second.data());
                    continue;
                }

                xml_attribute* attr = new xml_attribute();
                if(!attr) return;
                rootNode->append_attribute(attr);
                attr->name(i->first.data());
                attr->value(i->second.data());
            }
        }
        else if(_functionName == "volume")
        {
            for(auto i = _valuesToSet->begin(); i < _valuesToSet->end(); ++i)
            {
                if(i->first == "volume") rootNode->value(i->second.data());
                break;
            }
        }
        else if(_functionName == "ContentItem")
        {
            for(auto i = _valuesToSet->begin(); i < _valuesToSet->end(); ++i)
            {
                xml_attribute* attr = new xml_attribute();
                if(!attr) return;
                rootNode->append_attribute(attr);
                attr->name(i->first.data());
                attr->value(i->second.data());
            }
        }
        else if(_functionName == "zone")
        {
            xml_node* node = new xml_node(node_element);
            if(!node) return;
            rootNode->append_node(node);
            node->name("member");

            for(auto i = _valuesToSet->begin(); i < _valuesToSet->end(); ++i)
            {
                if(i->first == "master") {
                    xml_attribute *attr = new xml_attribute();
                    if (!attr) return;
                    rootNode->append_attribute(attr);
                    attr->name(i->first.data());
                    attr->value(i->second.data());
                }
                else if(i->first == "slaveIp") {
                    xml_attribute *attr = new xml_attribute();
                    if (!attr) return;
                    node->append_attribute(attr);
                    attr->name("ipaddress");
                    attr->value(i->second.data());
                }
                else if(i->first == "slaveMac") {
                    node->value(i->second.data());
                }
            }
        }
        else if(_functionName == "bass")
        {
            for(auto i = _valuesToSet->begin(); i < _valuesToSet->end(); ++i)
            {
                if(i->first == "bass") rootNode->value(i->second.data());
                break;
            }
        }
        else if(_functionName == "name")
        {
            for(auto i = _valuesToSet->begin(); i < _valuesToSet->end(); ++i)
            {
                if(i->first == "name") rootNode->value(i->second.data());
                break;
            }
        }


        request = ("<?xml version=\"1.0\"?>\r\n");
        print(std::back_inserter(request), doc, 1);

        doc.clear();

        if (GD::bl->debugLevel >= 5) GD::out.printInfo("Request is " + request);
        return;
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void BosePacket::parseInfo(xml_node* info){
    try{
        _functionName = "info";

        uint32_t networkInfo = 1;
        uint32_t components = 1;
        for(xml_node* node = info->first_node(); node; node = node->next_sibling())
        {
            std::string name= node->name();
            if(name == "components")
            {
                for(xml_node* subNode = node->first_node(); subNode; subNode = subNode->next_sibling())
                {
                    for(xml_node* subSubNode = subNode->first_node(); subSubNode; subSubNode = subSubNode->next_sibling())
                    {
                        std::string subNodeName = subSubNode->name();
                        subNodeName.append(std::to_string(components));
                        _values->operator[](subNodeName) = subSubNode->value();
                    }
                    components++;
                }
            }
            else if(name == "networkInfo")
            {
                xml_attribute* attr = node->first_attribute("type");
                if(attr) _values->operator []("networkInfo_type") = attr->value();
                for(xml_node* subNode = node->first_node(); subNode; subNode = subNode->next_sibling())
                {
                    std::string subNodeName = subNode->name();
                    subNodeName.append(std::to_string(networkInfo));
                    _values->operator[](subNodeName) = subNode->value();
                }
                networkInfo++;
            }
            else if(node->first_node() == node->last_node()) {
                _values->operator[](name) = node->value();
                continue;
            }
        }
    }
    catch(const std::exception& ex){
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(ex.what()));
    }
}

void BosePacket::parseNowPlaying(xml_node* nowPlaying)
{
    try
    {
        _functionName = "nowPlaying";

        _values->operator[]("skipEnabled") = "false";
        _values->operator[]("skipPreviousEnabled") = "false";
        _values->operator[]("favoriteEnabled") = "false";
        _values->operator[]("isFavorite") = "false";
        _values->operator[]("rateEnabled") = "false";

        for(xml_attribute* attr= nowPlaying->first_attribute(); attr; attr = attr->next_attribute()){
            _values->operator [](std::string(attr->name())) = std::string(attr->value());
        }

        for(xml_node* node = nowPlaying->first_node(); node; node = node->next_sibling()){
            std::string name= node->name();
            if(name == "ContentItem"){
                for(xml_attribute* attr= node->first_attribute(); attr; attr = attr->next_attribute()){
                    _values->operator [](std::string(attr->name())) = std::string(attr->value());
                }
                for(xml_node* subNode = node->first_node(); subNode; subNode = subNode->next_sibling()){
                    std::string name = subNode->name();
                    _values->operator[](name) = subNode->value();
                }
            }
            else if(name == "art"){
                xml_attribute* attr= node->first_attribute("artImageStatus");
                if(!attr) continue;

                _values->operator [](std::string(attr->name())) = std::string(attr->value());
                _values->operator[](name) = node->value();
            }
            else if(name == "time"){
                xml_attribute* attr= node->first_attribute("total");
                if(!attr) continue;

                _values->operator [](std::string(attr->name())) = std::string(attr->value());
                _values->operator[](name) = node->value();
            }
            else if(name == "ConnectionStatusInfo"){
                for(xml_attribute* attr= node->first_attribute(); attr; attr = attr->next_attribute()) {
                    _values->operator[](std::string(attr->name())) = std::string(attr->value());
                }
            }
            else if(name == "skipEnabled" || name == "skipPreviousEnabled" || name == "favoriteEnabled" || name == "isFavorite" || name == "rateEnabled"){
                _values->operator[](name) = "true";
            }
            else {
                _values->operator[](name) = node->value();
                continue;
            }
        }
        return;
    }
    catch(const std::exception& ex){
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(ex.what()));
    }
}

void BosePacket::parseVolume(xml_node* volume){
    try{
        _functionName = "volume";

        for(xml_node* subNode = volume->first_node(); subNode; subNode = subNode->next_sibling()){
            std::string subNodeName = subNode->name();
            _values->operator[](subNodeName) = subNode->value();
        }
        return;
    }
    catch(const std::exception& ex){
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(ex.what()));
    }
}

void BosePacket::parseSources(xml_node* sources){
    try{
        _functionName = "sources";

        uint32_t iMember = 1;
        for(xml_node* subNode = sources->first_node(); subNode; subNode = subNode->next_sibling())
        {
            _values->operator[](("isLocal" + std::to_string(iMember))) = "false";
            _values->operator[](("multiroomallowed"+ std::to_string(iMember))) = "false";

            for(xml_attribute* attr= subNode->first_attribute(); attr; attr = attr->next_attribute()) {
                std::string name = attr->name();
                name.append(std::to_string(iMember));
                _values->operator[](name) = std::string(attr->value());
            }
            std::string subNodeName = "name";
            subNodeName.append(std::to_string(iMember++));
            _values->operator[](subNodeName) = subNode->value();
        }
    }
    catch(const std::exception& ex){
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(ex.what()));
    }
}

void BosePacket::parseGetZone(xml_node* zone){
    try{
        _functionName = "zone";
        {
            xml_attribute *attr = zone->first_attribute("master");
            if (!attr) return;
            _values->operator[](std::string(attr->name())) = std::string(attr->value());
        }
        uint32_t iMember = 1;
        for(xml_node* subNode = zone->first_node(); subNode; subNode = subNode->next_sibling())
        {
            std::string subNodeName = subNode->name();
            if(subNodeName != "member") continue;

            subNodeName.append(std::to_string(iMember++));

            _values->operator[](subNodeName+"mac") = subNode->value();

            xml_attribute *attr = subNode->first_attribute("ipaddress");
            if (!attr) continue;
            _values->operator[](subNodeName + std::string(attr->name())) = std::string(attr->value());
        }
    }
    catch(const std::exception& ex){
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(ex.what()));
    }
}

void BosePacket::parseBassCapabilities(xml_node* bassCapabilities){
    try{
        _functionName = "bassCapabilities";

        for(xml_node* subNode = bassCapabilities->first_node(); subNode; subNode = subNode->next_sibling())
        {
            std::string subNodeName = subNode->name();
            _values->operator[](subNodeName) = subNode->value();
        }
    }
    catch(const std::exception& ex){
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(ex.what()));
    }
}

void BosePacket::parseBass(xml_node* bass){
    try{
        _functionName = "bass";

        for(xml_node* subNode = bass->first_node(); subNode; subNode = subNode->next_sibling())
        {
            std::string subNodeName = subNode->name();
            _values->operator[](subNodeName) = subNode->value();
        }
    }
    catch(const std::exception& ex){
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(ex.what()));
    }
}

void BosePacket::parsePresets(xml_node* presets){
    try{
        _functionName = "presets";

        for(xml_node* preset = presets->first_node(); preset; preset = preset->next_sibling())
        {
            parsePreset(preset);
        }
    }
    catch(const std::exception& ex){
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(ex.what()));
    }
}

void BosePacket::parsePreset(xml_node* preset) {
    try{
        std::string presetId = "preset";
        {
            xml_attribute *attr = preset->first_attribute("id");
            if (!attr) return;
            presetId += attr->value();
        }
        for(xml_attribute* attr= preset->first_attribute(); attr; attr = attr->next_attribute()) {
            std::string name = attr->name();
            if(name =="id") continue;
            _values->operator[](presetId + name) = std::string(attr->value());
        }

        xml_node* content = preset->first_node("ContentItem");
        if(!content) return;

        for(xml_attribute* attr= content->first_attribute(); attr; attr = attr->next_attribute()) {
            std::string name = attr->name();
            _values->operator[](presetId + name) = std::string(attr->value());
        }
        for(xml_node* subNode = content->first_node(); subNode; subNode = subNode->next_sibling())
        {
            std::string name = subNode->name();
            _values->operator[](presetId + name) = std::string(subNode->value());
        }
    }
    catch(const std::exception& ex){
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(ex.what()));
    }
}

void BosePacket::parseGetGroup(xml_node* group){
    try{
        _functionName = "group";

        xml_attribute *attr = group->first_attribute("id");
        if (!attr) return;
        _values->operator[]("groupId") = std::string(attr->value());

        for(xml_node* subNode = group->first_node(); subNode; subNode = subNode->next_sibling())
        {
            std::string name = subNode->name();
            if(name =="roles")
            {
                uint32_t iRole = 1;
                for(xml_node* groupRole = subNode->first_node(); groupRole; groupRole = groupRole->next_sibling())
                {
                    for(xml_node* subSubNode = groupRole->first_node(); subSubNode; subSubNode = subSubNode->next_sibling())
                    {
                        _values->operator[]("groupRole" + std::to_string(iRole) + subSubNode->name()) = std::string(subSubNode->value());
                    }
                    iRole++;
                }
                continue;
            }
            _values->operator[](name) = std::string(subNode->value());
        }
    }
    catch(const std::exception& ex){
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(ex.what()));
    }
}

void BosePacket::parseError(xml_node* error){
    try{
        _functionName = "error";

        _values->operator[](error->name()) = std::string(error->value());

        for(xml_attribute* attr= error->first_attribute(); attr; attr = attr->next_attribute()) {
            std::string name = attr->name();
            _values->operator[](name) = std::string(attr->value());
        }
    }
    catch(const std::exception& ex){
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(ex.what()));
    }
}

}

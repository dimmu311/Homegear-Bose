#ifndef BosePACKET_H_
#define BosePACKET_H_

#include <homegear-base/BaseLib.h>
#include "homegear-base/Encoding/RapidXml/rapidxml.h"
#include "homegear-base/Encoding/RapidXml/rapidxml_print.hpp"

#include <unordered_map>

namespace Bose
{

class BosePacket : public BaseLib::Systems::Packet
{
    public:
        BosePacket();
        BosePacket(std::string& soap, std::string serialNumber, int64_t timeReceived = 0);
        BosePacket(std::string& soap, int64_t timeReceived = 0);
        BosePacket(xml_node* node, std::string serialNumber, int64_t timeReceived = 0);
        BosePacket(xml_node* node, int64_t timeReceived = 0);
        BosePacket(std::string& functionName, std::shared_ptr<std::vector<std::pair<std::string, std::string>>> valuesToSet);
        virtual ~BosePacket();

        std::string serialNumber() { return _serialNumber; }
        std::string functionName() { return _functionName; }
        std::string deviceId() {return _deviceId;}

        std::shared_ptr<std::unordered_map<std::string, std::string>> values() { return _values; }
        //std::shared_ptr<std::pair<std::string, BaseLib::PVariable>> browseResult() { return _browseResult; }

        void getSoapRequest(std::string& request);
    protected:
        //To device
        std::shared_ptr<std::vector<std::pair<std::string, std::string>>> _valuesToSet;

        //From device
        std::string _serialNumber;
        std::string _functionName;
        std::string _deviceId;

        std::shared_ptr<std::unordered_map<std::string, std::string>> _values;

        bool setDeviceId(xml_node* node);

        //std::shared_ptr<std::pair<std::string, BaseLib::PVariable>> _browseResult;

        void parseInfo(xml_node* node);
        void parseNowPlaying(xml_node* node);
        void parseVolume(xml_node* node);
        void parseSources(xml_node* node);
        void parseGetZone(xml_node* node);
        void parseBassCapabilities(xml_node* node);
        void parseBass(xml_node* node);
        void parsePresets(xml_node* node);
        void parsePreset(xml_node* node);
        void parseGetGroup(xml_node* node);
        void parseError(xml_node* error);

};

}
#endif

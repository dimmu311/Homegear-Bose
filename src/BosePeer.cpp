#include "BosePeer.h"
#include "BoseCentral.h"
#include "BosePacket.h"
#include "GD.h"

#include <homegear-base/Managers/ProcessManager.h>

#include "sys/wait.h"

#include <iomanip>

namespace Bose
{
std::shared_ptr<BaseLib::Systems::ICentral> BosePeer::getCentral()
{
	try
	{
		if(_central) return _central;
		_central = GD::family->getCentral();
		return _central;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return std::shared_ptr<BaseLib::Systems::ICentral>();
}

BosePeer::BosePeer(uint32_t parentID, IPeerEventSink* eventHandler) : BaseLib::Systems::Peer(GD::bl, parentID, eventHandler)
{
	init();
}

BosePeer::BosePeer(int32_t id, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler) : BaseLib::Systems::Peer(GD::bl, id, -1, serialNumber, parentID, eventHandler)
{
	init();
}

BosePeer::~BosePeer()
{
	try{
        std::vector<char> output;
        BaseLib::WebSocket::encodeClose(output);
        try
        {
            GD::out.printInfo("Info: Sending command " + std::string(output.begin(), output.end()));
            _tcpSocket->proofwrite(output);
        }
        catch(const BaseLib::SocketOperationException& ex)
        {
            GD::out.printError("Error sending packet: " + std::string(ex.what()));
        }

        stopListening();
        _bl->threadManager.join(_initThread);
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

void BosePeer::init() {
    _isMaster = false;
    _isStream = false;

    _binaryEncoder.reset(new BaseLib::Rpc::RpcEncoder(GD::bl));
    _binaryDecoder.reset(new BaseLib::Rpc::RpcDecoder(GD::bl));
}

void BosePeer::wsInit(){
    GD::out.printInfo("openWebsocket Connection");

    _websocket = false;

    std::string header="GET / HTTP/1.1\r\nHost: " + _ip + ":8080\r\nUpgrade: websocket\r\nConnection: upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Protocol: gabbo\r\n\r\n";
                    //GET / HTTP/1.1<CR><LF>Host: 192.168.41.40:8080<CR><LF>Upgrade: websocket<CR><LF>Connection: Upgrade<CR><LF>Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==<CR><LF>Sec-WebSocket-Version: 13<CR><LF>Sec-WebSocket-Protocol: gabbo<CR><LF><CR><LF>
    auto responsePacket = getResponse("101", header);
    if (!responsePacket)
    {
        GD::out.printError("Error: Could not open Websocket Connection");
        _stopped = true;
        return;
    }
    /*
    if (!loxoneHttpPacket || loxoneHttpPacket->getResponseCode() != 101)
    {
        _out.printError("Error: Could not open Websocket Connection");
        _stopped = true;
        return;
    }
    auto webSocket = encodeWebSocket(command, WebSocket::Header::Opcode::Enum::text);
    auto responsePacket = getResponse("jdev/sys/keyexchange/", webSocket);
    */
}

void BosePeer::startListening()
{
    try
    {
        stopListening();

        _tcpSocket = std::make_shared<BaseLib::TcpSocket>(GD::bl, _ip, "8080", false, std::string(), false);
        _tcpSocket->setConnectionRetries(1);
        _tcpSocket->setReadTimeout(1000000);
        _tcpSocket->setWriteTimeout(1000000);
        _stopCallbackThread = false;

        //if(_settings->listenThreadPriority > -1) _bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &BosePeer::listen, this);
        //else
        _bl->threadManager.start(_listenThread, true, &BosePeer::listen, this);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}
void BosePeer::stopListening()
{
    try{
        _stopCallbackThread = true;
        if(_tcpSocket) _tcpSocket->close();
        _bl->threadManager.join(_listenThread);

        _stopped = true;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::shared_ptr<BosePacket> BosePeer::getResponse(const std::string& responseCommand, const std::string& command, int32_t waitForSeconds)
{
    try
    {
        if(_stopped) return std::shared_ptr<BosePacket>();

        std::lock_guard<std::mutex> sendPacketGuard(_sendPacketMutex);
        std::lock_guard<std::mutex> getResponseGuard(_getResponseMutex);
        std::shared_ptr<Request> request = std::make_shared<Request>();
        std::unique_lock<std::mutex> requestsGuard(_responsesMutex);
        _responses[responseCommand] = request;
        requestsGuard.unlock();
        std::unique_lock<std::mutex> lock(request->mutex);

        try
        {
            GD::out.printInfo("Info: Sending command " + command);
            _tcpSocket->proofwrite(command);
        }
        catch(const BaseLib::SocketOperationException& ex)
        {
            GD::out.printError("Error sending packet: " + std::string(ex.what()));
            return std::shared_ptr<BosePacket>();
        }

        int32_t i = 0;
        while(!request->conditionVariable.wait_for(lock, std::chrono::milliseconds(1000), [&]
        {
            i++;
            return request->mutexReady || _stopped || i == waitForSeconds;
        }));

        if (i == waitForSeconds || !request->response)
        {
            GD::out.printError("Error: No response received to command: " + command);
            return std::shared_ptr<BosePacket>();
        }
        auto responsePacket = request->response;

        requestsGuard.lock();
        _responses.erase(responseCommand);
        requestsGuard.unlock();

        return responsePacket;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return std::shared_ptr<BosePacket>();
}

void BosePeer::listen()
{
    try
    {
        _tcpSocket->open();
        if(_tcpSocket->connected())
        {
            GD::out.printInfo("Info: Successfully connected.");
            _stopped = false;
            _bl->threadManager.start(_initThread, true, &BosePeer::wsInit, this);
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    BaseLib::Http http;
    BaseLib::WebSocket websocket;
    while(!_stopCallbackThread) {
        try {
            if(_stopped || !_tcpSocket->connected()){
                if(_stopCallbackThread) return;
                if(_stopped) GD::out.printWarning("Warning: Connection to device closed. Trying to reconnect...");
                _tcpSocket->close();
                for(int32_t i = 0; i < 15; i++)
                {
                    if(_stopCallbackThread) continue;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
                _tcpSocket->open();
                if(_tcpSocket->connected())
                {
                    GD::out.printInfo("Info: Successfully connected.");
                    _stopped = false;
                    _bl->threadManager.start(_initThread, true, &BosePeer::wsInit, this);
                }
                continue;
            }

            std::vector<uint8_t> buffer(1024);
            uint32_t bytesRead = 0;
            try
            {
                bytesRead = _tcpSocket->proofread((char*)buffer.data(), buffer.size());
                //GD::out.printInfo("#########################" + std::to_string(bytesRead));
                //GD::out.printInfo("#########################" + std::string(buffer.begin(), buffer.end()));
            }
            catch (BaseLib::SocketTimeOutException& ex)
            {
                if (_stopCallbackThread) continue;
                continue;
            }
            if (bytesRead <= 0) continue;
            if (bytesRead > 1024) bytesRead = 1024;

            uint32_t processed = 0;
            do {
                //GD::out.printInfo("########### do while ");
                if(!_websocket) {
                    processed += http.process((char*)buffer.data() + processed, bytesRead - processed, false, true);

                    //GD::out.printInfo("########### processed " + std::to_string(processed) + " - - " + std::string(http.getContent().begin(), http.getContent().end()) + " - " + std::to_string(http.getHeader().responseCode));
                    //if (http.isFinished())
                    //i think there is a problem in BaseLib::http when trying to pare http telegramm that only contains a header and no content. isFinished() gets never true with the bose ws handshake.
                    //{
                        //GD::out.printInfo("########### finished " + std::to_string(http.getContentSize()));
                        if (http.getHeader().responseCode == 101)
                        {
                            _websocket = true;

                            std::string content(http.getContent().begin(),http.getContent().end());
                            auto packet= std::make_shared<BosePacket>(content);

                            std::unique_lock<std::mutex> requestsGuard(_responsesMutex);
                            auto responsesIterator = _responses.find("101");
                            if (responsesIterator != _responses.end())
                            {
                                auto request = responsesIterator->second;
                                requestsGuard.unlock();
                                request->response = packet;
                                {
                                    std::lock_guard<std::mutex> lock(request->mutex);
                                    request->mutexReady = true;
                                }
                                request->conditionVariable.notify_one();
                                continue;
                            }
                            else requestsGuard.unlock();
                        }
                        http.reset();
                        //GD::out.printInfo("########### now continue ");
                        continue;
                    //}
                }

                //GD::out.printInfo("########### do while websocket");
                processed += websocket.process((char*)buffer.data() + processed, bytesRead - processed);
                //GD::out.printInfo("------------ processed " + std::to_string(processed) + " - - " + std::to_string(websocket.getHeader().length));
                //GD::out.printInfo("------------" + std::string(websocket.getContent().begin(), websocket.getContent().end()));

                if(websocket.isFinished()){
                    std::string content(websocket.getContent().begin(),websocket.getContent().end());
                    auto packet= std::make_shared<BosePacket>(content);
                    packetReceived(packet);
                    websocket.reset();
                    continue;
                }
            } while (processed < bytesRead);
            //GD::out.printInfo("########### do while end");
        }
        catch(const std::exception& ex)
        {
            _stopped = true;
            GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
        }
    }
}

void BosePeer::setIp(std::string value)
{
	try
	{
		Peer::setIp(value);
		std::string settingName = "readtimeout";
		BaseLib::Systems::FamilySettings::PFamilySetting readTimeoutSetting = GD::family->getFamilySetting(settingName);
		int32_t readTimeout = 10000;
		if(readTimeoutSetting) readTimeout = readTimeoutSetting->integerValue;
		if(readTimeout < 1 || readTimeout > 120000) readTimeout = 10000;
		_httpClient.reset(new BaseLib::HttpClient(GD::bl, _ip, 8090, false));
		_httpClient->setTimeout(readTimeout);

        //_bl->threadManager.start(_initThread, true, &BosePeer::wsInit, this);
        startListening();
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

std::string BosePeer::getRinconId()
{
	try
	{
		if(!_rpcDevice) return "";
		Functions::iterator functionIterator = _rpcDevice->functions.find(1);
		if(functionIterator == _rpcDevice->functions.end()) return "";
		PParameter parameter = functionIterator->second->variables->getParameter("ID");
		if(!parameter) return "";
		std::vector<uint8_t> parameterData = valuesCentral[1]["ID"].getBinaryData();
		return parameter->convertFromPacket(parameterData, Role(), false)->stringValue;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return "";
}

void BosePeer::setRinconId(std::string value)
{
	try
	{
		BaseLib::Systems::RpcConfigurationParameter& configParameter = valuesCentral[1]["ID"];
		if(!configParameter.rpcParameter) return;
		std::vector<uint8_t> parameterData;
		configParameter.rpcParameter->convertToPacket(PVariable(new Variable(value)), Role(), parameterData);
		if(configParameter.equals(parameterData)) return;
		configParameter.setBinaryData(parameterData);
		if(configParameter.databaseId > 0) saveParameter(configParameter.databaseId, parameterData);
		else saveParameter(0, ParameterGroup::Type::Enum::variables, 1, "ID", parameterData);
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

void BosePeer::setRoomName(std::string value, bool broadCastEvent)
{
	try
	{
		BaseLib::Systems::RpcConfigurationParameter& configParameter = valuesCentral[1]["ROOMNAME"];
		if(!configParameter.rpcParameter) return;
		PVariable variable(new Variable(value));
		std::vector<uint8_t> parameterData;
		configParameter.rpcParameter->convertToPacket(variable, Role(), parameterData);
		if(configParameter.equals(parameterData)) return;
		configParameter.setBinaryData(parameterData);
		if(configParameter.databaseId > 0) saveParameter(configParameter.databaseId, parameterData);
		else saveParameter(0, ParameterGroup::Type::Enum::variables, 1, "ROOMNAME", parameterData);

		if(broadCastEvent)
		{
			std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>{ "ROOMNAME" });
			std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable>{ variable });
            std::string eventSource = "device-" + std::to_string(_peerID);
			std::string address = _serialNumber + ":1";
            raiseEvent(eventSource, _peerID, 1, valueKeys, values);
			raiseRPCEvent(eventSource, _peerID, 1, address, valueKeys, values);
		}
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

void BosePeer::homegearShuttingDown()
{
	try
	{
		Peer::homegearShuttingDown();
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

std::string BosePeer::handleCliCommand(std::string command)
{
	try
	{
		std::ostringstream stringStream;

		if(command == "help")
		{
			stringStream << "List of commands:" << std::endl << std::endl;
			stringStream << "For more information about the individual command type: COMMAND help" << std::endl << std::endl;
			stringStream << "unselect\t\tUnselect this peer" << std::endl;
			stringStream << "channel count\t\tPrint the number of channels of this peer" << std::endl;
			stringStream << "config print\t\tPrints all configuration parameters and their values" << std::endl;
			return stringStream.str();
		}
		if(command.compare(0, 13, "channel count") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command prints this peer's number of channels." << std::endl;
						stringStream << "Usage: channel count" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			stringStream << "Peer has " << _rpcDevice->functions.size() << " channels." << std::endl;
			return stringStream.str();
		}
		else if(command.compare(0, 12, "config print") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command prints all configuration parameters of this peer. The values are in BidCoS packet format." << std::endl;
						stringStream << "Usage: config print" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			return printConfig();
		}
		else return "Unknown command.\n";
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return "Error executing command. See log file for more details.\n";
}

void BosePeer::addPeer(std::shared_ptr<BaseLib::Systems::BasicPeer> peer)
{
	try
	{

		if(_rpcDevice->functions.find(1) == _rpcDevice->functions.end()) return;
		std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>& channel1Peers = _peers[1];
		for(std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>::iterator i = channel1Peers.begin(); i != channel1Peers.end(); ++i)
		{
			if((*i)->id == peer->id)
			{
				channel1Peers.erase(i);
				break;
			}
		}
		channel1Peers.push_back(peer);
		savePeers();
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void BosePeer::removePeer(uint64_t id)
{
	try
	{
		if(_peers.find(1) == _peers.end()) return;
		std::shared_ptr<BoseCentral> central(std::dynamic_pointer_cast<BoseCentral>(getCentral()));

		std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>& channel1Peers = _peers[1];
		for(std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>::iterator i = channel1Peers.begin(); i != channel1Peers.end(); ++i)
		{
			if((*i)->id == id)
			{
				channel1Peers.erase(i);
				savePeers();
				return;
			}
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void BosePeer::savePeers()
{
	try
	{
		std::vector<uint8_t> serializedData;
		serializePeers(serializedData);
		saveVariable(12, serializedData);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void BosePeer::serializePeers(std::vector<uint8_t>& encodedData)
{
	try
	{
		BaseLib::BinaryEncoder encoder(_bl);
		encoder.encodeInteger(encodedData, _peers.size());
		for(std::unordered_map<int32_t, std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>>::const_iterator i = _peers.begin(); i != _peers.end(); ++i)
		{
			encoder.encodeInteger(encodedData, i->first);
			encoder.encodeInteger(encodedData, i->second.size());
			for(std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				if(!*j) continue;
				encoder.encodeBoolean(encodedData, (*j)->isSender);
				encoder.encodeInteger(encodedData, (*j)->id);
				encoder.encodeInteger(encodedData, (*j)->address);
				encoder.encodeInteger(encodedData, (*j)->channel);
				encoder.encodeString(encodedData, (*j)->serialNumber);
				encoder.encodeBoolean(encodedData, (*j)->isVirtual);
				encoder.encodeString(encodedData, (*j)->linkName);
				encoder.encodeString(encodedData, (*j)->linkDescription);
				encoder.encodeInteger(encodedData, (*j)->data.size());
				encodedData.insert(encodedData.end(), (*j)->data.begin(), (*j)->data.end());
			}
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void BosePeer::unserializePeers(std::shared_ptr<std::vector<char>> serializedData)
{
	try
	{
		BaseLib::BinaryDecoder decoder(_bl);
		uint32_t position = 0;
		uint32_t peersSize = decoder.decodeInteger(*serializedData, position);
		for(uint32_t i = 0; i < peersSize; i++)
		{
			uint32_t channel = decoder.decodeInteger(*serializedData, position);
			uint32_t peerCount = decoder.decodeInteger(*serializedData, position);
			for(uint32_t j = 0; j < peerCount; j++)
			{
				std::shared_ptr<BaseLib::Systems::BasicPeer> basicPeer(new BaseLib::Systems::BasicPeer());
				basicPeer->hasSender = true;
				basicPeer->isSender = decoder.decodeBoolean(*serializedData, position);
				basicPeer->id = decoder.decodeInteger(*serializedData, position);
				basicPeer->address = decoder.decodeInteger(*serializedData, position);
				basicPeer->channel = decoder.decodeInteger(*serializedData, position);
				basicPeer->serialNumber = decoder.decodeString(*serializedData, position);
				basicPeer->isVirtual = decoder.decodeBoolean(*serializedData, position);
				_peers[channel].push_back(basicPeer);
				basicPeer->linkName = decoder.decodeString(*serializedData, position);
				basicPeer->linkDescription = decoder.decodeString(*serializedData, position);
				uint32_t dataSize = decoder.decodeInteger(*serializedData, position);
				if(position + dataSize <= serializedData->size()) basicPeer->data.insert(basicPeer->data.end(), serializedData->begin() + position, serializedData->begin() + position + dataSize);
				position += dataSize;
			}
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::string BosePeer::printConfig()
{
	try
	{
		std::ostringstream stringStream;
		stringStream << "MASTER" << std::endl;
		stringStream << "{" << std::endl;
		for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator i = configCentral.begin(); i != configCentral.end(); ++i)
		{
			stringStream << "\t" << "Channel: " << std::dec << i->first << std::endl;
			stringStream << "\t{" << std::endl;
			for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				stringStream << "\t\t[" << j->first << "]: ";
				if(!j->second.rpcParameter) stringStream << "(No RPC parameter) ";
				std::vector<uint8_t> parameterData = j->second.getBinaryData();
				for(std::vector<uint8_t>::const_iterator k = parameterData.begin(); k != parameterData.end(); ++k)
				{
					stringStream << std::hex << std::setfill('0') << std::setw(2) << (int32_t)*k << " ";
				}
				stringStream << std::endl;
			}
			stringStream << "\t}" << std::endl;
		}
		stringStream << "}" << std::endl << std::endl;

		stringStream << "VALUES" << std::endl;
		stringStream << "{" << std::endl;
		for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator i = valuesCentral.begin(); i != valuesCentral.end(); ++i)
		{
			stringStream << "\t" << "Channel: " << std::dec << i->first << std::endl;
			stringStream << "\t{" << std::endl;
			for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				stringStream << "\t\t[" << j->first << "]: ";
				if(!j->second.rpcParameter) stringStream << "(No RPC parameter) ";
				std::vector<uint8_t> parameterData = j->second.getBinaryData();
				for(std::vector<uint8_t>::const_iterator k = parameterData.begin(); k != parameterData.end(); ++k)
				{
					stringStream << std::hex << std::setfill('0') << std::setw(2) << (int32_t)*k << " ";
				}
				stringStream << std::endl;
			}
			stringStream << "\t}" << std::endl;
		}
		stringStream << "}" << std::endl << std::endl;

		return stringStream.str();
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return "";
}


void BosePeer::loadVariables(BaseLib::Systems::ICentral* central, std::shared_ptr<BaseLib::Database::DataTable>& rows)
{
	try
	{
		std::string settingName = "readtimeout";
		BaseLib::Systems::FamilySettings::PFamilySetting readTimeoutSetting = GD::family->getFamilySetting(settingName);
		int32_t readTimeout = 10000;
		if(readTimeoutSetting) readTimeout = readTimeoutSetting->integerValue;
		if(readTimeout < 1 || readTimeout > 120000) readTimeout = 10000;

		if(!rows) rows = _bl->db->getPeerVariables(_peerID);
		Peer::loadVariables(central, rows);
		for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
		{
			_variableDatabaseIDs[row->second.at(2)->intValue] = row->second.at(0)->intValue;
			switch(row->second.at(2)->intValue)
			{
			case 12:
				unserializePeers(row->second.at(5)->binaryValue);
				break;
			}
		}
		_httpClient.reset(new BaseLib::HttpClient(GD::bl, _ip, 8090, false));
		_httpClient->setTimeout(readTimeout);

        //_bl->threadManager.start(_initThread, true, &BosePeer::wsInit, this);
        startListening();
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

bool BosePeer::load(BaseLib::Systems::ICentral* central)
{
	try
	{
		std::shared_ptr<BaseLib::Database::DataTable> rows;
		loadVariables(central, rows);

		_rpcDevice = GD::family->getRpcDevices()->find(_deviceType, 0x10, -1);
		if(!_rpcDevice)
		{
			GD::out.printError("Error loading Bose peer " + std::to_string(_peerID) + ": Device type not found: 0x" + BaseLib::HelperFunctions::getHexString(_deviceType) + " Firmware version: " + std::to_string(_firmwareVersion));
			return false;
		}
		initializeTypeString();
		std::string entry;
		loadConfig();
		initializeCentralConfig();

		serviceMessages.reset(new BaseLib::Systems::ServiceMessages(_bl, _peerID, _serialNumber, this));
		serviceMessages->load();

		std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelOneIterator = valuesCentral.find(1);
		if(channelOneIterator != valuesCentral.end())
		{
			auto parameterIterator = channelOneIterator->second.find("IS_MASTER");
			if(parameterIterator != channelOneIterator->second.end())
			{
				std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
				PVariable variable = _binaryDecoder->decodeResponse(parameterData);
				if(variable) _isMaster = variable->booleanValue;
			}

            parameterIterator = channelOneIterator->second.find("IS_STREAM");
            if(parameterIterator != channelOneIterator->second.end())
            {
                std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
                PVariable variable = _binaryDecoder->decodeResponse(parameterData);
                if(variable) _isStream = variable->booleanValue;
            }
		}
		return true;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return false;
}

void BosePeer::saveVariables()
{
	try
	{
		if(_peerID == 0) return;
		Peer::saveVariables();
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void BosePeer::getValuesFromPacket(std::shared_ptr<BosePacket> packet, std::vector<FrameValues>& frameValues)
{
	try
	{
		if(!_rpcDevice) return;
		//equal_range returns all elements with "0" or an unknown element as argument
		if(_rpcDevice->packetsByFunction1.find(packet->functionName()) == _rpcDevice->packetsByFunction1.end()) return;
		std::pair<PacketsByFunction::iterator, PacketsByFunction::iterator> range = _rpcDevice->packetsByFunction1.equal_range(packet->functionName());
		if(range.first == _rpcDevice->packetsByFunction1.end()) return;
		PacketsByFunction::iterator i = range.first;
		std::shared_ptr<std::unordered_map<std::string, std::string>> soapValues;
		do
		{
			FrameValues currentFrameValues;
			PPacket frame(i->second);
			if(!frame) continue;
			if(frame->direction != Packet::Direction::Enum::toCentral) continue;
			int32_t channel = -1;
			if(frame->channel > -1) channel = frame->channel;
			currentFrameValues.frameID = frame->id;
            std::string field;

			for(JsonPayloads::iterator j = frame->jsonPayloads.begin(); j != frame->jsonPayloads.end(); ++j)
            {
                soapValues = packet->values();
                if((!soapValues || soapValues->find((*j)->key) == soapValues->end())) continue;
                field = (*j)->key;

				for(std::vector<PParameter>::iterator k = frame->associatedVariables.begin(); k != frame->associatedVariables.end(); ++k)
				{
					if((*k)->physical->groupId != (*j)->parameterId) continue;
					currentFrameValues.parameterSetType = (*k)->parent()->type();
					bool setValues = false;
					if(currentFrameValues.paramsetChannels.empty()) //Fill paramsetChannels
					{
						int32_t startChannel = (channel < 0) ? 0 : channel;
						int32_t endChannel;
						//When fixedChannel is -2 (means '*') cycle through all channels
						if(frame->channel == -2)
						{
							startChannel = 0;
							endChannel = _rpcDevice->functions.rbegin()->first;
						}
						else endChannel = startChannel;
						for(int32_t l = startChannel; l <= endChannel; l++)
						{
							Functions::iterator functionIterator = _rpcDevice->functions.find(l);
							if(functionIterator == _rpcDevice->functions.end()) continue;
							PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(currentFrameValues.parameterSetType);
							Parameters::iterator parameterIterator = parameterGroup->parameters.find((*k)->id);
							if(parameterIterator == parameterGroup->parameters.end()) continue;
							currentFrameValues.paramsetChannels.push_back(l);
							currentFrameValues.values[(*k)->id].channels.push_back(l);
							setValues = true;
						}
					}
					else //Use paramsetChannels
					{
						for(std::list<uint32_t>::const_iterator l = currentFrameValues.paramsetChannels.begin(); l != currentFrameValues.paramsetChannels.end(); ++l)
						{
							Functions::iterator functionIterator = _rpcDevice->functions.find(*l);
							if(functionIterator == _rpcDevice->functions.end()) continue;
							PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(currentFrameValues.parameterSetType);
							Parameters::iterator parameterIterator = parameterGroup->parameters.find((*k)->id);
							if(parameterIterator == parameterGroup->parameters.end()) continue;
							currentFrameValues.values[(*k)->id].channels.push_back(*l);
							setValues = true;
						}
					}

					if(setValues)
					{
						//This is a little nasty and costs a lot of resources, but we need to run the data through the packet converter
						std::vector<uint8_t> encodedData;
						_binaryEncoder->encodeResponse(Variable::fromString(soapValues->at(field), (*k)->physical->type), encodedData);
						 PVariable data = (*k)->convertFromPacket(encodedData, Role(), true);
						(*k)->convertToPacket(data, Role(), currentFrameValues.values[(*k)->id].value);
					}
				}
			}
			if(!currentFrameValues.values.empty()) frameValues.push_back(currentFrameValues);
		} while(++i != range.second && i != _rpcDevice->packetsByFunction1.end());
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void BosePeer::packetReceived(std::shared_ptr<BosePacket> packet)
{
	try
	{
		if(!packet) return;
		if(_disposing) return;
		if(!_rpcDevice) return;

		if(!packet->deviceId().empty() && packet->deviceId() != _serialNumber) return;

		setLastPacketReceived();
		std::vector<FrameValues> frameValues;
		getValuesFromPacket(packet, frameValues);
		std::map<uint32_t, std::shared_ptr<std::vector<std::string>>> valueKeys;
		std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>> rpcValues;

		//Loop through all matching frames
		for(std::vector<FrameValues>::iterator a = frameValues.begin(); a != frameValues.end(); ++a)
		{
			PPacket frame;
			if(!a->frameID.empty()) frame = _rpcDevice->packetsById.at(a->frameID);

			for(std::unordered_map<std::string, FrameValue>::iterator i = a->values.begin(); i != a->values.end(); ++i)
			{
				for(std::list<uint32_t>::const_iterator j = a->paramsetChannels.begin(); j != a->paramsetChannels.end(); ++j)
				{
					if(std::find(i->second.channels.begin(), i->second.channels.end(), *j) == i->second.channels.end()) continue;
					BaseLib::Systems::RpcConfigurationParameter& parameter = valuesCentral[*j][i->first];
					if(!valueKeys[*j] || !rpcValues[*j])
					{
						valueKeys[*j].reset(new std::vector<std::string>());
						rpcValues[*j].reset(new std::vector<PVariable>());
					}

					if(!valueKeys[1] || !rpcValues[1])
					{
						valueKeys[1].reset(new std::vector<std::string>());
						rpcValues[1].reset(new std::vector<PVariable>());
					}

					if(parameter.rpcParameter)
					{
						//Process service messages
						if(parameter.rpcParameter->service && !i->second.value.empty())
						{
							if(parameter.rpcParameter->logical->type == ILogical::Type::Enum::tEnum)
							{
								serviceMessages->set(i->first, i->second.value.at(i->second.value.size() - 1), *j);
							}
							else if(parameter.rpcParameter->logical->type == ILogical::Type::Enum::tBoolean)
							{
								serviceMessages->set(i->first, (bool)i->second.value.at(i->second.value.size() - 1));
							}
						}

						PVariable value = parameter.rpcParameter->convertFromPacket(i->second.value, parameter.mainRole(), true);

						parameter.setBinaryData(i->second.value);
						if(parameter.databaseId > 0) saveParameter(parameter.databaseId, i->second.value);
						else saveParameter(0, ParameterGroup::Type::Enum::variables, *j, i->first, i->second.value);
						if(_bl->debugLevel >= 4) GD::out.printInfo("Info: " + i->first + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(*j) + " was set.");

						valueKeys[*j]->push_back(i->first);
						rpcValues[*j]->push_back(value);
					}
				}
			}
		}

		if(!rpcValues.empty())
		{
			for(std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>::iterator j = valueKeys.begin(); j != valueKeys.end(); ++j)
			{
				if(j->second->empty()) continue;

                std::string eventSource = "device-" + std::to_string(_peerID);
                std::string address(_serialNumber + ":" + std::to_string(j->first));
                raiseEvent(eventSource, _peerID, j->first, j->second, rpcValues.at(j->first));
                raiseRPCEvent(eventSource, _peerID, j->first, address, j->second, rpcValues.at(j->first));
			}
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

PVariable BosePeer::getValueFromDevice(PParameter& parameter, int32_t channel, bool asynchronous)
{
	try
	{
		if(!parameter) return Variable::createError(-32500, "parameter is nullptr.");
		if(parameter->getPackets.empty()) return Variable::createError(-6, "Parameter can't be requested actively.");
		std::string getRequestFrame = parameter->getPackets.front()->id;
		std::string getResponseFrame = parameter->getPackets.front()->responseId;
		if(_rpcDevice->packetsById.find(getRequestFrame) == _rpcDevice->packetsById.end()) return Variable::createError(-6, "No frame was found for parameter " + parameter->id);
		PPacket frame = _rpcDevice->packetsById[getRequestFrame];
		PPacket responseFrame;
		if(_rpcDevice->packetsById.find(getResponseFrame) != _rpcDevice->packetsById.end()) responseFrame = _rpcDevice->packetsById[getResponseFrame];

		if(valuesCentral.find(channel) == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
		if(valuesCentral[channel].find(parameter->id) == valuesCentral[channel].end()) return Variable::createError(-5, "Unknown parameter.");

		PParameterGroup parameterGroup = getParameterSet(channel, ParameterGroup::Type::Enum::variables);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set.");

		std::shared_ptr<std::vector<std::pair<std::string, std::string>>> soapValues(new std::vector<std::pair<std::string, std::string>>());
		for(JsonPayloads::iterator i = frame->jsonPayloads.begin(); i != frame->jsonPayloads.end(); ++i)
		{
			if((*i)->constValueInteger > -1)
			{
				if((*i)->key.empty()) continue;
				soapValues->push_back(std::pair<std::string, std::string>((*i)->key, std::to_string((*i)->constValueInteger)));
				continue;
			}
			else if((*i)->constValueStringSet)
			{
				if((*i)->key.empty()) continue;
				soapValues->push_back(std::pair<std::string, std::string>((*i)->key, (*i)->constValueString));
				continue;
			}

			bool paramFound = false;
			for(Parameters::iterator j = parameterGroup->parameters.begin(); j != parameterGroup->parameters.end(); ++j)
			{
				if((*i)->parameterId == j->second->physical->groupId)
				{
					if((*i)->key.empty()) continue;
					std::vector<uint8_t> parameterData = valuesCentral[channel][j->second->id].getBinaryData();
					soapValues->push_back(std::pair<std::string, std::string>((*i)->key, _binaryDecoder->decodeResponse(parameterData)->toString()));
					paramFound = true;
					break;
				}
			}
			if(!paramFound) GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
		}

		std::string soapRequest;
		BosePacket packet(frame->function1, soapValues);
		packet.getSoapRequest(soapRequest);
		if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: Sending SOAP request:\n" + soapRequest);
		if(_httpClient)
		{
			BaseLib::Http response;
			try
			{
				_httpClient->sendRequest(soapRequest, response);
				std::string stringResponse(response.getContent().data(), response.getContentSize());
				if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: SOAP response:\n" + stringResponse);
				if(response.getHeader().responseCode < 200 || response.getHeader().responseCode > 299)
				{
					//if(response.getHeader().responseCode == -1) serviceMessages->setUnreach(true, false);
					return Variable::createError(-100, "Error sending value to Bose device: Response code was: " + std::to_string(response.getHeader().responseCode));
				}
				std::shared_ptr<BosePacket> responsePacket(new BosePacket(stringResponse));
				packetReceived(responsePacket);
				serviceMessages->setUnreach(false, true);
			}
			catch(const BaseLib::HttpClientException& ex)
			{
				if(ex.responseCode() == -1) serviceMessages->setUnreach(true, false);
				return Variable::createError(-100, "Error sending value to Bose device: " + std::string(ex.what()));
			}
		}

		auto& rpcConfigurationParameter = valuesCentral[channel][parameter->id];
		std::vector<uint8_t> parameterData = rpcConfigurationParameter.getBinaryData();
		return parameter->convertFromPacket(parameterData, rpcConfigurationParameter.mainRole(), true);
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}

PParameterGroup BosePeer::getParameterSet(int32_t channel, ParameterGroup::Type::Enum type)
{
	try
	{
		return _rpcDevice->functions.at(channel)->getParameterGroup(type);
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return PParameterGroup();
}

void BosePeer::getAll()
{
    execute("/info", true);
    execute("/now_playing", true);
    execute("/volume", true);
    execute("/sources", true);
    execute("/getZone", true);
    execute("/bassCapabilities", true);
    execute("/bass", true);
    execute("/presets", true);
    execute("/getGroup", true);
}

void BosePeer::execute(std::string functionName, bool ignoreErrors)
{
    try
    {
        if(serviceMessages->getUnreach()) return;
        BaseLib::Http response;
        _httpClient->get(functionName, response);

        std::string stringResponse(response.getContent().data(), response.getContentSize());
        std::shared_ptr<BosePacket> responsePacket(new BosePacket(stringResponse));
        packetReceived(responsePacket);
        serviceMessages->setUnreach(false, true);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


bool BosePeer::getAllValuesHook2(PRpcClientInfo clientInfo, PParameter parameter, uint32_t channel, PVariable parameters)
{
	try
	{
		if(channel == 1)
		{
			if(parameter->id == "IP_ADDRESS")
			{
				std::vector<uint8_t> parameterData;
                auto& rpcConfigurationParameter = valuesCentral[channel][parameter->id];
				parameter->convertToPacket(PVariable(new Variable(_ip)), rpcConfigurationParameter.mainRole(), parameterData);
                rpcConfigurationParameter.setBinaryData(parameterData);
			}
			else if(parameter->id == "PEER_ID")
			{
				std::vector<uint8_t> parameterData;
                auto& rpcConfigurationParameter = valuesCentral[channel][parameter->id];
				parameter->convertToPacket(PVariable(new Variable((int32_t)_peerID)), rpcConfigurationParameter.mainRole(), parameterData);
                rpcConfigurationParameter.setBinaryData(parameterData);
			}
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return false;
}

bool BosePeer::getParamsetHook2(PRpcClientInfo clientInfo, PParameter parameter, uint32_t channel, PVariable parameters)
{
	try
	{
		if(channel == 1)
		{
			if(parameter->id == "IP_ADDRESS")
			{
				std::vector<uint8_t> parameterData;
                auto& rpcConfigurationParameter = valuesCentral[channel][parameter->id];
				parameter->convertToPacket(PVariable(new Variable(_ip)), rpcConfigurationParameter.mainRole(), parameterData);
                rpcConfigurationParameter.setBinaryData(parameterData);
			}
			else if(parameter->id == "PEER_ID")
			{
				std::vector<uint8_t> parameterData;
                auto& rpcConfigurationParameter = valuesCentral[channel][parameter->id];
				parameter->convertToPacket(PVariable(new Variable((int32_t)_peerID)), rpcConfigurationParameter.mainRole(), parameterData);
                rpcConfigurationParameter.setBinaryData(parameterData);
			}
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return false;
}

PVariable BosePeer::getValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, bool requestFromDevice, bool asynchronous)
{
	try
	{
		if(serviceMessages->getUnreach()) requestFromDevice = false;
		return Peer::getValue(clientInfo, channel, valueKey, requestFromDevice, asynchronous);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable BosePeer::putParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool checkAcls, bool onlyPushing)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		if(remoteChannel < 0) remoteChannel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
		if(type == ParameterGroup::Type::none) type = ParameterGroup::Type::link;
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set.");
		if(variables->structValue->empty()) return PVariable(new Variable(VariableType::tVoid));

		auto central = getCentral();
		if(!central) return Variable::createError(-32500, "Could not get central.");

		if(type == ParameterGroup::Type::Enum::config)
		{
			bool configChanged = false;
			for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
			{
				if(i->first.empty() || !i->second) continue;
				std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator = configCentral.find(channel);
				if(channelIterator == configCentral.end()) continue;
				std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = channelIterator->second.find(i->first);
				if(parameterIterator == channelIterator->second.end()) continue;
				BaseLib::Systems::RpcConfigurationParameter& parameter = parameterIterator->second;
				if(!parameter.rpcParameter) continue;
				std::vector<uint8_t> parameterData;
				parameter.rpcParameter->convertToPacket(i->second, parameter.mainRole(), parameterData);
				parameter.setBinaryData(parameterData);
				if(parameter.databaseId > 0) saveParameter(parameter.databaseId, parameterData);
				else saveParameter(0, ParameterGroup::Type::Enum::config, channel, i->first, parameterData);
				GD::out.printInfo("Info: Parameter " + i->first + " of peer " + std::to_string(_peerID) + " and channel " + std::to_string(channel) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(parameterData) + ".");
				if(parameter.rpcParameter->physical->operationType != IPhysical::OperationType::Enum::config && parameter.rpcParameter->physical->operationType != IPhysical::OperationType::Enum::configString) continue;
				configChanged = true;
			}

			if(configChanged) raiseRPCUpdateDevice(_peerID, channel, _serialNumber + ":" + std::to_string(channel), 0);
		}
		else if(type == ParameterGroup::Type::Enum::variables)
		{
			for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
			{
				if(i->first.empty() || !i->second) continue;

				if(checkAcls && !clientInfo->acls->checkVariableWriteAccess(central->getPeer(_peerID), channel, i->first)) continue;

				setValue(clientInfo, channel, i->first, i->second, true);
			}
		}
		else
		{
			return Variable::createError(-3, "Parameter set type is not supported.");
		}
		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable BosePeer::setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait)
{
	try
	{
	    //todo check if it needed to send values to master.
		if(!_isMaster && (
		        //todo: correct the given valueKey to the new variabel names
				valueKey == "NEXT" ||
				valueKey == "PAUSE" ||
				valueKey == "PLAY" ||
				valueKey == "PLAY_AUDIO_FILE" ||
				valueKey == "PLAY_AUDIO_FILE_UNMUTE" ||
				valueKey == "PLAY_AUDIO_FILE_VOLUME" ||
				valueKey == "PLAY_FADE" ||
				valueKey == "PLAY_FAVORITE" ||
				valueKey == "PLAY_PLAYLIST" ||
				valueKey == "PLAY_RADIO_FAVORITE" ||
				valueKey == "PLAY_TTS" ||
				valueKey == "PLAY_TTS_LANGUAGE" ||
				valueKey == "PLAY_TTS_UNMUTE" ||
				valueKey == "PLAY_TTS VOICE" ||
				valueKey == "PLAY_TTS_VOLUME" ||
				valueKey == "PREVIOUS" ||
				valueKey == "STOP")){
			std::shared_ptr<BoseCentral> central(std::dynamic_pointer_cast<BoseCentral>(getCentral()));
			std::unordered_map<int32_t, std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>> peerMap = getPeers();

			for(auto basicPeer : peerMap[1])
			{
				if(!basicPeer->isSender) continue;
				std::shared_ptr<BosePeer> peer = central->getPeer(basicPeer->id);
				if(!peer) continue;
				return peer->setValue(clientInfo, channel, valueKey, value, wait);
			}
		}

		Peer::setValue(clientInfo, channel, valueKey, value, wait); //Ignore result, otherwise setHomegerValue might not be executed
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(valueKey.empty()) return Variable::createError(-5, "Value key is empty.");
		if(valuesCentral.find(channel) == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
		if(valuesCentral[channel].find(valueKey) == valuesCentral[channel].end()) return Variable::createError(-5, "Unknown parameter.");
		auto& parameter = valuesCentral[channel][valueKey];
		PParameter rpcParameter = valuesCentral[channel][valueKey].rpcParameter;
		if(!rpcParameter) return Variable::createError(-5, "Unknown parameter.");
		if(rpcParameter->service)
		{
			if(channel == 0 && value->type == VariableType::tBoolean)
			{
				if(serviceMessages->set(valueKey, value->booleanValue)) return std::make_shared<Variable>(VariableType::tVoid);
			}
			else if(value->type == VariableType::tInteger) serviceMessages->set(valueKey, value->integerValue, channel);
		}
		if(rpcParameter->logical->type == ILogical::Type::tAction && !value->booleanValue) value->booleanValue = true;
		std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>());
		std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable>());

		std::vector<uint8_t> parameterData;
		rpcParameter->convertToPacket(value, parameter.mainRole(), parameterData);
		value = rpcParameter->convertFromPacket(parameterData, parameter.mainRole(), true);

		valueKeys->push_back(valueKey);
		values->push_back(value);

        if(rpcParameter->physical->operationType == IPhysical::OperationType::Enum::command)
		{
            std::shared_ptr<std::vector<std::pair<std::string, std::string>>> soapValues(new std::vector<std::pair<std::string, std::string>>());
            if(valueKey == "ZONE_MEMBER:ADD_SLAVE_BY_ID")
			{
				std::shared_ptr<BoseCentral> central(std::dynamic_pointer_cast<BoseCentral>(getCentral()));
				std::shared_ptr<BosePeer> linkPeer = central->getPeer(value->integerValue);
				if(!linkPeer) return Variable::createError(-5, "Unknown remote peer.");
				central->addLink(clientInfo, _peerID, 1, linkPeer->getID(), 1, "Dynamic Bose Link", "");
				soapValues->push_back(std::pair<std::string, std::string>("slaveIp", linkPeer->_ip));
                soapValues->push_back(std::pair<std::string, std::string>("slaveMac", linkPeer->_serialNumber));
                soapValues->push_back(std::pair<std::string, std::string>("master", _serialNumber));
			}
            else if(valueKey == "ZONE_MEMBER:SET_ZONE_SLAVE_BY_ID")
            {
                std::shared_ptr<BoseCentral> central(std::dynamic_pointer_cast<BoseCentral>(getCentral()));
                std::shared_ptr<BosePeer> linkPeer = central->getPeer(value->integerValue);
                if(!linkPeer) return Variable::createError(-5, "Unknown remote peer.");
                central->addLink(clientInfo, _peerID, 1, linkPeer->getID(), 1, "Dynamic Bose Link", "");
                soapValues->push_back(std::pair<std::string, std::string>("slaveIp", linkPeer->_ip));
                soapValues->push_back(std::pair<std::string, std::string>("slaveMac", linkPeer->_serialNumber));
                soapValues->push_back(std::pair<std::string, std::string>("master", _serialNumber));
            }
			else if(valueKey == "ZONE_MEMBER:REMOVE_SLAVE_BY_ID")
			{
				std::shared_ptr<BoseCentral> central(std::dynamic_pointer_cast<BoseCentral>(getCentral()));
				std::shared_ptr<BosePeer> linkPeer = central->getPeer(value->integerValue);
				if(!linkPeer) return Variable::createError(-5, "Unknown remote peer.");
				central->removeLink(clientInfo, _peerID, 1, linkPeer->getID(), 1);
                soapValues->push_back(std::pair<std::string, std::string>("slaveIp", linkPeer->_ip));
                soapValues->push_back(std::pair<std::string, std::string>("slaveMac", linkPeer->_serialNumber));
                soapValues->push_back(std::pair<std::string, std::string>("master", _serialNumber));
			}
			else if(valueKey == "ZONE_MEMBER:ADD_SLAVE_BY_SERIAL")
			{
				std::shared_ptr<BoseCentral> central(std::dynamic_pointer_cast<BoseCentral>(getCentral()));
				std::shared_ptr<BosePeer> linkPeer = central->getPeer(value->stringValue);
				if(!linkPeer) return Variable::createError(-5, "Unknown remote peer.");
				central->addLink(clientInfo, _peerID, 1, linkPeer->getID(), 1, "Dynamic Bose Link", "");
                soapValues->push_back(std::pair<std::string, std::string>("slaveIp", linkPeer->_ip));
                soapValues->push_back(std::pair<std::string, std::string>("slaveMac", linkPeer->_serialNumber));
                soapValues->push_back(std::pair<std::string, std::string>("master", _serialNumber));
			}
            else if(valueKey == "ZONE_MEMBER:SET_ZONE_SLAVE_BY_SERIAL")
            {
                std::shared_ptr<BoseCentral> central(std::dynamic_pointer_cast<BoseCentral>(getCentral()));
                std::shared_ptr<BosePeer> linkPeer = central->getPeer(value->stringValue);
                if(!linkPeer) return Variable::createError(-5, "Unknown remote peer.");
                central->addLink(clientInfo, _peerID, 1, linkPeer->getID(), 1, "Dynamic Bose Link", "");
                soapValues->push_back(std::pair<std::string, std::string>("slaveIp", linkPeer->_ip));
                soapValues->push_back(std::pair<std::string, std::string>("slaveMac", linkPeer->_serialNumber));
                soapValues->push_back(std::pair<std::string, std::string>("master", _serialNumber));
            }
			else if(valueKey == "ZONE_MEMBER:REMOVE_SLAVE_BY_SERIAL")
			{
				std::shared_ptr<BoseCentral> central(std::dynamic_pointer_cast<BoseCentral>(getCentral()));
				std::shared_ptr<BosePeer> linkPeer = central->getPeer(value->stringValue);
				if(!linkPeer) return Variable::createError(-5, "Unknown remote peer.");
				central->removeLink(clientInfo, _peerID, 1, linkPeer->getID(), 1);
                soapValues->push_back(std::pair<std::string, std::string>("slaveIp", linkPeer->_ip));
                soapValues->push_back(std::pair<std::string, std::string>("slaveMac", linkPeer->_serialNumber));
                soapValues->push_back(std::pair<std::string, std::string>("master", _serialNumber));
			}

            if(valueKey == "PLAY_TTS"){
                std::pair<std::string, std::string> soapValue;
                if(playTTS(channel, valueKey, value, soapValue)) return std::make_shared<Variable>(VariableType::tVoid);
                soapValues->push_back(soapValue);
            }

            if(rpcParameter->setPackets.empty()) return Variable::createError(-6, "parameter is read only");
            std::string setRequest = rpcParameter->setPackets.front()->id;
            if(_rpcDevice->packetsById.find(setRequest) == _rpcDevice->packetsById.end()) return Variable::createError(-6, "No frame was found for parameter " + valueKey);
            PPacket frame = _rpcDevice->packetsById[setRequest];

            for(JsonPayloads::iterator i = frame->jsonPayloads.begin(); i != frame->jsonPayloads.end(); ++i)
            {
                if((*i)->constValueInteger > -1)
                {
                    if((*i)->key.empty()) continue;
                    soapValues->push_back(std::pair<std::string, std::string>((*i)->key, std::to_string((*i)->constValueInteger)));
                    continue;
                }
                else if(!(*i)->constValueString.empty())
                {
                    if((*i)->key.empty()) continue;
                    soapValues->push_back(std::pair<std::string, std::string>((*i)->key, (*i)->constValueString));
                    continue;
                }
                if((*i)->parameterId == rpcParameter->physical->groupId)
                {
                    if((*i)->key.empty()) continue;
                    soapValues->push_back(std::pair<std::string, std::string>((*i)->key, _binaryDecoder->decodeResponse(parameterData)->toString()));
                }
                //Search for all other parameters
                else
                {
                    bool paramFound = false;
                    for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = valuesCentral[channel].begin(); j != valuesCentral[channel].end(); ++j)
                    {
                        if(!j->second.rpcParameter) continue;
                        if((*i)->parameterId == j->second.rpcParameter->physical->groupId)
                        {
                            if((*i)->key.empty()) continue;
                            std::vector<uint8_t> parameterData2 = j->second.getBinaryData();
                            soapValues->push_back(std::pair<std::string, std::string>((*i)->key, _binaryDecoder->decodeResponse(parameterData2)->toString()));
                            paramFound = true;
                            break;
                        }
                    }
                    if(!paramFound) GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
                }
            }

            std::string soapRequest;
            BosePacket packet(frame->function1, soapValues);
            packet.getSoapRequest(soapRequest);

            if(soapRequest.empty())
            {
                if(GD::bl->debugLevel >= 4) GD::out.printDebug("Could not create request for: " + frame->function1);
                return Variable::createError(-5, "Could not create request for: " + frame->function1);
            }

            if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: Sending SOAP request:\n" + soapRequest);
            if(_httpClient)
            {
                BaseLib::Http response;
                try
                {
                    _httpClient->post(frame->function2, soapRequest, response);

                    std::string stringResponse(response.getContent().data(), response.getContentSize());
                    if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: SOAP response:\n" + stringResponse);
                    if(response.getHeader().responseCode < 200 || response.getHeader().responseCode > 299)
                    {
                        GD::out.printWarning("Warning: Error in UPnP request: Response code was: " + std::to_string(response.getHeader().responseCode));
                        GD::out.printMessage("Request was: \n" + soapRequest);
                        return Variable::createError(-100, "Error sending value to Bose device: Response code was: " + std::to_string(response.getHeader().responseCode));
                    }
                }
                catch(const BaseLib::HttpException& ex)
                {
                    GD::out.printWarning("Warning: Error in UPnP request: " + std::string(ex.what()));
                    GD::out.printMessage("Request was: \n" + soapRequest);
                    return Variable::createError(-100, "Error sending value to Bose device: " + std::string(ex.what()));
                }
                catch(const BaseLib::HttpClientException& ex)
                {
                    GD::out.printWarning("Warning: Error in UPnP request: " + std::string(ex.what()));
                    GD::out.printMessage("Request was: \n" + soapRequest);
                    return Variable::createError(-100, "Error sending value to Bose device: " + std::string(ex.what()));
                }
                catch(const std::exception& ex)
                {
                    GD::out.printWarning("Warning: Error in UPnP request: " + std::string(ex.what()));
                    GD::out.printMessage("Request was: \n" + soapRequest);
                    return Variable::createError(-100, "Error sending value to Bose device: " + std::string(ex.what()));
                }
            }
        }
		else if(rpcParameter->physical->operationType != IPhysical::OperationType::Enum::store) return Variable::createError(-6, "Only interface types \"store\" and \"command\" are supported for this device family.");

		parameter.setBinaryData(parameterData);
		if(parameter.databaseId > 0) saveParameter(parameter.databaseId, parameterData);
		else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameterData);

        std::string address(_serialNumber + ":" + std::to_string(channel));
        raiseEvent(clientInfo->initInterfaceId, _peerID, channel, valueKeys, values);
        raiseRPCEvent(clientInfo->initInterfaceId, _peerID, channel, address, valueKeys, values);

		return std::make_shared<BaseLib::Variable>();
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error. See error log for more details.");
}

bool BosePeer::playTTS(uint32_t channel, std::string valueKey, PVariable value, std::pair<std::string, std::string> &soapValue)
{
	try
	{
		if(valueKey == "PLAY_TTS")
		{
			if(value->stringValue.empty()) return true;
			std::string ttsProgram = GD::physicalInterface->ttsProgram();
			if(ttsProgram.empty())
			{
				GD::out.printError("Error: No program to generate TTS audio file specified in Bose.conf");
				return true;
			}

			std::string language;
			std::string voice;

			std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelOneIterator = valuesCentral.find(1);
			if(channelOneIterator == valuesCentral.end())
			{
				GD::out.printError("Error: Channel 1 not found.");
				return true;
			}

			std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator =  channelOneIterator->second.find("PLAY_TTS_LANGUAGE");
			if(parameterIterator != channelOneIterator->second.end())
			{
				std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
				PVariable variable = _binaryDecoder->decodeResponse(parameterData);
				if(variable) language = variable->stringValue;
				if(!BaseLib::HelperFunctions::isAlphaNumeric(language, std::unordered_set<char>{'-', '_'}))
				{
					GD::out.printError("Error: Language is not alphanumeric.");
					language = "en-US";
				}
			}

			parameterIterator = channelOneIterator->second.find("PLAY_TTS_VOICE");
			if(parameterIterator != channelOneIterator->second.end())
			{
				std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
				PVariable variable = _binaryDecoder->decodeResponse(parameterData);
				if(variable) voice = variable->stringValue;
				if(!BaseLib::HelperFunctions::isAlphaNumeric(language, std::unordered_set<char>{'-', '_'}))
				{
					GD::out.printError("Error: Voice is not alphanumeric.");
					language = "Justin";
				}
			}

			std::string audioPath = GD::bl->settings.tempPath() + "Bose/";
			std::string filename;
			BaseLib::HelperFunctions::stringReplace(value->stringValue, "\"", "");
			std::string execPath = ttsProgram + ' ' + language + ' ' + voice + " \"" + value->stringValue + "\"";
            auto exitCode = BaseLib::ProcessManager::exec(execPath, _bl->fileDescriptorManager.getMax(), filename);
			if(exitCode != 0)
			{
				GD::out.printError("Error: Error executing program to generate TTS audio file (exit code " + std::to_string(exitCode) + "): \"" + ttsProgram + ' ' + language + ' ' + value->stringValue + "\"");
				return true;
			}
			BaseLib::HelperFunctions::trim(filename);
			if(!BaseLib::Io::fileExists(filename))
			{
				GD::out.printError("Error: Error executing program to generate TTS audio file: File not found. Output needs to be the full path to the TTS audio file, but was: \"" + filename + "\"");
				return true;
			}
			if(filename.size() <= audioPath.size() || filename.compare(0, audioPath.size(), audioPath) != 0)
			{
				GD::out.printError("Error: Error executing program to generate TTS audio file. Output needs to be the full path to the TTS audio file and the file needs to be within \"" + audioPath + "\". Returned path was: \"" + filename + "\"");
				return true;
			}
			filename = filename.substr(audioPath.size());

			if(filename.size() < 5) return true;
            std::string tempPath = GD::bl->settings.tempPath() + "Bose/";
            if(!GD::bl->io.directoryExists(tempPath))
            {
                if(GD::bl->io.createDirectory(tempPath, S_IRWXU | S_IRWXG) == false)
                {
                    GD::out.printError("Error: Could not create temporary directory \"" + tempPath + '"');
                    return true;
                }
            }

            std::string playlistUri = "http://" + GD::physicalInterface->listenAddress() + ':' + std::to_string(GD::physicalInterface->listenPort()) + '/' + filename;
            soapValue = std::pair<std::string, std::string>("url", playlistUri);
			return false;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
	return true;
}

}

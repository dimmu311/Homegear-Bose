#include "BoseCentral.h"
#include "GD.h"

#include <iomanip>

namespace Bose {

BoseCentral::BoseCentral(ICentralEventSink* eventHandler) : BaseLib::Systems::ICentral(Bose_FAMILY_ID, GD::bl, eventHandler)
{
	init();
}

BoseCentral::BoseCentral(uint32_t deviceID, std::string serialNumber, ICentralEventSink* eventHandler) : BaseLib::Systems::ICentral(Bose_FAMILY_ID, GD::bl, deviceID, serialNumber, -1, eventHandler)
{
	init();
}

BoseCentral::~BoseCentral()
{
	dispose();
}

void BoseCentral::dispose(bool wait)
{
	try
	{
		if(_disposing) return;
		_disposing = true;
		GD::out.printDebug("Removing device " + std::to_string(_deviceId) + " from physical device's event queue...");
		GD::physicalInterface->removeEventHandler(_physicalInterfaceEventhandlers[GD::physicalInterface->getID()]);
		GD::out.printDebug("Debug: Waiting for worker thread of device " + std::to_string(_deviceId) + "...");
		_ssdp.reset();
	}
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void BoseCentral::homegearShuttingDown()
{
	_shuttingDown = true;
}

void BoseCentral::init()
{
	try
	{
		if(_initialized) return; //Prevent running init two times
		_initialized = true;

		_ssdp.reset(new BaseLib::Ssdp(GD::bl));
		_physicalInterfaceEventhandlers[GD::physicalInterface->getID()] = GD::physicalInterface->addEventHandler((BaseLib::Systems::IPhysicalInterface::IPhysicalInterfaceEventSink*)this);

		_shuttingDown = false;

		std::string settingName = "tempmaxage";
		BaseLib::Systems::FamilySettings::PFamilySetting tempMaxAgeSetting = GD::family->getFamilySetting(settingName);
		if(tempMaxAgeSetting) _tempMaxAge = tempMaxAgeSetting->integerValue;
		if(_tempMaxAge < 1) _tempMaxAge = 1;
		else if(_tempMaxAge > 87600) _tempMaxAge = 87600;

	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

void BoseCentral::deleteOldTempFiles()
{
	try
	{
		std::string BoseTempPath = GD::bl->settings.tempPath() + "/Bose/";
		if(!GD::bl->io.directoryExists(BoseTempPath)) return;

		auto tempFiles = GD::bl->io.getFiles(BoseTempPath, false);
		for(auto tempFile : tempFiles)
		{
			std::string path = BoseTempPath + tempFile;
			if(GD::bl->io.getFileLastModifiedTime(path) < ((int32_t)BaseLib::HelperFunctions::getTimeSeconds() - ((int32_t)_tempMaxAge * 3600)))
			{
				if(!GD::bl->io.deleteFile(path))
				{
					GD::out.printCritical("Critical: deleting temporary file \"" + path + "\": " + strerror(errno));
				}
			}
		}
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

bool BoseCentral::onPacketReceived(std::string& senderID, std::shared_ptr<BaseLib::Systems::Packet> packet)
{
	try
	{
		if(_disposing) return false;
		std::shared_ptr<BosePacket> BosePacket(std::dynamic_pointer_cast<BosePacket>(packet));
		if(!BosePacket) return false;
		std::shared_ptr<BosePeer> peer(getPeer(BosePacket->serialNumber()));
		if(!peer) return false;
		peer->packetReceived(BosePacket);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return false;
}

void BoseCentral::loadPeers()
{
	try
	{
		std::shared_ptr<BaseLib::Database::DataTable> rows = _bl->db->getPeers(_deviceId);
		for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
		{
			int32_t peerID = row->second.at(0)->intValue;
			GD::out.printMessage("Loading Bose peer " + std::to_string(peerID));
			std::shared_ptr<BosePeer> peer(new BosePeer(peerID, row->second.at(3)->textValue, _deviceId, this));
			if(!peer->load(this)) continue;
			if(!peer->getRpcDevice()) continue;

			peer->getAll();
			_peersMutex.lock();
			if(!peer->getSerialNumber().empty()) _peersBySerial[peer->getSerialNumber()] = peer;
			_peersById[peerID] = peer;
			_peersMutex.unlock();
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    	_peersMutex.unlock();
    }
}

std::shared_ptr<BosePeer> BoseCentral::getPeer(uint64_t id)
{
	try
	{
		_peersMutex.lock();
		if(_peersById.find(id) != _peersById.end())
		{
			std::shared_ptr<BosePeer> peer(std::dynamic_pointer_cast<BosePeer>(_peersById.at(id)));
			_peersMutex.unlock();
			return peer;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    _peersMutex.unlock();
    return std::shared_ptr<BosePeer>();
}

std::shared_ptr<BosePeer> BoseCentral::getPeer(std::string serialNumber)
{
	try
	{
		_peersMutex.lock();
		if(_peersBySerial.find(serialNumber) != _peersBySerial.end())
		{
			std::shared_ptr<BosePeer> peer(std::dynamic_pointer_cast<BosePeer>(_peersBySerial.at(serialNumber)));
			_peersMutex.unlock();
			return peer;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    _peersMutex.unlock();
    return std::shared_ptr<BosePeer>();
}

std::shared_ptr<BosePeer> BoseCentral::getPeerByRinconId(std::string rinconId)
{
	try
	{
		std::lock_guard<std::mutex> peersGuard(_peersMutex);
		for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
		{
			std::shared_ptr<BosePeer> peer = std::dynamic_pointer_cast<BosePeer>(i->second);
			if(!peer) continue;
			if(peer->getRinconId() == rinconId) return peer;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return std::shared_ptr<BosePeer>();
}

void BoseCentral::savePeers(bool full)
{
	try
	{
		_peersMutex.lock();
		for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
		{
			//Necessary, because peers can be assigned to multiple virtual devices
			if(i->second->getParentID() != _deviceId) continue;
			//We are always printing this, because the init script needs it
			GD::out.printMessage("(Shutdown) => Saving Bose peer " + std::to_string(i->second->getID()));
			i->second->save(full, full, full);
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
	_peersMutex.unlock();
}

void BoseCentral::deletePeer(uint64_t id)
{
	try
	{
		std::shared_ptr<BosePeer> peer(getPeer(id));
		if(!peer) return;
		peer->deleting = true;
		PVariable deviceAddresses(new Variable(VariableType::tArray));
		deviceAddresses->arrayValue->push_back(PVariable(new Variable(peer->getSerialNumber())));

		PVariable deviceInfo(new Variable(VariableType::tStruct));
		deviceInfo->structValue->insert(StructElement("ID", PVariable(new Variable((int32_t)peer->getID()))));
		PVariable channels(new Variable(VariableType::tArray));
		deviceInfo->structValue->insert(StructElement("CHANNELS", channels));

		std::shared_ptr<HomegearDevice> rpcDevice = peer->getRpcDevice();
		for(Functions::iterator i = rpcDevice->functions.begin(); i != rpcDevice->functions.end(); ++i)
		{
			deviceAddresses->arrayValue->push_back(PVariable(new Variable(peer->getSerialNumber() + ":" + std::to_string(i->first))));
			channels->arrayValue->push_back(PVariable(new Variable(i->first)));
		}

        std::vector<uint64_t> deletedIds{ id };
		raiseRPCDeleteDevices(deletedIds, deviceAddresses, deviceInfo);

        {
            std::lock_guard<std::mutex> peersGuard(_peersMutex);
            if(_peersBySerial.find(peer->getSerialNumber()) != _peersBySerial.end()) _peersBySerial.erase(peer->getSerialNumber());
            if(_peersById.find(id) != _peersById.end()) _peersById.erase(id);
        }

        int32_t i = 0;
        while(peer.use_count() > 1 && i < 600)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            i++;
        }
        if(i == 600) GD::out.printError("Error: Peer deletion took too long.");

		peer->deleteFromDatabase();

		GD::out.printMessage("Removed Bose peer " + std::to_string(peer->getID()));
	}
	catch(const std::exception& ex)
    {
		_peersMutex.unlock();
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::string BoseCentral::handleCliCommand(std::string command)
{
	try
	{
		std::ostringstream stringStream;
		if(command == "help" || command == "h")
		{
			stringStream << "List of commands:" << std::endl << std::endl;
			stringStream << "For more information about the individual command type: COMMAND help" << std::endl << std::endl;
			stringStream << "peers list (ls)\t\tList all peers" << std::endl;
			stringStream << "peers remove (pr)\tRemove a peer" << std::endl;
			stringStream << "peers select (ps)\tSelect a peer" << std::endl;
			stringStream << "peers setname (pn)\tName a peer" << std::endl;
			stringStream << "search (sp)\t\tSearches for new devices" << std::endl;
			stringStream << "unselect (u)\t\tUnselect this device" << std::endl;
			return stringStream.str();
		}
		if(command.compare(0, 12, "peers remove") == 0 || command.compare(0, 2, "pr") == 0)
		{
			uint64_t peerID = 0;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'r') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					peerID = BaseLib::Math::getNumber(element, false);
					if(peerID == 0) return "Invalid id.\n";
				}
				index++;
			}
			if(index == 1 + offset)
			{
				stringStream << "Description: This command removes a peer." << std::endl;
				stringStream << "Usage: peers unpair PEERID" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to remove. Example: 513" << std::endl;
				return stringStream.str();
			}

			if(!peerExists(peerID)) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				stringStream << "Removing peer " << std::to_string(peerID) << std::endl;
				deletePeer(peerID);
			}
			return stringStream.str();
		}
		else if(command.compare(0, 10, "peers list") == 0 || command.compare(0, 2, "pl") == 0 || command.compare(0, 2, "ls") == 0)
		{
			try
			{
				std::string filterType;
				std::string filterValue;

				std::stringstream stream(command);
				std::string element;
				int32_t offset = (command.at(1) == 'l' || command.at(1) == 's') ? 0 : 1;
				int32_t index = 0;
				while(std::getline(stream, element, ' '))
				{
					if(index < 1 + offset)
					{
						index++;
						continue;
					}
					else if(index == 1 + offset)
					{
						if(element == "help")
						{
							index = -1;
							break;
						}
						filterType = BaseLib::HelperFunctions::toLower(element);
					}
					else if(index == 2 + offset)
					{
						filterValue = element;
						if(filterType == "name") BaseLib::HelperFunctions::toLower(filterValue);
					}
					index++;
				}
				if(index == -1)
				{
					stringStream << "Description: This command lists information about all peers." << std::endl;
					stringStream << "Usage: peers list [FILTERTYPE] [FILTERVALUE]" << std::endl << std::endl;
					stringStream << "Parameters:" << std::endl;
					stringStream << "  FILTERTYPE:\tSee filter types below." << std::endl;
					stringStream << "  FILTERVALUE:\tDepends on the filter type. If a number is required, it has to be in hexadecimal format." << std::endl << std::endl;
					stringStream << "Filter types:" << std::endl;
					stringStream << "  ID: Filter by id." << std::endl;
					stringStream << "      FILTERVALUE: The id of the peer to filter (e. g. 513)." << std::endl;
					stringStream << "  SERIAL: Filter by serial number." << std::endl;
					stringStream << "      FILTERVALUE: The serial number of the peer to filter (e. g. JEQ0554309)." << std::endl;
					stringStream << "  NAME: Filter by name." << std::endl;
					stringStream << "      FILTERVALUE: The part of the name to search for (e. g. \"1st floor\")." << std::endl;
					stringStream << "  TYPE: Filter by device type." << std::endl;
					stringStream << "      FILTERVALUE: The 2 byte device type in hexadecimal format." << std::endl;
					return stringStream.str();
				}

				if(_peersById.empty())
				{
					stringStream << "No peers are paired to this central." << std::endl;
					return stringStream.str();
				}
				std::string bar(" │ ");
				const int32_t idWidth = 8;
				const int32_t nameWidth = 25;
                const int32_t addressWidth = 15;
				const int32_t serialWidth = 19;
				const int32_t typeWidth1 = 4;
				const int32_t typeWidth2 = 25;
				std::string nameHeader("Name");
				nameHeader.resize(nameWidth, ' ');
                std::string addressHeader("IP Address");
                addressHeader.resize(addressWidth, ' ');
				std::string typeStringHeader("Type String");
				typeStringHeader.resize(typeWidth2, ' ');
				stringStream << std::setfill(' ')
					<< std::setw(idWidth) << "ID" << bar
					<< nameHeader << bar
                    << addressHeader << bar
					<< std::setw(serialWidth) << "Serial Number" << bar
					<< std::setw(typeWidth1) << "Type" << bar
					<< typeStringHeader
					<< std::endl;
				stringStream << "─────────┼───────────────────────────┼─────────────────┼─────────────────────┼──────┼───────────────────────────" << std::endl;
				stringStream << std::setfill(' ')
					<< std::setw(idWidth) << " " << bar
					<< std::setw(nameWidth) << " " << bar
                    << std::setw(addressWidth) << " " << bar
					<< std::setw(serialWidth) << " " << bar
					<< std::setw(typeWidth1) << " " << bar
					<< std::setw(typeWidth2)
					<< std::endl;
				_peersMutex.lock();
				for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
				{
					if(filterType == "id")
					{
						uint64_t id = BaseLib::Math::getNumber(filterValue, false);
						if(i->second->getID() != id) continue;
					}
					else if(filterType == "name")
					{
						std::string name = i->second->getName();
						if((signed)BaseLib::HelperFunctions::toLower(name).find(filterValue) == (signed)std::string::npos) continue;
					}
                    else if(filterType == "address")
                    {
                        if(i->second->getIp() != filterValue) continue;
                    }
					else if(filterType == "serial")
					{
						if(i->second->getSerialNumber() != filterValue) continue;
					}
					else if(filterType == "type")
					{
						int32_t deviceType = BaseLib::Math::getNumber(filterValue, true);
						if((int32_t)i->second->getDeviceType() != deviceType) continue;
					}

					stringStream << std::setw(idWidth) << std::setfill(' ') << std::to_string(i->second->getID()) << bar;
					std::string name = i->second->getName();
					size_t nameSize = BaseLib::HelperFunctions::utf8StringSize(name);
					if(nameSize > (unsigned)nameWidth)
					{
						name = BaseLib::HelperFunctions::utf8Substring(name, 0, nameWidth - 3);
						name += "...";
					}
					else name.resize(nameWidth + (name.size() - nameSize), ' ');
                    std::string ipAddress = i->second->getIp();
                    ipAddress.resize(addressWidth, ' ');
					stringStream << name << bar
                        << ipAddress << bar
						<< std::setw(serialWidth) << i->second->getSerialNumber() << bar
						<< std::setw(typeWidth1) << BaseLib::HelperFunctions::getHexString(i->second->getDeviceType(), 4) << bar;
					if(i->second->getRpcDevice())
					{
						std::string typeString = i->second->getTypeString();
						if(typeString.size() > (unsigned)typeWidth2)
						{
							typeString.resize(typeWidth2 - 3);
							typeString += "...";
						}
						stringStream << std::setw(typeWidth2) << typeString;
					}
					else stringStream << std::setw(typeWidth2);
					stringStream << std::endl << std::dec;
				}
				_peersMutex.unlock();
				stringStream << "─────────┴───────────────────────────┴─────────────────┴─────────────────────┴──────┴───────────────────────────" << std::endl;

				return stringStream.str();
			}
			catch(const std::exception& ex)
			{
				_peersMutex.unlock();
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
		}
		else if(command.compare(0, 13, "peers setname") == 0 || command.compare(0, 2, "pn") == 0)
		{
			uint64_t peerID = 0;
			std::string name;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'n') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					else
					{
						peerID = BaseLib::Math::getNumber(element, false);
						if(peerID == 0) return "Invalid id.\n";
					}
				}
				else if(index == 2 + offset) name = element;
				else name += ' ' + element;
				index++;
			}
			if(index == 1 + offset)
			{
				stringStream << "Description: This command sets or changes the name of a peer to identify it more easily." << std::endl;
				stringStream << "Usage: peers setname PEERID NAME" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to set the name for. Example: 513" << std::endl;
				stringStream << "  NAME:\tThe name to set. Example: \"1st floor light switch\"." << std::endl;
				return stringStream.str();
			}

			if(!peerExists(peerID)) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				std::shared_ptr<BosePeer> peer = getPeer(peerID);
				peer->setName(name);
				stringStream << "Name set to \"" << name << "\"." << std::endl;
			}
			return stringStream.str();
		}
		else if(command.compare(0, 6, "search") == 0 || command.compare(0, 2, "sp") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'p') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help")
					{
						stringStream << "Description: This command searches for new devices." << std::endl;
						stringStream << "Usage: search" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			PVariable result = searchDevices(nullptr, false);
			if(result->errorStruct) stringStream << "Error: " << result->structValue->at("faultString")->stringValue << std::endl;
			else stringStream << "Search completed successfully." << std::endl;
			return stringStream.str();
		}
		else return "Unknown command.\n";
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return "Error executing command. See log file for more details.\n";
}

std::shared_ptr<BosePeer> BoseCentral::createPeer(uint32_t deviceType, std::string serialNumber, std::string ip, std::string softwareVersion, std::string idString, std::string typeString, bool save)
{
	try
	{
		std::shared_ptr<BosePeer> peer(new BosePeer(_deviceId, this));
		peer->setDeviceType(deviceType);
		peer->setSerialNumber(serialNumber);
		peer->setIp(ip);
		peer->setIdString(idString);
		peer->setTypeString(typeString);
		peer->setFirmwareVersionString(softwareVersion);
		peer->setRpcDevice(GD::family->getRpcDevices()->find(deviceType, 0x10, -1));
		if(!peer->getRpcDevice()) return std::shared_ptr<BosePeer>();
		peer->initializeCentralConfig();
		if(save) peer->save(true, true, false); //Save and create peerID
		return peer;
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return std::shared_ptr<BosePeer>();
}

PVariable BoseCentral::addLink(BaseLib::PRpcClientInfo clientInfo, std::string senderSerialNumber, int32_t senderChannelIndex, std::string receiverSerialNumber, int32_t receiverChannelIndex, std::string name, std::string description)
{
	try
	{
		if(senderSerialNumber.empty()) return Variable::createError(-2, "Given sender address is empty.");
		if(receiverSerialNumber.empty()) return Variable::createError(-2, "Given receiver address is empty.");
		std::shared_ptr<BosePeer> sender = getPeer(senderSerialNumber);
		std::shared_ptr<BosePeer> receiver = getPeer(receiverSerialNumber);
		if(!sender) return Variable::createError(-2, "Sender device not found.");
		if(!receiver) return Variable::createError(-2, "Receiver device not found.");
		return addLink(clientInfo, sender->getID(), senderChannelIndex, receiver->getID(), receiverChannelIndex, name, description);
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}

PVariable BoseCentral::addLink(BaseLib::PRpcClientInfo clientInfo, uint64_t senderID, int32_t senderChannelIndex, uint64_t receiverID, int32_t receiverChannelIndex, std::string name, std::string description)
{
	try
	{
		if(senderID == 0) return Variable::createError(-2, "Sender id is not set.");
		if(receiverID == 0) return Variable::createError(-2, "Receiver is not set.");
		if(senderID == receiverID) return Variable::createError(-2, "Sender and receiver are the same.");
		std::shared_ptr<BosePeer> sender = getPeer(senderID);
		std::shared_ptr<BosePeer> receiver = getPeer(receiverID);
		if(!sender) return Variable::createError(-2, "Sender device not found.");
		if(!receiver) return Variable::createError(-2, "Receiver device not found.");

		if(sender->getValue(BaseLib::PRpcClientInfo(new BaseLib::RpcClientInfo()), 1, "AV_TRANSPORT_URI", false, false)->stringValue.compare(0, 9, "x-rincon:") == 0) return Variable::createError(-101, "Sender is already part of a group.");

		BaseLib::PVariable result = receiver->setValue(BaseLib::PRpcClientInfo(new BaseLib::RpcClientInfo()), 1, "AV_TRANSPORT_URI", BaseLib::PVariable(new BaseLib::Variable("x-rincon:" + sender->getRinconId())), true);
		if(result->errorStruct) return result;

		std::shared_ptr<BaseLib::Systems::BasicPeer> senderPeer(new BaseLib::Systems::BasicPeer());
		senderPeer->address = sender->getAddress();
		senderPeer->channel = 1;
		senderPeer->id = sender->getID();
		senderPeer->serialNumber = sender->getSerialNumber();
		senderPeer->hasSender = true;
		senderPeer->isSender = true;
		senderPeer->linkDescription = description;
		senderPeer->linkName = name;

		std::shared_ptr<BaseLib::Systems::BasicPeer> receiverPeer(new BaseLib::Systems::BasicPeer());
		receiverPeer->address = receiver->getAddress();
		receiverPeer->channel = 1;
		receiverPeer->id = receiver->getID();
		receiverPeer->serialNumber = receiver->getSerialNumber();
		receiverPeer->hasSender = true;
		receiverPeer->linkDescription = description;
		receiverPeer->linkName = name;

		sender->addPeer(receiverPeer);
		receiver->addPeer(senderPeer);

		raiseRPCUpdateDevice(sender->getID(), 1, sender->getSerialNumber() + ":" + std::to_string(1), 1);
		raiseRPCUpdateDevice(receiver->getID(), 1, receiver->getSerialNumber() + ":" + std::to_string(1), 1);

		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}

PVariable BoseCentral::deleteDevice(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t flags)
{
	try
	{
		if(serialNumber.empty()) return Variable::createError(-2, "Unknown device.");

        uint64_t peerId = 0;

        {
            std::shared_ptr<BosePeer> peer = getPeer(serialNumber);
            if(!peer) return PVariable(new Variable(VariableType::tVoid));
            peerId = peer->getID();
        }

		return deleteDevice(clientInfo, peerId, flags);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable BoseCentral::deleteDevice(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t flags)
{
	try
	{
		if(peerID == 0) return Variable::createError(-2, "Unknown device.");
        {
            std::shared_ptr<BosePeer> peer = getPeer(peerID);
            if(!peer) return PVariable(new Variable(VariableType::tVoid));
        }

		deletePeer(peerID);

		if(peerExists(peerID)) return Variable::createError(-1, "Error deleting peer. See log for more details.");

		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable BoseCentral::removeLink(BaseLib::PRpcClientInfo clientInfo, std::string senderSerialNumber, int32_t senderChannelIndex, std::string receiverSerialNumber, int32_t receiverChannelIndex)
{
	try
	{
		if(senderSerialNumber.empty()) return Variable::createError(-2, "Given sender address is empty.");
		if(receiverSerialNumber.empty()) return Variable::createError(-2, "Given receiver address is empty.");
		std::shared_ptr<BosePeer> sender = getPeer(senderSerialNumber);
		std::shared_ptr<BosePeer> receiver = getPeer(receiverSerialNumber);
		if(!sender) return Variable::createError(-2, "Sender device not found.");
		if(!receiver) return Variable::createError(-2, "Receiver device not found.");
		return removeLink(clientInfo, sender->getID(), senderChannelIndex, receiver->getID(), receiverChannelIndex);
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}

PVariable BoseCentral::removeLink(BaseLib::PRpcClientInfo clientInfo, uint64_t senderID, int32_t senderChannelIndex, uint64_t receiverID, int32_t receiverChannelIndex)
{
	try
	{
		if(senderID == 0) return Variable::createError(-2, "Sender id is not set.");
		if(receiverID == 0) return Variable::createError(-2, "Receiver id is not set.");
		std::shared_ptr<BosePeer> sender = getPeer(senderID);
		std::shared_ptr<BosePeer> receiver = getPeer(receiverID);
		if(!sender) return Variable::createError(-2, "Sender device not found.");
		if(!receiver) return Variable::createError(-2, "Receiver device not found.");
		if(!sender->getPeer(1, receiver->getID()) && !receiver->getPeer(1, sender->getID())) return Variable::createError(-6, "Devices are not paired to each other.");

		std::string receiverUri = receiver->getValue(BaseLib::PRpcClientInfo(new BaseLib::RpcClientInfo()), 1, "AV_TRANSPORT_URI", false, false)->stringValue;
		std::string senderRinconId = sender->getRinconId();
		if(receiverUri.compare(0, 9, "x-rincon:") != 0 || receiverUri.compare(9, senderRinconId.size(), senderRinconId) != 0)
		{
			std::string senderUri = sender->getValue(BaseLib::PRpcClientInfo(new BaseLib::RpcClientInfo()), 1, "AV_TRANSPORT_URI", false, false)->stringValue;
			std::string receiverRinconId = receiver->getRinconId();
			if(senderUri.compare(0, 9, "x-rincon:") == 0 && senderUri.compare(9, receiverRinconId.size(), receiverRinconId) == 0)
			{
				sender.swap(receiver);
			}
			else
			{
				sender->removePeer(receiver->getID());
				receiver->removePeer(sender->getID());
				return Variable::createError(-6, "Devices are not paired to each other.");
			}
		}

		sender->removePeer(receiver->getID());
		receiver->removePeer(sender->getID());

		BaseLib::PVariable result = receiver->setValue(BaseLib::PRpcClientInfo(new BaseLib::RpcClientInfo()), 1, "AV_TRANSPORT_URI", BaseLib::PVariable(new BaseLib::Variable("x-rincon-queue:" + receiver->getRinconId() + "#0")), true);
		if(result->errorStruct) return result;

		raiseRPCUpdateDevice(sender->getID(), 1, sender->getSerialNumber() + ":" + std::to_string(1), 1);
		raiseRPCUpdateDevice(receiver->getID(), 1, receiver->getSerialNumber() + ":" + std::to_string(1), 1);

		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}

PVariable BoseCentral::searchDevices(BaseLib::PRpcClientInfo clientInfo, const std::string& interfaceId)
{
	return searchDevices(clientInfo, false);
}

PVariable BoseCentral::searchDevices(BaseLib::PRpcClientInfo clientInfo, bool updateOnly)
{
	try
	{
		std::lock_guard<std::mutex> searchDevicesGuard(_searchDevicesMutex);
		std::string stHeader("urn:schemas-upnp-org:device:MediaRenderer:1");
		std::vector<BaseLib::SsdpInfo> searchResult;
		std::vector<std::shared_ptr<BosePeer>> newPeers;
		_ssdp->searchDevices(stHeader, 5000, searchResult);

		for(std::vector<BaseLib::SsdpInfo>::iterator i = searchResult.begin(); i != searchResult.end(); ++i)
		{
		    PVariable info = i->info();
			if(!info ||	info->structValue->find("serialNumber") == info->structValue->end() || info->structValue->find("UDN") == info->structValue->end())
			{
				GD::out.printWarning("Warning: Device does not provide serial number or UDN: " + i->ip());
				continue;
			}
			if(GD::bl->debugLevel >= 5)
			{
				GD::out.printDebug("Debug: Search response:");
				info->print(true, false);
			}
			std::string serialNumber = info->structValue->at("serialNumber")->stringValue;
			std::string softwareVersion = (info->structValue->find("softwareVersion") == info->structValue->end()) ? "" : info->structValue->at("softwareVersion")->stringValue;
			std::string friendlyName = (info->structValue->find("friendlyName") == info->structValue->end()) ? "" : info->structValue->at("friendlyName")->stringValue;
			std::string idString = (info->structValue->find("modelNumber") == info->structValue->end()) ? "" : info->structValue->at("modelNumber")->stringValue;
			std::string typeString = (info->structValue->find("modelName") == info->structValue->end()) ? "" : info->structValue->at("modelName")->stringValue;
			std::shared_ptr<BosePeer> peer = getPeer(serialNumber);
			if(peer)
			{
				if(peer->getIp() != i->ip()) peer->setIp(i->ip());
				if(!softwareVersion.empty() && peer->getFirmwareVersionString() != softwareVersion) peer->setFirmwareVersionString(softwareVersion);
			}
			else if(!updateOnly)
			{
				peer = createPeer(1, serialNumber, i->ip(), softwareVersion, idString, typeString, true);
				if(!peer)
				{
					GD::out.printWarning("Warning: No matching XML file found for device with IP: " + i->ip());
					continue;
				}
				if(peer->getID() == 0) continue;
				_peersMutex.lock();
				try
				{
					if(!peer->getSerialNumber().empty()) _peersBySerial[peer->getSerialNumber()] = peer;
					_peersById[peer->getID()] = peer;
				}
				catch(const std::exception& ex)
				{
					GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
				}
				_peersMutex.unlock();
				GD::out.printMessage("Added peer " + std::to_string(peer->getID()) + ".");
				newPeers.push_back(peer);
			}
			if(peer)
			{
				std::string udn = info->structValue->at("UDN")->stringValue;
				std::string::size_type colonPos = udn.find(':');
				if(colonPos != std::string::npos && colonPos + 1 < udn.size()) udn = udn.substr(colonPos + 1);
				peer->setRinconId(udn);
				if(!friendlyName.empty()) peer->setRoomName(friendlyName, updateOnly);
				if(peer->getName().empty()) peer->setName(typeString + " " + friendlyName);
			}
		}

        if(!newPeers.empty())
        {
            std::vector<uint64_t> newIds;
            newIds.reserve(newPeers.size());
            PVariable deviceDescriptions = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tArray);
            deviceDescriptions->arrayValue->reserve(100);
            for(auto& newPeer : newPeers)
            {
                std::shared_ptr<std::vector<PVariable>> descriptions = newPeer->getDeviceDescriptions(clientInfo, true, std::map<std::string, bool>());
                if(!descriptions) continue;
                newIds.push_back(newPeer->getID());
                for(auto& description : *descriptions)
                {
                    if(deviceDescriptions->arrayValue->size() + 1 > deviceDescriptions->arrayValue->capacity()) deviceDescriptions->arrayValue->reserve(deviceDescriptions->arrayValue->size() + 100);
                    deviceDescriptions->arrayValue->push_back(description);
                }

                {
                    auto pairingState = std::make_shared<PairingState>();
                    pairingState->peerId = newPeer->getID();
                    pairingState->state = "success";
                    std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);
                    _newPeers[BaseLib::HelperFunctions::getTime()].emplace_back(std::move(pairingState));
                }
            }
            raiseRPCNewDevices(newIds, deviceDescriptions);
        }
		return std::make_shared<Variable>((int32_t)newPeers.size());
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}
}

#ifndef BoseCENTRAL_H_
#define BoseCENTRAL_H_

#include <homegear-base/BaseLib.h>
#include "BosePeer.h"

#include <memory>
#include <mutex>
#include <string>

namespace Bose
{

class BoseCentral : public BaseLib::Systems::ICentral
{
public:
	BoseCentral(ICentralEventSink* eventHandler);
	BoseCentral(uint32_t deviceType, std::string serialNumber, ICentralEventSink* eventHandler);
	virtual ~BoseCentral();
	virtual void dispose(bool wait = true);

	virtual bool onPacketReceived(std::string& senderID, std::shared_ptr<BaseLib::Systems::Packet> packet);
	std::string handleCliCommand(std::string command);
	uint64_t getPeerIdFromSerial(std::string& serialNumber) { std::shared_ptr<BosePeer> peer = getPeer(serialNumber); if(peer) return peer->getID(); else return 0; }

	std::shared_ptr<BosePeer> getPeer(uint64_t id);
	std::shared_ptr<BosePeer> getPeer(std::string serialNumber);
	std::shared_ptr<BosePeer> getPeerByRinconId(std::string rinconId);
	virtual void loadPeers();
	virtual void savePeers(bool full);
	virtual void loadVariables() {}
	virtual void saveVariables() {}

	virtual void homegearShuttingDown();

	virtual PVariable addLink(BaseLib::PRpcClientInfo clientInfo, std::string senderSerialNumber, int32_t senderChannel, std::string receiverSerialNumber, int32_t receiverChannel, std::string name, std::string description);
	virtual PVariable addLink(BaseLib::PRpcClientInfo clientInfo, uint64_t senderID, int32_t senderChannel, uint64_t receiverID, int32_t receiverChannel, std::string name, std::string description);
	virtual PVariable deleteDevice(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t flags);
	virtual PVariable deleteDevice(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t flags);
	virtual PVariable removeLink(BaseLib::PRpcClientInfo clientInfo, std::string senderSerialNumber, int32_t senderChannel, std::string receiverSerialNumber, int32_t receiverChannel);
	virtual PVariable removeLink(BaseLib::PRpcClientInfo clientInfo, uint64_t senderID, int32_t senderChannel, uint64_t receiverID, int32_t receiverChannel);
	virtual PVariable searchDevices(BaseLib::PRpcClientInfo clientInfo, const std::string& interfaceId);
	virtual PVariable searchDevices(BaseLib::PRpcClientInfo clientInfo, bool updateOnly);
protected:
	std::unique_ptr<BaseLib::Ssdp> _ssdp;
	std::atomic_bool _shuttingDown;

	std::mutex _searchDevicesMutex;

	uint32_t _tempMaxAge = 720;

	std::shared_ptr<BosePeer> createPeer(uint32_t deviceType, std::string serialNumber, std::string ip, std::string softwareVersion, std::string idString, std::string typeString, bool save = true);
	void deletePeer(uint64_t id);
	void init();
	void deleteOldTempFiles();
};

}

#endif

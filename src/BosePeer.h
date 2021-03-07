#ifndef BosePEER_H_
#define BosePEER_H_

#include <homegear-base/BaseLib.h>

#include <list>

using namespace BaseLib;
using namespace BaseLib::DeviceDescription;

namespace Bose
{
class BoseCentral;
class BosePacket;

class FrameValue
{
public:
	std::list<uint32_t> channels;
	std::vector<uint8_t> value;
};

class FrameValues
{
public:
	std::string frameID;
	std::list<uint32_t> paramsetChannels;
	ParameterGroup::Type::Enum parameterSetType;
	std::unordered_map<std::string, FrameValue> values;
};

class BosePeer : public BaseLib::Systems::Peer
{
public:
	BosePeer(uint32_t parentID, IPeerEventSink* eventHandler);
	BosePeer(int32_t id, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler);
	virtual ~BosePeer();
	void init();

	//Features
	virtual bool wireless() { return false; }
	//End features

	// {{{ In table variables
	virtual void setIp(std::string value);
	// }}}

	virtual std::string getRinconId();
	virtual void setRinconId(std::string value);

	virtual void setRoomName(std::string value, bool broadCastEvent);

	virtual std::string handleCliCommand(std::string command);

	virtual bool load(BaseLib::Systems::ICentral* central);
    void serializePeers(std::vector<uint8_t>& encodedData);
    void unserializePeers(std::shared_ptr<std::vector<char>> serializedData);
    virtual void savePeers();
	bool hasPeers(int32_t channel) { if(_peers.find(channel) == _peers.end() || _peers[channel].empty()) return false; else return true; }
	void addPeer(std::shared_ptr<BaseLib::Systems::BasicPeer> peer);
	void removePeer(uint64_t id);

	virtual int32_t getChannelGroupedWith(int32_t channel) { return -1; }
	virtual int32_t getNewFirmwareVersion() { return 0; }
	virtual std::string getFirmwareVersionString() { return Peer::getFirmwareVersionString(); }
	virtual std::string getFirmwareVersionString(int32_t firmwareVersion) { return _firmwareVersionString; }
    virtual bool firmwareUpdateAvailable() { return false; }

    void packetReceived(std::shared_ptr<BosePacket> packet);
	void getAll();

    std::string printConfig();

    /**
	 * {@inheritDoc}
	 */
    virtual void homegearShuttingDown();

	//RPC methods
	virtual PVariable getValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, bool requestFromDevice, bool asynchronous);
	virtual PVariable putParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool checkAcls, bool onlyPushing = false);
	virtual PVariable setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait);
	//End RPC methods
protected:
    std::atomic_bool _isMaster;
    std::atomic_bool _isStream;
	std::shared_ptr<BaseLib::Rpc::RpcEncoder> _binaryEncoder;
	std::shared_ptr<BaseLib::Rpc::RpcDecoder> _binaryDecoder;
	std::shared_ptr<BaseLib::HttpClient> _httpClient;

	//Websocket Stuff
	//{{{
    struct Request
    {
        std::mutex mutex;
        std::condition_variable conditionVariable;
        bool mutexReady = false;
        std::shared_ptr<BosePacket> response;
    };


    std::shared_ptr<BaseLib::TcpSocket> _tcpSocket;
    std::atomic_bool _stopCallbackThread;
    std::atomic_bool _stopped;
    std::thread _listenThread;
    std::thread _initThread;
    bool _websocket = false;

    std::unordered_map<std::string, std::shared_ptr<Request>> _responses;
    void wsInit();
    void listen();

    void startListening();
    void stopListening();

    std::shared_ptr<BosePacket> getResponse(const std::string &responseCommand, const std::string &command, int32_t waitForSeconds = 15);

    std::mutex _sendPacketMutex;
    std::mutex _getResponseMutex;
    std::mutex _responsesMutex;
    //}}}

	virtual void loadVariables(BaseLib::Systems::ICentral* central, std::shared_ptr<BaseLib::Database::DataTable>& rows);
    virtual void saveVariables();

	virtual std::shared_ptr<BaseLib::Systems::ICentral> getCentral();
	void getValuesFromPacket(std::shared_ptr<BosePacket> packet, std::vector<FrameValues>& frameValue);
	bool playTTS(uint32_t channel, std::string valueKey, PVariable value, std::pair<std::string, std::string> &soapValue);

	/**
	 * {@inheritDoc}
	 */
	virtual PVariable getValueFromDevice(PParameter& parameter, int32_t channel, bool asynchronous);

	virtual PParameterGroup getParameterSet(int32_t channel, ParameterGroup::Type::Enum type);
	void execute(const std::string functionName, bool ignoreErrors);

	// {{{ Hooks
		/**
		 * {@inheritDoc}
		 */
		virtual bool getAllValuesHook2(PRpcClientInfo clientInfo, PParameter parameter, uint32_t channel, PVariable parameters);

		/**
		 * {@inheritDoc}
		 */
		virtual bool getParamsetHook2(PRpcClientInfo clientInfo, PParameter parameter, uint32_t channel, PVariable parameters);
	// }}}

};

}

#endif

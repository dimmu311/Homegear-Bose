#ifndef EVENTSERVER_H
#define EVENTSERVER_H

#include "../BosePacket.h"
#include "IBoseInterface.h"

namespace Bose
{

class EventServer  : public IBoseInterface
{
    public:
        EventServer(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings);
        virtual ~EventServer();
        void startListening();
        void stopListening();
        void sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet) {}
        int64_t lastAction() { return _lastAction; }
        virtual bool isOpen() { return true; /* Always return true, because there is no continuous connection. */ }
        std::string listenAddress() { return _listenAddress; }
        int32_t listenPort() { return _listenPort; }
        std::string ttsProgram() { return _settings->ttsProgram; }
        std::string dataPath() { return _settings->dataPath; }
    protected:
        std::atomic_bool _stopServer;
        int64_t _lastAction = 0;
        std::string _listenAddress;
        int32_t _listenPort = 7373;
        int32_t _backLog = 10;
        std::shared_ptr<BaseLib::FileDescriptor> _serverFileDescriptor;
        std::vector<char> _httpOkHeader;

        void setListenAddress();
        void getSocketDescriptor();
        std::shared_ptr<BaseLib::FileDescriptor> getClientSocketDescriptor(std::string& ipAddress, int32_t& port);
        void mainThread();
        void readClient(std::shared_ptr<BaseLib::TcpSocket> socket, const std::string& ipAddress, int32_t port);
        std::string getHttpHeader(uint32_t contentLength, std::string contentType, int32_t code, std::string codeDescription, std::vector<std::string>& additionalHeaders);
        void getHttpError(int32_t code, std::string codeDescription, std::string longDescription, std::vector<char>& content);
        void getHttpError(int32_t code, std::string codeDescription, std::string longDescription, std::vector<char>& content, std::vector<std::string>& additionalHeaders);
        void httpGet(BaseLib::Http& http, std::vector<char>& content);
};

}
#endif

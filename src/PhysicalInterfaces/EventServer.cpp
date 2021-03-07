#include "EventServer.h"
#include "../GD.h"
#include "homegear-base/Encoding/RapidXml/rapidxml.h"
#include <ifaddrs.h>

namespace Bose
{
EventServer::EventServer(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IBoseInterface(settings)
{
	_out.init(GD::bl);
	_out.setPrefix(GD::out.getPrefix() + "Event server \"" + settings->id + "\": ");

	_stopServer = true;

	if(!settings)
	{
		_out.printCritical("Critical: Error initializing. Settings pointer is empty.");
		return;
	}
	_listenPort = BaseLib::Math::getNumber(settings->port);
	if(_listenPort <= 0  || _listenPort > 65535) _listenPort = 7373;

	std::string httpOkHeader("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n");
	_httpOkHeader.insert(_httpOkHeader.end(), httpOkHeader.begin(), httpOkHeader.end());
}

EventServer::~EventServer()
{
	try
	{
		_stopServer = true;
		GD::bl->threadManager.join(_listenThread);
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void EventServer::setListenAddress()
{
	try
	{
		if(!_settings->host.empty() && !BaseLib::Net::isIp(_settings->host))
		{
			//Assume address is interface name
			_listenAddress = BaseLib::Net::getMyIpAddress(_settings->host);
		}
		else if(_settings->host.empty())
		{
			_listenAddress = BaseLib::Net::getMyIpAddress();
			if(_listenAddress.empty()) _bl->out.printError("Error: No IP address could be found to bind the server to. Please specify the IP address manually in Bose.conf.");
		}
		else _listenAddress = _settings->host;
	}
	catch(const std::exception& ex)
	{
		_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

void EventServer::startListening()
{
	try
	{
		stopListening();
		setListenAddress();
		if(_listenAddress.empty())
		{
			GD::out.printError("Error: Could not get listen automatically. Please specify it in Bose.conf");
			return;
		}
		_ipAddress = _listenAddress;
		_hostname = _listenAddress;
		_stopServer = false;
		_bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &EventServer::mainThread, this);
		IPhysicalInterface::startListening();
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void EventServer::stopListening()
{
	try
	{
		if(_stopServer) return;
		_stopServer = true;
		GD::bl->threadManager.join(_listenThread);

		IPhysicalInterface::stopListening();
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void EventServer::mainThread()
{
    try
    {
    	getSocketDescriptor();

    	std::string ipAddress;
    	int32_t port = -1;
    	std::shared_ptr<BaseLib::FileDescriptor> clientFileDescriptor;

        while(!_stopServer)
        {
			try
			{
				if(!_serverFileDescriptor || _serverFileDescriptor->descriptor == -1)
				{
					if(_stopServer) break;
					std::this_thread::sleep_for(std::chrono::milliseconds(5000));
					getSocketDescriptor();
					continue;
				}
				clientFileDescriptor = getClientSocketDescriptor(ipAddress, port);
				if(!clientFileDescriptor || clientFileDescriptor->descriptor == -1) continue;

				std::shared_ptr<BaseLib::TcpSocket> socket(new BaseLib::TcpSocket(GD::bl, clientFileDescriptor));
				readClient(socket, ipAddress, port);
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			GD::bl->fileDescriptorManager.shutdown(clientFileDescriptor);
        }
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    GD::bl->fileDescriptorManager.shutdown(_serverFileDescriptor);
}

void EventServer::readClient(std::shared_ptr<BaseLib::TcpSocket> socket, const std::string& ipAddress, int32_t port)
{
	try
	{
		if(!socket) return;
		int32_t bufferMax = 1024;
		char buffer[bufferMax + 1];
		//Make sure the buffer is null terminated.
		buffer[bufferMax] = '\0';
		std::vector<char> packet;
		int32_t bytesRead;
		BaseLib::Http http;

		while(!_stopServer)
		{
			try
			{
				bytesRead = socket->proofread(buffer, bufferMax);
				buffer[bufferMax] = 0; //Even though it shouldn't matter, make sure there is a null termination.
				//Some clients send only one byte in the first packet
				if(!http.headerProcessingStarted() && bytesRead == 1) bytesRead += socket->proofread(&buffer[1], bufferMax - 1);
			}
			catch(const BaseLib::SocketTimeOutException& ex)
			{
				_out.printWarning("Warning: Connection timed out.");
				break;
			}
			catch(const BaseLib::SocketClosedException& ex)
			{
				_out.printInfo("Info: " + std::string(ex.what()));
				break;
			}
			catch(const BaseLib::SocketOperationException& ex)
			{
				_out.printError(ex.what());
				break;
			}

			if(GD::bl->debugLevel >= 5)
			{
				std::vector<uint8_t> rawPacket(buffer, buffer + bytesRead);
				_out.printDebug("Debug: Packet received: " + BaseLib::HelperFunctions::getHexString(rawPacket));
			}
			buffer[bytesRead] = '\0';
			if(!http.headerProcessingStarted() && (!strncmp(&buffer[0], "NOTIFY", 6) || !strncmp(&buffer[0], "GET", 3) || !strncmp(&buffer[0], "HTTP/1.", 7))) http.reset();
			else if(!http.headerProcessingStarted())
			{
				_out.printError("Error: Uninterpretable packet received. Closing connection. Packet was: " + std::string(buffer, bytesRead));
				break;
			}

			try
			{
				http.process(buffer, bytesRead);
			}
			catch(BaseLib::HttpException& ex)
			{
				_out.printError("Error: Could not process HTTP packet: " + std::string(ex.what()) + " Buffer: " + std::string(buffer, bytesRead));
				http.reset();
			}

			if(http.getContentSize() > 10485760)
			{
				_out.printError("Error: Received HTTP packet larger than 10MiB.");
				http.reset();
			}

			if(http.isFinished())
			{
                std::vector<char> response;
				if(http.getHeader().method == "GET")
				{
					http.getHeader().remoteAddress = ipAddress;
					http.getHeader().remotePort = port;
					httpGet(http, response);
					if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: Webserver response: " + BaseLib::HelperFunctions::getHexString(response));

					try
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(22)); //Wait a little. Otherwise clients might not receive response.
						socket->proofwrite(response);
					}
					catch(BaseLib::SocketDataLimitException& ex)
					{
						_out.printWarning("Warning: " + std::string(ex.what()));
					}
					catch(const BaseLib::SocketOperationException& ex)
					{
						_out.printInfo("Info: " + std::string(ex.what()));
					}
					break;
				}
			}
		}
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void EventServer::getSocketDescriptor()
{
	try
	{
		addrinfo hostInfo;
		addrinfo *serverInfo = nullptr;

		int32_t yes = 1;

		memset(&hostInfo, 0, sizeof(hostInfo));

		hostInfo.ai_family = AF_UNSPEC;
		hostInfo.ai_socktype = SOCK_STREAM;
		hostInfo.ai_flags = AI_PASSIVE;
		char buffer[100];
		std::string port = std::to_string(_listenPort);
		int32_t result;
		if((result = getaddrinfo(_listenAddress.c_str(), port.c_str(), &hostInfo, &serverInfo)) != 0)
		{
			_out.printCritical("Error: Could not get address information: " + std::string(gai_strerror(result)));
			return;
		}

		bool bound = false;
		int32_t error = 0;
		for(struct addrinfo *info = serverInfo; info != 0; info = info->ai_next)
		{
			_serverFileDescriptor = GD::bl->fileDescriptorManager.add(socket(info->ai_family, info->ai_socktype, info->ai_protocol));
			if(_serverFileDescriptor->descriptor == -1) continue;
			if(!(fcntl(_serverFileDescriptor->descriptor, F_GETFL) & O_NONBLOCK))
			{
				if(fcntl(_serverFileDescriptor->descriptor, F_SETFL, fcntl(_serverFileDescriptor->descriptor, F_GETFL) | O_NONBLOCK) < 0) throw BaseLib::Exception("Error: Could not set socket options.");
			}
			if(setsockopt(_serverFileDescriptor->descriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int32_t)) == -1) throw BaseLib::Exception("Error: Could not set socket options.");
			if(bind(_serverFileDescriptor->descriptor.load(), info->ai_addr, info->ai_addrlen) == -1)
			{
				error = errno;
				continue;
			}
			switch (info->ai_family)
			{
				case AF_INET:
					inet_ntop (info->ai_family, &((struct sockaddr_in *) info->ai_addr)->sin_addr, buffer, 100);
					break;
				case AF_INET6:
					inet_ntop (info->ai_family, &((struct sockaddr_in6 *) info->ai_addr)->sin6_addr, buffer, 100);
					break;
			}
			_out.printInfo("Info: Started listening on address " + _listenAddress + " and port " + port);
			bound = true;
			break;
		}
		freeaddrinfo(serverInfo);
		if(!bound)
		{
			GD::bl->fileDescriptorManager.shutdown(_serverFileDescriptor);
			_out.printCritical("Error: Could not start listening on port " + port + ": " + std::string(strerror(error)));
			return;
		}
		if(_serverFileDescriptor->descriptor == -1 || !bound || listen(_serverFileDescriptor->descriptor, _backLog) == -1)
		{
			GD::bl->fileDescriptorManager.shutdown(_serverFileDescriptor);
			_out.printCritical("Error: Could not start listening on port " + port + ": " + std::string(strerror(errno)));
			return;
		}
    }
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::shared_ptr<BaseLib::FileDescriptor> EventServer::getClientSocketDescriptor(std::string& ipAddress, int32_t& port)
{
	std::shared_ptr<BaseLib::FileDescriptor> fileDescriptor;
	try
	{
		timeval timeout;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		fd_set readFileDescriptor;
		int32_t nfds = 0;
		FD_ZERO(&readFileDescriptor);
		{
			auto fileDescriptorGuard = GD::bl->fileDescriptorManager.getLock();
			fileDescriptorGuard.lock();
			nfds = _serverFileDescriptor->descriptor + 1;
			if(nfds <= 0)
			{
				fileDescriptorGuard.unlock();
				GD::out.printError("Error: Server file descriptor is invalid.");
				return fileDescriptor;
			}
			FD_SET(_serverFileDescriptor->descriptor, &readFileDescriptor);
		}
		if(!select(nfds, &readFileDescriptor, NULL, NULL, &timeout)) return fileDescriptor;

		struct sockaddr_storage clientInfo;
		socklen_t addressSize = sizeof(addressSize);
		fileDescriptor = GD::bl->fileDescriptorManager.add(accept(_serverFileDescriptor->descriptor, (struct sockaddr *) &clientInfo, &addressSize));
		if(!fileDescriptor) return fileDescriptor;

		getpeername(fileDescriptor->descriptor, (struct sockaddr*)&clientInfo, &addressSize);

		char ipString[INET6_ADDRSTRLEN];
		if (clientInfo.ss_family == AF_INET) {
			struct sockaddr_in *s = (struct sockaddr_in *)&clientInfo;
			port = ntohs(s->sin_port);
			inet_ntop(AF_INET, &s->sin_addr, ipString, sizeof(ipString));
		} else { // AF_INET6
			struct sockaddr_in6 *s = (struct sockaddr_in6 *)&clientInfo;
			port = ntohs(s->sin6_port);
			inet_ntop(AF_INET6, &s->sin6_addr, ipString, sizeof(ipString));
		}

		ipAddress = std::string(&ipString[0]);
		_out.printInfo("Info: Connection from " + ipAddress + ":" + std::to_string(port) + " accepted. Client number: " + std::to_string(fileDescriptor->id));
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return fileDescriptor;
}

std::string EventServer::getHttpHeader(uint32_t contentLength, std::string contentType, int32_t code, std::string codeDescription, std::vector<std::string>& additionalHeaders)
{
	try
	{
		std::string additionalHeader;
		additionalHeader.reserve(1024);
		for(std::vector<std::string>::iterator i = additionalHeaders.begin(); i != additionalHeaders.end(); ++i)
		{
			BaseLib::HelperFunctions::trim(*i);
			if((*i).find("Location: ") == 0)
			{
				code = 301;
				codeDescription = "Moved Permanently";
			}
			if(!(*i).empty()) additionalHeader.append(*i + "\r\n");
		}

		std::string header;
		header.reserve(1024);
		header.append("HTTP/1.1 " + std::to_string(code) + " " + codeDescription + "\r\n");
		header.append("Connection: close\r\n");
		if(!contentType.empty()) header.append("Content-Type: " + contentType + "\r\n");
		header.append(additionalHeader);
		header.append("Content-Length: ").append(std::to_string(contentLength)).append("\r\n\r\n");
		return header;
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return "";
}

void EventServer::getHttpError(int32_t code, std::string codeDescription, std::string longDescription, std::vector<char>& content)
{
	try
	{
		std::vector<std::string> additionalHeaders;
		std::string contentString = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\"><html><head><title>" + std::to_string(code) + " " + codeDescription + "</title></head><body><h1>" + codeDescription + "</h1><p>" + longDescription + "<br/></p><hr><address>Homegear at Port " + std::to_string(_listenPort) + "</address></body></html>";
		std::string header = getHttpHeader(contentString.size(), "text/html", code, codeDescription, additionalHeaders);
		content.insert(content.end(), header.begin(), header.end());
		content.insert(content.end(), contentString.begin(), contentString.end());
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void EventServer::getHttpError(int32_t code, std::string codeDescription, std::string longDescription, std::vector<char>& content, std::vector<std::string>& additionalHeaders)
{
	try
	{
		std::string contentString = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\"><html><head><title>" + std::to_string(code) + " " + codeDescription + "</title></head><body><h1>" + codeDescription + "</h1><p>" + longDescription + "<br/></p><hr><address>Homegear at Port " + std::to_string(_listenPort) + "</address></body></html>";
		std::string header = getHttpHeader(contentString.size(), "text/html", code, codeDescription, additionalHeaders);
		content.insert(content.end(), header.begin(), header.end());
		content.insert(content.end(), contentString.begin(), contentString.end());
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void EventServer::httpGet(BaseLib::Http& http, std::vector<char>& content)
{
	try
	{
		std::string path = http.getHeader().path;
		std::vector<std::string> headers;

		if(!path.empty() && path.front() == '/') path = path.substr(1);
		if(!GD::bl->io.directoryExists(GD::bl->settings.tempPath() + "Bose"))
		{
			if(!GD::bl->io.createDirectory(GD::bl->settings.tempPath() + "Bose", S_IRWXU | S_IRWXG))
			{
				GD::out.printError("Error: Cannot create temp directory \"" + GD::bl->settings.tempPath() + "Bose");
			}
		}
		std::string contentPath = _bl->settings.tempPath() + "Bose/" + path;
		if(!BaseLib::Io::fileExists(contentPath)) contentPath = GD::dataPath + path;
		if(!BaseLib::Io::fileExists(contentPath))
		{
			getHttpError(404, http.getStatusText(404), "The requested URL was not found on this server.", content);
			return;
		}

		try
		{
			_out.printInfo("Client is requesting: " + http.getHeader().path + " (translated to " + contentPath + ", method: GET)");
			std::string ending = "";
			int32_t pos = path.find_last_of('.');
			if(pos != (signed)std::string::npos && (unsigned)pos < path.size() - 1) ending = path.substr(pos + 1);
			GD::bl->hf.toLower(ending);
			std::string contentString;

			std::string contentType = http.getMimeType(ending);
			if(contentType.empty()) contentType = "application/octet-stream";
			//Don't return content when method is "HEAD"
			if(http.getHeader().method == "GET") contentString = GD::bl->io.getFileContent(contentPath);
			std::string header = getHttpHeader(contentString.size(), contentType, 200, "OK", headers);
			content.insert(content.end(), header.begin(), header.end());
			if(!contentString.empty()) content.insert(content.end(), contentString.begin(), contentString.end());
		}
		catch(const std::exception& ex)
		{
			getHttpError(404, http.getStatusText(404), "The requested URL " + path + " was not found on this server.", content);
			return;
		}
	}
	catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}
}

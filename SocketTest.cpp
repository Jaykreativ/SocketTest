#include <iostream>
#include <string>
#include <cstring>
#include <stdio.h>
#include <vector>
#include <memory>


#ifdef _WIN32 // windows specific socket include

#include <WinSock2.h>
#include <WS2tcpip.h>

#elif __linux__

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>

typedef in_addr IN_ADDR;
typedef in6_addr IN6_ADDR;

#endif 

#define MAX_MSG_LEN 50

namespace sock {
	int close(int socket){
#ifdef _WIN32
		return closesocket(socket);
#elif __linux__
		return close(socket);
#endif
	}

	int pollState(pollfd fds[], size_t nfds, int timeout) {
#ifdef _WIN32
		return WSAPoll(fds, nfds, timeout);
#elif __linux__
		return poll(fds, nfds, timeout);
#endif
	}

	IN_ADDR presentationToAddrIPv4(std::string presentation) {
		IN_ADDR addr;
		if (inet_pton(AF_INET, presentation.c_str(), &addr) <= 0) {
			fprintf(stderr, "error while decoding ip address\n");
			exit(3);
		}
		return addr;
	}
	IN6_ADDR presentationToAddrIPv6(std::string presentation) {
		IN6_ADDR addr;
		if (inet_pton(AF_INET6, presentation.c_str(), &addr) <= 0) {
			fprintf(stderr, "error while decoding ip address\n");
			exit(3);
		}
		return addr;
	}

	std::string addrToPresentationIPv4(IN_ADDR addr) {
		char ip4[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr, ip4, INET_ADDRSTRLEN);
		return std::string(ip4);
	}
	std::string addrToPresentationIPv6(IN6_ADDR addr) {
		char ip6[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &addr, ip6, INET6_ADDRSTRLEN);
		return ip6;
	}
	std::string addrToPresentation(sockaddr* sa) {
		if (sa->sa_family == AF_INET) {
			return addrToPresentationIPv4(reinterpret_cast<sockaddr_in*>(sa)->sin_addr);
		}

		return addrToPresentationIPv6(reinterpret_cast<sockaddr_in6*>(sa)->sin6_addr);
	}

	int lastError() {
#ifdef _WIN32
		return WSAGetLastError();
#elif __linux__
		return errno;
#endif
	}

	void printLastError(const char* msg) {
#ifdef _WIN32
		char* s = NULL;
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, WSAGetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&s, 0, NULL);
		fprintf(stderr, "%s: %s\n", msg, s);
#elif __linux__
		perror(msg);
#endif
	}
}

#ifdef _WIN32
void startWSA() {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		fprintf(stderr, "WSAStartup failed\n");
		exit(1);
	}

	if (LOBYTE(wsaData.wVersion) != 2 ||
		HIBYTE(wsaData.wVersion) != 2)
	{
		fprintf(stderr, "Version 2.2 of Winsock not available\n");
		WSACleanup();
		exit(1);
	}

}
#endif // _WIN32

enum Result {
	eSTOP,
	eSUCCESS,
	eERROR,
	ePOLL_TIMEOUT
};

Result receiveData(int socket) {
	char buf[MAX_MSG_LEN + 1] = "";
	int bytesRead = recv(socket, buf, MAX_MSG_LEN, 0);
	if (bytesRead == -1) {
		sock::printLastError("recv");
		return eERROR;
	}
	// exit on stop
	if (strcmp(buf, "stop") == 0) {
		return eSTOP;
	}
	buf[bytesRead] = '\0';
	printf("message received: %s\n", buf);
}

Result destroyClient(int clientSocket, int clientNum, std::vector<int>& clientSockets) {
	if (sock::close(clientSocket) < 0) {
		perror("close");
		exit(errno);
	}
	clientSockets.erase(clientSockets.begin()+clientNum);
	return eSUCCESS;
}

Result handlePoll(const std::vector<pollfd>& pollfds, int pollCount, int serverStreamSocket, int serverDgramSocket, std::vector<int>& clientSockets) {
	int checkedPollCount = 0;
	pollfd streamPoll = pollfds[0];
	pollfd dgramPoll = pollfds[1];
	if (streamPoll.revents & POLLIN) {// accept client
		sockaddr_storage clientAddr;
		socklen_t addrSize = sizeof clientAddr;
		int clientSocket;
		if ((clientSocket = accept(serverStreamSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrSize)) < 0) {
			perror("accept");
			exit(errno);
		}
		clientSockets.push_back(clientSocket);
		
		printf("Client connected: %s\n", sock::addrToPresentation(reinterpret_cast<sockaddr*>(&clientAddr)).c_str());
	}
	if (dgramPoll.revents & POLLIN) {
		sockaddr_storage clientAddr;
		socklen_t addrSize = sizeof clientAddr;
		char buf[MAX_MSG_LEN + 1] = "";
		int bytesRead = recvfrom(serverDgramSocket, buf, MAX_MSG_LEN, 0, reinterpret_cast<sockaddr*>(&clientAddr), &addrSize);
		buf[bytesRead] = '\0';

		printf("message received from: %s\n>> %s\n", sock::addrToPresentation(reinterpret_cast<sockaddr*>(&clientAddr)).c_str(), buf);
	}
	for (size_t i = 2; i < pollfds.size(); i++) {
		pollfd poll = pollfds[i];
		if (poll.revents & POLLIN) { // read the polled socket with recv
			Result result = receiveData(poll.fd);
			if (result == eSTOP)// return stop
				return result;
		}
		if (poll.revents & POLLHUP) { // destroy the polled clientSocket
			sockaddr clientAddr;
			socklen_t addrSize = sizeof clientAddr;
			getpeername(poll.fd, &clientAddr, &addrSize);
			printf("Client disconnected: %s", sock::addrToPresentation(&clientAddr).c_str());
			Result result = destroyClient(poll.fd, i-1, clientSockets);
		}

		if (poll.revents & (POLLIN | POLLHUP))
			checkedPollCount++;
		if (checkedPollCount >= pollCount)
			return eSUCCESS;
		i++;
	}

	return ePOLL_TIMEOUT;
}

void generatePollArray(std::vector<pollfd>& pollfds, int serverStreamSocket, int serverDgramSocket, const std::vector<int>& clientSockets) {
	pollfds.resize(clientSockets.size() + 2); // clientSockets + serverSockets
	pollfds[0].fd = serverStreamSocket;
	pollfds[0].events = POLLIN;
	pollfds[1].fd = serverDgramSocket;
	pollfds[1].events = POLLIN;
	for (int i = 2; i < pollfds.size(); i++) {
		pollfds[i].fd = clientSockets[i - 2];
		pollfds[i].events = POLLIN;
	}
}

void serverLoop(int serverStreamSocket, int serverDgramSocket) {
	int pollTimeout = -1;

	std::vector<int> clientSockets;

	// server loop
	while (true) {
		std::vector<pollfd> pollfds;
		generatePollArray(pollfds, serverStreamSocket, serverDgramSocket, clientSockets);

		int pollCount = sock::pollState(pollfds.data(), pollfds.size(), pollTimeout);

		if (pollCount == -1) {
			sock::printLastError("poll");
			exit(sock::lastError());
		}

		Result result = handlePoll(pollfds, pollCount, serverStreamSocket, serverDgramSocket, clientSockets);
		if (result == eSTOP)// stop has been sent
			return;
	}
}

void runServer() {
	const std::string port = "12525";
	const int backlog = 20;

	addrinfo hints;
	addrinfo* serverInfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_flags = AI_PASSIVE;

	int status;
	if ((status = getaddrinfo(NULL, port.c_str(), &hints, &serverInfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		exit(1);
	}

	for (addrinfo* p = serverInfo; p != nullptr; p = p->ai_next) {
		if (p->ai_family == AF_INET) {
			printf("IPv4 Address: %s\n", sock::addrToPresentationIPv4(reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr).c_str());
		}
		if (p->ai_family == AF_INET6) {
			printf("IPv6 Address: %s\n", sock::addrToPresentationIPv6(reinterpret_cast<sockaddr_in6*>(p->ai_addr)->sin6_addr).c_str());
		}
	}

	// creating the socket
	int serverStreamSocket;
	int serverDgramSocket;
	if ((serverStreamSocket = socket(serverInfo->ai_family, SOCK_STREAM, serverInfo->ai_protocol)) < 0) {
		sock::printLastError("socket(stream)");
		exit(sock::lastError());
	}
	if ((serverDgramSocket = socket(serverInfo->ai_family, SOCK_DGRAM, serverInfo->ai_protocol)) < 0) {
		sock::printLastError("socket(dgram)");
		exit(sock::lastError());
	}

	// enable port reuse
	/*const char yes = 1;
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
		perror("setsockopt");
		exit(3);
	}*/

	// bind to port
	if (bind(serverStreamSocket, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0) {
		sock::printLastError("bind(stream)");
		exit(sock::lastError());
	}
	if (bind(serverDgramSocket, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0) {
		sock::printLastError("bind(dgram)");
		exit(sock::lastError());
	}

	if (listen(serverStreamSocket, backlog) < 0) {
		sock::printLastError("listen");
		exit(sock::lastError());
	}

	freeaddrinfo(serverInfo);

	serverLoop(serverStreamSocket, serverDgramSocket);

	sock::close(serverStreamSocket);
	sock::close(serverDgramSocket);

	printf("server done\n");
}

void runClient() {
	const std::string port = "12525";

	addrinfo hints;
	addrinfo* serverInfo;
	int streamSocket;
	int dgramSocket;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;

	// get ip address from user and connect
	bool wrongIP;
	do {
		wrongIP = false;

		std::string ipString;
		printf("To which IP address do you want to connect to? (format x.x.x.x with x 0 - 255)\n>> ");
		std::cin >> ipString;

		int status;
		if ((status = getaddrinfo(ipString.c_str(), port.c_str(), &hints, &serverInfo)) != 0) {
			fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
			wrongIP = true;
			continue;
		}

		for (addrinfo* p = serverInfo; p != nullptr; p = p->ai_next) {
			if (p->ai_family == AF_INET) {
				printf("connecting to IPv4 Address: %s\n", sock::addrToPresentationIPv4(reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr).c_str());
			}
			if (p->ai_family == AF_INET6) {
				printf("connecting to IPv6 Address: %s\n", sock::addrToPresentationIPv6(reinterpret_cast<sockaddr_in6*>(p->ai_addr)->sin6_addr).c_str());
			}
		}

		streamSocket = socket(serverInfo->ai_family, SOCK_STREAM, serverInfo->ai_protocol);
		if (streamSocket < 0) {
			perror("socket(stream)");
			wrongIP = true;
			continue;
		}
		dgramSocket = socket(serverInfo->ai_family, SOCK_DGRAM, serverInfo->ai_protocol);
		if (dgramSocket < 0) {
			perror("socket(dgram)");
			wrongIP = true;
			continue;
		}

		if (connect(streamSocket, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0) {
			perror("connect(stream)");
			wrongIP = true;
			continue;
		}
		if (connect(dgramSocket, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0) {
			perror("connect(dgram)");
			wrongIP = true;
			continue;
		}
	}
	while (wrongIP);

	// client loop
	while(true) {
		char msg[MAX_MSG_LEN] = "";
		printf(">> ");
		std::cin.getline(msg, MAX_MSG_LEN);
		int len = strlen(msg);
		if (strcmp(msg, "esc")==0)
			break;
		send(streamSocket, msg, len, 0);
		send(dgramSocket, msg, len, 0);
		if (strcmp(msg, "stop") == 0)
			break;
		printf("message sent: %s\n", msg);
	}

	sock::close(streamSocket);
	freeaddrinfo(serverInfo);
	
	printf("client done\n");
}

int main() {
#ifdef _WIN32
	startWSA();
#endif // _WIN32

	bool isServer = false;
	printf("Do you want to run as a Server? (1 server | 0 client)\n>> ");
	std::cin >> isServer;
	if (isServer) {
		runServer();
	}
	else {
		runClient();
	}

#ifdef _WIN32
	system("pause");
#endif // _WIN32

	return 0;
}

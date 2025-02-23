#include <iostream>
#include <string>
#include <cstring>
#include <stdio.h>
#include <thread>

#ifdef _WIN32 // windows specific socket include

#include <WinSock2.h>
#include <WS2tcpip.h>

#elif __linux__

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

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

void serverLoop(int clientSocket, bool& stopped) {
	// server loop
	while (true) {
		char buf[MAX_MSG_LEN + 1] = "";
		int bytesRead = recv(clientSocket, buf, MAX_MSG_LEN, 0);
		if (bytesRead == -1) {
			perror("recv");
			continue;
		}

		// return on disconnect
		if (bytesRead == 0) {
			return;
		}
		// exit on stop
		if (strcmp(buf, "stop") == 0) {
			stopped = true;
			return;
		}
		buf[bytesRead] = '\0';
		printf("message received: %s\n", buf);
	}
}

void runServer() {
	const std::string port = "12525";
	const int backlog = 20;

	addrinfo hints;
	addrinfo* serverInfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
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
	int serverSocket;
	if ((serverSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol)) < 0) {
		perror("socket");
		exit(2);
	}

	// enable port reuse
	/*const char yes = 1;
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
		perror("setsockopt");
		exit(3);
	}*/

	// bind to port
	if (bind(serverSocket, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0) {
		perror("bind");
		exit(4);
	}

	if (listen(serverSocket, backlog) < 0) {
		perror("listen");
		exit(5);
	}

	bool stopped = false;
	while (!stopped)
	{
		sockaddr_storage clientAddr;
		socklen_t addrSize = sizeof clientAddr;
		int clientSocket;
		if ((clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrSize)) < 0) {
			perror("accept");
			exit(5);
		}

		serverLoop(clientSocket, stopped);
		sock::close(clientSocket);
	}

	// free resources
	sock::close(serverSocket);
	freeaddrinfo(serverInfo);

	printf("server done\n");
}

void runClient() {
	const std::string port = "12525";

	addrinfo hints;
	addrinfo* clientInfo;
	int clientSocket;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// get ip address from user and connect
	bool wrongIP;
	do {
		wrongIP = false;

		std::string ipString;
		printf("To which IP address do you want to connect to? (format x.x.x.x with x 0 - 255)\n>> ");
		std::cin >> ipString;

		int status;
		if ((status = getaddrinfo(ipString.c_str(), port.c_str(), &hints, &clientInfo)) != 0) {
			fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
			wrongIP = true;
			continue;
		}

		for (addrinfo* p = clientInfo; p != nullptr; p = p->ai_next) {
			if (p->ai_family == AF_INET) {
				printf("connecting to IPv4 Address: %s\n", sock::addrToPresentationIPv4(reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr).c_str());
			}
			if (p->ai_family == AF_INET6) {
				printf("connecting to IPv6 Address: %s\n", sock::addrToPresentationIPv6(reinterpret_cast<sockaddr_in6*>(p->ai_addr)->sin6_addr).c_str());
			}
		}

		clientSocket = socket(clientInfo->ai_family, clientInfo->ai_socktype, clientInfo->ai_protocol);
		if (clientSocket < 0) {
			perror("socket");
			wrongIP = true;
			continue;
		}

		if (connect(clientSocket, clientInfo->ai_addr, clientInfo->ai_addrlen) < 0) {
			perror("connect");
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
		send(clientSocket, msg, len, 0);
		if (strcmp(msg, "stop") == 0)
			break;
		printf("message sent: %s\n", msg);
	}

	sock::close(clientSocket);
	freeaddrinfo(clientInfo);
	
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

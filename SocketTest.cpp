#include <iostream>
#include <string>
#include <stdio.h>
#include <thread>

#include <WinSock2.h>
#include <WS2tcpip.h>

#define MAX_MSG_LEN 50

namespace sockets {
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
			printf("IPv4 Address: %s\n", sockets::addrToPresentationIPv4(reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr).c_str());
		}
		if (p->ai_family == AF_INET6) {
			printf("IPv6 Address: %s\n", sockets::addrToPresentationIPv6(reinterpret_cast<sockaddr_in6*>(p->ai_addr)->sin6_addr).c_str());
		}
	}

	// creating the socket
	SOCKET serverSocket;
	if ((serverSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol)) < 0) {
		perror("socket");
		exit(2);
	}

	// enable port reuse
	const char yes = 1;
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
		perror("setsockopt");
		exit(3);
	}

	// bind to port
	if (bind(serverSocket, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0) {
		perror("bind");
		exit(4);
	}

	if (listen(serverSocket, backlog) < 0) {
		perror("listen");
		exit(5);
	}

	sockaddr_storage clientAddr;
	socklen_t addrSize = sizeof clientAddr;
	SOCKET clientSocket;
	if ((clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrSize)) < 0) {
		perror("accept");
		exit(5);
	}

	while(true) {
		char buf[MAX_MSG_LEN] = "";
		int bytesRead = recv(clientSocket, buf, MAX_MSG_LEN, 0);
		if (bytesRead == -1) {
			perror("recv");
			continue;
		}
		// exit on disconnect
		if (bytesRead == 0) {
			break;
		}
		if (bytesRead < MAX_MSG_LEN) {
			buf[bytesRead] = '\0';
			printf("message received: %s\n", buf);
		}
		else
			printf("message too long(max %i chars)\n", MAX_MSG_LEN);
	}

	// free resources
	closesocket(clientSocket);
	closesocket(serverSocket);
	freeaddrinfo(serverInfo);

	printf("server done\n");
}

void runClient() {
	const std::string port = "12525";

	addrinfo hints;
	addrinfo* clientInfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int status;
	if ((status = getaddrinfo("127.0.0.1", port.c_str(), &hints, &clientInfo)) != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		exit(1);
	}

	for (addrinfo* p = clientInfo; p != nullptr; p = p->ai_next) {
		if (p->ai_family == AF_INET) {
			printf("IPv4 Address: %s\n", sockets::addrToPresentationIPv4(reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr).c_str());
		}
		if (p->ai_family == AF_INET6) {
			printf("IPv6 Address: %s\n", sockets::addrToPresentationIPv6(reinterpret_cast<sockaddr_in6*>(p->ai_addr)->sin6_addr).c_str());
		}
	}

	SOCKET clientSocket = socket(clientInfo->ai_family, clientInfo->ai_socktype, clientInfo->ai_protocol);
	if (clientSocket < 0) {
		perror("socket");
		exit(2);
	}

	if (connect(clientSocket, clientInfo->ai_addr, clientInfo->ai_addrlen) < 0) {
		perror("connect");
		exit(3);
	}

	while(true) {
		char msg[MAX_MSG_LEN] = "";
		std::cin.getline(msg, MAX_MSG_LEN);
		int len = strlen(msg);
		if (strcmp(msg, "esc")==0)
			break;
		send(clientSocket, msg, len, 0);
		printf("message sent: %s\n", msg);
	}

	closesocket(clientSocket);
	freeaddrinfo(clientInfo);
	
	printf("client done\n");
}

void main() {
	startWSA();

	bool isServer = false;
	printf("Do you want to run as a Server? (1 server | 0 client)\n");
	std::cin >> isServer;
	if (isServer) {
		runServer();
	}
	else {
		runClient();
	}

	system("pause");
	return;
}
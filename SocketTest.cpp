#include <iostream>

#include <WinSock2.h>
#include <WS2tcpip.h>

namespace sockets {
	IN_ADDR presentationToAddr(int family, std::string presentation) {
		IN_ADDR addr;
		if (inet_pton(family, presentation.c_str(), &addr) <= 0) {
			fprintf(stderr, "error while decoding ip address\n");
			exit(3);
		}
	}
}

void main() {
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
		exit(2);
	}

	sockaddr_in socketAdressIPv4;

	system("pause");
	return;
}
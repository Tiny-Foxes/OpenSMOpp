#ifndef Main_Hpp
#define Main_Hpp

#include <mutex>
#include "TCPServer.h"

bool Running = true;

std::mutex m_Mutex;

CTCPServer* m_TCPServer;

struct Clients {
	ASocket::Socket Client;
	bool LoggedIn;
	std::string IP;
	std::string UserName = "Unknown";
	int RoomID = -1;
};


std::vector<Clients> ConnectedClients;

std::string m_IP;
std::string ServerName;
unsigned ServerVersion;
unsigned ProtocolVersion;
unsigned ServerPort;
unsigned MaxPlayers;
std::string ServerDB;
std::string PWSalt;

#endif
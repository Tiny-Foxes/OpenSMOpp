#ifndef Main_Hpp
#define Main_Hpp

#include <mutex>
#include <map>
#include "TCPServer.h"

bool Running = true;

std::mutex m_Mutex;

CTCPServer* m_TCPServer;
long long g_RoomID = 0;

struct Rooms {
	long long RoomID = -1;
	std::string Owner = "";
	std::string RoomName = "";
	std::string RoomDescription = "";
	std::string RoomPassword = "";
	int NumPlayers = 0;
};

struct Clients {
	ASocket::Socket Client;
	bool LoggedIn = false;
	std::string IP = "0.0.0.0";
	std::string UserName = "Unknown";
	long long RoomID = -1;
};

std::vector<Rooms> PlayerRooms;
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
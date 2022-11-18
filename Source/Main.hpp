#ifndef Main_Hpp
#define Main_Hpp

#include <mutex>
#include <map>
#include <array>
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
	int State = 0;
	bool PassFlag = false;
	std::string UsersMissingSong = "";
	bool SongSelected = false;
	int NumPlayersWaiting = 0;
	int NumPlayersPlaying = 0;
	std::array<std::string, 3> CurSong = {};
	std::vector<std::string> CurPlayers = {};
	bool FreeMode = false;
};

struct Clients {
	ASocket::Socket Client;
	bool LoggedIn = false;
	std::string IP = "0.0.0.0";
	std::string UserName = "Unknown";
	unsigned UserType = 0;
	long long RoomID = -1;
	std::array<unsigned, 9> TNSs = {};
	unsigned ScoreTracker = 0;
	unsigned SMClientID = 0;
};

std::vector<Rooms> PlayerRooms;
std::vector<Clients> ConnectedClients;

std::string m_IP;
bool m_GotIP = false;
std::string ServerName;
std::string ElevatedUserLogin;
unsigned ServerVersion;
unsigned ProtocolVersion;
unsigned ServerPort;
unsigned MaxPlayers;
std::string ServerDB;
std::string PWSalt;

#endif
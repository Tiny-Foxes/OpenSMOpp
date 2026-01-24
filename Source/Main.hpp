#ifndef Main_Hpp
#define Main_Hpp

#include <mutex>
#include <map>
#include <array>
#include <atomic>

#include "TCPServer.h"

inline std::atomic<bool> Running{true};

inline std::mutex m_Mutex;

inline CTCPServer* m_TCPServer;
inline long long g_RoomID = 0;

struct Rooms {
	long long RoomID = -1;
	std::string Owner;
	std::string RoomName;
	std::string RoomDescription;
	std::string RoomPassword;
	int NumPlayers = 0;
	int State = 0;
	bool PassFlag = false;
	std::string UsersMissingSong;
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
	std::vector<std::string> vInput;
	bool Connected = true;
};

inline std::vector<Rooms> PlayerRooms;
inline std::vector<Clients> ConnectedClients;

inline std::string m_IP;
inline bool m_GotIP = false;
inline std::string ServerName;
inline std::string ElevatedUserLogin;
inline unsigned ServerVersion;
inline unsigned ProtocolVersion;
inline unsigned ServerPort;
inline unsigned MaxPlayers;
inline std::string ServerDB;
inline std::string PWSalt;

#endif
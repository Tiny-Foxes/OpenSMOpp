#include <iostream>

#ifdef WIN32
#define WINDOWS
#endif

#define SCALE(x, l1, h1, l2, h2)	(((x) - (l1)) * ((h2) - (l2)) / ((h1) - (l1)) + (l2))

#include "Main.hpp"
#include "mini/ini.h"
#include <thread>
#include <chrono>
#include "SQLiteCpp/SQLiteCpp.h"

/*	EzSocket Info.
	0 - Ezsocket empty.
	0 - Ezsocket empty.
	0 - Ezsocket empty.
	1 - Data Size.
	//
	128 - SMOProtocol.
	rest = Data
*/

/*	SMOProtocol Info.
	0 - Ping.
	1 - Ping Respond.
	2 - Hello.
	3 - GameStart.
	4 - GameOver.
	5 - GameStatusUpdate.
	6 - StyleUpdate.
	7 - Chat.
	8 - RequestStart.
	9 - Reserved1. -- Playernames update.
	10 - MusicSelect.
	11 - PlayerOptions.
	12 - StepManiaOnline.
	--	 Are these even used?
	13 - RESERVED1.
	14 - RESERVED2.
	15 - RESERVED3.
	16 - FriendListUpdate.
*/

unsigned CurGradeCalc(std::array<unsigned, 9> TNSs, unsigned ScoreTracker)
{
	float AllNotes = static_cast<float>(TNSs[0] + TNSs[1] + TNSs[2] + TNSs[3] + TNSs[4] + TNSs[5]) * 8.f;

	float Percent = (static_cast<float>(ScoreTracker) / AllNotes) * 100.f;

	if (TNSs[5] == 0 &&
		TNSs[4] == 0 &&
		TNSs[3] == 0 &&
		TNSs[2] == 0)
	{
		if (TNSs[1] == 0)
			return 0; // AAAA
		return 1; // AAA
	}
	if (Percent >= 90.f)
		return 2; // AA
	if (Percent >= 80.f)
		return 3; // A
	if (Percent >= 70.f)
		return 4; // B
	if (Percent >= 60.f)
		return 5; // C
	if (Percent >= 50.f)
		return 6; // D
	return 20; // E - F ailed
}

std::string TapNoteScoreCalc(float tns, int Type, std::array<unsigned, 9>& TNSs, unsigned& ScoreTracker)
{
	if (TNSs[7] < TNSs[8])
		TNSs[7] = TNSs[8];

	float input = std::abs(tns);

	if (Type == 0 || Type == 16)
		return "TapNoteScore_Unknown";

	if (Type == 9 || Type == 25)
		return "HoldNoteScore_LetGo";

	if (Type == 10 || Type == 26)
	{
		++TNSs[6];
		return "HoldNoteScore_Held";
	}

	if (Type == 2 || Type == 18)
		return "TapNoteScore_AvoidMine";

	else if (Type == 1 || Type == 17)
	{
		TNSs[8] = 0;
		return "TapNoteScore_MineHit";
	}
	if (input <= 0.001f && (Type == 3 || Type == 19))
	{
		++TNSs[5];
		TNSs[8] = 0;
		return "TapNoteScore_Miss";
	}
	if (input <= 5.f)
	{
		++TNSs[0];
		++TNSs[8];
		ScoreTracker += 8;
		return "TapNoteScore_W1";
	}
	if (input <= 10.f)
	{
		++TNSs[1];
		++TNSs[8];
		ScoreTracker += 7;
		return "TapNoteScore_W2";
	}
	if (input <= 15.f)
	{
		++TNSs[2];
		++TNSs[8];
		ScoreTracker += 6;
		return "TapNoteScore_W3";
	}
	if (input <= 20.f)
	{
		++TNSs[3];
		TNSs[8] = 0;
		ScoreTracker += 4;
		return "TapNoteScore_W4";
	}
	if (input <= 25.f)
	{
		++TNSs[4];
		TNSs[8] = 0;
		ScoreTracker += 2;
		return "TapNoteScore_W5";
	}
	++TNSs[5];
	TNSs[8] = 0;
	return "TapNoteScore_Miss";
}

void UpdateRooms(ASocket::Socket Client)
{
	std::string RoomNames;
	std::string RoomStates;
	std::string RoomFlags;

	for (auto& Room : PlayerRooms)
	{
		RoomNames += Room.RoomName + std::string(1, '\0') + Room.RoomDescription + std::string(1, '\0');
		RoomStates += std::string(1, static_cast<char>(Room.State));
		RoomFlags += std::string(1, Room.PassFlag ? '\1' : '\0');
	}

	std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 12)) + std::string(2, '\1') + std::string(1, static_cast<char>(PlayerRooms.size())) + RoomNames + RoomStates + RoomFlags;;
	std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
	m_TCPServer->Send(Client, Header + Out);
}

void JoinPlayer(Clients& client, std::vector<Clients>& clients)
{
	for (auto& c : clients)
	{
		if (c.RoomID != client.RoomID || c.UserName == client.UserName) 
			continue;
		std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "User Joined: " + client.UserName;
		std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
		m_TCPServer->Send(c.Client, Header + Out);
	}
}

void LeavePlayer(Clients& client, std::vector<Clients>& clients)
{
	for (auto& c : clients)
	{
		if (c.RoomID != client.RoomID || c.UserName == client.UserName)
			continue;
		std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "User Left: " + client.UserName;
		std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
		m_TCPServer->Send(c.Client, Header + Out);
	}
}

void ListPlayers(Clients& client, std::vector<Clients>& clients)
{
	std::string Users;

	for (auto& c : clients)
		if (c.RoomID == client.RoomID && c.LoggedIn && c.UserName != client.UserName) // do we want to display ourself.
			Users += std::string(1, ' ') + c.UserName;

	std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "Other Players in room : " + Users;
	std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
	m_TCPServer->Send(client.Client, Header + Out);
}

void JoinRoom(Clients& client, std::vector<std::string> Vals)
{
	auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&Vals](Rooms& Room) { return Vals[0] == Room.RoomName; });

	if (result != PlayerRooms.end())
	{
		if (result->RoomPassword.empty() || result->RoomPassword == Vals[2])
		{
			++result->NumPlayers;
			client.RoomID = result->RoomID;

			std::string RoomNames;
			std::string RoomStates;
			std::string RoomFlags;

			for (auto& Room : PlayerRooms)
			{
				RoomNames += Room.RoomName + std::string(1, '\0') + Room.RoomDescription + std::string(1, '\0');
				RoomStates += std::string(1, static_cast<char>(Room.State));
				RoomFlags += std::string(1, Room.PassFlag ? '\1' : '\0');
			}

			std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 12)) + std::string(1, '\1') + std::string(1, '\0') + Vals.at(0) + std::string(1, '\0') + Vals.at(1) + std::string(1, '\0') + std::string(1, '\1') + std::string(1, static_cast<char>(PlayerRooms.size())) + RoomNames + RoomStates + RoomFlags;;
			std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
			m_TCPServer->Send(client.Client, Header + Out);
		}
	}
}

void LeaveRoom(Clients& client, std::vector<Clients>& clients)
{
	auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](Rooms& Room) { return client.RoomID == Room.RoomID; });

	if (result != PlayerRooms.end())
	{
		--result->NumPlayers;
		long long OldRoomID = client.RoomID;
		client.RoomID = -1;

		if (client.UserType != 2)
			client.UserType = 0;

		if (result->NumPlayers == 0)
		{
			PlayerRooms.erase(result);
		}
		else
		{
			auto player = std::find(result->CurPlayers.begin(), result->CurPlayers.end(), client.UserName);

			if (player != result->CurPlayers.end())
			{
				(*result).CurPlayers.erase(std::find(result->CurPlayers.begin(), result->CurPlayers.end(), client.UserName), result->CurPlayers.end());
				if (result->NumPlayersPlaying > 0)
					--(*result).NumPlayersPlaying;
			}

			if (client.UserName == result->Owner)
			{
				for (auto& c : clients)
				{
					if (c.RoomID != OldRoomID)
						continue;
					(*result).Owner = c.UserName;
					c.UserType = 1;
					break;
				}

				for (auto& c : clients)
				{
					if (c.RoomID != OldRoomID)
						continue;
					std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "Old room owner left, New room owner: " + result->Owner;
					std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
					m_TCPServer->Send(c.Client, Header + Out);
				}
			}
		}
	}
}

void SMOReader(Clients Client)
{
	while (true)
	{
		m_Mutex.lock();
		auto result = std::find_if(ConnectedClients.begin(), ConnectedClients.end(), [&Client](Clients c) { return Client.Client == c.Client; });
		Clients c = *result;
		m_Mutex.unlock();

		if (c.Connected)
		{
			char Input[1024] = {};
			int read = m_TCPServer->Receive(c.Client, Input, 1024, false);

			if (read < 0)
				continue;

			m_Mutex.lock();
			if (read == 0)
			{
				std::cout << "User: " << c.UserName << " '" << c.IP << "' Disconnected.\n";
				m_TCPServer->Disconnect(c.Client);
				if (result->LoggedIn)
				{
					LeavePlayer(c, ConnectedClients);
					LeaveRoom(c, ConnectedClients);
				}
				(*result).Connected = false;
			}

			if (read > 0)
			{
				(*result).vInput.push_back(std::string(Input, 1024));
			}
			m_Mutex.unlock();
			continue;
		}

		m_Mutex.lock();
		ConnectedClients.erase(std::remove_if(ConnectedClients.begin(), ConnectedClients.end(), [&](Clients const& client) { return client.Client == c.Client; }), ConnectedClients.end());
		m_Mutex.unlock();

		break;
	}
}

void SMOListener()
{
	std::vector<std::thread> ReaderThreads;

	while (Running)
	{
		ASocket::Socket ConnectedClient;

		if (m_TCPServer->Listen(ConnectedClient))
		{
			char Input[1024] = {};
			m_TCPServer->Receive(ConnectedClient, Input, 1024, false);

			if (Input[4] = 2)
			{
				while (!m_GotIP)
					std::this_thread::sleep_for(std::chrono::milliseconds(50));

				m_Mutex.lock();
				std::cout << std::string(Input, Input[6]+2).erase(0,6) + " '" + m_IP + "'"+ " Connected with StepManiaOnline Protocol: V" + std::to_string(Input[5]) + "\n";
				std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 2)) + std::string(1, static_cast<char>(ServerVersion)) + ServerName;
				//std::string Salt = std::string(1, PWSalt[0]) + std::string(1, PWSalt[1]) + std::string(1, PWSalt[2]) + std::string(1, PWSalt[3]); // need to figure this out.
				std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));

				m_TCPServer->Send(ConnectedClient, Header + Out);

				Clients c({ ConnectedClient, false, m_IP });
				ConnectedClients.push_back(c);
				ReaderThreads.push_back(std::thread(SMOReader,c));
				m_Mutex.unlock();
				m_GotIP = false;
			}
			else
			{
				m_TCPServer->Disconnect(ConnectedClient);
			}
		}
	}

	for (auto& thread : ReaderThreads)
		thread.join();
}

int main()
{
	std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();

	mINI::INIFile file("Config.ini");
	mINI::INIStructure ini;
	file.read(ini);

	ServerName = ini["Server"]["Name"];
	ServerVersion = atol(ini["Server"]["ServerVersion"].c_str());
	ProtocolVersion = atol(ini["Server"]["ProtocolVersion"].c_str());
	ServerPort = atol(ini["Server"]["ServerPort"].c_str());
	MaxPlayers = atol(ini["Server"]["MaxPlayers"].c_str());
	ElevatedUserLogin = ini["Server"]["ServerPassword"];
	ServerDB = ini["ServerDB"]["File"];
	PWSalt = ini["ServerDB"]["PasswordSalt"];

	if (ServerName.empty())
	{
		ini["Server"]["Name"] = "New OpenSMO++ Server";
		ServerName = "New OpenSMO++ Server";
	}

	if (ServerVersion <= 0)
	{
		ini["Server"]["ServerVersion"] = "128";
		ServerVersion = 128;
	}

	if (ProtocolVersion <= 0)
	{
		ini["Server"]["ProtocolVersion"] = "128";
		ProtocolVersion = 128;
	}

	if (ServerPort <= 0)
	{
		ini["Server"]["ServerPort"] = "8765";
		ServerPort = 8765;
	}

	if (MaxPlayers <= 0)
	{
		ini["Server"]["MaxPlayers"] = "255";
		MaxPlayers = 255;
	}

	if (ElevatedUserLogin.empty())
	{
		ini["Server"]["ServerPassword"] = "ChangeMe";
		ElevatedUserLogin = "ChangeMe";
	}

	if (ServerDB.empty())
	{
		ini["ServerDB"]["File"] = "ServerDB.db";
		ServerDB = "ServerDB.db";
	}

	if (PWSalt.empty())
	{
		ini["ServerDB"]["PasswordSalt"] = "Pass";
		PWSalt = "Pass";
	}

	file.write(ini);

	std::cout << "OpenSMO++ 1.0.2: By Jousway\n";
	std::cout << ("Server Name: " + ServerName + "\n").c_str();
	std::cout << ("Server (sm uses 128): " + std::to_string(ServerVersion) + "\n").c_str();
	std::cout << ("Server Port: " + std::to_string(ServerPort) + "\n").c_str();
	std::cout << ("Server MaxPlayers: " + std::to_string(MaxPlayers) + "\n").c_str();
	std::cout << "\nServerDB:\n";
	std::cout << ("ServerDB File: " + ServerDB + "\n").c_str();
	//std::cout << ("ServerDB PW Salt (4 values): " + PWSalt.substr(0, 4) + "\n").c_str();
	std::cout << "\n";

	SQLite::Database db(ServerDB.c_str(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

	SQLite::Transaction transaction(db);

	db.exec("CREATE TABLE IF NOT EXISTS Users(UserName CHAR, Password CHAR, Banned int)");

	transaction.commit();

	std::string IP = "unknown";
	bool GotIP = false;

	auto LogPrinter = [&IP, &GotIP](const std::string& strLogMsg) {
		if (strLogMsg.find("Incoming connection from") != std::string::npos)
		{
			m_Mutex.lock();
			IP = strLogMsg;
			IP.erase(0, IP.find_first_of('\'') + 1);
			IP.erase(IP.find_first_of('\''), IP.length());
			GotIP = true;
			m_Mutex.unlock();
		}
		else
		{
			//std::cout << strLogMsg << std::endl;
		}
	};

	m_TCPServer = new CTCPServer(LogPrinter, std::to_string(ServerPort).c_str());

	std::thread SMOListen(SMOListener);

	std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	std::time_t end_time = std::chrono::system_clock::to_time_t(end);
	std::cout << "Server Started: " << std::ctime(&end_time);
	std::cout << "Starting Took: " << elapsed_seconds.count() << "s\n\n";

	std::cout << "ServerLog:\n\n";

	while (true)
	{
		m_Mutex.lock();
		m_IP = IP;
		if (!m_GotIP)
			m_GotIP = GotIP;
		GotIP = false;
		std::vector<Clients> CurClients = ConnectedClients;
		m_Mutex.unlock();


		for (auto& Values : CurClients)
		{
			auto& Client = Values.Client;
			auto& LoggedIn = Values.LoggedIn;
			auto& Ip = Values.IP;
			auto& UserType = Values.UserType;
			auto& UserName = Values.UserName;
			auto& RoomID = Values.RoomID;
			auto& TNSs = Values.TNSs;
			auto& SMClientID = Values.SMClientID;
			auto& ScoreTracker = Values.ScoreTracker;
			
			m_Mutex.lock();
			auto result = std::find_if(ConnectedClients.begin(), ConnectedClients.end(), [&Values](Clients c) { return Values.Client == c.Client; });
			std::vector<std::string> vInput = result->vInput;
			(*result).vInput.clear();
			m_Mutex.unlock();

			for (auto& Input : vInput)
			{
				if (Input[4] == 6)
					continue;

				// Debug.
				//std::cout << std::string(Input, 1024) << std::endl;

				if (Input[4] == 7)
				{
					if (Input[5] == '/') // Server Command
					{
						std::string Command = Input;
						Command.erase(0, 6);
						std::string Argument;
						if (Command.find(' ') != std::string::npos)
						{
							Command.erase(Command.find_first_of(' '));
							Argument = Input;
							Argument.erase(0, 6 + Command.size() + 1);
							Argument.erase(std::remove(Argument.begin(), Argument.end(), '\0'), Argument.end());
						}

						Command.erase(std::remove(Command.begin(), Command.end(), '\0'), Command.end());

						// Use lower text for commands
						std::transform(Command.begin(), Command.end(), Command.begin(), [](unsigned char c) { return std::tolower(c); });

						if (Command == "help")
						{
							std::string LCommands;

							LCommands += "/users - Show Online Users in current Room.\n";
							LCommands += "/login - login as admin.\n";

							if (UserType == 1 || (UserType == 2 && RoomID >= 0)) // Room Owner Commands
							{
								LCommands += "/free - Let anyone pick a song.\n";
							}

							if (UserType == 2) // Admin Commands
							{
								LCommands += "/adminkick - kick an user.\n";
								LCommands += "/userip - get an users IP.\n";
							}

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + LCommands;
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							m_TCPServer->Send(Client, Header + Out);
							continue;
						}

						if (Command == "users")
						{
							ListPlayers(Values, CurClients);
							continue;
						}

						if (Command == "login")
						{
							if (!Argument.empty() && Argument != "ChangeMe" && Argument == ElevatedUserLogin)
							{
								UserType = 2;

								std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "Logged in as admin";
								std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
								m_TCPServer->Send(Client, Header + Out);
								continue;
							}
						}

						if (UserType == 1 || (UserType == 2 && RoomID >= 0))
						{
							auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](Rooms& Room) { return RoomID == Room.RoomID; });

							if (Command == "free")
							{
								(*result).FreeMode = result->FreeMode ? false : true;

								for (auto& c : CurClients)
								{
									if (c.RoomID != RoomID)
										continue;

									std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "Set Room to Free mode: " + (result->FreeMode ? "Enabled" : "Disabled");
									std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
									m_TCPServer->Send(c.Client, Header + Out);
								}
								continue;
							}
						}

						if (UserType == 2) // Admin Commands
						{
							if (Command == "adminkick")
							{
								auto result = std::find_if(CurClients.begin(), CurClients.end(), [&Argument](Clients& client) { return Argument == client.UserName; });

								if (result != CurClients.end())
								{
									std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "User Kicked: " + Argument;
									std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
									m_TCPServer->Send(Client, Header + Out);

									std::cout << "User: " << result->UserName << " '" << result->IP << "' Kicked.\n";
									m_TCPServer->Disconnect(result->Client);
									(*result).Connected = false;
								}
								else
								{
									std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "User Not Connected: " + Argument;
									std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
									m_TCPServer->Send(Client, Header + Out);
								}
								continue;
							}

							if (Command == "userip")
							{
								auto result = std::find_if(CurClients.begin(), CurClients.end(), [&Argument](Clients& client) { return Argument == client.UserName; });

								if (result != CurClients.end())
								{
									std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "User IP: '" + result->IP + "' For " + Argument;
									std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
									m_TCPServer->Send(Client, Header + Out);
								}
								else
								{
									std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "User Not Connected: " + Argument;
									std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
									m_TCPServer->Send(Client, Header + Out);
								}
								continue;
							}

						}

						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "Invalid Command: " + Command;
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						m_TCPServer->Send(Client, Header + Out);
						continue;
					}

					for (auto& c : CurClients)
					{
						if (c.RoomID != RoomID)
							continue;

						std::string usertype = "[|c000ff00User|c0ffffff] ";

						if (UserType == 2)
							usertype = "[|c0ff0000Admin|c0ffffff] ";
						else if (UserType == 1)
							usertype = "[|c00000ffRoomHost|c0ffffff] ";

						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + usertype + UserName + ": " + Input.erase(0, 5).erase(Input.find_first_of('\0'));
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						m_TCPServer->Send(c.Client, Header + Out);
					}
					continue;
				}

				if (Input[4] == 5)
				{
					auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](Rooms& Room) { return RoomID == Room.RoomID; });

					std::string offset = Input;
					offset.erase(0, 15);

					float InOffset = static_cast<float>((ntohs(static_cast<unsigned char>(offset[0]) + static_cast<unsigned char>(offset[1]))) / 2000.f) - 16.384f;

					std::string TNS = TapNoteScoreCalc(InOffset, Input[5], TNSs, ScoreTracker);

					//std::cout << TNS << std::endl;

					std::string Grades;
					std::string Combos;

					for (auto& c : CurClients)
					{
						if (c.RoomID != RoomID)
							continue;

						Grades += std::string(1, static_cast<char>(CurGradeCalc(c.TNSs, c.ScoreTracker)));

						unsigned short value = htons(static_cast<unsigned short>(c.TNSs[8]));

						char first = static_cast<char>(value);
						value = value >> 8;
						char second = static_cast<char>(value);

						Combos += std::string(1, first) + std::string(1, second);
					}

					for (auto& c : CurClients)
					{
						if (c.RoomID != RoomID)
							continue;

						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 5)) + std::string(1, '\1') + std::string(1, static_cast<char>(result->CurPlayers.size())) + Combos;
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						m_TCPServer->Send(c.Client, Header + Out);

						Out = std::string(1, static_cast<char>(ProtocolVersion + 5)) + std::string(1, '\2') + std::string(1, static_cast<char>(result->CurPlayers.size())) + Grades;
						Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						m_TCPServer->Send(c.Client, Header + Out);
					}
					continue;
				}

				if (Input[4] == 3 && RoomID >= 0)
				{
					auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](Rooms& Room) { return RoomID == Room.RoomID; });

					--(*result).NumPlayersWaiting;

					if (result->NumPlayersWaiting == 0)
					{
						unsigned PlayerID = 0;
						for (auto& c : CurClients)
						{
							if (c.RoomID != RoomID)
								continue;

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 3));
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							m_TCPServer->Send(c.Client, Header + Out);
							if (Input[7] == 16)
							{
								(*result).CurPlayers.push_back(c.UserName);
								c.SMClientID = PlayerID++;
							}

						}
						(*result).NumPlayersWaiting = result->NumPlayers;
						(*result).NumPlayersPlaying = result->NumPlayers;
						(*result).SongSelected = false;

						if (Input[7] != 16)
							continue;

						std::string Players;

						for (auto& Player : result->CurPlayers)
							Players += std::string(1, '\1') + Player + std::string(1, '\0');

						std::string PlayerNums;

						int pnum = 0;

						for (auto& Player : result->CurPlayers)
							PlayerNums += std::string(1, static_cast<char>(pnum++));

						for (auto& c : CurClients)
						{
							if (c.RoomID != RoomID)
								continue;

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 9)) + std::string(1, '\0') + std::string(1, static_cast<char>(result->CurPlayers.size())) + Players;
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							m_TCPServer->Send(c.Client, Header + Out);

							Out = std::string(1, static_cast<char>(ProtocolVersion + 5)) + std::string(1, '\0') + std::string(1, static_cast<char>(result->CurPlayers.size())) + PlayerNums;
							Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							m_TCPServer->Send(c.Client, Header + Out);

							Out = std::string(1, static_cast<char>(ProtocolVersion + 5)) + std::string(1, '\1') + std::string(1, static_cast<char>(result->CurPlayers.size())) + std::string(result->CurPlayers.size()*2, '\0');
							Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							m_TCPServer->Send(c.Client, Header + Out);

							Out = std::string(1, static_cast<char>(ProtocolVersion + 5)) + std::string(1, '\2') + std::string(1, static_cast<char>(result->CurPlayers.size())) + std::string(result->CurPlayers.size(), '\0');
							Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							m_TCPServer->Send(c.Client, Header + Out);
						}
					}
					continue;
				}

				if (Input[4] == 10 && RoomID >= 0 && Input[5] == 5)
				{
					auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](Rooms& Room) { return RoomID == Room.RoomID; });

					std::string Players;
					std::string Scores;
					std::string Grades;
					std::string Difficultys;
					std::string Taps;
					int NumPlayers = 0;

					for (auto& c : CurClients)
					{
						if (c.RoomID != RoomID)
							continue;

						++NumPlayers;

						Players += std::string(1, static_cast<char>(c.SMClientID));

						unsigned long value = htonl(static_cast<unsigned long>(c.ScoreTracker));
						char first = static_cast<char>(value >> 24);
						char second = static_cast<char>(value >> 16);
						char third = static_cast<char>(value >> 8);
						char fourth = static_cast<char>(value);

						Scores += std::string(1, fourth) + std::string(1, third) + std::string(1, second) + std::string(1, first);
						Grades += std::string(1, static_cast<char>(CurGradeCalc(c.TNSs, c.ScoreTracker)));
						Difficultys += std::string(1, '\0');
						unsigned count = 0;
						for (auto& tns : c.TNSs)
						{
							if (count == 8)
								break;

							unsigned short valueTNS = htons(static_cast<unsigned short>(tns));
							char firstTNS = static_cast<char>(valueTNS >> 8);
							char secondTNS = static_cast<char>(valueTNS);
							Taps += std::string(1, secondTNS) + std::string(1, firstTNS);
							++count;
						}
					}

					for (auto& c : CurClients)
					{
						if (c.RoomID != RoomID)
							continue;

						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 4)) + std::string(1,static_cast<char>(NumPlayers)) + Players + Scores + Grades + Difficultys + Taps;
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						m_TCPServer->Send(c.Client, Header + Out);
					}
					continue;
				}

				if ((Input[4] == 10 && RoomID >= 0 && Input[5] == 4))
				{
					auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](Rooms& Room) { return RoomID == Room.RoomID; });

					(*result).CurPlayers.erase(std::find(result->CurPlayers.begin(), result->CurPlayers.end(), UserName), result->CurPlayers.end());

					--(*result).NumPlayersPlaying;
					ScoreTracker = 0;
					TNSs = {};

					std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 9)) + std::string(1, '\0') + std::string(1, '\0');
					std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
					m_TCPServer->Send(Client, Header + Out);

					continue;
				}

				if (Input[4] == 10 && RoomID >= 0 && Input[5] == 3)
				{
					auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](Rooms& Room) { return RoomID == Room.RoomID; });

					++(*result).NumPlayersPlaying;
					(*result).CurPlayers.push_back(UserName);
				}

				if (Input[4] == 10 && RoomID >= 0 && Input[5] == 1)
				{
					auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](Rooms& Room) { return RoomID == Room.RoomID; });

					--(*result).NumPlayersPlaying;
					(*result).CurPlayers.erase(std::find(result->CurPlayers.begin(), result->CurPlayers.end(), UserName), result->CurPlayers.end());
				}

				if (Input[4] == 10 && RoomID >= 0 && Input[5] == 0)
				{
					auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](Rooms& Room) { return RoomID == Room.RoomID; });

					if (result->SongSelected)
						continue;
					LeavePlayer(Values, CurClients);
					LeaveRoom(Values, CurClients);
					for (auto& c : CurClients)
						UpdateRooms(c.Client);

					continue;
				}

				if (RoomID >= 0 && Input[4] == 8 && Input[5] == 1)
				{
					auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](Rooms& Room) { return RoomID == Room.RoomID; });

					(*result).UsersMissingSong += UserName + std::string(1, '\0');
					(*result).SongSelected = false;

					std::stringstream in(Input.erase(0, 6));
					std::string Val;
					std::vector<std::string> Vals;

					while (std::getline(in, Val, '\0'))
					{
						Vals.push_back(Val);
					}

					Vals.erase(std::remove(Vals.begin() + 3, Vals.end(), "\0"), Vals.end());

					for (auto& c : CurClients)
					{
						if (c.RoomID != RoomID)
							continue;

						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + UserName + " doesn't have Song: " + Vals[0];
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						m_TCPServer->Send(c.Client, Header + Out);
					}
				}

				if (RoomID >= 0 && Input[4] == 8 && Input[5] == 2)
				{
					std::stringstream in(Input.erase(0, 6));
					std::string Val;
					std::vector<std::string> Vals;

					while (std::getline(in, Val, '\0'))
					{
						Vals.push_back(Val);
					}

					Vals.erase(std::remove(Vals.begin() + 3, Vals.end(), "\0"), Vals.end());

					auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](Rooms& Room) { return RoomID == Room.RoomID; });

					if (UserType == 0 && !result->FreeMode)
					{
						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "FreeMode disabled, Ask Roomhost for /free";
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						m_TCPServer->Send(Client, Header + Out);
						continue;
					}

					if (result->NumPlayersPlaying > 0)
					{

						std::string Players;

						for (std::string player : result->CurPlayers)
							Players += player + std::string(1, ' ');

						for (auto& c : CurClients)
						{
							if (c.RoomID != RoomID)
								continue;

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + Players + "havent finished yet, please wait.";
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							m_TCPServer->Send(c.Client, Header + Out);
						}
						continue;
					}

					if (!result->SongSelected ||
						result->CurSong[0] != Vals[0] ||
						result->CurSong[1] != Vals[1] ||
						result->CurSong[2] != Vals[2])
					{
						for (auto& c : CurClients)
						{
							if (c.RoomID != RoomID)
								continue;

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + UserName + " selected song: " + Vals[0];
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							m_TCPServer->Send(c.Client, Header + Out);

							if (c.Client == Client)
								continue;

							Out = std::string(1, static_cast<char>(ProtocolVersion + 8)) + std::string(1, '\1') + Vals[0] + std::string(1, '\0') + Vals[1] + std::string(1, '\0') + Vals[2];
							Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							m_TCPServer->Send(c.Client, Header + Out);

						}
						(*result).SongSelected = true;
						(*result).UsersMissingSong.clear();
						(*result).CurSong = { Vals[0], Vals[1], Vals[2] };
					}
					else if (result->UsersMissingSong.empty())
					{
						for (auto& c : CurClients)
						{
							if (c.RoomID != RoomID)
								continue;

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 8)) + std::string(1, '\2') + Vals[0] + std::string(1, '\0') + Vals[1] + std::string(1, '\0') + Vals[2];
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							m_TCPServer->Send(c.Client, Header + Out);
						}
						(*result).NumPlayersWaiting = result->NumPlayers;
					}
					continue;
				}

				if (Input[4] == 10 && RoomID == -1 && Input[5] == 6)
				{
					UpdateRooms(Client);
					continue;
				}

				if (Input[4] == 10 && Input[5] == 7)
				{
					LeaveRoom(Values, CurClients);
					JoinPlayer(Values, CurClients);

					std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + std::string(20, '\n') + "Welcome to the Server, Use CTRL+ENTER to select, type /help for info.";
					std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
					m_TCPServer->Send(Client, Header + Out);
					UpdateRooms(Client);
					ListPlayers(Values, CurClients);
					continue;
				}

				if (!LoggedIn && Input[4] == 12)
				{
					std::stringstream in(Input.erase(0, 8));
					std::string Val;
					std::vector<std::string> Vals;

					while (std::getline(in, Val, '\0'))
					{
						Vals.push_back(Val);
					}

					Vals.erase(std::remove(Vals.begin() + 2, Vals.end(), "\0"), Vals.end());

					bool FoundUser = false;

					try {
						SQLite::Statement query(db, (std::string("SELECT * FROM Users WHERE UserName =") + "\"" + Vals[0] + "\"").c_str());

						bool invalidpass = false;

						while (query.executeStep())
						{
							FoundUser = true;
							const char* name = query.getColumn(0);
							UserName = name;
							const char* password = query.getColumn(1);
							int Banned = query.getColumn(2);

							if (Banned == 1)
							{
								std::cout << "User: " << UserName << " '" << Ip << "' Is Banned, Disconnecting\n";
								m_TCPServer->Disconnect(Client);
								Values.Connected = false;
								invalidpass = true;
								break;
							}

							if (password != Vals[1])
							{
								std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 12)) + std::string(1, '\0') + std::string(1, '\1') + "Wrong Password.\n";
								std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
								m_TCPServer->Send(Client, Header + Out);
								invalidpass = true;
								break;
							}
						}

						if (invalidpass)
							continue;
					}
					catch (...) {}

					if (!FoundUser)
					{
						std::cout << "Creating New User: " << Vals[0] << "\n";

						SQLite::Transaction transaction(db);

						db.exec(("INSERT INTO Users VALUES (\"" + Vals[0] + "\", \"" + Vals[1] + "\", 0)").c_str());

						transaction.commit();

						UserName = Vals[0];
					}

					std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 12)) + std::string(2, '\0') + "Correct Password.";
					std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
					m_TCPServer->Send(Client, Header + Out);
					LoggedIn = true;
					continue;
				}

				if (RoomID < 0 && Input[4] == 12 && Input[5] == 2)
				{
					std::stringstream in(Input.erase(0, 7));
					std::string Val;
					std::vector<std::string> Vals;

					while (std::getline(in, Val, '\0'))
					{
						Vals.push_back(Val);
					}

					Vals.erase(std::remove(Vals.begin() + 3, Vals.end(), "\0"), Vals.end());

					if (Vals[0].empty())
					{
						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "You can't have an empty room name you silly xd.";
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						m_TCPServer->Send(Client, Header + Out);
						continue;
					}

					auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&Vals](Rooms& Room) { return Vals[0] == Room.RoomName; });

					if (result == PlayerRooms.end())
					{
						PlayerRooms.push_back({ g_RoomID++, UserName, Vals[0], Vals[1], Vals[2], 0, 0, !Vals[2].empty() });

						if (UserType != 2)
							UserType = 1;

						for (auto& c : ConnectedClients)
						{
							std::string RoomNames;
							std::string RoomStates;
							std::string RoomFlags;

							for (auto& Room : PlayerRooms)
							{
								RoomNames += Room.RoomName + std::string(1, '\0') + Room.RoomDescription + std::string(1, '\0');
								RoomStates += std::string(1, static_cast<char>(Room.State));
								RoomFlags += std::string(1, Room.PassFlag ? '\1' : '\0');
							}

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 12)) + std::string(1, '\1') + std::string(1, '\1') + std::string(1, static_cast<char>(PlayerRooms.size())) + RoomNames + RoomStates + RoomFlags;
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							m_TCPServer->Send(c.Client, Header + Out);
						}
						LeavePlayer(Values, CurClients);
						JoinRoom(Values, Vals);
						JoinPlayer(Values, CurClients);
						ListPlayers(Values, CurClients);
					}
					continue;
				}

				if (RoomID < 0 && Input[4] == 12 && Input[5] == 1)
				{
					std::stringstream in(Input.erase(0, 7));
					std::string Val;
					std::vector<std::string> Vals;

					while (std::getline(in, Val, '\0'))
					{
						Vals.push_back(Val);
					}

					Vals.erase(std::remove(Vals.begin() + 3, Vals.end(), "\0"), Vals.end());

					Vals[2] = Vals[1];
					Vals[1] = "";

					LeavePlayer(Values, CurClients);
					JoinRoom(Values, Vals);
					JoinPlayer(Values, CurClients);
					ListPlayers(Values, CurClients);
					continue;
				}
			}
		}

		for (auto& Client : CurClients)
		{
			m_Mutex.lock();
			auto result = std::find_if(ConnectedClients.begin(), ConnectedClients.end(), [&Client](Clients c) { return Client.Client == c.Client; });
			if (result != ConnectedClients.end())
			{
				(*result).Connected = Client.Connected;
				(*result).LoggedIn = Client.LoggedIn;
				(*result).RoomID = Client.RoomID;
				(*result).TNSs = Client.TNSs;
				(*result).ScoreTracker = Client.ScoreTracker;
				(*result).SMClientID = Client.SMClientID;
				(*result).UserName = Client.UserName;
				(*result).UserType = Client.UserType;
			}
			m_Mutex.unlock();
		}
	}

	Running = false;
	SMOListen.join();

	return 0;
}
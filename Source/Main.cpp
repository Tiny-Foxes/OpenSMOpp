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

unsigned CurGradeCalc(const std::array<unsigned, 9> &TNSs, unsigned ScoreTracker)
{
	const float AllNotes = static_cast<float>(TNSs[0] + TNSs[1] + TNSs[2] + TNSs[3] + TNSs[4] + TNSs[5]) * 8.f;

	const float Percent = (static_cast<float>(ScoreTracker) / AllNotes) * 100.f;

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

std::string TapNoteScoreCalc(const double tns, const int Type, std::array<unsigned, 9>& TNSs, unsigned& ScoreTracker, const int NumNotes)
{
	if (TNSs[7] < TNSs[8])
		TNSs[7] = TNSs[8];

	const double input = std::abs(tns);

	if (Type == 9 || Type == 25)
	{
		TNSs[8] = 0;
		return "HoldNoteScore_LetGo";
	}
	if (Type == 10 || Type == 26)
	{
		TNSs[6] += NumNotes;
		return "HoldNoteScore_Held";
	}
	if (Type == 2 || Type == 18)
		return "TapNoteScore_AvoidMine";

	if (Type == 1 || Type == 17)
	{
		TNSs[8] = 0;
		return "TapNoteScore_MineHit";
	}
	if (Type == 0 || Type == 16)
		return "TapNoteScore_Unknown";

	if (Type == 3 || Type == 19)
	{
		TNSs[5] += NumNotes;
		TNSs[8] = 0;
		return "TapNoteScore_Miss";
	}
	if (input <= 0.0225)
	{
		TNSs[0] += NumNotes;
		TNSs[8] += NumNotes;
		ScoreTracker += 8 * NumNotes;
		return "TapNoteScore_W1";
	}
	if (input <= 0.0450)
	{
		TNSs[1] += NumNotes;
		TNSs[8] += NumNotes;
		ScoreTracker += 7 * NumNotes;
		return "TapNoteScore_W2";
	}
	if (input <= 0.0900)
	{
		TNSs[2] += NumNotes;
		TNSs[8] += NumNotes;
		ScoreTracker += 6 * NumNotes;
		return "TapNoteScore_W3";
	}
	if (input <= 0.1350)
	{
		TNSs[3] += NumNotes;
		TNSs[8] = 0;
		ScoreTracker += 4 * NumNotes;
		return "TapNoteScore_W4";
	}
	if (input <= 0.1800)
	{
		TNSs[4] += NumNotes;
		TNSs[8] = 0;
		ScoreTracker += 2 * NumNotes;
		return "TapNoteScore_W5";
	}
	TNSs[5] += NumNotes;
	TNSs[8] = 0;
	return "TapNoteScore_Miss";
}

void UpdateRooms(const ASocket::Socket Client)
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

	const std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 12)) + std::string(2, '\1') + std::string(1, static_cast<char>(PlayerRooms.size())) + RoomNames + RoomStates + RoomFlags;;
	const std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
	(void)m_TCPServer->Send(Client, Header + Out);
}

void JoinPlayer(const Clients& client, const std::vector<Clients>& clients)
{
	for (auto& c : clients)
	{
		if (c.RoomID != client.RoomID || c.UserName == client.UserName)
			continue;
		std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "User Joined: " + client.UserName;
		std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
		(void)m_TCPServer->Send(c.Client, Header + Out);
	}
}

void LeavePlayer(const Clients& client, const std::vector<Clients>& clients)
{
	for (auto& c : clients)
	{
		if (c.RoomID != client.RoomID || c.UserName == client.UserName)
			continue;
		std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "User Left: " + client.UserName;
		std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
		(void)m_TCPServer->Send(c.Client, Header + Out);
	}
}

void ListPlayers(const Clients& client, const std::vector<Clients>& clients)
{
	std::string Users;

	for (auto& c : clients)
		if (c.RoomID == client.RoomID && c.LoggedIn && c.UserName != client.UserName) // do we want to display ourselves?
			Users += std::string(1, ' ') + c.UserName;

	std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "Other Players in room : " + Users;
	std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
	(void)m_TCPServer->Send(client.Client, Header + Out);
}

void JoinRoom(Clients& client, std::vector<std::string> Vals)
{
	if (auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&Vals](const Rooms& Room) { return Vals[0] == Room.RoomName; }); result != PlayerRooms.end())
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

			const std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 12)) + std::string(1, '\1') + std::string(1, '\0') + Vals.at(0) + std::string(1, '\0') + Vals.at(1) + std::string(1, '\0') + std::string(1, '\1') + std::string(1, static_cast<char>(PlayerRooms.size())) + RoomNames + RoomStates + RoomFlags;;
			const std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
			(void)m_TCPServer->Send(client.Client, Header + Out);
		}
	}
}

void LeaveRoom(Clients& client, std::vector<Clients>& clients)
{
	if (const auto result = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](const Rooms& Room) { return client.RoomID == Room.RoomID; }); result != PlayerRooms.end())
	{
		--result->NumPlayers;
		const long long OldRoomID = client.RoomID;
		client.RoomID = -1;

		if (client.UserType != 2)
			client.UserType = 0;

		if (result->NumPlayers == 0)
		{
			PlayerRooms.erase(result);
		}
		else
		{
			if (const auto player = std::find(result->CurPlayers.begin(), result->CurPlayers.end(), client.UserName); player != result->CurPlayers.end())
			{
				result->CurPlayers.erase(std::find(result->CurPlayers.begin(), result->CurPlayers.end(), client.UserName), result->CurPlayers.end());
				if (result->NumPlayersPlaying > 0)
					--result->NumPlayersPlaying;
			}

			if (client.UserName == result->Owner)
			{
				for (auto& c : clients)
				{
					if (c.RoomID != OldRoomID)
						continue;
					result->Owner = c.UserName;
					c.UserType = 1;
					break;
				}

				for (const auto& c : clients)
				{
					if (c.RoomID != OldRoomID)
						continue;
					std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "Old room owner left, New room owner: " + result->Owner;
					std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
					(void)m_TCPServer->Send(c.Client, Header + Out);
				}
			}
		}
	}
}

void SMOReader(Clients Client)
{
	while (true)
	{
		std::vector<Clients>::iterator result;
		Clients c;
		{
			const std::lock_guard lock(m_Mutex);
			result = std::find_if(ConnectedClients.begin(), ConnectedClients.end(), [&Client](const Clients& c) { return Client.Client == c.Client; });
			c = *result;
		}

		if (c.Connected)
		{
			char Input[1024] = {};
			const int read = m_TCPServer->Receive(c.Client, Input, 1024, false);

			if (read < 0)
				continue;

			{
				const std::lock_guard lock(m_Mutex);
				result = std::find_if(ConnectedClients.begin(), ConnectedClients.end(), [&Client](const Clients& c) { return Client.Client == c.Client; });
				if (read == 0)
				{
					std::cout << "User: " << c.UserName << " '" << c.IP << "' Disconnected.\n";
					(void)m_TCPServer->Disconnect(c.Client);
					if (result->LoggedIn)
					{
						LeavePlayer(c, ConnectedClients);
						LeaveRoom(c, ConnectedClients);
					}
					result->Connected = false;
				}

				if (read > 0)
				{
					result->vInput.emplace_back(Input, 1024);
				}
			}
			continue;
		}

		{
			const std::lock_guard lock(m_Mutex);
			ConnectedClients.erase(std::remove_if(ConnectedClients.begin(), ConnectedClients.end(), [&](Clients const& client) { return client.Client == c.Client; }), ConnectedClients.end());
		}

		break;
	}
}

void SMOListener()
{
	std::vector<std::thread> ReaderThreads;

	while (Running)
	{
		if (ASocket::Socket ConnectedClient; m_TCPServer->Listen(ConnectedClient))
		{
			char Input[1024] = {};
			m_TCPServer->Receive(ConnectedClient, Input, 1024, false);

			if (Input[4] == 2)
			{
				while (!m_GotIP)
					std::this_thread::sleep_for(std::chrono::milliseconds(50));

				const std::lock_guard lock(m_Mutex);
				{
					std::cout << std::string(Input, Input[6]+2).erase(0,6) + " '" + m_IP + "'"+ " Connected with StepManiaOnline Protocol: V" + std::to_string(Input[5]) + "\n";
					std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 2)) + std::string(1, static_cast<char>(ServerVersion)) + ServerName;
					//std::string Salt = std::string(1, PWSalt[0]) + std::string(1, PWSalt[1]) + std::string(1, PWSalt[2]) + std::string(1, PWSalt[3]); // need to figure this out.
					std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));

					(void)m_TCPServer->Send(ConnectedClient, Header + Out);

					Clients c({ ConnectedClient, false, m_IP });
					ConnectedClients.push_back(c);
					ReaderThreads.emplace_back(SMOReader,c);
					m_GotIP = false;
				}
			}
			else
			{
				(void)m_TCPServer->Disconnect(ConnectedClient);
			}
		}
	}

	for (auto& thread : ReaderThreads)
		thread.join();
}

void handle_signal(int) {
	Running = false;
}

int main()
{
	std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();

	mINI::INIFile file("Config.ini");
	mINI::INIStructure ini;
	file.read(ini);

	char* endptr = nullptr;
	ServerName = ini["Server"]["Name"];
	ServerVersion = strtol(ini["Server"]["ServerVersion"].c_str(), &endptr, 10);
	ProtocolVersion = strtol(ini["Server"]["ProtocolVersion"].c_str(), &endptr, 10);
	ServerPort = strtol(ini["Server"]["ServerPort"].c_str(), &endptr, 10);
	MaxPlayers = strtol(ini["Server"]["MaxPlayers"].c_str(), &endptr, 10);
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

	std::cout << "OpenSMO++ 1.0.3: By Jousway\n";
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
			const std::lock_guard lock(m_Mutex);
			IP = strLogMsg;
			IP.erase(0, IP.find_first_of('\'') + 1);
			IP.erase(IP.find_first_of('\''), IP.length());
			GotIP = true;
		}
		else
		{
			//std::cout << strLogMsg << std::endl;
		}
	};

	m_TCPServer = new CTCPServer(LogPrinter, std::to_string(ServerPort));

	std::thread SMOListen(SMOListener);

	std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	std::time_t end_time = std::chrono::system_clock::to_time_t(end);
	std::cout << "Server Started: " << std::ctime(&end_time);
	std::cout << "Starting Took: " << elapsed_seconds.count() << "s\n\n";

	std::cout << "ServerLog:\n\n";

	while (Running)
	{
		std::vector<Clients> CurClients;
		{
			const std::lock_guard lock(m_Mutex);
			m_IP = IP;
			if (!m_GotIP)
				m_GotIP = GotIP;
			GotIP = false;
			CurClients = ConnectedClients;
		}


		for (auto& Values : CurClients)
		{
			auto& Client = Values.Client;
			auto& LoggedIn = Values.LoggedIn;
			auto& Ip = Values.IP;
			auto& UserType = Values.UserType;
			auto& UserName = Values.UserName;
			auto& RoomID = Values.RoomID;
			auto& TNSs = Values.TNSs;
			//auto& SMClientID = Values.SMClientID; // Unused for now.
			auto& ScoreTracker = Values.ScoreTracker;

			std::vector<std::string> vInput;
			{
				const std::lock_guard lock(m_Mutex);
				auto result = std::find_if(ConnectedClients.begin(), ConnectedClients.end(), [&Values](const Clients& c) { return Values.Client == c.Client; });
				vInput = result->vInput;
				result->vInput.clear();
			}

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
							(void)m_TCPServer->Send(Client, Header + Out);
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
								(void)m_TCPServer->Send(Client, Header + Out);
								continue;
							}
						}

						if (UserType == 1 || (UserType == 2 && RoomID >= 0))
						{
							auto rooms = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](const Rooms& Room) { return RoomID == Room.RoomID; });

							if (Command == "free")
							{
								rooms->FreeMode = !(rooms->FreeMode);

								for (auto& c : CurClients)
								{
									if (c.RoomID != RoomID)
										continue;

									std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "Set Room to Free mode: " + (rooms->FreeMode ? "Enabled" : "Disabled");
									std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
									(void)m_TCPServer->Send(c.Client, Header + Out);
								}
								continue;
							}
						}

						if (UserType == 2) // Admin Commands
						{
							if (Command == "adminkick")
							{
								auto clients = std::find_if(CurClients.begin(), CurClients.end(), [&Argument](const Clients& client) { return Argument == client.UserName; });

								if (clients != CurClients.end())
								{
									std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "User Kicked: " + Argument;
									std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
									(void)m_TCPServer->Send(Client, Header + Out);

									std::cout << "User: " << clients->UserName << " '" << clients->IP << "' Kicked.\n";
									m_TCPServer->Disconnect(clients->Client);
									clients->Connected = false;
								}
								else
								{
									std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "User Not Connected: " + Argument;
									std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
									(void)m_TCPServer->Send(Client, Header + Out);
								}
								continue;
							}

							if (Command == "userip")
							{
								auto clients = std::find_if(CurClients.begin(), CurClients.end(), [&Argument](const Clients& client) { return Argument == client.UserName; });

								if (clients != CurClients.end())
								{
									std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "User IP: '" + clients->IP + "' For " + Argument;
									std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
									(void)m_TCPServer->Send(Client, Header + Out);
								}
								else
								{
									std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "User Not Connected: " + Argument;
									std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
									(void)m_TCPServer->Send(Client, Header + Out);
								}
								continue;
							}

						}

						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "Invalid Command: " + Command;
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						(void)m_TCPServer->Send(Client, Header + Out);
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

						std::string Text = Input;
						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + usertype.append(UserName) + ": " + Text.erase(0, 5).erase(Text.find_first_of('\0'));
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						(void)m_TCPServer->Send(c.Client, Header + Out);
					}
					continue;
				}

				if (Input[4] == 5)
				{
					auto rooms = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](const Rooms& Room) { return RoomID == Room.RoomID; });

					const unsigned char GradeAndNumNote = Input[6];
					const int NumNotes = GradeAndNumNote & 0x0F;
					unsigned short iOffset;
					std::memcpy(&iOffset, &Input[15], 2);
					// 112 == failed.
					iOffset = GradeAndNumNote == 112 ? 0 : ntohs(iOffset);
					const double InOffset = iOffset == 0 ? 0.0 : static_cast<double>(iOffset) / 2000.0 - 16.384;
					std::string TNS = TapNoteScoreCalc(InOffset, Input[5], TNSs, ScoreTracker, NumNotes <= 0 ? 1 : NumNotes);

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

						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 5)) + std::string(1, '\1') + std::string(1, static_cast<char>(rooms->CurPlayers.size())) + Combos;
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						(void)m_TCPServer->Send(c.Client, Header + Out);

						Out = std::string(1, static_cast<char>(ProtocolVersion + 5)) + std::string(1, '\2') + std::string(1, static_cast<char>(rooms->CurPlayers.size())) + Grades;
						Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						(void)m_TCPServer->Send(c.Client, Header + Out);
					}
					continue;
				}

				if (Input[4] == 3 && RoomID >= 0)
				{
					auto rooms = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](const Rooms& Room) { return RoomID == Room.RoomID; });

					--rooms->NumPlayersWaiting;

					if (rooms->NumPlayersWaiting == 0)
					{
						unsigned PlayerID = 0;
						for (auto& c : CurClients)
						{
							if (c.RoomID != RoomID)
								continue;

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 3));
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							(void)m_TCPServer->Send(c.Client, Header + Out);
							if (Input[7] == 16)
							{
								rooms->CurPlayers.push_back(c.UserName);
								c.SMClientID = PlayerID++;
							}

						}
						rooms->NumPlayersWaiting = rooms->NumPlayers;
						rooms->NumPlayersPlaying = rooms->NumPlayers;
						rooms->SongSelected = false;

						if (Input[7] != 16)
							continue;

						std::string Players;

						for (auto& Player : rooms->CurPlayers)
							Players += std::string(1, '\1') + Player + std::string(1, '\0');

						std::string PlayerNums;

						int pnum = 0;

						for ([[maybe_unused]] auto& Player : rooms->CurPlayers)
							PlayerNums += std::string(1, static_cast<char>(pnum++));

						for (auto& c : CurClients)
						{
							if (c.RoomID != RoomID)
								continue;

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 9)) + std::string(1, '\0') + std::string(1, static_cast<char>(rooms->CurPlayers.size())) + Players;
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							(void)m_TCPServer->Send(c.Client, Header + Out);

							Out = std::string(1, static_cast<char>(ProtocolVersion + 5)) + std::string(1, '\0') + std::string(1, static_cast<char>(rooms->CurPlayers.size())) + PlayerNums;
							Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							(void)m_TCPServer->Send(c.Client, Header + Out);

							Out = std::string(1, static_cast<char>(ProtocolVersion + 5)) + std::string(1, '\1') + std::string(1, static_cast<char>(rooms->CurPlayers.size())) + std::string(rooms->CurPlayers.size()*2, '\0');
							Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							(void)m_TCPServer->Send(c.Client, Header + Out);

							Out = std::string(1, static_cast<char>(ProtocolVersion + 5)) + std::string(1, '\2') + std::string(1, static_cast<char>(rooms->CurPlayers.size())) + std::string(rooms->CurPlayers.size(), '\0');
							Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							(void)m_TCPServer->Send(c.Client, Header + Out);
						}
					}
					continue;
				}

				if (Input[4] == 10 && RoomID >= 0 && Input[5] == 5)
				{
					std::string Players;
					std::string Scores;
					std::string Grades;
					std::string Difficulties;
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
						Difficulties += std::string(1, '\0');
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

						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 4)) + std::string(1,static_cast<char>(NumPlayers)) += Players += Scores += Grades += Difficulties += Taps;
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						(void)m_TCPServer->Send(c.Client, Header + Out);
					}
					continue;
				}

				if ((Input[4] == 10 && RoomID >= 0 && Input[5] == 4))
				{
					auto rooms = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](const Rooms& Room) { return RoomID == Room.RoomID; });

					rooms->CurPlayers.erase(std::find(rooms->CurPlayers.begin(), rooms->CurPlayers.end(), UserName), rooms->CurPlayers.end());

					--rooms->NumPlayersPlaying;
					ScoreTracker = 0;
					TNSs = {};

					std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 9)) + std::string(1, '\0') + std::string(1, '\0');
					std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
					(void)m_TCPServer->Send(Client, Header + Out);

					continue;
				}

				if (Input[4] == 10 && RoomID >= 0 && Input[5] == 3)
				{
					auto rooms = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](const Rooms& Room) { return RoomID == Room.RoomID; });

					++rooms->NumPlayersPlaying;
					rooms->CurPlayers.push_back(UserName);
				}

				if (Input[4] == 10 && RoomID >= 0 && Input[5] == 1)
				{
					auto rooms = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](const Rooms& Room) { return RoomID == Room.RoomID; });

					--rooms->NumPlayersPlaying;
					rooms->CurPlayers.erase(std::find(rooms->CurPlayers.begin(), rooms->CurPlayers.end(), UserName), rooms->CurPlayers.end());
				}

				if (Input[4] == 10 && RoomID >= 0 && Input[5] == 0)
				{
					auto rooms = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](const Rooms& Room) { return RoomID == Room.RoomID; });

					if (rooms->SongSelected)
						continue;
					LeavePlayer(Values, CurClients);
					LeaveRoom(Values, CurClients);
					for (auto& c : CurClients)
						UpdateRooms(c.Client);

					continue;
				}

				if (RoomID >= 0 && Input[4] == 8 && Input[5] == 1)
				{
					auto rooms = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](const Rooms& Room) { return RoomID == Room.RoomID; });

					rooms->UsersMissingSong += UserName + std::string(1, '\0');
					rooms->SongSelected = false;

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
						(void)m_TCPServer->Send(c.Client, Header + Out);
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

					auto rooms = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&](const Rooms& Room) { return RoomID == Room.RoomID; });

					if (UserType == 0 && !rooms->FreeMode)
					{
						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + "FreeMode disabled, Ask Roomhost for /free";
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						(void)m_TCPServer->Send(Client, Header + Out);
						continue;
					}

					if (rooms->NumPlayersPlaying > 0)
					{

						std::string Players;

						for (const std::string& player : rooms->CurPlayers)
							Players += player + std::string(1, ' ');

						for (auto& c : CurClients)
						{
							if (c.RoomID != RoomID)
								continue;

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + Players + "havent finished yet, please wait.";
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							(void)m_TCPServer->Send(c.Client, Header + Out);
						}
						continue;
					}

					if (!rooms->SongSelected ||
						rooms->CurSong[0] != Vals[0] ||
						rooms->CurSong[1] != Vals[1] ||
						rooms->CurSong[2] != Vals[2])
					{
						for (auto& c : CurClients)
						{
							if (c.RoomID != RoomID)
								continue;

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 7)) + UserName + " selected song: " + Vals[0];
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							(void)m_TCPServer->Send(c.Client, Header + Out);

							if (c.Client == Client)
								continue;

							Out = std::string(1, static_cast<char>(ProtocolVersion + 8)) + std::string(1, '\1') + Vals[0] + std::string(1, '\0') + Vals[1] + std::string(1, '\0') + Vals[2];
							Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							(void)m_TCPServer->Send(c.Client, Header + Out);

						}
						rooms->SongSelected = true;
						rooms->UsersMissingSong.clear();
						rooms->CurSong = { Vals[0], Vals[1], Vals[2] };
					}
					else if (rooms->UsersMissingSong.empty())
					{
						for (auto& c : CurClients)
						{
							if (c.RoomID != RoomID)
								continue;

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 8)) + std::string(1, '\2') + Vals[0] + std::string(1, '\0') + Vals[1] + std::string(1, '\0') + Vals[2];
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							(void)m_TCPServer->Send(c.Client, Header + Out);
						}
						rooms->NumPlayersWaiting = rooms->NumPlayers;
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
					(void)m_TCPServer->Send(Client, Header + Out);
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
								(void)m_TCPServer->Disconnect(Client);
								Values.Connected = false;
								invalidpass = true;
								break;
							}

							if (password != Vals[1])
							{
								std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 12)) + std::string(1, '\0') + std::string(1, '\1') + "Wrong Password.\n";
								std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
								(void)m_TCPServer->Send(Client, Header + Out);
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

						SQLite::Transaction transaction1(db);

						db.exec(("INSERT INTO Users VALUES (\"" + Vals[0] + "\", \"" + Vals[1] + "\", 0)").c_str());

						transaction1.commit();

						UserName = Vals[0];
					}

					std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 12)) + std::string(2, '\0') + "Correct Password.";
					std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
					(void)m_TCPServer->Send(Client, Header + Out);
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
						(void)m_TCPServer->Send(Client, Header + Out);
						continue;
					}

					if (auto rooms = std::find_if(PlayerRooms.begin(), PlayerRooms.end(), [&Vals](const Rooms& Room) { return Vals[0] == Room.RoomName; }); rooms == PlayerRooms.end())
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

							std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 12)) + std::string(1, '\1') + std::string(1, '\1') + std::string(1, static_cast<char>(PlayerRooms.size())) += RoomNames += RoomStates += RoomFlags;
							std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
							(void)m_TCPServer->Send(c.Client, Header + Out);
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
			const std::lock_guard lock(m_Mutex);
			auto result = std::find_if(ConnectedClients.begin(), ConnectedClients.end(), [&Client](const Clients& c) { return Client.Client == c.Client; });
			if (result != ConnectedClients.end())
			{
				result->Connected = Client.Connected;
				result->LoggedIn = Client.LoggedIn;
				result->RoomID = Client.RoomID;
				result->TNSs = Client.TNSs;
				result->ScoreTracker = Client.ScoreTracker;
				result->SMClientID = Client.SMClientID;
				result->UserName = Client.UserName;
				result->UserType = Client.UserType;
			}
		}
	}

	Running = false;
	SMOListen.join();

	return 0;
}
#include <iostream>

#ifdef WIN32
#define WINDOWS
#endif

#include "Main.hpp"
#include "mini/ini.h"
#include <thread>
#include <chrono>
#include "SQLiteCpp/SQLiteCpp.h"

void SMOListener()
{
	while (Running)
	{
		ASocket::Socket ConnectedClient;

		if (m_TCPServer->Listen(ConnectedClient))
		{
			char Input[1024] = {};
			m_TCPServer->Receive(ConnectedClient, Input, 1024, false);

			if (Input[4] = 2)
			{
				m_Mutex.lock();
				std::cout << std::string(Input, Input[6]+2).erase(0,6) + " '" + m_IP + "'"+ " Connected with StepManiaOnline Protocol: V" + std::to_string(Input[5]) + "\n";
				std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 2)) + std::string(1, static_cast<char>(ServerVersion)) + ServerName;
				//std::string Salt = std::string(1, PWSalt[0]) + std::string(1, PWSalt[1]) + std::string(1, PWSalt[2]) + std::string(1, PWSalt[3]); // need to figure this out.
				std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));

				m_TCPServer->Send(ConnectedClient, Header + Out);
				ConnectedClients.push_back({ ConnectedClient, false, m_IP});
				m_Mutex.unlock();
			}
			else
			{
				m_TCPServer->Disconnect(ConnectedClient);
			}
		}
	}
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

	std::cout << "OpenSMO++ Alpha1: By Jousway\n";
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

	auto LogPrinter = [&IP](const std::string& strLogMsg) {
		if (strLogMsg.find("Incoming connection from") != std::string::npos)
		{
			m_Mutex.lock();
			IP = strLogMsg;
			IP.erase(0, IP.find_first_of('\'')+1);
			IP.erase(IP.find_first_of('\''), IP.length());
			m_Mutex.unlock();
		}
		else
			std::cout << strLogMsg << std::endl;
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
		m_Mutex.unlock();


		m_Mutex.lock();
		for (auto& Values : ConnectedClients)
		{
			auto& Client = Values.Client;
			auto& LoggedIn = Values.LoggedIn;
			auto& Ip = Values.IP;
			auto& UserName = Values.UserName;
			auto& RoomID = Values.RoomID;

			int ret = ASocket::SelectSocket(Client, 300);

			if (ret > 0)
			{
				char Input[1024] = {};
				int read = m_TCPServer->Receive(Client, Input, 1024, false);

				if (read == 0)
				{
					std::cout << "User: " << UserName << " '" << Ip << "' Disconnected.\n";
					m_TCPServer->Disconnect(Client);
					ConnectedClients.erase(std::remove_if(ConnectedClients.begin(), ConnectedClients.end(), [&](Clients const& client) { return client.Client == Client; }), ConnectedClients.end());
					continue;
				}

				if (read > 0)
				{
					// Debug.
					//std::cout << std::string(Input, 1024) << std::endl;

					if (!LoggedIn)
					{
						if (Input[4] == 6)
							continue;

						std::stringstream in(std::string(Input, 1024).erase(0,6));
						std::string Val;
						std::vector<std::string> Vals;

						while (std::getline(in, Val, '\0'))
						{
							Vals.push_back(Val);
						}

						Vals.erase(std::remove(Vals.begin(), Vals.end(), "\0"), Vals.end());

						bool FoundUser = false;

						try {
							SQLite::Statement query(db, (std::string("SELECT * FROM Users WHERE UserName =") + "\""+ Vals[0] + "\"").c_str());

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
									ConnectedClients.erase(std::remove_if(ConnectedClients.begin(), ConnectedClients.end(), [&](Clients const& client) { return client.Client == Client; }), ConnectedClients.end());
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
						catch (...){ }

						if (!FoundUser)
						{
							std::cout << "Creating New User: " << Vals[0] << "\n";

							SQLite::Transaction transaction(db);

							db.exec(("INSERT INTO Users VALUES (\"" + Vals[0] + "\", \""+ Vals[1] + "\", 0)").c_str());

							transaction.commit();

							UserName = Vals[0];
						}

						std::string Out = std::string(1, static_cast<char>(ProtocolVersion + 12)) + std::string(2, '\0') + "Correct Password.\n";
						std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
						m_TCPServer->Send(Client, Header + Out);
						LoggedIn = true;

					}

					if (RoomID < 0)
					{







					}
				}
			}
			// ping pong.
			//std::string Out = std::string(1, static_cast<char>(ProtocolVersion));
			//std::string Header = std::string(3, '\0') + std::string(1, static_cast<char>(Out.size()));
			//if (!m_TCPServer->Send(Client, Header + Out))
			//{
			//	m_TCPServer->Disconnect(Client);
			//	ConnectedClients.erase(std::remove_if(ConnectedClients.begin(), ConnectedClients.end(), [&](Clients const& client) { return client.Client == Client; }), ConnectedClients.end());
			//}

		}
		m_Mutex.unlock();
	}

	Running = false;
	SMOListen.join();

	return 0;
}
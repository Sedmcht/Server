#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <string>

std::string parse(std::string packet, std::string mark)
{
	if (packet.find(mark) == std::string::npos)
		return "";
	return packet.substr(packet.find("{", packet.find(mark)) + 1, packet.find("}", packet.find("{", packet.find(mark))) - packet.find("{", packet.find(mark)) - 1);
}


struct User
{
	std::string _login;
	std::string _passwd;
	std::string _nickName;

	User(std::string login, std::string passwd, std::string nickName) : _login(login), _passwd(passwd), _nickName(nickName) {}
};

struct Message
{
	std::string _nickName;
	std::string _text;
	std::string _privateUser;
	int _numberOnServer;

	Message(std::string nickName, std::string text, std::string privateUser, int numberOnServer) : _nickName(nickName), _text(text), _privateUser(privateUser), _numberOnServer(numberOnServer) {}
};

struct DataBase
{
	std::vector <User> _users;
	std::vector <Message> _messages;
};

struct Request
{
	std::string _method;
	std::string _lastClientMessage;
	std::string _login;
	std::string _passwd;
	std::string _nickName;
	std::string _text;
	std::string _privateUser;

	Request(std::string packet, int bytes)
	{

		packet.resize(bytes);

		_method = parse(packet, "method");
		_lastClientMessage = parse(packet, "lastClientMessage");
		_login = parse(packet, "login");
		_passwd = parse(packet, "passwd");
		_nickName = parse(packet, "nickName");
		_text = parse(packet, "text");
		_privateUser = parse(packet, "privateUser");
	}
};


struct Response
{
	std::string _status;
	std::string _owner;
	std::string _data;
	int _messageCount;

	Response(std::string status, std::string owner = "", std::string data = "", int messageCount = 0) : _status(status), _owner(owner), _data(data), _messageCount(messageCount) {}

	std::string toString()
	{
		std::string tmp = "status{" + _status + "}" + "owner{" + _owner + "}" + "data{" + _data + "}" + "messageCount{" + std::to_string(_messageCount) + "}";
		return tmp;
	}
};

struct Server
{
	DataBase _dataBase;

	Response userAdd(std::string& login, std::string& passwd, std::string& nickName)
	{
		if (login.size() == 0 || passwd.size() == 0 || nickName.size() == 0)
			return Response("emptyData");

		if (_dataBase._users.size() > 0)
		{
			for (User& user : _dataBase._users)
			{
				if (user._login == login || user._nickName == nickName)
					return Response("loginOrNickName");
			}
		}


		_dataBase._users.push_back(User(login, passwd, nickName));
		return Response("ok");
	}

	Response userCheck(std::string& login, std::string& passwd)
	{
		for (User& user : _dataBase._users)
		{
			if (user._login == login && user._passwd == passwd)
				return Response("ok");
		}

		return Response("fail");
	}

	Response reciveMessage(std::string& login, std::string& passwd, std::string& text, std::string& privateUser)
	{

		if (_dataBase._users.size() > 0)
		{
			for (User& user : _dataBase._users)
			{
				if (user._login == login && user._passwd == passwd)
				{
					_dataBase._messages.push_back(Message(user._nickName, text, privateUser, _dataBase._messages.size() + 1));
					return Response("ok");
				}
			}
		}

		return Response("error");
	}

	Response getUserMessage(std::string& login, std::string& passwd, std::string lastClientMessage)
	{
		int messageNumber;

		try
		{
			messageNumber = std::stoi(lastClientMessage);
		}
		catch (std::invalid_argument& e)
		{
			return Response("invalid_argument");
		}
		catch (std::out_of_range& e)
		{
			return Response("out_of_range");
		}



		if (_dataBase._users.size() > 0)
		{
			for (User& user : _dataBase._users)
			{
				if (user._login == login && user._passwd == passwd)
				{
					for (Message& msg : _dataBase._messages)
					{
						if (msg._numberOnServer > messageNumber && (msg._privateUser == "" || msg._privateUser == user._nickName) && msg._nickName != user._nickName)
						{
							return Response("ok", msg._nickName, msg._text, msg._numberOnServer);
						}
					}
				}
			}
		}
		return Response("NoNewMessages");
	}



};


Server myServer;


void connection(SOCKET newConnection)
{

	char inputBuffer[1024];
	int bytes;
	std::string tmp;
	while (1)
	{


		bytes = recv(newConnection, inputBuffer, sizeof(inputBuffer), NULL);

		if (bytes > 0)
		{
			Request req(inputBuffer, bytes);



			if (req._method == "reg")
			{
				tmp = myServer.userAdd(req._login, req._passwd, req._nickName).toString();
				send(newConnection, tmp.c_str(), tmp.size(), NULL);
			}

			if (req._method == "post")
			{
				tmp = myServer.reciveMessage(req._login, req._passwd, req._text, req._privateUser).toString();
				send(newConnection, tmp.c_str(), tmp.size(), NULL);
			}

			if (req._method == "get")
			{
				tmp = myServer.getUserMessage(req._login, req._passwd, req._lastClientMessage).toString();
				send(newConnection, tmp.c_str(), tmp.size() + 1, NULL);
			}

			if (req._method == "check")
			{
				tmp = myServer.userCheck(req._login, req._passwd).toString();
				send(newConnection, tmp.c_str(), tmp.size() + 1, NULL);
			}
		}
	}
}


void getConnections(SOCKET sListen, SOCKADDR_IN addr, int sizeofaddr)
{
	SOCKET newConnection;

	while (1)
	{
		newConnection = accept(sListen, (SOCKADDR*)&addr, &sizeofaddr);
		std::thread(connection, newConnection).detach();
	}
}




int main()
{
	WSAData wsaData;
	WORD DLLVersion = MAKEWORD(2, 1);
	if (WSAStartup(DLLVersion, &wsaData) != 0) {
		std::cout << "Error" << std::endl;
		exit(1);
	}

	int IP;
	InetPtonW(AF_INET, L"127.0.0.1", &IP);
	SOCKADDR_IN addr;
	int sizeofaddr = sizeof(addr);
	addr.sin_addr.s_addr = IP;
	addr.sin_port = htons(1111);
	addr.sin_family = AF_INET;


	SOCKET sListen = socket(AF_INET, SOCK_STREAM, NULL);
	bind(sListen, (SOCKADDR*)&addr, sizeof(addr));
	listen(sListen, SOMAXCONN);

	getConnections(sListen, addr, sizeofaddr);

}
#pragma once
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <comdef.h>
#include <process.h>
#include <string>
#include <chrono>
#include <mutex>
#include <utils/net.h>
#include <utils/flog.h>
#include <tinyxml2.h>


using namespace std;

#define BUF_SIZE 1024
#define DOWNFACTOR 1//32768.0

typedef chrono::high_resolution_clock Clock;
typedef Clock::time_point ClockTime;

typedef char buf_t[BUF_SIZE];

const char Server_Parameter_Format[] = "<Cluster>"
"<Name>Server_Parameters</Name>"
"<NumElts>7</NumElts>"
"<DBL>"
"<Name>Frequency</Name>"
"<Val>0</Val>"
"</DBL>"
"<DBL>"
"<Name>Bandwidth</Name>"
"<Val>0</Val>"
"</DBL>"
"<DBL>"
"<Name>InputLevel</Name>"
"<Val>0</Val>"
"</DBL>"
"<DBL>"
"<Name>AmplifierGain</Name>"
"<Val>0</Val>"
"</DBL>"
"<DBL>"
"<Name>FreqMax</Name>"
"<Val>0</Val>"
"</DBL>"
"<DBL>"
"<Name>FreqMin</Name>"
"<Val>0</Val>"
"</DBL>"
"<Boolean>"
"<Name>CAG</Name>"
"<Val>0 </Val>"
"</Boolean>"
"</Cluster>";

const char Server_Channel_Format[] = "<Cluster>"
"<Name>Server_Channel</Name>"
"<NumElts>5</NumElts>"
"<EW>"
"<Name>Command</Name>"
"<Choice>Create</Choice>"
"<Choice>Destroy</Choice>"
"<Val>0</Val>"
"</EW>"
"<U16>"
"<Name>NetworkPort</Name>"
"<Val>0</Val>"
"</U16>"
"<DBL>"
"<Name>Frequency</Name>"
"<Val>0</Val>"
"</DBL>"
"<DBL>"
"<Name>Bandwidth</Name>"
"<Val>0</Val>"
"</DBL>"
"<DBL>"
"<Name>Gain</Name>"
"<Val>0</Val>"
"</DBL>"
"</Cluster>";

const char Server_infos_Format[] = "<Cluster>"
"<Name>Server_Channel_Info</Name>"
"<NumElts>6</NumElts>"
"<U16>"
"<Name>NetworkPort</Name>"
"<Val>0</Val>"
"</U16>"
"<DBL>"
"<Name>Frequency</Name>"
"<Val>0</Val>"
"</DBL>"
"<DBL>"
"<Name>SampleRate</Name>"
"<Val>0</Val>"
"</DBL>"
"<EW>"
"<Name>Status</Name>"
"<Choice>Off</Choice>"
"<Choice>On</Choice>"
"<Val>0</Val>"
"</EW>"
"<DBL>"
"<Name>FreqMax</Name>"
"<Val>0</Val>"
"</DBL>"
"<DBL>"
"<Name>FreqMin</Name>"
"<Val>0</Val>"
"</DBL>"
"</Cluster>";



class ArcaleRfTCPServer
{
private:

	string _IP;
	int _configPort;
	int _dataPort;
	SOCKET _configSock;
	SOCKET _dataSock;
	fd_set input_set;

	bool _partialConnect;
	bool _connected;
	bool _active;
	timeval _timevaltimeout;
	uint32_t _timeout;
	bool _sendingInProgress;

	uint32_t _IQpairs;
	uint32_t _packetByteSize;

	double _maxFrequency;
	double _minFrequency;

	void sendParameter(double& Frequency)
	{
		try {
			buf_t message_to_send;

			sprintf_s(message_to_send, BUF_SIZE, "%010i", sizeof(Server_Parameter_Format));


			send(_configSock, message_to_send, 10, 0);
			send(_configSock, Server_Parameter_Format, sizeof(Server_Parameter_Format), 0);

			char buff[BUF_SIZE];
			char buffsize[10];


			receiveMessage(_configSock, buffsize, 10);



			int nextpaketsize = atoi(buffsize);

			memset(buff, 0, BUF_SIZE);

			receiveMessage(_configSock, buff, nextpaketsize);

			tinyxml2::XMLDocument message;

			message.Parse(buff);

			if (!message.ErrorID())
			{
				tinyxml2::XMLNode* cluster = message.FirstChildElement("Cluster");
				for (tinyxml2::XMLNode* DBL = cluster->FirstChildElement("DBL"); DBL; DBL = DBL->NextSiblingElement("DBL"))
				{

					tinyxml2::XMLElement* elmt = DBL->FirstChildElement("Name");
					if (strcmp(elmt->FirstChild()->Value(), "Frequency") == 0) {
						Frequency = atof(elmt->NextSiblingElement("Val")->FirstChild()->Value());
					}
					else if (strcmp(elmt->FirstChild()->Value(), "Freq Max") == 0) {
						_maxFrequency = atof(elmt->NextSiblingElement("Val")->FirstChild()->Value());
					}
					else if (strcmp(elmt->FirstChild()->Value(), "Freq Min") == 0) {
						_minFrequency = atof(elmt->NextSiblingElement("Val")->FirstChild()->Value());
					}
				}
			}
			else
			{
				buf_t buff;
				// sprintf_s(buff, "Error parsing Message, tinyXML error is %s at collumn %i line %i",
				// 	message.ErrorDesc(), message.ErrorCol(), message.ErrorRow());
				throw exception(buff);
			}
		}
		catch (exception ex) {
			throw ex;
		}
	}

	uint32_t getIQpair()
	{
		char buff[10];
		memset(buff, 0, 10);
		receiveMessage(_dataSock, buff, 10);
		_packetByteSize = atoi(buff);
		return _packetByteSize / 4;
	}

	int receiveMessage(SOCKET receptSocket, char* sizeBuff, size_t PacketSize)
	{
		FD_ZERO(&input_set);
		FD_SET(receptSocket, &input_set);
		int s = select(0, &input_set, NULL, NULL, &_timevaltimeout);
		int ByteRead = 0;

		if (s > 0) {
			ByteRead = recv(receptSocket, sizeBuff, PacketSize, 0);

		}
		else if (s == SOCKET_ERROR) {
			char* buf = NULL;
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, WSAGetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPSTR)&buf, 0, NULL);
			throw exception(buf);
		}
		else
		{
			buf_t buff;
			sprintf_s(buff, "Timeout on TCP reception on port %d", receptSocket == _dataSock ? _dataPort : _configPort);
			throw exception(buff);
		}
		return ByteRead;
	}

	void stopReception()
	{
		try
		{
			_active = false;
			double DontCare = 0;
			sendChannelInfo(DontCare, DontCare, &DontCare, true);
			closesocket(_dataSock);
		}
		catch (const std::exception& ex)
		{
		}
	}



public:
	ArcaleRfTCPServer(int configPort, int DataPort, int timeout)
	{
		try
		{
			WSADATA wsd;
			WSAStartup(MAKEWORD(2, 2), &wsd);
			_IP = "";
			_configPort = configPort;
			_dataPort = DataPort;
			_timeout = timeout;
			_timevaltimeout.tv_sec = timeout;
			_timevaltimeout.tv_usec = 0;
			
			_connected = false;
			_partialConnect = false;
			_sendingInProgress = false;
		}
		catch (const std::exception& ex)
		{
			flog::info("ArcaleRFSuiteTCPServer '{0}': Menu Deselect!", ex.what());
		}
	}

	~ArcaleRfTCPServer()
	{
		WSACleanup();
	}

	void setIP(string IP)
	{
		_IP = IP;
	}

	int getPacketBytesSize()
	{
		return _packetByteSize;
	}

	int sendChannelInfo(double& frequency, double& bandwith, double *samplerate, bool destroyChannel = FALSE)
	{
		try
		{
			frequency = frequency > _maxFrequency ? _maxFrequency : frequency;
			frequency = frequency < _minFrequency ? _minFrequency : frequency;

			if (_connected) {
				tinyxml2::XMLDocument message;
				message.Parse(Server_Channel_Format);

				if (!message.ErrorID()) {

					message.FirstChildElement("Cluster")->FirstChildElement("EW")->FirstChildElement("Val")->FirstChild()->SetValue(destroyChannel ? "1" : "0");
					message.FirstChildElement("Cluster")->FirstChildElement("U16")->FirstChildElement("Val")->FirstChild()->SetValue(_bstr_t(_dataPort));

					tinyxml2::XMLNode* cluster = message.FirstChildElement("Cluster");
					for (tinyxml2::XMLNode* DBL = cluster->FirstChildElement("DBL"); DBL; DBL = DBL->NextSiblingElement("DBL"))
					{

						tinyxml2::XMLElement* elmt = DBL->FirstChildElement("Name");
						if ((strcmp(elmt->FirstChild()->Value(), "Frequency") == 0)) {
							elmt->NextSiblingElement("Val")->FirstChild()->SetValue(to_string(frequency).c_str());
						}
						else if ((strcmp(elmt->FirstChild()->Value(), "Bandwidth") == 0)) {
							elmt->NextSiblingElement("Val")->FirstChild()->SetValue(to_string(bandwith).c_str());
						}
						else if ((strcmp(elmt->FirstChild()->Value(), "Gain") == 0)) {
							elmt->NextSiblingElement("Val")->FirstChild()->SetValue("9999");
						}
					}

					buf_t message_to_send;
					tinyxml2::XMLPrinter printer;

					message.Accept(&printer);

					sprintf_s(message_to_send, BUF_SIZE, "%010i", printer.CStrSize());

					send(_configSock, message_to_send, 10, 0);
					send(_configSock, printer.CStr(), printer.CStrSize(), 0);

					ClockTime start = Clock::now();


					char buff[BUF_SIZE];
					char sizeBuff[12];
					int status = 0;
					uint32_t LoopedFor = 0;
					while (LoopedFor < _timeout)
					{
						LoopedFor = (int)chrono::duration_cast<chrono::seconds>(start - Clock::now()).count();

						memset(sizeBuff, 0, 10);

						tinyxml2::XMLPrinter printer2;
						tinyxml2::XMLDocument message2;

						message2.Parse(Server_infos_Format);
						message2.FirstChildElement("Cluster")->FirstChildElement("U16")->FirstChildElement("Val")->FirstChild()->SetValue(_bstr_t(_dataPort));

						message2.Accept(&printer2);

						sprintf_s(sizeBuff, "%010i", printer2.CStrSize());

						send(_configSock, sizeBuff, 10, 0);
						send(_configSock, printer2.CStr(), printer2.CStrSize(), 0);
						int nextpaketsize = 0;

						receiveMessage(_configSock, sizeBuff, 10);

						nextpaketsize = atoi(sizeBuff);
						memset(buff, 0, BUF_SIZE);


						receiveMessage(_configSock, buff, nextpaketsize);


						message2.Clear();

						message2.Parse(buff);

						if (!message2.ErrorID())
						{
							tinyxml2::XMLNode* cluster = message2.FirstChildElement("Cluster");

							status = atoi(cluster->FirstChildElement("EW")->FirstChildElement("Val")->FirstChild()->Value());
							if (status == 1)
							{
								for (tinyxml2::XMLNode* DBL = cluster->FirstChildElement("DBL"); DBL; DBL = DBL->NextSiblingElement("DBL"))
								{
									tinyxml2::XMLElement* elmt = DBL->FirstChildElement("Name");

									if (strcmp(elmt->FirstChild()->Value(), "Frequency") == 0) {
										frequency = atoi(elmt->NextSiblingElement("Val")->FirstChild()->Value());
									}
									else if (strcmp(elmt->FirstChild()->Value(), "Sample Rate") == 0)
									{
										*samplerate = atoi(elmt->NextSiblingElement("Val")->FirstChild()->Value());
									}
								}
								break;
							}
							else if (destroyChannel == true)
							{
								return 0;
							}
						}
						else
						{
							buf_t buff;
							// sprintf_s(buff, "Error parsing Message, tinyXML error is %s at collumn %i line %i",
							// 	message.ErrorDesc(), message.ErrorCol(), message.ErrorRow());
							throw exception(buff);
						}
						Sleep(1000);
					}
					if (status == 0)
					{
						throw exception("Channel is not created");

					}
				}
				else
				{
					buf_t buff;
					// sprintf_s(buff, "Error parsing Message, tinyXML error is %s at collumn %i line %i",
					// 	message.ErrorDesc(), message.ErrorCol(), message.ErrorRow());
					throw exception(buff);
				}
				return 0;
			}
			return -1;
		}
		catch (const std::exception& ex)
		{
			MessageBoxA(NULL, ex.what(), "Error", NULL);
			return -1;
		}
	}

	int Connect(string IP, double& Frequency)
	{
		_maxFrequency = (long)20e9;
		_minFrequency = 0;
		_IP = IP;
		try
		{
			struct sockaddr_in serverCtrl;

			HOSTENT* host;

			if (!_connected)
			{
				_configSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (_configSock == INVALID_SOCKET) throw exception("Invalid Config Socket");

				/// connection to Control Port
				serverCtrl.sin_family = AF_INET;
				serverCtrl.sin_port = htons(_configPort);
				inet_pton(AF_INET, _bstr_t(_IP.c_str()), &serverCtrl.sin_addr);

				// resolve host
				if (serverCtrl.sin_addr.s_addr == INADDR_NONE) {

					host = gethostbyname(_bstr_t(_IP.c_str()));

					if (host == NULL) {
						wchar_t* buf = NULL;
						FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
							NULL, WSAGetLastError(),
							MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
							(LPWSTR)&buf, 0, NULL);
						throw exception(_bstr_t(buf));
					}
					CopyMemory(&serverCtrl.sin_addr, host->h_addr, host->h_length);

				}

				if (connect(_configSock, (struct sockaddr*)&serverCtrl, sizeof(serverCtrl)) == SOCKET_ERROR) {
					wchar_t* buf = NULL;
					FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
						NULL, WSAGetLastError(),
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						(LPWSTR)&buf, 0, NULL);
					throw exception(_bstr_t(buf));
				}

			}

			_partialConnect;
			sendParameter(Frequency);
			_connected = true;
			return 0;
		}
		catch (exception& ex)
		{
			MessageBoxA(NULL, ex.what(), "Error", 0);
			_connected = false;
			_partialConnect = false;
			return -1;
		}
	}

	void getFrequencyLimits(double* min, double* max)
	{
		*min = _minFrequency;
		*max = _maxFrequency;
	}

	int ConnectToChannel(double& frequency, double& bandwith, double * samplerate)
	{

		buf_t buf;
		struct sockaddr_in serverData;
		HOSTENT* host;
		try
		{
			if (_connected)
			{
				if (sendChannelInfo(frequency, bandwith, samplerate) < 0) throw exception("Error Creating Channel");

				Sleep(500);
				/* connection to Data Port */
				serverData.sin_family = AF_INET;
				serverData.sin_port = htons(_dataPort);

				inet_pton(AF_INET, _bstr_t(_IP.c_str()), &serverData.sin_addr);

				_dataSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (_dataSock == INVALID_SOCKET) throw exception("Invalid Data Socket");

				// resolve host
				if (serverData.sin_addr.s_addr == INADDR_NONE) {

					host = gethostbyname(_bstr_t(_IP.c_str()));

					if (host == NULL) {
						sprintf_s(buf, "Cannot resolve '%s', winsock error: %d.",
							_IP.c_str(), WSAGetLastError());
						throw exception(buf);
					}
					CopyMemory(&serverData.sin_addr, host->h_addr, host->h_length);
				}

				if (connect(_dataSock, (struct sockaddr*)&serverData, sizeof(serverData)) == SOCKET_ERROR) {
					sprintf_s(buf, "Cannot connect to %s:%d, winsock error: %d.",
						inet_ntoa(serverData.sin_addr), ntohs(serverData.sin_port), WSAGetLastError());
					throw exception(buf);
				}


				/* We are online */
				Sleep(500);

				_active = true;

				_IQpairs = getIQpair();


				return _IQpairs;
			}
			throw exception("not connected to server");
		}
		catch (exception& ex)
		{
			MessageBoxA(NULL, ex.what(), "Error", 0);
			return 0;
		}

	}


	int receiveSDRPP(char *buff, size_t nbdata)
	{
		return (recv(_dataSock, buff, nbdata, 0));
	}


	void disconnect()
	{
		try
		{

			if (_active)
			{
				stopReception();
			}
			closesocket(_configSock);
			_connected = false;
			_partialConnect = false;
		}
		catch (const std::exception& ex)
		{
		}

	}
};

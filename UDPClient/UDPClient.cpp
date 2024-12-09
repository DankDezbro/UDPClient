#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <cmath>

#pragma comment (lib, "ws2_32.lib")

using namespace std;

#define PORTNUM 80
#define PACKETSIZE 1025 // 1KB + ACK
#define FILELEN 9319


/** declare variable wsa **/
WSADATA wsa;

/** declare socket variables **/
struct sockaddr_in si_other;
struct sockaddr_in si_ack;

int slen = sizeof(si_other);

SOCKET s;

u_long noBlock = 1;
u_long block = 0;

typedef struct {
	int seqNum;
	char* data;
}packet;

/** declare timer variables **/
LARGE_INTEGER current, frequency;

double pcFreq;
double pcPeriod;

u_int64 startACK = 0;
u_int64 elapsedACK = 0;
u_int64 maxDelay = 20000;

int initalizeWinsock()
{
	printf("\n****** INITIALIZING WINSOCK ***********");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		return 1;
	}
	else printf("\nWINSOCK INITIALIZED");

	return 0;
}

int initializeSocket(int portNum)
{
	/*****  CREATE CLIENT SOCKET  ****/
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		printf("Could not create socket : %d", WSAGetLastError());
		return 1;
	}
	else printf("\nUDP CLIENT SOCKET CREATED");

	/*****  INITIALIZE SOCKET STRUCT   - Non Blocking Client ****/
	ioctlsocket(s, FIONBIO, &noBlock);
	si_other.sin_addr.s_addr = inet_addr("127.0.0.1"); // or INADDR_ANY
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(portNum);
	return 0;
}

void initalizeTimer()
{
	/***GET CLOCK INFO ****************/
	printf("\n****** INITIALIZING TIMER ***********");

	QueryPerformanceFrequency(&frequency); //get frequnecy
	pcFreq = (double)(frequency.QuadPart); //frequency in Hz
	pcPeriod = 1 / (double)(frequency.QuadPart); // t = 1/f

	cout << "pcFreq = " << pcFreq << " Hz" << endl;
	cout << "pcPeriod = " << pcPeriod << " s" << endl;
	printf("\nTIMER INITIALIZED");
}

int main()
{

	/*****  INITALIZE TIMER  ****/
	initalizeTimer();

	/*****  INITALIZE WINSOCK  ****/
	initalizeWinsock();

	/*****  CREATE CLIENT SOCKET  ****/
	initializeSocket(PORTNUM);

	/**file variables **/
	unsigned long fileLen; //length of test.jpg
	FILE* fp; //pointer
	char* buffer; //char array to store test.jpg

	/***  OPEN IMAGE FILE AND COPY TO DATA STRUCTURE  ***/
	fp = fopen(".\\test.jpg", "rb");
	if (fp == NULL) {
		printf("\n Error Opening Image - read");
		fclose(fp);
		exit(0);
	}

	/*** ALLOCATE MEMORY (BUFFER) TO HOLD IMAGE *****/
	fseek(fp, 0, SEEK_END); //go to EOF
	fileLen = ftell(fp); //determine length
	fseek(fp, 0, SEEK_SET); //reset fp

	int numPackets = ceil((float)fileLen / PACKETSIZE); //number of packets that need to be sent after fragmenting into 1KB packets

	buffer = (char*)malloc(numPackets * PACKETSIZE);  //allocated memory
	if (!buffer) {
		printf("\n memory error allocating buffer");
		fclose(fp);
		return 1;
	}

	/*** ALLOCATE MEMORY (BUFFER) TO HOLD ACKS *****/
	char* ackBuff = (char*)malloc(15); //allocated memory

	/*********  READ FILE DATA INTO BUFFER AND CLOSE FILE  *************/
	fread(buffer, fileLen, 1, fp);
	fclose(fp);

	/*** FRAGMENT PAYLOAD (test.jpg) AND SEND TO SERVER*****/
	char* payloadFragment = (char*)malloc(PACKETSIZE);

	int numPacketsSent = 0;
	int count = 0;

	while (numPacketsSent < numPackets)
	{
		int ACK = 0;
		packet p;
		p.seqNum = numPacketsSent;
		p.data = payloadFragment;
		p.data[0] = p.seqNum;
		count++;

		for (int i = 1; i < PACKETSIZE;i++)
		{
			p.data[i] = buffer[(PACKETSIZE - 1) * numPacketsSent + i - 1];
		}

		if (sendto(s, payloadFragment, PACKETSIZE, 0, (struct sockaddr*)&si_other, slen) == SOCKET_ERROR)
		{
			printf("sendto() failed with error code : %d", WSAGetLastError());
			exit(EXIT_FAILURE);
		}
		else
		{
			cout << "\nsent " << numPacketsSent << " packets" << " out of " << numPackets - 1 << endl;
			
			/***Start Timer ****/
			QueryPerformanceCounter(&current);
			startACK = (unsigned int)current.QuadPart;

			/*** CHECK ACKs *****/
			while (true)
			{
				int recvlen = recvfrom(s, ackBuff, 1, 0, (struct sockaddr*)&si_other, &slen);
				int numACK = ackBuff[0];

				if (numACK == p.seqNum)
				{
					cout << "\n" << numACK << " ACK received" << endl;
					numPacketsSent++;
					break;
				}
				else if (elapsedACK < maxDelay)
				{
					/***Check Elapsed Time ****/
					QueryPerformanceCounter(&current);
					elapsedACK = (unsigned int)current.QuadPart - startACK;
				}
				else if (elapsedACK > maxDelay)
				{
					cout << "\n PACKET TIMED OUT" << endl;
					break;
				}
			}

		}

	}

	return 0;

}
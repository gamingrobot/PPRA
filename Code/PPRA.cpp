#include <iostream>
#include <conio.h>
#include <map>
#include <sstream>
#include <cassert>
#include <time.h>
#include <math.h>
#include <vector>
#include <string>

#define PI 3.14

/*#include <stdio.h>
#include <stdlib.h>
#include <windows.h>*/


#include "EmoStateDLL.h"
#include "edk.h"
#include "edkErrorCode.h"
#include "Socket.h"
#include "CognitivControl.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "../lib/edk.lib")

void sendCognitivAnimation(SocketClient& sock, EmoStateHandle eState);
void sendCognitiv(EmoStateHandle eState);
void handleCognitivEvent(std::ostream& os, EmoEngineEventHandle cognitivEvent);
void split(const std::string& input, std::vector<std::string>& tokens);
bool handleUserInput();
void promptUser();

//IMPORTANT FUNCTION PROTOTYPES with descriptions
HANDLE openPort(int);//Serial protocol stuff, don't worry about it
HANDLE comPort;//Serial protocol stuff, don't worry about it
void put(int servo, int angle);//Moves servo (0-7) to angle (500-5500)
void servoOff();//Stops signal to all servos, analog servos will go slack, digital servos wont!
void neutral();//Sends all servos to halfway between max and min
void waitMS(int ms);//Milisecond delay function

int step=50;//Decrease steop size for finer control
int angles[8]={3250,3000,2400,2600,1000,3000,3000,3000};
int center[8]={3250,3000,2400,2600,1000,3000,3000,3000};
int minS[8]={1200,1750,1200,800,900,0,0,0};
int maxS[8]={5200,4400,4600,4200,1800,5000,5000,5000};
int done=0;
int powerThreshold = 20;


int portS = 3;
bool paused = true;


int main(int argc, char** argv) {

	comPort=openPort(portS);//***PUT YOUR COM PORT NUMBER HERE!***

	// location of the machine running the 3D motion cube
	std::string receiverHost = "localhost";
	
	if (argc > 2) {
		std::cout << "Usage: " << argv[0] << " <hostname>" << std::endl;
		std::cout << "The arguments specify the host of the motion cube (Default: localhost)" << std::endl;
		return 1;
	}

	if (argc > 1) {
		receiverHost = std::string(argv[1]);
	}

	EmoEngineEventHandle eEvent	= EE_EmoEngineEventCreate();
	EmoStateHandle eState		= EE_EmoStateCreate();
	unsigned int userID			= 0;
	
	try {

//		if (EE_EngineConnect() != EDK_OK) {
		if (EE_EngineRemoteConnect("127.0.0.1", 3008) != EDK_OK) {
			throw std::exception("Emotiv Engine start up failed.");
		}
		else {
			std::cout << "Emotiv Engine started!" << std::endl;
			
				neutral();//send all servos to their neutral positions
		}

		int startSendPort = 6868;
		std::map<unsigned int, SocketClient> socketMap;
		promptUser();
		
		while (true) {
			
			// Handle the user input
			if (_kbhit()) {
				if (!handleUserInput()) {
					break;
				}
			}

				
			if(paused == true){
				int state = EE_EngineGetNextEvent(eEvent);

				// New event needs to be handled
				if (state == EDK_OK) {

					EE_Event_t eventType = EE_EmoEngineEventGetType(eEvent);
					EE_EmoEngineEventGetUserId(eEvent, &userID);

					switch (eventType) {

						// New headset connected, create a new socket to send the animation
						case EE_UserAdded:
						{
							std::cout << std::endl << "New user " << userID << " added, sending Cognitiv animation to ";
							std::cout << receiverHost << ":" << startSendPort << "..." << std::endl;
							promptUser();

							socketMap.insert(std::pair<unsigned int, SocketClient>(
								userID, SocketClient(receiverHost, startSendPort, UDP)));
							
							startSendPort++;
							break;
						}
					
						// Headset disconnected, remove the existing socket
						case EE_UserRemoved:
						{
							std::cout << std::endl << "User " << userID << " has been removed." << std::endl;
							promptUser();

							std::map<unsigned int, SocketClient>::iterator iter;
							iter = socketMap.find(userID);
							if (iter != socketMap.end()) {
								socketMap.erase(iter);
							}
							break;
						}
						
						// Send the Cognitiv animation if EmoState has been updated
						case EE_EmoStateUpdated:
						{
							//std::cout << "New EmoState from user " << userID << "..." << std::endl;
							EE_EmoEngineEventGetEmoState(eEvent, eState);

							std::map<unsigned int, SocketClient>::iterator iter;
							iter = socketMap.find(userID);
							if (iter != socketMap.end()) {
								sendCognitiv(eState);
							}
							break;
						}

						// Handle Cognitiv training related event
						case EE_CognitivEvent:
						{
							handleCognitivEvent(std::cout, eEvent);
							break;
						}

						default:
							break;
					}
				}
				else if (state != EDK_NO_EVENT) {
					std::cout << "Internal error in Emotiv Engine!" << std::endl;
					break;
				}
			}
			Sleep(1);
		}
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		std::cout << "Press any keys to exit..." << std::endl;
		getchar();
	}

	EE_EngineDisconnect();
	EE_EmoStateFree(eState);
	EE_EmoEngineEventFree(eEvent);

	return 0;
}


void sendCognitivAnimation(SocketClient& sock, EmoStateHandle eState) {

	std::ostringstream os;

	EE_CognitivAction_t actionType	= ES_CognitivGetCurrentAction(eState);
	float				actionPower = ES_CognitivGetCurrentActionPower(eState);
	int					intType = static_cast<int>(actionType);
	int					intPower = static_cast<int>(actionPower*100.0f);

	os << intType << "," << intPower;
	std::cout << os.str()<< std::endl;

	sock.SendBytes(os.str());
}

void sendCognitiv(EmoStateHandle eState) {
	int i;
	std::ostringstream os;

	EE_CognitivAction_t actionType	= ES_CognitivGetCurrentAction(eState);
	float				actionPower = ES_CognitivGetCurrentActionPower(eState);
	int					intType = static_cast<int>(actionType);
	int					intPower = static_cast<int>(actionPower*100.0f);


	/*servo config*/
	int Base = 0;
	int Bicep = 1;
	int Elbow = 2;
	int Wrist = 3;
	int Gripper = 4;

	/*intType*/
	int Neutral = 1;
	int Push = 2;
	int Pull = 4;
	int Lift = 8;
	int Drop = 16;
	int Left = 32;
	int Right = 64;
	int RotateLeft = 128;
	int RotateRight = 256;
	int RotateClockwise = 512;
	int RotateCounterClockwise = 1024;
	int RotateForward = 2048;
	int RotateReverse = 4096;
	int Disappear = 8192;

		/*increse the values as that is called*/
	/*only if power is above threshold will it increment, prevents little querks*/
	if(intPower >= powerThreshold){
		if(intType == Right){
			i=0;
			if(angles[i]<=(maxS[i]-step)) angles[i]+=step;
			put(i,angles[i]);
			//doInverse();
		}
		if(intType == Left){
			i=0;
			if(angles[i]>=(minS[i]+step)) angles[i]-=step;
			put(i,angles[i]);
			//doInverse();
		}
		if(intType == Pull){
			i=1;
			if(angles[i]<=(maxS[i]-step)) angles[i]+=step;
			put(i,angles[i]);
			//doInverse();
		}
		if(intType == Push){
			i=1;
			if(angles[i]>=(minS[i]+step)) angles[i]-=step;
			put(i,angles[i]);
			//doInverse();
		}
		if(intType == Lift){
			i=2;
			if(angles[i]<=(maxS[i]-step)) angles[i]+=step;
			put(i,angles[i]);
			//doInverse();
		}
		if(intType == Drop){
			i=2;
			if(angles[i]>=(minS[i]+step)) angles[i]-=step;
			put(i,angles[i]);
			//doInverse();
		}
		if(intType == RotateRight){ 
			i=3;
			if(angles[i]<=(maxS[i]-step)) angles[i]+=step;
			put(i,angles[i]);
		}
		if(intType == RotateLeft){ 
			i=3;
			if(angles[i]>=(minS[i]+step)) angles[i]-=step;
			put(i,angles[i]);
		}
	}


	os << intType << "," << intPower;
	std::cout << os.str()<< std::endl;

}



void handleCognitivEvent(std::ostream& os, EmoEngineEventHandle cognitivEvent) {

	unsigned int userID = 0;
	EE_EmoEngineEventGetUserId(cognitivEvent, &userID);
	EE_CognitivEvent_t eventType = EE_CognitivEventGetType(cognitivEvent);


	switch (eventType) {

		case EE_CognitivTrainingStarted:
		{
			os << std::endl << "Cognitiv training for user " << userID << " STARTED!" << std::endl;
			break;
		}

		case EE_CognitivTrainingSucceeded:
		{
			os << std::endl << "Cognitiv training for user " << userID << " SUCCEEDED!" << std::endl;
			break;
		}

		case EE_CognitivTrainingFailed:
		{
			os << std::endl << "Cognitiv training for user " << userID << " FAILED!" << std::endl;
			break;
		}

		case EE_CognitivTrainingCompleted:
		{
			os << std::endl << "Cognitiv training for user " << userID << " COMPLETED!" << std::endl;
			break;
		}

		case EE_CognitivTrainingDataErased:
		{
			os << std::endl << "Cognitiv training data for user " << userID << " ERASED!" << std::endl;
			break;
		}

		case EE_CognitivTrainingRejected:
		{
			os << std::endl << "Cognitiv training for user " << userID << " REJECTED!" << std::endl;
			break;
		}

		case EE_CognitivTrainingReset:
		{
			os << std::endl << "Cognitiv training for user " << userID << " RESET!" << std::endl;
			break;
		}

		case EE_CognitivAutoSamplingNeutralCompleted:
		{
			os << std::endl << "Cognitiv auto sampling neutral for user " << userID << " COMPLETED!" << std::endl;
			break;
		}

		case EE_CognitivSignatureUpdated:
		{
			os << std::endl << "Cognitiv signature for user " << userID << " UPDATED!" << std::endl;
			break;
		}

		case EE_CognitivNoEvent:
			break;

		default:
			//@@ unhandled case
			assert(0);
			break;
	}
    promptUser();
}


bool handleUserInput() {

	static std::string inputBuffer;

	char c = _getch();

	if (c == '\r') {
		std::cout << std::endl;
		std::string command;

		const size_t len = inputBuffer.length();
		command.reserve(len);

		// Convert the input to lower case first
		for (size_t i=0; i < len; i++) {
			command.append(1, tolower(inputBuffer.at(i)));
		}

		inputBuffer.clear();

		bool success = parseCommand(command, std::cout);
        promptUser();
		return success;
	}
	else if (c == '~'){
		
		if(paused == false){
			std::cout << "UNPAUSED" << std::endl;
			paused = true;
		}
		else if(paused == true){
			std::cout << "PAUSED press ~ to unpause" << std::endl;
			paused = false;
		}		
		promptUser();
		return true;

	}
	else {
		if (c == '\b') { // Backspace key
			if (inputBuffer.length()) {
				putchar(c);
				putchar(' ');
				putchar(c);
				inputBuffer.erase(inputBuffer.end()-1);
			}
		}
		else {
			inputBuffer.append(1,c);
			std::cout << c;
		}
	}	

	return true;
}

void promptUser()
{
	std::cout << "PPRA> ";
}

//servo methods
void neutral(){
	int i;
	for(i=0;i<8;i++){
		put(i, center[i]);
	}
	printf("\n");
}

void put(int servo, int angle){
	unsigned char buff[6];
	DWORD len;

	unsigned short int temp;
	unsigned char pos_hi,pos_low;
	
	temp=angle&0x1f80;
	pos_hi=temp>>7;
	pos_low=angle & 0x7f;

	buff[0]=0x80;//start byte
	buff[1]=0x01;//device id
	buff[2]=0x04;//command number
	buff[3]=servo;//servo number
	buff[4]=pos_hi;//data1
	buff[5]=pos_low;//data2
	WriteFile(comPort, &buff, 6, &len, 0);

	printf("Servo %d Set to %d\n", servo, angle);
}

void servoOff(){
	unsigned char buff[6];
	DWORD len;
	int servo;

	buff[0]=0x80;//start byte
	buff[1]=0x01;//device id
	buff[2]=0x00;//command number
	buff[4]=0x0f;//data1
	for(servo=0;servo<8;servo++) {
		buff[3]=servo;//servo number
		WriteFile(comPort, &buff, 5, &len, 0);
		}
}

void waitMS(int ms){
	clock_t endwait;
	endwait=clock()+ms*CLOCKS_PER_SEC/1000;
	while(clock()<endwait);
}

HANDLE openPort(int portnum){
	char port[10]="com";//ask for com port
	char pnum[]="Error";
	itoa(portnum,pnum,10);
	strcat(port,pnum);
	//printf(port);
	
	HANDLE serial=CreateFile(port,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
	if(serial==INVALID_HANDLE_VALUE){
		if(GetLastError()==ERROR_FILE_NOT_FOUND){
			printf("Error, %s Not Found\n", port);
			done=1;
			return serial;
		}
		printf("Com Error\n");
		done=1;
		return serial;
	}

	DCB dcbSerialParams={0};
	dcbSerialParams.DCBlength=sizeof(dcbSerialParams);

	if(!GetCommState(serial, &dcbSerialParams)){
		printf("Com State Error\n");
		done=1;
		return serial;
	}

	dcbSerialParams.BaudRate=CBR_19200;//CBR_baudrate
	dcbSerialParams.ByteSize=8;
	dcbSerialParams.Parity=NOPARITY;//NOPARITY, ODDPARITY, EVENPARITY
	dcbSerialParams.StopBits=ONESTOPBIT;//ONESTOPBIT, ONE5STOPBITS, TWOSTOPBITS

	if(!SetCommState(serial, &dcbSerialParams)){
		printf("Serial Protocol Error\n");
		done=1;
		return serial;
	}
	
	COMMTIMEOUTS timeouts={0};
	timeouts.ReadIntervalTimeout=50;
	timeouts.ReadTotalTimeoutConstant=50;
	timeouts.ReadTotalTimeoutMultiplier=10;
	timeouts.WriteTotalTimeoutConstant=50;
	timeouts.WriteTotalTimeoutMultiplier=10;

	if(!SetCommTimeouts(serial,&timeouts)){
		printf("Timeout Setting Error\n");
		done=1;
		return serial;
	}
	
	return serial;
}


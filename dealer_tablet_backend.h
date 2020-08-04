//.............header files..................
#ifndef DEALER_TABLET_BACKEND_H
#define DEALER_TABLET_BACKEND_H
	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>
	#include <inttypes.h>
	#include <time.h>
	#include <stdbool.h>
	#ifdef WIN32
	 #include <windows.h>
	#else
	 #include <pthread.h>
	 #include <unistd.h> // for usleep
	#endif
	//for extraction of MAC and IP , TCP communication to send the packets to the server
	#ifndef WIN32
	#include <poll.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/ioctl.h>
	#include <netinet/in.h>
	#include <net/if.h>
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <errno.h>
	#endif // !WIN32
//............................................
//............defines.........................
	#define SERVER_PORT 7600
	#define DATALOGGER_PORT 2424
	#define PORT_FOR_LOCK_AND_UNLOCK 7620
	#define IPADD "10.66.0.1" //"10.66.0.1"// IP address to send the event on the server when jackpot_confirmButton_pressed occured
	#define SA struct sockaddr

//	#define SEND_CHANNEL "dealer_frontend"// send channel name
//	#define RECEIVE_CHANNEL "dealer_backEndChannel"//receive channel name
	#define SEND_CHANNEL "tcp://localhost:50000"
	#define RECEIVE_CHANNEL "tcp://localhost:50001"

	#define SNOOZE_TIME 80//sleep time 
	#define MAX 300
	#define VERSION "2"//version number of dealer tablet
	#define ACTION "2"//action 
	#define PENDING_JACKPOT_EVENT "pendingJackpotEvent"//name of event occured on GUI
	#define ZERO "0" 
	#define BOTH "Both"
	#define ETH0 "eth0"
	#define LOCKBETS "lockBets"//name of event occured on GUI
	#define RESPONSE "99"//response from the client to server for lock and unlock event
	#define PROTOCOL_VERSION "2"
	#define ACTION_CODE "0"// for dealerpad init event 
	#define ACTION_CODE_FOR_DEALERPAD_CONFIG "3"// for dealerpad configuration
	#define HANDIDMATRIXFORMAT "1u1 matrixIndex 1u1 D1_H1_ID 1u1 D2_H1_ID 1u1 Both_H1_ID 1u1 D1_H2_ID 1u1 D2_H2_ID 1u1 Both_H2_ID 1u1 D1_H3_ID 1u1 D2_H3_ID 1u1 Both_H3_ID"//for setHandIDMatrix
	#define HANDIDMATRIXEVENT "setHandIDMatrix"//for event name
	#define PAYTABLEID_EVENT "setPayTableID"
	#define PAYTABLEID_FORMAT "1u1 payTableIndex 1u1 ID"
	#define DENOM_NAME_EVENT "setDenomName"
	#define DENOM_NAME_FORMAT "1u1 denomIndex 1s0 name"
	#define LOCK_BETS__IPADDR_EVENT "setLockBetsIPAddr"
	#define LOCK_BETS__IPADDR_FORMAT  "1s0 ipAddr"
	#define TABLEID_EVENT "setTableID"
	#define TABLEID_FORMAT "1u1 tableID"
	#define PAY_TABLE_NAME_EVENT "setPayTableName"
	#define PAY_TABLE_NAME_FORMAT "1u1 payTableIndex 1s0 name"
	#define HANDNAME_EVENT "setHandName"
	#define HANDNAME_FORMAT "1u1 handIndex 1s0 name"
	#define BET_SPOTID_EVENT "setBetSpotID"
	#define BET_SPOTID_FORMAT "1u1 betSpotIndex 1u1 ID"
	#define SET_STYLE_EVENT "setStyle"
	#define SET_STYLE_FORMAT "1u1 style"
//............................................

//............structures........................
	typedef struct
	{
	        uint8_t tableID;
	        uint8_t dealerID;
	        uint8_t playerID;
	        uint8_t betSpotID;
	        uint8_t payTableID;
	        uint8_t handID;
	}event_data_type;//structure to be received from the Stroyboard IO channel
//...............................................
//..............functions........................
void getIPAddr();
void getMacAddr();
bool compareEvents(struct sockaddr_in *);
int SendDataToServer(int);
void openSocketAndSendDataToServer();
void *receive_thread(void*);
void *lockAndUnlock(void*);
void sleep_ms(int);
void *initDealerPad(void *);

int main(void);

#endif //DEALER_TABLET_BACKEND_H

//......................................................


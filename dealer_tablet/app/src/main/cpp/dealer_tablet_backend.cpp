/*****************************************Header***********************************************************************/
#include <android/log.h>
#include"dealer_tablet_backend.h"
extern "C" {
#include <dlfcn.h>
#include <errno.h>
#include "gre/greio.h"
}

#define TAG   "DealerBackend"

//............variables.......................
static int dataChanged = 1; //Default to 1 so we send data to the ui once it connects
#ifdef WIN32
static CRITICAL_SECTION lock;
	static HANDLE thread1;
#else
static pthread_mutex_t lock;
static pthread_t thread1;
#endif

char MacBuff[20]; //to collect the Mac of device
char IPBuff[20]; //to collect the ip address of the device
char send_to_server_buffer[MAX];//buffer that contains the data to be sent during jackpot_confirmButton_pressed event on gui
char *ipAddr;//received ip address from the front end GUI when lockBet event occured
char *Event_Name;//event name
int lockFlag=0;
char lock_unlock_response[MAX]; //to collect the response from the server while lock and unlock event
char initDealerPadBuff[MAX];
char dealerPadConfigBuff[MAX];


typedef gre_io_t* (*gre_io_open_t)(const char *io_name, int flag, ...);
gre_io_open_t gre_io_open_api;
typedef int (*gre_io_unserialize_t)(gre_io_serialized_data_t *buffer,
					   char **event_target,
					   char **event_name,
					   char **event_format,
					   void **event_data);

gre_io_unserialize_t gre_io_unserialize_api;

typedef int (*gre_io_receive_t)(gre_io_t *handle, gre_io_serialized_data_t **buffer);
gre_io_receive_t gre_io_receive_api;

typedef gre_io_serialized_data_t * (*gre_io_size_buffer_t)(gre_io_serialized_data_t *buffer, int nbytes);
gre_io_size_buffer_t gre_io_size_buffer_api;

typedef void (*gre_io_zero_buffer_t)(gre_io_serialized_data_t *buffer);
gre_io_zero_buffer_t gre_io_zero_buffer_api;

typedef gre_io_serialized_data_t * (*gre_io_serialize_t)(gre_io_serialized_data_t *buffer,
                                                         const char *event_target,
                                                         const char *event_name,
                                                         const char *event_format,
                                                         const void *event_data, int event_nbytes);
gre_io_serialize_t gre_io_serialize_api;

typedef int (*gre_io_send_t)(gre_io_t *handle, gre_io_serialized_data_t *buffer);
gre_io_send_t gre_io_send_api;

#define gre_io_open gre_io_open_api
#define gre_io_unserialize gre_io_unserialize_api
#define gre_io_receive gre_io_receive_api
#define gre_io_size_buffer gre_io_size_buffer_api
#define gre_io_zero_buffer gre_io_zero_buffer_api
#define gre_io_serialize gre_io_serialize_api
#define gre_io_send gre_io_send_api
//..............................................
/***********************************************************************************************************************
* module             : sleep_ms(int)
* return type        : void
* Description        : Function, for generating sleep
* param              : int
* Author             : CHETU
***********************************************************************************************************************/
void sleep_ms(int milliseconds)
{
	#ifdef WIN32
	        Sleep(milliseconds);
	#else
	        usleep(milliseconds * 1000);
	#endif
}

/***********************************************************************************************************************
* module             : getMacAddr(void)
* return type        : void
* Description        : Function, for extracting a MAC address of boundary board device
* param              : void
* Author             : CHETU
***********************************************************************************************************************/
void getMacAddr()
{
	int fileDescriptor =-1;
	struct ifreq ifr ={0,};

	fileDescriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if( fileDescriptor == -1)
	{
                __android_log_print(ANDROID_LOG_DEBUG, TAG, "socket creation failed... %d", errno);
                exit(0);
        }

	ifr.ifr_addr.sa_family = AF_INET;
 	strncpy(ifr.ifr_name, ETH0, IFNAMSIZ - 1);

	if (ioctl( fileDescriptor, SIOCGIFHWADDR, &ifr) < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, TAG, "SIOCGIFHWADDR failed... %d", errno);
	}
	close( fileDescriptor);
	sprintf(MacBuff, "%.2x%.2x%.2x%.2x%.2x%.2x",
	(unsigned char)ifr.ifr_hwaddr.sa_data[0],
	(unsigned char)ifr.ifr_hwaddr.sa_data[1],
	(unsigned char)ifr.ifr_hwaddr.sa_data[2],
	(unsigned char)ifr.ifr_hwaddr.sa_data[3],
	(unsigned char)ifr.ifr_hwaddr.sa_data[4],
	(unsigned char)ifr.ifr_hwaddr.sa_data[5]);

	__android_log_print(ANDROID_LOG_DEBUG, TAG, "MAC Address = %s", MacBuff);
}

/***********************************************************************************************************************
* module             : getIPAddr(void)
* return type        : void
* Description        : Function, for extracting a IP address of boundary board device
* param              : void
* Author             : CHETU
***********************************************************************************************************************/
void getIPAddr()
{
	int  fileDescriptor = -1;
	struct ifreq ifr = {0,};

	/*AF_INET - to define network interface IPv4*/
	/*Creating soket for it.*/
	fileDescriptor = socket(AF_INET, SOCK_DGRAM,0);
	if( fileDescriptor == -1)
	{
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "socket creation failed...%d", errno);
		exit(0);
	}
	/*AF_INET - to define IPv4 Address type.*/
	ifr.ifr_addr.sa_family = AF_INET;

	/*eth0 - define the ifr_name - port name
	where network attached.*/
	memcpy(ifr.ifr_name, ETH0, IFNAMSIZ - 1);

	/*Accessing network interface information by
	passing address using ioctl.*/
	if (ioctl( fileDescriptor, SIOCGIFADDR, &ifr) < 0) {
	    __android_log_print(ANDROID_LOG_DEBUG, TAG, "SIOCGIFADDR failed...%d", errno);
	}

	/*closing fd*/
	close( fileDescriptor);

	/*Extract IP Address*/
	strcpy(IPBuff, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    __android_log_print(ANDROID_LOG_DEBUG, TAG, "IP Address %s", IPBuff);
}

/***********************************************************************************************************************
* module             : compareEvents(struct sockaddr_in *)
* return type        : bool
* Description        : Function, to assign port after comparing the events
* param              : struct sockaddr_in *
* Author             : CHETU
***********************************************************************************************************************/
bool compareEvents(struct sockaddr_in *servaddr)
{
        if(strcmp(Event_Name, PENDING_JACKPOT_EVENT) == 0)
        {
                servaddr->sin_addr.s_addr = inet_addr(IPADD);//for testing purpose
                //servaddr.sin_addr.s_addr = inet_addr(ipAddr);
                servaddr->sin_port = htons(SERVER_PORT);
		return true;
        }
        else if(strcmp(Event_Name, LOCKBETS) == 0)
        {
                // servaddr.sin_addr.s_addr = inet_addr(IPADD);//for testing purpose
                servaddr->sin_addr.s_addr = inet_addr(ipAddr);
                servaddr->sin_port = htons(DATALOGGER_PORT);
		return true;
        }
        else
        {
                __android_log_print(ANDROID_LOG_DEBUG, TAG, "unknown event received can't assign port\n");
                return false;
        }
}

/***********************************************************************************************************************
* module             : SendDataToServer(int)
* return type        : int
* Description        : Function, to send the data to BJS Server according to the received events
* param              : int
* Author             : CHETU
***********************************************************************************************************************/
int SendDataToServer(int sockfd)
{
         int akw=0;
	 char lockBetChar[]="b";
	 if(strcmp(Event_Name, PENDING_JACKPOT_EVENT) == 0)
        {
                __android_log_print(ANDROID_LOG_DEBUG, TAG, "data from client : %s\n ",send_to_server_buffer);
                akw= write(sockfd, send_to_server_buffer, sizeof(send_to_server_buffer));
	        __android_log_print(ANDROID_LOG_DEBUG, TAG, "send number of bytes to server= %d\n",akw);
		return akw;
        }
        else if(strcmp(Event_Name,LOCKBETS)== 0)
        {
                __android_log_print(ANDROID_LOG_DEBUG, TAG, "data from client : %s \n",lockBetChar);
                akw = write(sockfd,lockBetChar, sizeof(lockBetChar));
                __android_log_print(ANDROID_LOG_DEBUG, TAG, "send number of bytes to server= %d\n",akw);
		return akw;
        }
	else
	{
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "unknown event to send to server returning....\n");
		return 0;
	}
}
/***********************************************************************************************************************
* module             : send_data_to_channel(gre_io_t *,gre_io_serialized_data_t *)
* return type        : int 
* Description        : Function, to send data to Storyboard IO channel
* param              : gre_io_t *,gre_io_serialized_data_t *
* Author             : CHETU
***********************************************************************************************************************/ 
int send_data_to_channel(gre_io_t *send_handle,gre_io_serialized_data_t *md_buffer)
{
        int akw = gre_io_send(send_handle, md_buffer);
        return akw;
}
/***********************************************************************************************************************
* module             : configureDealerPad(void)
* return type        : void*
* Description        : Function, to configure delaerpad 
* param              : void
* Author             : CHETU
***********************************************************************************************************************/
void * configureDealerPad(void * agrs)
{
	int sockfd = 0;
	int akw =0;
	struct sockaddr_in serv_addr; 
	char buffer[MAX] = {0};
	int count =0;
	int Style =0;
	int Table_ID =0;
	int PT1_ID =0, PT2_ID =0;
	char DL_IP[20] ={0};
	char PT1_Name[50]={0},PT2_Name[50]={0};
	int Betspot_ID1=0,Betspot_ID2=0;
	char Denom1_name1[50]={0},Denom2_name1[50]={0},Hand1_name1[50]={0};
	char Denom1_name2[50]={0},Denom2_name2[50]={0},Hand1_name2[50]={0};
	int D1_H1_Hand_ID1=0, D2_H1_Hand_ID1=0, D3_H1_Hand_ID1=0;
	int D1_H1_Hand_ID2=0, D2_H1_Hand_ID2=0, D3_H1_Hand_ID2=0;
	char Hand2_name1[50]={0}, Hand2_name2[50]={0};
	int D1_H2_Hand_ID1=0, D2_H2_Hand_ID1=0, D3_H2_Hand_ID1=0;
	int D1_H2_Hand_ID2=0, D2_H2_Hand_ID2=0, D3_H2_Hand_ID2=0;
	char Hand3_name1[50]={0},Hand3_name2[50]={0};
	int D1_H3_Hand_ID1=0,	D2_H3_Hand_ID1=0, D3_H3_Hand_ID1=0;
	int D1_H3_Hand_ID2=0,	D2_H3_Hand_ID2=0, D3_H3_Hand_ID2=0;
	char *tok=NULL;

	typedef struct table_ID
	{
	   uint8_t tableID;
	}table_ID_var;

	typedef struct payTableName
	{
	  uint8_t payTableIndex;
	  char name[50];
	}payTableNameVariable;

	typedef struct payTableID
	{
	  uint8_t payTableIndex;
	  uint8_t ID;
	}payTableIDVariable;

	typedef struct betSpotID
	{
	 uint8_t betSpotIndex;
	 uint8_t ID;
	}betSpotIdVariable;

	typedef struct denomName
	{
	  uint8_t denomIndex;
	  char name[50];
	}denomNameVariable;

	typedef struct handName
	{
	  uint8_t handIndex;
	  char name[50];
	}handNameVariable;

	typedef struct handIdMatrix
	{
	  uint8_t matrixIndex,D1_H1_ID,D2_H1_ID,Both_H1_ID;
          uint8_t D1_H2_ID,D2_H2_ID,Both_H2_ID;
          uint8_t D1_H3_ID,D2_H3_ID,Both_H3_ID;
	}handIdMatrixVariable;

	typedef struct lockBetsIPAddr
	{
	  char ipAddr[20];
	}lockBetsIPAddrVariable;
	
	typedef struct setStyle
	{
	 uint8_t style;
	}setStyleVariable;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "\n Socket creation error \n");
		return NULL;
	}
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "socket for configure dealerpad created \n ");
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERVER_PORT);
	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, IPADD, &serv_addr.sin_addr)<=0)
	{
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "\nInvalid address/ Address not supported \n");
		return NULL;
	}

	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "\nConnection Failed \n");
		return NULL;
	}
	send(sockfd,dealerPadConfigBuff,strlen(dealerPadConfigBuff),0);
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "dealerPad configuration structure : %s send to the server\n",dealerPadConfigBuff);
	recv(sockfd,buffer,sizeof(buffer),0);
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "From Server for resquestConfigure : %s \n",buffer);


	tok =strtok(buffer,",");
	if(tok==NULL)
	{
	    return NULL;
	}
	while(tok!=NULL)
	{
	   switch(count)
	   {
		case 0: if(strncmp("92",tok,2)==0)
			{
			    __android_log_print(ANDROID_LOG_DEBUG, TAG, "server does not have any configuration data \n");
			    close(sockfd);
			    return NULL;
			}
			else
			{
			    strncpy(DL_IP,tok,strlen(tok)+1);
			    break;
			}
		case 1: sscanf(tok,"%d",&Style);
			break;
		case 2: sscanf(tok,"%d",&Table_ID);
			break;
		case 3: sscanf(tok,"%d",&PT1_ID);
			break;
		case 4: strncpy(PT1_Name,tok,strlen(tok)+1);
			break;
		case 5: sscanf(tok,"%d",&Betspot_ID1);
			break;
		case 6: strncpy(Denom1_name1,tok,strlen(tok)+1);
			//sscanf(tok,"%s",Denom1_name1);
			break;
	        case 7:  strncpy(Denom2_name1,tok,strlen(tok)+1);
			//sscanf(tok,"%s",Denom2_name1);
	                break;
		case 8: strncpy(Hand1_name1,tok,strlen(tok)+1);
			//sscanf(tok,"%s",Hand1_name1);
	                break;
		case 9: sscanf(tok,"%d",&D1_H1_Hand_ID1);
	                break;
		case 10: sscanf(tok,"%d",&D2_H1_Hand_ID1);
	                break;
		case 11: sscanf(tok,"%d",&D3_H1_Hand_ID1);
	                break;
	        case 12:strncpy(Hand2_name1,tok,strlen(tok)+1); 
			//sscanf(tok,"%s",Hand2_name1);
	                break;
	        case 13: sscanf(tok,"%d",&D1_H2_Hand_ID1);
	                break;
	        case 14: sscanf(tok,"%d",&D2_H2_Hand_ID1);
	                break;
	        case 15: sscanf(tok,"%d",&D3_H2_Hand_ID1);
	                break;
	        case 16: strncpy(Hand3_name1,tok,strlen(tok)+1); 
			//sscanf(tok,"%s",Hand3_name1);
	                break;
	        case 17: sscanf(tok,"%d",&D1_H3_Hand_ID1);
	                break;
	        case 18: sscanf(tok,"%d",&D2_H3_Hand_ID1);
	                break;
	        case 19: sscanf(tok,"%d",&D3_H3_Hand_ID1);
	                break;
		case 20: sscanf(tok,"%d",&PT2_ID);
			break;
		case 21: strncpy(PT2_Name,tok,strlen(tok)+1);
			break;
		case 22: sscanf(tok,"%d",&Betspot_ID2);
			break;
		case 23:strncpy(Denom1_name2,tok,strlen(tok)+1); 
			//sscanf(tok,"%s",Denom1_name2);
			break;
	        case 24:strncpy(Denom2_name2,tok,strlen(tok)+1);
			// sscanf(tok,"%s",Denom2_name2);
	                break;
		case 25: strncpy(Hand1_name2,tok,strlen(tok)+1); 
			//sscanf(tok,"%s",Hand1_name2);
	                break;
		case 26: sscanf(tok,"%d",&D1_H1_Hand_ID2);
	                break;
		case 27: sscanf(tok,"%d",&D2_H1_Hand_ID2);
	                break;
		case 28: sscanf(tok,"%d",&D3_H1_Hand_ID2);
	                break;
	        case 29: strncpy(Hand2_name2,tok,strlen(tok)+1); 
			//sscanf(tok,"%s",Hand2_name2);
	                break;
	        case 30: sscanf(tok,"%d",&D1_H2_Hand_ID2);
	                break;
	        case 31: sscanf(tok,"%d",&D2_H2_Hand_ID2);
	                break;
	        case 32: sscanf(tok,"%d",&D3_H2_Hand_ID2);
	                break;
	        case 33: strncpy(Hand3_name2,tok,strlen(tok)+1); 
			//sscanf(tok,"%s",Hand3_name2);
	                break;
	        case 34: sscanf(tok,"%d",&D1_H3_Hand_ID2);
	                break;
	        case 35: sscanf(tok,"%d",&D2_H3_Hand_ID2);
	                break;
	        case 36: sscanf(tok,"%d",&D3_H3_Hand_ID2);
	                break;
	  }
	     count++;
	     tok=strtok(NULL,",");
	}
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "%s %d %d %d %s %d %s %s %s %d %d %d %s %d %d %d %s %d %d %d %d %s %d %s %s %s %d %d %d %s %d %d %d %s %d %d %d \n",DL_IP,Style,Table_ID,PT1_ID,PT1_Name,
									Betspot_ID1,Denom1_name1,Denom2_name1,Hand1_name1,D1_H1_Hand_ID1,
									D2_H1_Hand_ID1,D3_H1_Hand_ID1,Hand2_name1,D1_H2_Hand_ID1,
									D2_H2_Hand_ID1,D3_H2_Hand_ID1,Hand3_name1,D1_H3_Hand_ID1,
									D2_H3_Hand_ID1,D3_H3_Hand_ID1,PT2_ID,PT2_Name,
									Betspot_ID2,Denom1_name2,Denom2_name2,Hand1_name2,D1_H1_Hand_ID2,
									D2_H1_Hand_ID2,D3_H1_Hand_ID2,Hand2_name2,D1_H2_Hand_ID2,
									D2_H2_Hand_ID2,D3_H2_Hand_ID2,Hand3_name2,D1_H3_Hand_ID2,
									D2_H3_Hand_ID2,D3_H3_Hand_ID2);
	gre_io_serialized_data_t *send_buffer = NULL;
	gre_io_t *send_handle=NULL;

	__android_log_print(ANDROID_LOG_DEBUG, TAG, "Trying to open the connection to the frontend\n");
	while (1)
	{
		// Connect to a channel to send messages (write)
		sleep_ms(SNOOZE_TIME);
		send_handle = gre_io_open(SEND_CHANNEL, GRE_IO_TYPE_WRONLY);
		if (send_handle != NULL)
		{
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Send channel: %s successfully opened\n",SEND_CHANNEL);
			break;
		}
	}
//********************************************************** setStyle ***********************************************************************************************//
                setStyleVariable setStyleVar;
                setStyleVar.style =Style;
                send_buffer = gre_io_serialize(send_buffer, NULL,SET_STYLE_EVENT,SET_STYLE_FORMAT,&setStyleVar,sizeof(setStyleVar));
                akw = send_data_to_channel(send_handle,send_buffer);

                __android_log_print(ANDROID_LOG_DEBUG, TAG,"setStyle event send to IO channel %d\n",akw);
                gre_io_zero_buffer(send_buffer);
				
//**************************************** setLockBetsIPAddr ***************************************************************************************//
				lockBetsIPAddrVariable lockBetsIPAddrVar;
				strcpy(lockBetsIPAddrVar.ipAddr,DL_IP);
				send_buffer =  gre_io_serialize(send_buffer, NULL,LOCK_BETS__IPADDR_EVENT,LOCK_BETS__IPADDR_FORMAT,&lockBetsIPAddrVar,sizeof(lockBetsIPAddrVar));
				akw = send_data_to_channel(send_handle,send_buffer);
				
				__android_log_print(ANDROID_LOG_DEBUG, TAG, "setLockBetsIPAddr event send to IO channel %d\n",akw);
				gre_io_zero_buffer(send_buffer);

//****************************************** setTableID ********************************************************************************************//
				table_ID_var table_ID_varData;
				table_ID_varData.tableID =Table_ID;
				send_buffer = gre_io_serialize(send_buffer, NULL, TABLEID_EVENT,TABLEID_FORMAT,&table_ID_varData ,sizeof(table_ID_varData));
				akw = send_data_to_channel(send_handle,send_buffer);
				
				__android_log_print(ANDROID_LOG_DEBUG, TAG, "setTableID event send to IO channel %d\n",akw);

//********************************************** setPayTablename  ************************************************************************************//
//******************* for paytable 1 **************************************//
				gre_io_zero_buffer(send_buffer);
				
				payTableNameVariable payTableNameVar;
				payTableNameVar.payTableIndex =1;
				strcpy(payTableNameVar.name,PT1_Name);
				
				send_buffer = gre_io_serialize(send_buffer, NULL,PAY_TABLE_NAME_EVENT,PAY_TABLE_NAME_FORMAT,&payTableNameVar ,sizeof(payTableNameVar));
				akw = send_data_to_channel(send_handle,send_buffer);
				
//***************** for paytable 2 ****************************************//
				gre_io_zero_buffer(send_buffer);
				bzero(&payTableNameVar,sizeof(payTableNameVar));
				
				payTableNameVar.payTableIndex =2;
				strcpy(payTableNameVar.name,PT2_Name);
				
				send_buffer = gre_io_serialize(send_buffer, NULL,PAY_TABLE_NAME_EVENT,PAY_TABLE_NAME_FORMAT,&payTableNameVar ,sizeof(payTableNameVar));
				akw = send_data_to_channel(send_handle,send_buffer);
				__android_log_print(ANDROID_LOG_DEBUG, TAG, "setPayTableName event send to IO channel %d\n",akw);
				
//************************************************* setBetSpotID  *************************************************************************************//
//************************** for paytable 1 ******************************//
				gre_io_zero_buffer(send_buffer);
		
				betSpotIdVariable betSpotIdVar;
				betSpotIdVar.ID =Betspot_ID1;
				betSpotIdVar.betSpotIndex =1;
		
				send_buffer = gre_io_serialize(send_buffer, NULL,BET_SPOTID_EVENT,BET_SPOTID_FORMAT,&betSpotIdVar ,sizeof(betSpotIdVar));
				akw = send_data_to_channel(send_handle,send_buffer);

//************************* for paytable 2 ********************************//
				gre_io_zero_buffer(send_buffer);
				bzero(&betSpotIdVar,sizeof(betSpotIdVar));
		
				betSpotIdVar.ID =Betspot_ID2;
				betSpotIdVar.betSpotIndex =2;
		
				send_buffer = gre_io_serialize(send_buffer, NULL,BET_SPOTID_EVENT,BET_SPOTID_FORMAT,&betSpotIdVar ,sizeof(betSpotIdVar));
				akw = send_data_to_channel(send_handle,send_buffer);
				__android_log_print(ANDROID_LOG_DEBUG, TAG, "setBetSpotID evnet send to IO channel %d\n",akw);
				
//************************************ for setHandIDMatrix ***********************************************************************//
				//.............for paytable 1.............
				gre_io_zero_buffer(send_buffer);
				handIdMatrixVariable handIdMatrixVar;
		
				handIdMatrixVar.matrixIndex =1;
				if(strcmp(Denom2_name1,ZERO)==0 && strcmp(Denom1_name1,ZERO)!=0)
				{
					// Only the middle button of the dealerPad GUI is active, 
					// so assign the handID value to this GUI element from denom1 ID value
		
					handIdMatrixVar.D1_H1_ID   = 0;
					handIdMatrixVar.D2_H1_ID   = 0;
					handIdMatrixVar.Both_H1_ID = D1_H1_Hand_ID1;
					handIdMatrixVar.D1_H2_ID   = 0;
					handIdMatrixVar.D2_H2_ID   = 0;
					handIdMatrixVar.Both_H2_ID = D1_H2_Hand_ID1;
					handIdMatrixVar.D1_H3_ID   = 0;
					handIdMatrixVar.D2_H3_ID   = 0;
					handIdMatrixVar.Both_H3_ID = D1_H3_Hand_ID1;
				} 
				else if(strcmp(Denom1_name1,ZERO)==0 && strcmp(Denom2_name1,ZERO)!=0)
				{
					// Only the middle button of the dealerPad GUI is active, 
					// so assign the handID value to this GUI element from denom2 ID value
					handIdMatrixVar.D1_H1_ID   = 0;
					handIdMatrixVar.D2_H1_ID   = 0;
					handIdMatrixVar.Both_H1_ID = D2_H1_Hand_ID1;
					handIdMatrixVar.D1_H2_ID   = 0;
					handIdMatrixVar.D2_H2_ID   = 0;
					handIdMatrixVar.Both_H2_ID = D2_H2_Hand_ID1;
					handIdMatrixVar.D1_H3_ID   = 0;
					handIdMatrixVar.D2_H3_ID   = 0;
					handIdMatrixVar.Both_H3_ID = D2_H3_Hand_ID1;
				}
				else
				{
					handIdMatrixVar.D1_H1_ID =D1_H1_Hand_ID1;
					handIdMatrixVar.D2_H1_ID =D2_H1_Hand_ID1;
					handIdMatrixVar.Both_H1_ID =D3_H1_Hand_ID1;
					handIdMatrixVar.D1_H2_ID =D1_H2_Hand_ID1;
					handIdMatrixVar.D2_H2_ID =D2_H2_Hand_ID1;
					handIdMatrixVar.Both_H2_ID =D3_H2_Hand_ID1;
					handIdMatrixVar.D1_H3_ID =D1_H3_Hand_ID1;
					handIdMatrixVar.D2_H3_ID =D2_H3_Hand_ID1;
					handIdMatrixVar.Both_H3_ID =D3_H3_Hand_ID1;
				}
		
				send_buffer = gre_io_serialize(send_buffer, NULL,HANDIDMATRIXEVENT,HANDIDMATRIXFORMAT,&handIdMatrixVar ,sizeof(handIdMatrixVar));
				akw = send_data_to_channel(send_handle,send_buffer);
				//..................for paytable 2...........
				gre_io_zero_buffer(send_buffer);
				bzero(&handIdMatrixVar,sizeof(handIdMatrixVar));
		
				handIdMatrixVar.matrixIndex =2;
				if(strcmp(Denom2_name2,ZERO)==0 && strcmp(Denom1_name2,ZERO)!=0)
				{
					// Only the middle button of the dealerPad GUI is active, 
					// so assign the handID value to this GUI element from denom1 ID value
		
					handIdMatrixVar.D1_H1_ID   = 0;
					handIdMatrixVar.D2_H1_ID   = 0;
					handIdMatrixVar.Both_H1_ID = D1_H1_Hand_ID2;
					handIdMatrixVar.D1_H2_ID   = 0;
					handIdMatrixVar.D2_H2_ID   = 0;
					handIdMatrixVar.Both_H2_ID = D1_H2_Hand_ID2;
					handIdMatrixVar.D1_H3_ID   = 0;
					handIdMatrixVar.D2_H3_ID   = 0;
					handIdMatrixVar.Both_H3_ID = D1_H3_Hand_ID2;
				} 
				else if(strcmp(Denom1_name2,ZERO)==0 && strcmp(Denom2_name2,ZERO)!=0)
				{
					// Only the middle button of the dealerPad GUI is active, 
					// so assign the handID value to this GUI element from denom2 ID value
					handIdMatrixVar.D1_H1_ID   = 0;
					handIdMatrixVar.D2_H1_ID   = 0;
					handIdMatrixVar.Both_H1_ID = D2_H1_Hand_ID2;
					handIdMatrixVar.D1_H2_ID   = 0;
					handIdMatrixVar.D2_H2_ID   = 0;
					handIdMatrixVar.Both_H2_ID = D2_H2_Hand_ID2;
					handIdMatrixVar.D1_H3_ID   = 0;
					handIdMatrixVar.D2_H3_ID   = 0;
					handIdMatrixVar.Both_H3_ID = D2_H3_Hand_ID2;
				}
				else
				{
					handIdMatrixVar.D1_H1_ID =D1_H1_Hand_ID2;
					handIdMatrixVar.D2_H1_ID =D2_H1_Hand_ID2;
					handIdMatrixVar.Both_H1_ID =D3_H1_Hand_ID2;
					handIdMatrixVar.D1_H2_ID =D1_H2_Hand_ID2;
					handIdMatrixVar.D2_H2_ID =D2_H2_Hand_ID2;
					handIdMatrixVar.Both_H2_ID =D3_H2_Hand_ID2;
					handIdMatrixVar.D1_H3_ID =D1_H3_Hand_ID2;
					handIdMatrixVar.D2_H3_ID =D2_H3_Hand_ID2;
					handIdMatrixVar.Both_H3_ID =D3_H3_Hand_ID2;
				}
		
				send_buffer = gre_io_serialize(send_buffer, NULL,HANDIDMATRIXEVENT,HANDIDMATRIXFORMAT,&handIdMatrixVar ,sizeof(handIdMatrixVar));
				akw = send_data_to_channel(send_handle,send_buffer);
				__android_log_print(ANDROID_LOG_DEBUG, TAG, "setHandIDMatrix event send to IO channel %d\n",akw);
//********************************************* setPayTableID **************************************************************************//
				payTableIDVariable payTableIdVar;
				payTableIdVar.payTableIndex=1;
				payTableIdVar.ID=PT1_ID;
		
				send_buffer = gre_io_serialize(send_buffer,NULL,PAYTABLEID_EVENT,PAYTABLEID_FORMAT,&payTableIdVar,sizeof(payTableIdVar));
				akw = send_data_to_channel(send_handle,send_buffer);
		
				gre_io_zero_buffer(send_buffer);
				bzero(&payTableIdVar,sizeof(payTableIdVar));
		
				payTableIdVar.payTableIndex=2;
				payTableIdVar.ID=PT2_ID;
				send_buffer = gre_io_serialize(send_buffer,NULL,PAYTABLEID_EVENT,PAYTABLEID_FORMAT,&payTableIdVar,sizeof(payTableIdVar));
				akw = send_data_to_channel(send_handle,send_buffer);
		
		        __android_log_print(ANDROID_LOG_DEBUG, TAG, "setPayTableID event send to IO channel %d\n",akw);

//***************************************************for style 1 ************************************************************************//
	if(Style ==1)
	{
		//************************************************** setDenomName **********************************************************************************//
		//*********************************** for paytable 1 ***************************************//

		gre_io_zero_buffer(send_buffer);
		denomNameVariable denomNameVar;

		if(strcmp(Denom2_name1,ZERO)==0 && strcmp(Denom1_name1,ZERO)!=0)
		{

			 denomNameVar.denomIndex =1;
			 strcpy(denomNameVar.name,ZERO);
			 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
			 akw = send_data_to_channel(send_handle,send_buffer);

			 gre_io_zero_buffer(send_buffer);
			 bzero(&denomNameVar,sizeof(denomNameVar));

			 denomNameVar.denomIndex =2;
			 strcat(denomNameVar.name,Denom1_name1);
			 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
		         akw = send_data_to_channel(send_handle,send_buffer);

			 gre_io_zero_buffer(send_buffer);
			 bzero(&denomNameVar,sizeof(denomNameVar));

			 denomNameVar.denomIndex =3;
			 strcpy(denomNameVar.name,ZERO);
			 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
			 akw = send_data_to_channel(send_handle,send_buffer);
		}
		else if(strcmp(Denom1_name1,ZERO)==0 && strcmp(Denom2_name1,ZERO)!=0)
		{
			 denomNameVar.denomIndex =1;
			 strcpy(denomNameVar.name,ZERO);
			 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
			 akw = send_data_to_channel(send_handle,send_buffer);

			 gre_io_zero_buffer(send_buffer);
			 bzero(&denomNameVar,sizeof(denomNameVar));

		         denomNameVar.denomIndex =2;
		         strcat(denomNameVar.name,Denom2_name1);
			 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
		         akw = send_data_to_channel(send_handle,send_buffer);

			 gre_io_zero_buffer(send_buffer);
			 bzero(&denomNameVar,sizeof(denomNameVar));

			 denomNameVar.denomIndex =3;
			 strcpy(denomNameVar.name,ZERO);
			 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
		         akw = send_data_to_channel(send_handle,send_buffer);
		}
		else
		{
			denomNameVar.denomIndex =1;
			strcat(denomNameVar.name,Denom1_name1);
			 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
			akw = send_data_to_channel(send_handle,send_buffer);

			gre_io_zero_buffer(send_buffer);
			bzero(&denomNameVar,sizeof(denomNameVar));

			denomNameVar.denomIndex =3;
			strcat(denomNameVar.name,Denom2_name1);
			send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
			akw = send_data_to_channel(send_handle,send_buffer);

			gre_io_zero_buffer(send_buffer);
			bzero(&denomNameVar,sizeof(denomNameVar));

			denomNameVar.denomIndex =2;
			strcpy(denomNameVar.name,BOTH);
		 	send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
			akw = send_data_to_channel(send_handle,send_buffer);
		}
		//*********************************** for paytable 2 ***************************************//
		gre_io_zero_buffer(send_buffer);
		bzero(&denomNameVar,sizeof(denomNameVar));

		if(strcmp(Denom2_name2,ZERO)==0 && strcmp(Denom1_name2,ZERO)!=0)
		{
			 denomNameVar.denomIndex =4;
			 strcpy(denomNameVar.name,ZERO);
			 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
			 akw = send_data_to_channel(send_handle,send_buffer);

			 gre_io_zero_buffer(send_buffer);
			 bzero(&denomNameVar,sizeof(denomNameVar));

			 denomNameVar.denomIndex =5;
			 strcat(denomNameVar.name,Denom1_name2);
		 	 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
		         akw = send_data_to_channel(send_handle,send_buffer);

			 gre_io_zero_buffer(send_buffer);
			 bzero(&denomNameVar,sizeof(denomNameVar));

			 denomNameVar.denomIndex =6;
			 strcpy(denomNameVar.name,ZERO);
			 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
		         akw = send_data_to_channel(send_handle,send_buffer);
		}
		else if(strcmp(Denom1_name2,ZERO)==0 && strcmp(Denom2_name2,ZERO)!=0)
		{
			 denomNameVar.denomIndex =4;
			 strcpy(denomNameVar.name,ZERO);
			 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
			 akw = send_data_to_channel(send_handle,send_buffer);

			 gre_io_zero_buffer(send_buffer);
			 bzero(&denomNameVar,sizeof(denomNameVar));

		         denomNameVar.denomIndex =5;
		         strcat(denomNameVar.name,Denom2_name2);
			 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
		         akw = send_data_to_channel(send_handle,send_buffer);

			 gre_io_zero_buffer(send_buffer);
			 bzero(&denomNameVar,sizeof(denomNameVar));

			 denomNameVar.denomIndex =6;
			 strcpy(denomNameVar.name,ZERO);
			 send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
			 akw = send_data_to_channel(send_handle,send_buffer);
		}
		else
		{
			denomNameVar.denomIndex =4;
			strcat(denomNameVar.name,Denom1_name2);
			send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
			akw = send_data_to_channel(send_handle,send_buffer);

			gre_io_zero_buffer(send_buffer);
			bzero(&denomNameVar,sizeof(denomNameVar));

			denomNameVar.denomIndex =6;
			strcat(denomNameVar.name,Denom2_name2);
			send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
			akw = send_data_to_channel(send_handle,send_buffer);

			gre_io_zero_buffer(send_buffer);
			bzero(&denomNameVar,sizeof(denomNameVar));

			denomNameVar.denomIndex =5;
			strcpy(denomNameVar.name,BOTH);
			send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVar ,sizeof(denomNameVar));
			akw = send_data_to_channel(send_handle,send_buffer);
		}
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "setDenomName event send to IO channel %d\n",akw);

		//*************************************************** setHandname ***********************************************************************************//
		//*********************** for paytable 1 ***********************************//
		//.....................for hand 1............
		gre_io_zero_buffer(send_buffer);

		handNameVariable handNameVar;
		handNameVar.handIndex =1;
		strcpy(handNameVar.name,Hand1_name1);

		send_buffer = gre_io_serialize(send_buffer, NULL,HANDNAME_EVENT,HANDNAME_FORMAT,&handNameVar ,sizeof(handNameVar));
		akw = send_data_to_channel(send_handle,send_buffer);
		//.....................for hand 2............
		gre_io_zero_buffer(send_buffer);
		bzero(&handNameVar,sizeof(handNameVar));

		handNameVar.handIndex =2;
		strcpy(handNameVar.name,Hand2_name1);

		send_buffer = gre_io_serialize(send_buffer, NULL,HANDNAME_EVENT,HANDNAME_FORMAT,&handNameVar ,sizeof(handNameVar));
		akw = send_data_to_channel(send_handle,send_buffer);
		//.....................for hand 3............
		gre_io_zero_buffer(send_buffer);
		bzero(&handNameVar,sizeof(handNameVar));

		handNameVar.handIndex =3;
		strcpy(handNameVar.name,Hand3_name1);

		send_buffer = gre_io_serialize(send_buffer, NULL,HANDNAME_EVENT,HANDNAME_FORMAT,&handNameVar ,sizeof(handNameVar));
		akw = send_data_to_channel(send_handle,send_buffer);

		//*********************** for paytable 2***********************************//
		//.....................for hand 4............
		gre_io_zero_buffer(send_buffer);
		bzero(&handNameVar,sizeof(handNameVar));

		handNameVar.handIndex =4;
		strcpy(handNameVar.name,Hand1_name2);

		send_buffer = gre_io_serialize(send_buffer, NULL,HANDNAME_EVENT,HANDNAME_FORMAT,&handNameVar ,sizeof(handNameVar));
		akw = send_data_to_channel(send_handle,send_buffer);
		//.....................for hand 5............
		gre_io_zero_buffer(send_buffer);
		bzero(&handNameVar,sizeof(handNameVar));

		handNameVar.handIndex =5;
		strcpy(handNameVar.name,Hand2_name2);

		send_buffer = gre_io_serialize(send_buffer, NULL,HANDNAME_EVENT,HANDNAME_FORMAT,&handNameVar ,sizeof(handNameVar));
		akw = send_data_to_channel(send_handle,send_buffer);
		//.....................for hand 6............
		gre_io_zero_buffer(send_buffer);
		bzero(&handNameVar,sizeof(handNameVar));

		handNameVar.handIndex =6;
		strcpy(handNameVar.name,Hand3_name2);

		send_buffer = gre_io_serialize(send_buffer, NULL,HANDNAME_EVENT,HANDNAME_FORMAT,&handNameVar ,sizeof(handNameVar));
		akw = send_data_to_channel(send_handle,send_buffer);

		__android_log_print(ANDROID_LOG_DEBUG, TAG, "setHandName event send to IO channel %d\n",akw);
	}
//************************************************************ for Style 2 *****************************************************************************************//
	else if(Style == 2)
	{
	
//************************************************ Make Pay Table 2 section visible by setting the Title to blank (" ") **************************//
		gre_io_zero_buffer(send_buffer);
		bzero(&payTableNameVar,sizeof(payTableNameVar));
		
		payTableNameVar.payTableIndex =2;
		strcpy(payTableNameVar.name," ");
		
		send_buffer = gre_io_serialize(send_buffer, NULL,PAY_TABLE_NAME_EVENT,PAY_TABLE_NAME_FORMAT,&payTableNameVar ,sizeof(payTableNameVar));
		akw = send_data_to_channel(send_handle,send_buffer);
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "setPayTableName event send to IO channel %d\n",akw);

//************************************************ for setting denom name for style 2 ************************************************************//
        gre_io_zero_buffer(send_buffer);
        denomNameVariable denomNameVarForStyle2;

		denomNameVarForStyle2.denomIndex =1;
		strcat(denomNameVarForStyle2.name,Denom1_name1);

		send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVarForStyle2 ,sizeof(denomNameVarForStyle2));
        akw = send_data_to_channel(send_handle,send_buffer);

		gre_io_zero_buffer(send_buffer);
        bzero(&denomNameVarForStyle2,sizeof(denomNameVarForStyle2));

		denomNameVarForStyle2.denomIndex =2;
		strcpy(denomNameVarForStyle2.name,BOTH);

        send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVarForStyle2 ,sizeof(denomNameVarForStyle2));
        akw = send_data_to_channel(send_handle,send_buffer);


        gre_io_zero_buffer(send_buffer);
        bzero(&denomNameVarForStyle2,sizeof(denomNameVarForStyle2));

		denomNameVarForStyle2.denomIndex =6;
        strcat(denomNameVarForStyle2.name,Denom2_name1);

        send_buffer = gre_io_serialize(send_buffer, NULL,DENOM_NAME_EVENT,DENOM_NAME_FORMAT,&denomNameVarForStyle2 ,sizeof(denomNameVarForStyle2));
        akw = send_data_to_channel(send_handle,send_buffer);
//************************************************ for setting the hand name for style 2 *********************************************************//
		//........... for hand 1 ...................
  	    gre_io_zero_buffer(send_buffer);

        handNameVariable handNameVar;
        handNameVar.handIndex =1;
        strcpy(handNameVar.name,Hand1_name1);

        send_buffer = gre_io_serialize(send_buffer, NULL,HANDNAME_EVENT,HANDNAME_FORMAT,&handNameVar ,sizeof(handNameVar));
        akw = send_data_to_channel(send_handle,send_buffer);
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "hand 1 sent for style 2 %d \n",akw);
        //.....................for hand 2............
        gre_io_zero_buffer(send_buffer);
        bzero(&handNameVar,sizeof(handNameVar));

        handNameVar.handIndex =2;
        strcpy(handNameVar.name,Hand2_name1);

        send_buffer = gre_io_serialize(send_buffer, NULL,HANDNAME_EVENT,HANDNAME_FORMAT,&handNameVar ,sizeof(handNameVar));
        akw = send_data_to_channel(send_handle,send_buffer);
		 __android_log_print(ANDROID_LOG_DEBUG, TAG, "hand 2 sent for style 2 %d \n",akw);
        //.....................for hand 3............
        gre_io_zero_buffer(send_buffer);
        bzero(&handNameVar,sizeof(handNameVar));

        handNameVar.handIndex =3;
        strcpy(handNameVar.name,Hand3_name1);

        send_buffer = gre_io_serialize(send_buffer, NULL,HANDNAME_EVENT,HANDNAME_FORMAT,&handNameVar ,sizeof(handNameVar));
        akw = send_data_to_channel(send_handle,send_buffer);
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "hand 3 sent for style 2 %d \n",akw);
	}

	close(sockfd);
}

/***********************************************************************************************************************
* module             : openSocketAndSendDataToServer(void)
* return type        : void
* Description        : Function, to open socket and call SendDataToServer and compareEvent
* param              : void
* Author             : CHETU
***********************************************************************************************************************/
void  openSocketAndSendDataToServer()
{
 	int akw = 0;
	struct sockaddr_in servaddr ={0,};
	char buff[MAX]={0};
	int wait=5;
	int sockfd = -1;
	bool retVal = false;
	int MaxTries=0;

	 //socket create and varification
        sockfd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK,0);
	if (sockfd == -1)
	{
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "socket creation failed...%d\n", errno);
		return ;
	}
	else
	{
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "Socket successfully created..\n");
	}

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
        retVal = compareEvents(&servaddr);
	if(!retVal)
	{
		return;
	}
	// connect the client socket to server socket
	if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0)
	{
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "trying to get connected with server...\n");
               	while( (errno == EALREADY && MaxTries < 5) || (errno == EINPROGRESS && MaxTries < 5) )// trying to get connected with the server
                {
                        poll(NULL,0,100); //100 millisec sleep for 5 retries max.
                        connect(sockfd, (SA *)&servaddr, sizeof(servaddr));
                        MaxTries = MaxTries + 1;
                }
                if(errno == EISCONN)
		{
                      __android_log_print(ANDROID_LOG_DEBUG, TAG, "Connected with the server\n");
		}
                else
		{
                      __android_log_print(ANDROID_LOG_DEBUG, TAG, "Not Connected with the server\n");
                 		return ;
		}
	}
	else
	{
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "connected to the server..\n");
	}

	akw =SendDataToServer(sockfd);
	if(akw == 0)
	{
	 	return;
	}
        bzero(buff, sizeof(buff));
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "Waiting for server response....\n");

        akw = recv(sockfd,buff,sizeof(buff),MSG_DONTWAIT);

	if(strlen(buff)==0)
        {
	 	recv(sockfd,buff,sizeof(buff),MSG_DONTWAIT );
		while((wait >=0) && strlen(buff)==0)
		{
			 recv(sockfd,buff,sizeof(buff),MSG_DONTWAIT );
			 sleep_ms(1000);
			 wait--;
		}
	}
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "From Server : %s\n", buff);
	close(sockfd);

}
/***********************************************************************************************************************
* module             : initDealerPad(void *)
* return type        : void
* Description        : Function, to open socket and send data to BJS server for initDealerPad Event
* param              : void *
* Author             : CHETU
***********************************************************************************************************************/
void* initDealerPad(void *args)
{
	int sockfd = 0;
	int akw=0;
	struct sockaddr_in serv_addr={0,};
        int wait=5;
	int MaxTries=0;
        char buff[MAX]={0}; 
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
        	__android_log_print(ANDROID_LOG_DEBUG, TAG, "\n Socket creation error for initDealer event \n");
        	return NULL;
	}
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "socket created for initDealer event\n");
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERVER_PORT);

	if(inet_pton(AF_INET, IPADD, &serv_addr.sin_addr)<=0)
	{
        	__android_log_print(ANDROID_LOG_DEBUG, TAG, "\nInvalid address/ Address not supported \n");
        	return NULL;
	}

	while(connect(sockfd, (SA*)&serv_addr, sizeof(serv_addr)) != 0)
	{
        	__android_log_print(ANDROID_LOG_DEBUG, TAG, "trying to get connected with server...\n");
        	while( (errno == EALREADY && MaxTries < 5) || (errno == EINPROGRESS && MaxTries < 5) )// trying to get connected with the server
        	{
                	poll(NULL,0,100); //100 millisec sleep for 5 retries max.
                	connect(sockfd, (SA *)&serv_addr, sizeof(serv_addr));
                	MaxTries = MaxTries + 1;
               	}
        	if(errno == EISCONN)
        	{
                	__android_log_print(ANDROID_LOG_DEBUG, TAG, "Connected with the server\n");
        	}
        	else
        	{
                        sleep_ms(120000);
        	}
	}

	send(sockfd,initDealerPadBuff,strlen(initDealerPadBuff),0);
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "initDealerPad packet send to the server\n");
        akw = recv(sockfd,buff,sizeof(buff),MSG_DONTWAIT);
	if(strlen(buff)==0)
        {
	 	recv(sockfd,buff,sizeof(buff),MSG_DONTWAIT );
		while((wait >=0) && strlen(buff)==0)
		{
			 recv(sockfd,buff,sizeof(buff),MSG_DONTWAIT );
			 sleep_ms(1000);
			 wait--;
		}
	}
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "From Server : %s\n", buff);
	close(sockfd);

}

/***********************************************************************************************************************
* module             : receive_thread(void)
* return type        : void *
* Description        : Function, to receive the events from the Stroyboard IO channel and call openSocketAndSendDataTOServer
* param              : void *
* Author             : CHETU
***********************************************************************************************************************/ 
void *receive_thread(void *arg)
 {
	gre_io_t *handle=NULL;
	gre_io_serialized_data_t *nbuffer = NULL;
	char *event_addr=NULL;
	char *event_name=NULL;
	char *event_format=NULL;
	void *event_data=NULL;
	int ret=0;
	int nbytes=0;
	pthread_t initDealerPadThread;
	pthread_t configureDealerPadThread;
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "Opening a channel for receive\n");

	// Connect to a channel to receive messages
	handle = gre_io_open(RECEIVE_CHANNEL, GRE_IO_TYPE_RDONLY);
	if (handle == NULL)
	{
	        __android_log_print(ANDROID_LOG_DEBUG, TAG, "Can't open receive channel\n");
		return 	NULL;
	} else {
        __android_log_print(ANDROID_LOG_DEBUG, TAG, "open receive channel\n");
	}

	nbuffer = gre_io_size_buffer(NULL, 100);

       	while (1)
	{
		ret = gre_io_receive(handle, &nbuffer);
		if (ret < 0)
		{
			return NULL;
		}
		event_name = NULL;
		nbytes = gre_io_unserialize(nbuffer, &event_addr, &event_name, &event_format, &event_data);
		if (!event_name)
		{
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Missing event name\n");
			return NULL;
		}
		Event_Name=event_name;
		if(strcmp(event_name, "pendingJackpotEvent") == 0)
		{
			 event_data_type *rcv_data = (event_data_type *)event_data;
			 __android_log_print(ANDROID_LOG_DEBUG, TAG, "Received Event %s nbytes: %d format: %s \n", event_name, nbytes, event_format);

			 __android_log_print(ANDROID_LOG_DEBUG, TAG, " tableID: %u dealerID: %u playerID: %u betSpotID: %u payTableID: %u handID: %u \n",
                         rcv_data->tableID,
 			 rcv_data->dealerID,
			 rcv_data->playerID,
 			 rcv_data->betSpotID,
			 rcv_data->payTableID,
			 rcv_data->handID);

			 sprintf(send_to_server_buffer,"%s,%s,%s,%s,%u,%u,%u,%u,%u,%u",
			 VERSION,
			 MacBuff,
			 IPBuff,
			 ACTION,
			 rcv_data->tableID,
			 rcv_data->dealerID,
			 rcv_data->playerID,
			 rcv_data->betSpotID,
			 rcv_data->payTableID,
			 rcv_data->handID);
			 //__android_log_print(ANDROID_LOG_DEBUG, TAG, " size of server buffer=%d \n", strlen(send_to_server_buffer));
			 __android_log_print(ANDROID_LOG_DEBUG, TAG, "%s\n", send_to_server_buffer);

			 openSocketAndSendDataToServer();
		}
		else if(strcmp(event_name, "lockBets") == 0)
                {
			ipAddr = (char*)event_data;
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Received Event %s nbytes: %d format: %s \n", event_name, nbytes, event_format);
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Received IP address =%s\n",ipAddr);

			openSocketAndSendDataToServer();
		}
		else if(strcmp(event_name, "initDealerPad")==0)
		{
			sprintf(initDealerPadBuff,"%s,%s,%s,%s",PROTOCOL_VERSION,MacBuff,IPBuff,ACTION_CODE);
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Received Event %s nbytes: %d format: %s \n", event_name, nbytes, event_format);
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "initDealerPadBuff: %s \n",initDealerPadBuff);
			if (pthread_create(&initDealerPadThread,NULL,initDealerPad,NULL) != 0)
			{
			        __android_log_print(ANDROID_LOG_DEBUG, TAG, "Thread create failed for initDealerPad\n");
			}
		}
		else if(strcmp(event_name,"requestConfiguration")==0)
		{
			 sprintf(dealerPadConfigBuff,"%s,%s,%s,%s",VERSION,MacBuff,IPBuff,ACTION_CODE_FOR_DEALERPAD_CONFIG);
			 __android_log_print(ANDROID_LOG_DEBUG, TAG, "Received Event %s nbytes: %d format: %s \n", event_name, nbytes, event_format);
                         __android_log_print(ANDROID_LOG_DEBUG, TAG, "dealerPadConfigBuff:%s \n",dealerPadConfigBuff);
			 if(pthread_create(&configureDealerPadThread,NULL,configureDealerPad,NULL)!=0)
			 {
				 __android_log_print(ANDROID_LOG_DEBUG, TAG, "Thread create failed for configureDealerPad\n");
			 }
		}
		else
		{
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "unknown event received from the front end\n");
		}

	}
	//Release the buffer memory, close the send handle
	gre_io_free_buffer(nbuffer);
	gre_io_close(handle);
}
/***********************************************************************************************************************
* module             : lockAndUnlock(void)
* return type        : void *
* Description        : Function, to receive the lock and unlock events
* param              : void *
* Author             : CHETU
***********************************************************************************************************************/ 
void* lockAndUnlock(void* args)
{
	int server_fd=-1;
	int  accepted_socket =-1;
	int valread=0;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);

	int length =0;
	int akw=0;
	gre_io_serialized_data_t *send_buffer = NULL;
	gre_io_t *send_handle=NULL;

	__android_log_print(ANDROID_LOG_DEBUG, TAG, "Trying to open the connection to the frontend\n");
	while (1)
	{
		// Connect to a channel to send messages (write)
		sleep_ms(SNOOZE_TIME);
		send_handle = gre_io_open(SEND_CHANNEL, GRE_IO_TYPE_WRONLY);
		if (send_handle != NULL)
		{
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Send channel: %s successfully opened\n",SEND_CHANNEL);
			break;
		}
	}
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "opening the socket to recieve lock and unlock event\n");
	// Creating socket file descriptor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
        __android_log_print(ANDROID_LOG_DEBUG, TAG,"socket failed");
		exit(EXIT_FAILURE);
	}
	// Forcefully attaching socket to the port 7620
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt)))
	{
        __android_log_print(ANDROID_LOG_DEBUG, TAG, "setsockopt");
		exit(EXIT_FAILURE);
	}
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "binding with the port\n");
	address.sin_family = AF_INET;
	//address.sin_addr.s_addr = htonl(INADDR_ANY);
	inet_pton(AF_INET, IPADD, &address.sin_addr);
	address.sin_port = htons(PORT_FOR_LOCK_AND_UNLOCK);
	// Forcefully attaching socket to the port 7620
	if (bind(server_fd, (struct sockaddr *)&address,sizeof(address))<0)
	{
        __android_log_print(ANDROID_LOG_DEBUG, TAG, "bind failed");
		exit(EXIT_FAILURE);
	}
	while(1)
	{
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "listening.......\n");
		//listing for client request
		if (listen(server_fd, 3) < 0)
		{
            __android_log_print(ANDROID_LOG_DEBUG, TAG, "listen");
			exit(EXIT_FAILURE);
		}
		//accepting the client request
		if ((accepted_socket = accept(server_fd, (struct sockaddr *)&address,(socklen_t*)&addrlen))<0)
		{
            __android_log_print(ANDROID_LOG_DEBUG, TAG, "accept");
			exit(EXIT_FAILURE);
		}
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "connected with client\n");
		valread = read( accepted_socket , lock_unlock_response,MAX);
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "%s\n",lock_unlock_response );
		length=strlen(lock_unlock_response);
		if(length !=0)
		{
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "response from server in thread lockAndUnlock =%s\n",lock_unlock_response);

			if(strncmp(lock_unlock_response,"1,4",3)==0)
			{
				send_buffer = gre_io_serialize(send_buffer, NULL, "lockDealer", NULL,NULL ,sizeof("lockDealer"));
				__android_log_print(ANDROID_LOG_DEBUG, TAG, "lockDealer event searialized\n");
			}

			else if(strncmp(lock_unlock_response,"1,5",3)==0)
			{
				send_buffer = gre_io_serialize(send_buffer, NULL, "unlockDealer", NULL,NULL , sizeof("unlockDealer"));
				__android_log_print(ANDROID_LOG_DEBUG, TAG, "unlockDealer event searialized\n");

			}
			else
			{
				__android_log_print(ANDROID_LOG_DEBUG, TAG, "unknown response from BJS\n");
			}
			akw =send_data_to_channel(send_handle, send_buffer);
			if (akw < 0)
			{
				__android_log_print(ANDROID_LOG_DEBUG, TAG, "sending failed to storyboard IO channel....\n");
			}
			else
			{
				 if(strncmp(lock_unlock_response,"1,4",3)==0)
				{
				        __android_log_print(ANDROID_LOG_DEBUG, TAG, "lock has been send from the backend to Storyboard IO channel\n");
				}
				else if(strncmp(lock_unlock_response,"1,5",3)==0)
				{
				         __android_log_print(ANDROID_LOG_DEBUG, TAG, "unlock has been send from the backend to Storyboard IO channel\n");
				}
				else
				{
				        __android_log_print(ANDROID_LOG_DEBUG, TAG, "data send to Storyboard IO channel is invalid\n");
				}
			}
		}

		if(length !=0)
		{
			send(accepted_socket , RESPONSE , strlen(RESPONSE), 0);
			close(accepted_socket);
		}
	}
	gre_io_free_buffer(send_buffer);
	gre_io_close(send_handle);
}

/***********************************************************************************************************************
* module             : main(int,char **)
* return type        : int
* Description        : Function, our program starts from here which create two threads for sending and receiving events
		       from StoryboardIO channel 
* param              : int,char **
* Author             : CHETU
***********************************************************************************************************************/ 
int backend_main(const char* mac)
{
    //Manually load shared libraries
	void* libgre = dlopen("/data/data/com.example.dealer_tablet/lib/libgre.so", RTLD_NOW);
	if (libgre == nullptr) {
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "libgre.so not loaded");
		return -1;
	}

	void* libgreiotcp = dlopen("/data/data/com.example.dealer_tablet/lib/libgreio-tcp.so", RTLD_NOW);
	if (libgreiotcp == nullptr) {
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "libgreio-tcp.so not loaded");
		return -1;
	}

	void* libgreio = dlopen("/data/data/com.example.dealer_tablet/lib/libgreio.so", RTLD_NOW);
	if (libgreio == nullptr) {
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "libgreio.so not loaded");
		return -1;
	}

	    //Load function address
		gre_io_open_api = (gre_io_open_t)dlsym(libgreio, "gre_io_open");
		if (gre_io_open_api == nullptr) {
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Unable to find gre_io_open");
		}
		gre_io_unserialize_api = (gre_io_unserialize_t)dlsym(libgreio, "gre_io_unserialize");
		if (gre_io_unserialize_api == nullptr) {
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Unable to find gre_io_unserialize");
		}
		gre_io_receive_api = (gre_io_receive_t)dlsym(libgreio, "gre_io_receive");
		if (gre_io_receive_api == nullptr) {
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Unable to find gre_io_receive");
		}
		gre_io_size_buffer_api = (gre_io_size_buffer_t)dlsym(libgreio, "gre_io_size_buffer");
		if (gre_io_size_buffer_api == nullptr) {
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Unable to find gre_io_size_buffer");
		}
		gre_io_zero_buffer_api = (gre_io_zero_buffer_t)dlsym(libgreio, "gre_io_zero_buffer");
		if (gre_io_zero_buffer_api == nullptr) {
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Unable to find gre_io_zero_buffer");
		}
		gre_io_serialize_api = (gre_io_serialize_t)dlsym(libgreio, "gre_io_serialize");
		if (gre_io_serialize_api == nullptr) {
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Unable to find gre_io_serialize");
		}
		gre_io_send_api = (gre_io_send_t)dlsym(libgreio, "gre_io_send");
		if (gre_io_send_api == nullptr) {
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Unable to find gre_io_send");
		}

	pthread_t lockAndUnlockThread;
        #ifndef WIN32
	//getMacAddr();
	strncpy(MacBuff, mac, 20);
    __android_log_print(ANDROID_LOG_DEBUG, TAG, "MAC Address received from JAVA = %s", MacBuff);
	getIPAddr();
        #endif // !WIN32gre_io_size_buffer

	if (pthread_create(&lockAndUnlockThread,NULL,lockAndUnlock,NULL) != 0)
        {
                __android_log_print(ANDROID_LOG_DEBUG, TAG, "Thread create failed\n");
                return -1;
        }

	if (pthread_create(&thread1,NULL,receive_thread,NULL) != 0)
        {
                __android_log_print(ANDROID_LOG_DEBUG, TAG, "Thread create failed\n");
                return -1;
        }

//__android_log_print(ANDROID_LOG_DEBUG, TAG, "socket creation failed...%d", errno);

        //pthread_join(thread1, NULL);
	//pthread_join(lockAndUnlockThread,NULL);

	return 0;
}

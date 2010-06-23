#include "stdafx.h"
#include "csocket.h"
#include "csocketevents.h"
#include "cphidgetlist.h"
#include "cphidgetmanager.h"
#include "cphidgetdictionary.h"
#ifdef USE_ZEROCONF
#include "zeroconf.h"
#endif

/*
*	Phidget WebService Protocol version history
*	1.0 - Initial version
*
*	1.0.1
*		-first version to be enforced
*		-we changed around the device id numbers, so old webservice won't be able to talk to new
*
*	1.0.2
*		-Authorization is asynchronous, so we had to add Tags and now it's not compatible with old webservice
*		-Doesn't match the old version checking! So could be ugly for users, get an unexpected error rather then a version error
*		-Sends out all initial data, so it's just like opening locally
*		-supports interfacekit Raw sensor value
*		-supports labels on remoteIP managers
*
*	1.0.3
*		-supports servoType and setServoParameters for PhidgetServo and PhidgetAdvancedServo
*
*	1.0.4
*		-fixed RFID initialization - wasn't getting tag events if tag is on reader before open
*		-fixed RFID sometimes not attaching in Flash
*
*	1.0.5
*		-added dataRate for InterfaceKit
*
*	1.0.6
*		-added brightness for TextLCD
*		-support PhidgetSpatial
*		-support PhidgetIR
*		-1047 support (enable, index)
*
*/
const char *ws_protocol_ver = "1.0.6";

void DNSServiceResolve_CallBack(DNSServiceRef sdRef,
				DNSServiceFlags                     flags,
				uint32_t                            interfaceIndex,
				DNSServiceErrorType                 errorCode,
				const char                          *fullname,
				const char                          *hosttarget,
				uint16_t                            port,
				uint16_t                            txtLen,
				const char                          *txtRecord,
				void                                *context);

typedef struct _CServerInfo
{
	CPhidgetSocketClientHandle server;
	CPhidgetListHandle phidgets;
	CPhidgetManagerListHandle managers;
	CPhidgetDictionaryListHandle dictionaries;
} CServerInfo, *CServerInfoHandle;

typedef struct _CServerList
{
	struct _CServerList *next;
	CServerInfoHandle serverInfo;
} CServerList, *CServerListHandle;

CThread_func_return_t CentralRemoteThreadFunction(CThread_func_arg_t arg);
static CThread CentralRemoteThread;
CThread_mutex_t CentralRemoteThreadLock;
int CentralRemoteThreadLockInitialized = PFALSE;

// list of connected servers
CServerListHandle servers = NULL;
/* Protects servers */
int serverLockInitialized = PFALSE;
CThread_mutex_t serverLock;
CThread_mutex_t serverLockLock;

CPhidgetRemoteListHandle zeroconfServers = NULL;
CPhidgetListHandle zeroconfPhidgets = NULL;
CPhidgetSBCListHandle zeroconfSBCs = NULL;
/* Protects zeroconf lists */
int zeroconfListLockInitialized = PFALSE;
CThread_mutex_t zeroconfServersLock;
CThread_mutex_t zeroconfPhidgetsLock;
CThread_mutex_t zeroconfSBCsLock;
CThread_mutex_t zeroconfInitLock;

//Lists of objects that we have called openRemote or openRemoteIP on
//These may or may not actually be attached or connected
CPhidgetListHandle activeRemotePhidgets = NULL;
CPhidgetManagerListHandle activeRemoteManagers = NULL;
CPhidgetSBCManagerListHandle activeSBCManagers = NULL;
CPhidgetDictionaryListHandle activeRemoteDictionaries = NULL;
/* Protects the lists */
int activeRemoteLocksInitialized = PFALSE;
CThread_mutex_t activeRemotePhidgetsLock;
CThread_mutex_t activeRemoteManagersLock;
CThread_mutex_t activeSBCManagersLock;
CThread_mutex_t activeRemoteDictionariesLock;

int NetworkInitialized = PFALSE;

int inErrorEvent = PFALSE;

int closeServer(CServerInfoHandle server, unsigned char force);

#ifdef _WINDOWS
int start_WSA_server()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD( 2, 2 );

	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		LOG(PHIDGET_LOG_DEBUG,"Cannot start WSA");
		return EPHIDGET_NETWORK;
	}
	return EPHIDGET_OK;
}
#endif

int CServerInfo_areEqual(void *arg1, void *arg2)
{
	CServerInfoHandle server1 = (CServerInfoHandle)arg1;
	CServerInfoHandle server2 = (CServerInfoHandle)arg2;
	
	TESTPTRS(server1, server2)
	TESTPTRS(server1->server, server2->server)
	
	return CPhidgetSocketClient_areEqual(server1->server, server2->server);
}

void CServerInfo_free(void *arg)
{
	CServerInfoHandle server = (CServerInfoHandle)arg;
	
	if(!server)
		return;
	if(server->server)
		CPhidgetSocketClient_free(server->server); server->server = NULL;
	//CList_emptyList((CListHandle *)&server->listen_ids, PFALSE, NULL);

	//empty the lists but don't free their objects
	CList_emptyList((CListHandle *)&server->phidgets, PFALSE, NULL);
	CList_emptyList((CListHandle *)&server->managers, PFALSE, NULL);
	CList_emptyList((CListHandle *)&server->dictionaries, PFALSE, NULL);

	free(server); server = NULL;
	return;
}

typedef enum 
{ 
	PHIDGET,
	MANAGER,
	DICTIONARY
} ListElementType;

static int addToServerInfoList(CServerInfoHandle newServerInfo, void *element, ListElementType type)
{
	switch(type)
	{
		case PHIDGET:
			return CList_addToList((CListHandle *)&newServerInfo->phidgets, element, CPhidget_areEqual);
		case MANAGER:
			return CList_addToList((CListHandle *)&newServerInfo->managers, element, CPhidgetManager_areEqual);
		case DICTIONARY:
			return CList_addToList((CListHandle *)&newServerInfo->dictionaries, element, CPhidgetDictionary_areEqual);
	}
	return EPHIDGET_UNEXPECTED;
}

static int removeFromServerInfoList(CServerInfoHandle newServerInfo, void *element, ListElementType type)
{
	switch(type)
	{
		case PHIDGET:
			return CList_removeFromList((CListHandle *)&newServerInfo->phidgets, element, CPhidget_areEqual, PFALSE, NULL);
		case MANAGER:
			return CList_removeFromList((CListHandle *)&newServerInfo->managers, element, CPhidgetManager_areEqual, PFALSE, NULL);
		case DICTIONARY:
			return CList_removeFromList((CListHandle *)&newServerInfo->dictionaries, element, CPhidgetDictionary_areEqual, PFALSE, NULL);
	}
	return EPHIDGET_UNEXPECTED;
}

int CCONV CPhidgetRemote_create(CPhidgetRemoteHandle *arg)
{
	CPhidgetRemoteHandle remote;
	
	if(!(remote = malloc(sizeof(CPhidgetRemote))))
		return EPHIDGET_NOMEMORY;
	ZEROMEM(remote, sizeof(CPhidgetRemote));

	CThread_mutex_init(&remote->zeroconf_ref_lock);
	remote->cancelSocket = INVALID_SOCKET;

	*arg = remote;
	 
	 return EPHIDGET_OK;
}

void CCONV CPhidgetRemote_free(void *arg)
{
	CPhidgetRemoteHandle remote = (CPhidgetRemoteHandle)arg;

	if(!remote) return;
	if(remote->requested_port) free(remote->requested_port); remote->requested_port=NULL;
	if(remote->requested_address) free(remote->requested_address); remote->requested_address=NULL;
	if(remote->requested_serverID) free(remote->requested_serverID); remote->requested_serverID=NULL;
	if(remote->password) free(remote->password); remote->password=NULL;

	if(remote->zeroconf_name) free(remote->zeroconf_name); remote->zeroconf_name=NULL;
	if(remote->zeroconf_domain) free(remote->zeroconf_domain); remote->zeroconf_domain=NULL;
	if(remote->zeroconf_type) free(remote->zeroconf_type); remote->zeroconf_type=NULL;
	if(remote->zeroconf_host) free(remote->zeroconf_host); remote->zeroconf_host=NULL;
	if(remote->zeroconf_port) free(remote->zeroconf_port); remote->zeroconf_port=NULL;
	if(remote->zeroconf_server_id) free(remote->zeroconf_server_id); remote->zeroconf_server_id=NULL;

	CPhidgetSocketClient_free(remote->server);

	CThread_mutex_destroy(&remote->zeroconf_ref_lock);

	free(remote);

	return;
}

int CCONV CPhidgetRemote_areEqual(void *arg1, void *arg2)
{
	CPhidgetRemoteHandle remote1 = (CPhidgetRemoteHandle)arg1;
	CPhidgetRemoteHandle remote2 = (CPhidgetRemoteHandle)arg2;
	
	TESTPTRS(remote1, remote2)

	if(remote1->zeroconf_name != NULL || remote2->zeroconf_name != NULL)
		if((strcmp(remote1->zeroconf_name,remote2->zeroconf_name)))
			return 0;
		
	if(remote1->zeroconf_domain != NULL || remote2->zeroconf_domain != NULL)
		if((strcmp(remote1->zeroconf_domain,remote2->zeroconf_domain)))
			return 0;
		
	if(remote1->zeroconf_type != NULL || remote2->zeroconf_type != NULL)
		if((strcmp(remote1->zeroconf_type,remote2->zeroconf_type)))
			return 0;
		
	if(remote1->requested_port != NULL || remote2->requested_port != NULL)
		if((strcmp(remote1->requested_port,remote2->requested_port)))
			return 0;
		
	if(remote1->requested_address != NULL || remote2->requested_address != NULL)
		if((strcmp(remote1->requested_address,remote2->requested_address)))
			return 0;
		
	if(remote1->requested_serverID != NULL || remote2->requested_serverID != NULL)
		if((strcmp(remote1->requested_serverID,remote2->requested_serverID)))
			return 0;

	return 1;
}

int CCONV CPhidgetSocketClient_create(CPhidgetSocketClientHandle *arg)
{
	CPhidgetSocketClientHandle socket_client;
	
	if(!(socket_client = malloc(sizeof(CPhidgetSocketClient))))
		return EPHIDGET_NOMEMORY;
	ZEROMEM(socket_client, sizeof(CPhidgetSocketClient));

	CThread_mutex_init(&socket_client->lock);
	CThread_mutex_init(&socket_client->pdc_lock);

	CPhidget_clearStatusFlag(&socket_client->status, PHIDGETSOCKET_CONNECTED_FLAG, &socket_client->lock);

	*arg = socket_client;
	 
	 return EPHIDGET_OK;
}

void CCONV CPhidgetSocketClient_free(void *arg)
{
	CPhidgetSocketClientHandle socket_client = (CPhidgetSocketClientHandle)arg;

	if(!socket_client) return;
	if(socket_client->port) free(socket_client->port); socket_client->port=NULL;
	if(socket_client->address) free(socket_client->address); socket_client->address=NULL;
	//if(socket_client->serverID) free(socket_client->serverID); socket_client->serverID=NULL;
	if(socket_client->pdcs) free(socket_client->pdcs); socket_client->pdcs=NULL;

	CThread_mutex_destroy(&socket_client->lock);
	CThread_mutex_destroy(&socket_client->pdc_lock);

	free(socket_client);
	 
	return;
}

int CCONV CPhidgetSocketClient_areEqual(void *arg1, void *arg2)
{
	CPhidgetSocketClientHandle socket1 = (CPhidgetSocketClientHandle)arg1;
	CPhidgetSocketClientHandle socket2 = (CPhidgetSocketClientHandle)arg2;
	
	TESTPTRS(socket1, socket2)
	TESTPTRS(socket1->address, socket2->address)
	TESTPTRS(socket1->port, socket2->port)
	
	if(!(strcmp(socket1->address,socket2->address))
		&& !(strcmp(socket1->port, socket2->port)))
		return 1;
		
	return 0;
}

void internal_async_network_error_handler(const char *error, void *ptr)
{
	CPhidgetHandle phid = (CPhidgetHandle)ptr;
	if(phid && phid->fptrError)
	{
		phid->fptrError(phid, phid->fptrErrorptr, EEPHIDGET_NETWORK, error);
		return;
	}
	LOG(PHIDGET_LOG_WARNING,"Got an async network error: %s\n\tTip: Set up an error handler to catch this properly.", error);
}

static int initialize_locks()
{
	if(!CentralRemoteThreadLockInitialized)
	{
		CThread_mutex_init(&CentralRemoteThreadLock);
		CentralRemoteThreadLockInitialized = PTRUE;
	}
	if(!serverLockInitialized)
	{
		CThread_mutex_init(&serverLock);
		CThread_mutex_init(&serverLockLock);
		serverLockInitialized = PTRUE;
	}
	if(!zeroconfListLockInitialized)
	{
		CThread_mutex_init(&zeroconfServersLock);
		CThread_mutex_init(&zeroconfPhidgetsLock);
		CThread_mutex_init(&zeroconfSBCsLock);
		CThread_mutex_init(&zeroconfInitLock);
		zeroconfListLockInitialized = PTRUE;
	}
	if(!activeRemoteLocksInitialized)
	{
		CThread_mutex_init(&activeRemotePhidgetsLock);
		CThread_mutex_init(&activeRemoteManagersLock);
		CThread_mutex_init(&activeRemoteDictionariesLock);
		CThread_mutex_init(&activeSBCManagersLock);
		activeRemoteLocksInitialized = PTRUE;
	}
	return EPHIDGET_OK;
}

int InitializeNetworking()
{
	int res;
	const char *setpattern = "^/PSK/([a-zA-Z_0-9]*)/([0-9]*)/([a-zA-Z_0-9]*)/?([a-zA-Z_0-9]*)/?([a-zA-Z_0-9]*)$";
	const char *managerpattern = "^/PSK/List/([a-zA-Z_0-9]*)/([0-9]*)$";
	const char *managervalpattern = "^([a-zA-Z]*) Version=([0-9]*) ID=([0-9]*) Label=(.*)$";

#ifdef _WINDOWS
	if (start_WSA_server())
	{
		LOG(PHIDGET_LOG_WARNING,"Cannot start Windows Sockets");
		return EPHIDGET_NETWORK;
	}
#endif
	
	if (!pdc_init()) {
		return EPHIDGET_UNEXPECTED;
	}

	if ((res = regcomp(&phidgetsetex, setpattern, REG_EXTENDED)) != 0) {
		LOG_STDERR(PHIDGET_LOG_CRITICAL,"set command pattern compilation error %d", res);
		abort();
	}
	if ((res = regcomp(&managerex, managerpattern, REG_EXTENDED)) != 0) {
		LOG_STDERR(PHIDGET_LOG_CRITICAL,"set command pattern compilation error %d", res);
		abort();
	}
	if ((res = regcomp(&managervalex, managervalpattern, REG_EXTENDED)) != 0) {
		LOG_STDERR(PHIDGET_LOG_CRITICAL,"set command pattern compilation error %d", res);
		abort();
	}

	NetworkInitialized = PTRUE;

	return EPHIDGET_OK;
}

void cleanup_after_socket(void *ptr)
{
	CPhidgetSocketClientHandle serverInfo = ptr;
	CServerList *travServers;
	CServerInfoHandle foundServer = NULL;
	CPhidgetListHandle travPhidgets;
	CPhidgetDictionaryListHandle travDicts;
	CPhidgetManagerListHandle travManagers;

	CPhidgetListHandle detachEvents = NULL;
	CPhidgetListHandle disconnectEvents = NULL;

	//make sure the auth threads are not running
	//we can safely kill them because if m_ThreadHandle != 0 then they are simply waiting on ServerLockLock, which we hold here
	/*if(serverInfo->auth_thread.thread_status == TRUE)
	{
		serverInfo->auth_thread.thread_status = FALSE;
		CThread_join(&serverInfo->auth_thread);
	}
	if(serverInfo->auth_error_thread.thread_status == TRUE)
	{
		serverInfo->auth_error_thread.thread_status = FALSE;
		CThread_kill(&serverInfo->auth_error_thread);
	}*/

	//wait for the threads to be done
	while(serverInfo->auth_thread.thread_status == TRUE)
	{
		SLEEP(10);
	}
	while(serverInfo->auth_error_thread.thread_status == TRUE)
	{
		SLEEP(10);
	}

	/* For each Phidget associated with this server, send a detach event, and a disconnect event */
	// We then get rid of the socket object everywhere
	// If this is called because we already closed all the connections to this socket, no events will be raised because
	//  all the handles are already nulled...
	CThread_mutex_lock(&serverLock);

	for(travServers = servers; travServers; travServers = travServers->next)
	{
		if(travServers->serverInfo->server->socket == serverInfo->socket)
		{
			foundServer = travServers->serverInfo;
			CPhidget_clearStatusFlag(&foundServer->server->status, PHIDGETSOCKET_CONNECTED_FLAG, &foundServer->server->lock);
			for(travPhidgets = foundServer->phidgets; travPhidgets; travPhidgets = travPhidgets->next)
			{
				if(CPhidget_statusFlagIsSet(travPhidgets->phid->status, PHIDGET_ATTACHED_FLAG))
				{
					CPhidget_clearStatusFlag(&travPhidgets->phid->status, PHIDGET_ATTACHED_FLAG, &travPhidgets->phid->lock);
					CPhidget_setStatusFlag(&travPhidgets->phid->status, PHIDGET_DETACHING_FLAG, &travPhidgets->phid->lock);
					if(travPhidgets->phid->fptrDetach)
						CList_addToList((CListHandle *)&detachEvents, travPhidgets->phid, CPhidget_areEqual);
				}

				CPhidget_clearStatusFlag(&travPhidgets->phid->status, PHIDGET_SERVER_CONNECTED_FLAG, &travPhidgets->phid->lock);
				if(travPhidgets->phid->fptrDisconnect)
					CList_addToList((CListHandle *)&disconnectEvents, travPhidgets->phid, CPhidgetHandle_areEqual);	
			}
			for(travDicts = foundServer->dictionaries; travDicts; travDicts = travDicts->next)
			{
				CPhidget_clearStatusFlag(&travDicts->dict->status, PHIDGET_ATTACHED_FLAG, &travDicts->dict->lock);

				CPhidget_clearStatusFlag(&travDicts->dict->status, PHIDGET_SERVER_CONNECTED_FLAG, &travDicts->dict->lock);
				if(travDicts->dict->fptrDisconnect)
					CList_addToList((CListHandle *)&disconnectEvents, travDicts->dict, CPhidgetHandle_areEqual);
			}
			for(travManagers = foundServer->managers; travManagers; travManagers = travManagers->next)
			{
				CPhidget_clearStatusFlag(&travManagers->phidm->status, PHIDGET_ATTACHED_FLAG, &travManagers->phidm->lock);

				CPhidget_clearStatusFlag(&travManagers->phidm->status, PHIDGET_SERVER_CONNECTED_FLAG, &travManagers->phidm->lock);
				if(travManagers->phidm->fptrDisconnect)
					CList_addToList((CListHandle *)&disconnectEvents, travManagers->phidm, CPhidgetHandle_areEqual);
			}
			break;
		}
	}

	for(travPhidgets = disconnectEvents; travPhidgets; travPhidgets = travPhidgets->next)
	{
		travPhidgets->phid->fptrDisconnect((CPhidgetHandle)travPhidgets->phid, travPhidgets->phid->fptrDisconnectptr);
		//internal_async_network_error_handler("The Network Connection has been Closed", travPhidgets->phid);
	}
	for(travPhidgets = detachEvents; travPhidgets; travPhidgets = travPhidgets->next)
	{
		travPhidgets->phid->fptrDetach((CPhidgetHandle)travPhidgets->phid, travPhidgets->phid->fptrDetachptr);
		CPhidget_clearStatusFlag(&travPhidgets->phid->status, PHIDGET_DETACHING_FLAG, &travPhidgets->phid->lock);
	}

	CList_emptyList((CListHandle *)&detachEvents, FALSE, NULL);
	CList_emptyList((CListHandle *)&disconnectEvents, FALSE, NULL);

	if(foundServer)
	{
		//need to null all servers in list
		//do it herer so detach/disconnect handlers can call getAddress, etc.
		for(travPhidgets = foundServer->phidgets; travPhidgets; travPhidgets = travPhidgets->next)
		{
			travPhidgets->phid->networkInfo->server = NULL;
		}
		for(travDicts = foundServer->dictionaries; travDicts; travDicts = travDicts->next)
		{
			travDicts->dict->networkInfo->server = NULL;
		}
		for(travManagers = foundServer->managers; travManagers; travManagers = travManagers->next)
		{
			travManagers->phidm->networkInfo->server = NULL;
		}

		//then remove from server list
		CList_removeFromList((CListHandle *)&servers, foundServer, CServerInfo_areEqual, PTRUE, CServerInfo_free);
	}
	CThread_mutex_unlock(&serverLock);
}

int setupHeartbeat(CPhidgetSocketClientHandle server, pdc_listen_id_t *id)
{
	char errdesc[1024];
	char listenKey[1024];
	struct sockaddr name;
	struct sockaddr_in *name_in;
	socklen_t namelen = sizeof(name);

	char key[1024], val[1024];

	TESTPTR(server)

	getsockname(server->socket, &name, &namelen);
	name_in = (struct sockaddr_in *)&name;

	//listen for heartbeat events
	snprintf(listenKey, sizeof(listenKey), "/PCK/Heartbeat/%s/%d", inet_ntoa(name_in->sin_addr), (int)name_in->sin_port);
	
	CThread_mutex_lock(&server->pdc_lock);
	if (!(*id = pdc_listen(server->pdcs, listenKey, network_heartbeat_event_handler, server, errdesc, sizeof (errdesc))))
	{
		LOG(PHIDGET_LOG_DEBUG,"pdc_listen: %s", errdesc);
		CThread_mutex_unlock(&server->pdc_lock);
		return EPHIDGET_UNEXPECTED;
	}
	CThread_mutex_unlock(&server->pdc_lock);

	snprintf(key, sizeof(key), "/PCK/Heartbeat/%s/%d", inet_ntoa(name_in->sin_addr), (int)name_in->sin_port);
	snprintf(val, sizeof(val), "%d", server->heartbeatCount);
	setTimeNow(&server->lastHeartbeatTime);
	server->waitingForHeartbeat = PTRUE;
	pdc_async_set(server->pdcs, key, val, (int)strlen(val), PTRUE, NULL, NULL);

	return EPHIDGET_OK;
}

int setupKeysAndListeners_phidget(CPhidgetHandle phid, pdc_listen_id_t *id)
{
	char errdesc[1024];
	char listenKey[1024];
	struct sockaddr name;
	struct sockaddr_in *name_in;
	socklen_t namelen = sizeof(name);

	char key[1024], val[1024];

	TESTPTR(phid)
	TESTPTR(phid->networkInfo)
	TESTPTR(phid->networkInfo->server)

	//listen for everything to do with this specific phidget
	if(phid->specificDevice)
	{
		snprintf(listenKey, sizeof(listenKey), "^/PSK/%s/%d/", Phid_DeviceName[phid->deviceID], phid->serialNumber);
	}
	else
	{
		snprintf(listenKey, sizeof(listenKey), "^/PSK/%s/", Phid_DeviceName[phid->deviceID]);
	}
	
	CThread_mutex_lock(&phid->networkInfo->server->pdc_lock);
	if (!(*id = pdc_listen(phid->networkInfo->server->pdcs, listenKey, network_phidget_event_handler, phid, errdesc, sizeof (errdesc))))
	{
		LOG(PHIDGET_LOG_DEBUG,"pdc_listen: %s", errdesc);
		CThread_mutex_unlock(&phid->networkInfo->server->pdc_lock);
		return EPHIDGET_UNEXPECTED;
	}
	CThread_mutex_unlock(&phid->networkInfo->server->pdc_lock);
	//Open the remote device
	//get socket info
	getsockname(phid->networkInfo->server->socket, &name, &namelen);
	name_in = (struct sockaddr_in *)&name;
	if(phid->specificDevice)
	{
		snprintf(key, sizeof(key), "/PCK/Client/%s/%d/%s/%d", inet_ntoa(name_in->sin_addr), (int)name_in->sin_port, 
			Phid_DeviceName[phid->deviceID], phid->serialNumber);
	}
	else
	{
		snprintf(key, sizeof(key), "/PCK/Client/%s/%d/%s", inet_ntoa(name_in->sin_addr), (int)name_in->sin_port, 
			Phid_DeviceName[phid->deviceID]);
	}
	snprintf(val, sizeof(val), "Open");
	pdc_async_set(phid->networkInfo->server->pdcs, key, val, (int)strlen(val), PTRUE, internal_async_network_error_handler, phid);

	return EPHIDGET_OK;
}

int setupKeysAndListeners_manager(CPhidgetManagerHandle phidm, pdc_listen_id_t *id)
{
	char errdesc[1024];
	char listenKey[1024];

	TESTPTR(phidm)
	TESTPTR(phidm->networkInfo)
	TESTPTR(phidm->networkInfo->server)

	//listen for everything to do with the PhidgetManager
	snprintf(listenKey, sizeof(listenKey), "^/PSK/List/");

	CThread_mutex_lock(&phidm->networkInfo->server->pdc_lock);
	if (!(*id = pdc_listen(phidm->networkInfo->server->pdcs, listenKey, network_manager_event_handler, phidm, errdesc, sizeof (errdesc))))
	{
		LOG(PHIDGET_LOG_DEBUG,"pdc_listen: %s", errdesc);
		CThread_mutex_unlock(&phidm->networkInfo->server->pdc_lock);
		return EPHIDGET_UNEXPECTED;
	}
	CThread_mutex_unlock(&phidm->networkInfo->server->pdc_lock);

	return EPHIDGET_OK;
}

typedef struct _AuthHandlerThreadData
{
	void *ptr;
	void (*error)(const char *errdesc, void *arg);
} AuthHandlerThreadData, *AuthHandlerThreadDataHandle;

//An auth succeeded so now we mark this as a connected server, send out connect events, etc.
CThread_func_return_t async_authorization_handler_thread(CThread_func_arg_t lpdwParam)
{
	AuthHandlerThreadDataHandle data = (AuthHandlerThreadDataHandle)lpdwParam;

	CPhidgetListHandle travPhidgets;
	CPhidgetDictionaryListHandle travDicts;
	CPhidgetManagerListHandle travManagers;

	CPhidgetListHandle connectEvents = NULL;
	CPhidgetListHandle phidErrorEvents = NULL;
	CPhidgetManagerListHandle managerErrorEvents = NULL;

	char errdesc[1024];
	CServerInfoHandle newServerInfo = (CServerInfoHandle)data->ptr;
	
	//make sure that we can cancel this thread
#ifndef _WINDOWS
	int temp;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &temp);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &temp);
#endif

	//CThread_mutex_lock(&serverLockLock);
	CThread_mutex_lock(&serverLock);

	//close was called, pdcs isn't valid any longer
	if(!newServerInfo->server->pdcs)
	{
		newServerInfo->server->auth_thread.thread_status = FALSE;

		CThread_mutex_unlock(&serverLock);
		return (CThread_func_return_t)0;
	}
	
	CThread_mutex_lock(&newServerInfo->server->pdc_lock);
	if(!pdc_enable_periodic_reports(newServerInfo->server->pdcs, 10, errdesc, sizeof(errdesc)))
	{
		LOG(PHIDGET_LOG_DEBUG,"pdc_enable_periodic_reports: %s", errdesc);

		//this will call back to async_authorization_error_handler - the whole server connection is now bad
		if(data->error)
		{
			data->error(errdesc, data->ptr);
		}

		CThread_mutex_unlock(&newServerInfo->server->pdc_lock);
		
		newServerInfo->server->auth_thread.thread_status = FALSE;

		CThread_mutex_unlock(&serverLock);
		//CThread_mutex_unlock(&serverLockLock);
		
		return (CThread_func_return_t)0;
	}
	CThread_mutex_unlock(&newServerInfo->server->pdc_lock);

	//set connected
	CPhidget_setStatusFlag(&newServerInfo->server->status, PHIDGETSOCKET_CONNECTED_FLAG, &newServerInfo->server->lock);
	CPhidget_clearStatusFlag(&newServerInfo->server->status, PHIDGETSOCKET_CONNECTING_FLAG, &newServerInfo->server->lock);

	//Setup the heartbeat listener
	setupHeartbeat(newServerInfo->server, &newServerInfo->server->heartbeat_listen_id);

	//now run through all phids, managers, dicts in the list and connect them
	for(travPhidgets = newServerInfo->phidgets; travPhidgets; travPhidgets = travPhidgets->next)
	{
		CPhidget_setStatusFlag(&travPhidgets->phid->status, PHIDGET_SERVER_CONNECTED_FLAG, &travPhidgets->phid->lock);	
		if(setupKeysAndListeners_phidget(travPhidgets->phid, &travPhidgets->phid->networkInfo->listen_id))
		{
			if(travPhidgets->phid->fptrError)
				CList_addToList((CListHandle *)&phidErrorEvents, travPhidgets->phid, CPhidgetHandle_areEqual);
			CPhidget_clearStatusFlag(&travPhidgets->phid->status, PHIDGET_SERVER_CONNECTED_FLAG, &travPhidgets->phid->lock);	
			//we have to remove this phid from the server list, 
			//and remove it's reference to the server, so that another connection attempt will be made.
			travPhidgets->phid->networkInfo->server = NULL;
		}
		else
		{
			if(travPhidgets->phid->fptrConnect)
				CList_addToList((CListHandle *)&connectEvents, travPhidgets->phid, CPhidgetHandle_areEqual);
		}
	}
	for(travDicts = newServerInfo->dictionaries; travDicts; travDicts = travDicts->next)
	{
		CPhidget_setStatusFlag(&travDicts->dict->status, PHIDGET_SERVER_CONNECTED_FLAG, &travDicts->dict->lock);
		CPhidget_setStatusFlag(&travDicts->dict->status, PHIDGET_ATTACHED_FLAG, &travDicts->dict->lock);
		if(travDicts->dict->fptrConnect)
			CList_addToList((CListHandle *)&connectEvents, travDicts->dict, CPhidgetHandle_areEqual);
	}
	for(travManagers = newServerInfo->managers; travManagers; travManagers = travManagers->next)
	{
		CPhidget_setStatusFlag(&travManagers->phidm->status, PHIDGET_SERVER_CONNECTED_FLAG, &travManagers->phidm->lock);
		CPhidget_setStatusFlag(&travManagers->phidm->status, PHIDGET_ATTACHED_FLAG, &travManagers->phidm->lock);
		if(setupKeysAndListeners_manager(travManagers->phidm, &travManagers->phidm->networkInfo->listen_id))
		{
			if(travManagers->phidm->fptrError)
				CList_addToList((CListHandle *)&managerErrorEvents, travManagers->phidm, CPhidgetHandle_areEqual);	
			CPhidget_clearStatusFlag(&travManagers->phidm->status, PHIDGET_SERVER_CONNECTED_FLAG, &travManagers->phidm->lock);
			CPhidget_clearStatusFlag(&travManagers->phidm->status, PHIDGET_ATTACHED_FLAG, &travManagers->phidm->lock);
			travManagers->phidm->networkInfo->server = NULL;
		}
		else
		{
			if(travManagers->phidm->fptrConnect)
				CList_addToList((CListHandle *)&connectEvents, travManagers->phidm, CPhidgetHandle_areEqual);
		}
	}
	
	//do this here or it could interfere with close
	for(travPhidgets = phidErrorEvents; travPhidgets; travPhidgets = travPhidgets->next)
	{
		removeFromServerInfoList(newServerInfo, travPhidgets->phid, PHIDGET);
	}
	for(travManagers = managerErrorEvents; travManagers; travManagers = travManagers->next)
	{
		removeFromServerInfoList(newServerInfo, travManagers->phidm, MANAGER);
	}

	//set this here so we can call close from events
	newServerInfo->server->auth_thread.thread_status = FALSE;
	
	CThread_mutex_unlock(&serverLock);
	//CThread_mutex_unlock(&serverLockLock);

	//send out events
	for(travPhidgets = connectEvents; travPhidgets; travPhidgets = travPhidgets->next)
	{
		travPhidgets->phid->fptrConnect((CPhidgetHandle)travPhidgets->phid, travPhidgets->phid->fptrConnectptr);
	}
	for(travPhidgets = phidErrorEvents; travPhidgets; travPhidgets = travPhidgets->next)
	{
		travPhidgets->phid->fptrError((CPhidgetHandle)travPhidgets->phid, travPhidgets->phid->fptrErrorptr, EEPHIDGET_NETWORK, "Error setting up listeners");
	}
	for(travManagers = managerErrorEvents; travManagers; travManagers = travManagers->next)
	{
		travManagers->phidm->fptrError((CPhidgetManagerHandle)travManagers->phidm, travManagers->phidm->fptrErrorptr, EEPHIDGET_NETWORK, "Error setting up listeners");
	}
	return (CThread_func_return_t)0;
}

//this is only called when an auth succeeds
void async_authorization_handler(void *ptr, void (*error)(const char *errdesc, void *arg))
{
	//need to start a thread because we can't call synchronous network functions like pdc_enable_periodic_reports from this callback
	//They will just deadlock
	CServerInfoHandle newServerInfo = (CServerInfoHandle)ptr;
	AuthHandlerThreadDataHandle data;
	data = malloc(sizeof(AuthHandlerThreadData));
	data->error = error;
	data->ptr = ptr;
	
	//we do need to keep track of this thread so we can kill it on close if needed
	// - it's associated with the server, and there should only be one running per server at a time!
	if(newServerInfo->server->auth_thread.thread_status == TRUE)
	{
		newServerInfo->server->auth_thread.thread_status = FALSE;
		CThread_join(&newServerInfo->server->auth_thread);
	}

	newServerInfo->server->auth_thread.thread_status = TRUE;
	CThread_create(&newServerInfo->server->auth_thread, async_authorization_handler_thread, data);
}

typedef struct _AuthErrorHandlerThreadData
{
	char *error;
	void *ptr;
} AuthErrorHandlerThreadData, *AuthErrorHandlerThreadDataHandle;

CThread_func_return_t async_authorization_error_handler_thread(CThread_func_arg_t lpdwParam)
{
	AuthErrorHandlerThreadDataHandle data = (AuthErrorHandlerThreadDataHandle)lpdwParam;
	
	CPhidgetListHandle travPhidgets;
	CPhidgetDictionaryListHandle travDicts;
	CPhidgetManagerListHandle travManagers;

	CPhidgetListHandle errorEvents = NULL;

	CServerInfoHandle newServerInfo = (CServerInfoHandle)data->ptr;

	int errCode;

	const char *badPassStr = "Authentication Failed";
	const char *badVerStr = "Version Mismatch";
	
	//make sure that we can cancel this thread
#ifndef _WINDOWS
	int temp;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &temp);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &temp);
#endif
	
	//CThread_mutex_lock(&serverLockLock);
	CThread_mutex_lock(&serverLock);

	if(strlen(data->error)>=strlen(badPassStr) && !strncmp(data->error, badPassStr, strlen(badPassStr)))
	{
		CPhidget_setStatusFlag(&newServerInfo->server->status, PHIDGETSOCKET_AUTHERROR_FLAG, &newServerInfo->server->lock);
		errCode = EEPHIDGET_BADPASSWORD;
	}
	else if(strlen(data->error)>=strlen(badVerStr) && !strncmp(data->error, badVerStr, strlen(badVerStr)))
	{
		CPhidget_setStatusFlag(&newServerInfo->server->status, PHIDGETSOCKET_CONNECTIONERROR_FLAG, &newServerInfo->server->lock);
		errCode = EEPHIDGET_BADVERSION;
	}
	else
	{
		CPhidget_setStatusFlag(&newServerInfo->server->status, PHIDGETSOCKET_CONNECTIONERROR_FLAG, &newServerInfo->server->lock);
		errCode = EEPHIDGET_NETWORK;
	}

	//for each in server list, do the error handler - don't do a disconnect handler
	//also remove the socket reference from the devices, and clears the lists in the serverInfo so that closeServer works
	//PHIDGET_INERROREVENT_FLAG is set so that things don't break if we call close in the error event
	//(don't want findActiveDevices to use these until after the error event fire)
	for(travPhidgets = newServerInfo->phidgets; travPhidgets; travPhidgets = travPhidgets->next)
	{
		if(travPhidgets->phid->fptrError)
		{
			CPhidget_setStatusFlag(&travPhidgets->phid->status, PHIDGET_INERROREVENT_FLAG, &travPhidgets->phid->lock);
			CList_addToList((CListHandle *)&errorEvents, travPhidgets->phid, CPhidgetHandle_areEqual);
		}
		travPhidgets->phid->networkInfo->server = NULL;
	}
	for(travDicts = newServerInfo->dictionaries; travDicts; travDicts = travDicts->next)
	{
		if(travDicts->dict->fptrError)
		{
			CPhidget_setStatusFlag(&travDicts->dict->status, PHIDGET_INERROREVENT_FLAG, &travDicts->dict->lock);
			CList_addToList((CListHandle *)&errorEvents, travDicts->dict, CPhidgetHandle_areEqual);
		}
		travDicts->dict->networkInfo->server = NULL;
	}
	for(travManagers = newServerInfo->managers; travManagers; travManagers = travManagers->next)
	{
		if(travManagers->phidm->fptrError)
		{
			CPhidget_setStatusFlag(&travManagers->phidm->status, PHIDGET_INERROREVENT_FLAG, &travManagers->phidm->lock);
			CList_addToList((CListHandle *)&errorEvents, travManagers->phidm, CPhidgetHandle_areEqual);
		}
		travManagers->phidm->networkInfo->server = NULL;
	}

	CList_emptyList((CListHandle *)&newServerInfo->phidgets, PFALSE, NULL);
	CList_emptyList((CListHandle *)&newServerInfo->managers, PFALSE, NULL);
	CList_emptyList((CListHandle *)&newServerInfo->dictionaries, PFALSE, NULL);

	//clear this here, so we don't mess things up with findActiveDevices trying to attach these
	CPhidget_clearStatusFlag(&newServerInfo->server->status, PHIDGETSOCKET_CONNECTING_FLAG, &newServerInfo->server->lock);

	//do this here so we don't attempt a join in closeServer
	newServerInfo->server->auth_error_thread.thread_status = FALSE;

	//now we call closserver explicitely - it doens't matter if they call close in the error event, as the server is already closed.
	closeServer(newServerInfo, PFALSE);

	CThread_mutex_unlock(&serverLock);
	//CThread_mutex_unlock(&serverLockLock);
	

	for(travPhidgets = errorEvents; travPhidgets; travPhidgets = travPhidgets->next)
	{
		//they can block, close the phidgets, delete it, etc. in the error event and that shouldn't break anything.
		//This thread will just exit right away, and that will be that...
		travPhidgets->phid->fptrError((CPhidgetHandle)travPhidgets->phid, travPhidgets->phid->fptrErrorptr, errCode, data->error);
		CPhidget_clearStatusFlag(&travPhidgets->phid->status, PHIDGET_INERROREVENT_FLAG, &travPhidgets->phid->lock);
	}

	free(data->error);
	free(data);

	return (CThread_func_return_t)0;
}

void async_authorization_error_handler(const char *error, void *ptr)
{
	//need to start a thread because we can't call synchronous netowrk functions from this callback
	CServerInfoHandle newServerInfo = (CServerInfoHandle)ptr;
	AuthErrorHandlerThreadDataHandle data;
	data = malloc(sizeof(AuthErrorHandlerThreadData));
	data->error = strdup(error);
	data->ptr = ptr;

	//we do need to keep track of this thread so we can kill it on close if needed
	// - it's associated with the server, and there should only be one running per server at a time!
	if(newServerInfo->server->auth_error_thread.thread_status == TRUE)
	{
		newServerInfo->server->auth_error_thread.thread_status = FALSE;
		CThread_join(&newServerInfo->server->auth_error_thread);
	}
	
	newServerInfo->server->auth_error_thread.thread_status = TRUE;
	CThread_create(&newServerInfo->server->auth_error_thread, async_authorization_error_handler_thread, data);
}

//This is only ever called once at a time
int connectToServer(CPhidgetRemoteHandle remoteInfo, char *errdesc, int errlen, void *list_element, ListElementType type)
{
#ifdef ZEROCONF_LOOKUP
	struct hostent * addr_lookup;
	const char * addr_lookup_str;
#endif
	int result = EPHIDGET_OK;
	CServerInfoHandle foundServer = NULL;
	CServerInfoHandle newServerInfo;
	
	//Initialize the network if not already done
	if(!NetworkInitialized)
		if((result = InitializeNetworking()))
			return result;
	
	//Creating a new server info object
	if(!(newServerInfo = malloc(sizeof(CServerInfo))))
		return EPHIDGET_NOMEMORY;
	ZEROMEM(newServerInfo, sizeof(CServerInfo));
	if((result = CPhidgetSocketClient_create(&newServerInfo->server)))
		return result;
	
	//openRemoteIP
	if(remoteInfo->requested_address != NULL)
	{
		if(!(newServerInfo->server->address = strdup(remoteInfo->requested_address)))
			return EPHIDGET_NOMEMORY;
		if(!(newServerInfo->server->port = strdup(remoteInfo->requested_port)))
			return EPHIDGET_NOMEMORY;
	}
	//openRemote
#ifdef USE_ZEROCONF
	else //seems we've found an mDNS server we want to connect to
	{
		if(getZeroconfHostPort(remoteInfo))
			return EPHIDGET_NETWORK;
		if(!(newServerInfo->server->address = strdup(remoteInfo->zeroconf_host)))
			return EPHIDGET_NOMEMORY;
		if(!(newServerInfo->server->port = strdup(remoteInfo->zeroconf_port)))
			return EPHIDGET_NOMEMORY;
	}
#else
	else
	{
		return EPHIDGET_INVALIDARG;
	}
#endif

	//check to see if there is already a connection to this server
	result = CList_findInList((CListHandle)servers, newServerInfo, CServerInfo_areEqual, (void **)&foundServer);
	switch(result)
	{
		case EPHIDGET_OK: //Found
			remoteInfo->server = foundServer->server;
			CServerInfo_free(newServerInfo); newServerInfo = NULL;

			//add device to list in serverInfo here
			if((result = addToServerInfoList(foundServer, list_element, type)))
				return result;

			//if the server is already connected, register listeners, and send out connect event, etc.
			// - otherwise, they will be registered when it connects
			if(CPhidget_statusFlagIsSet(remoteInfo->server->status, PHIDGETSOCKET_CONNECTED_FLAG))
			{	
				switch(type)
				{
					case PHIDGET:
						{
							CPhidgetHandle phid = (CPhidgetHandle)list_element;
							CPhidget_setStatusFlag(&phid->status, PHIDGET_SERVER_CONNECTED_FLAG, &phid->lock);
							if(setupKeysAndListeners_phidget(phid, &phid->networkInfo->listen_id))
							{
								CPhidget_clearStatusFlag(&phid->status, PHIDGET_SERVER_CONNECTED_FLAG, &phid->lock);
								if(phid->fptrError)
									phid->fptrError((CPhidgetHandle)phid, phid->fptrErrorptr, EEPHIDGET_NETWORK, "Error setting up listeners.");
								removeFromServerInfoList(foundServer, list_element, type);
								remoteInfo->server = NULL;
							}
							else
							{
								if(phid->fptrConnect)
									phid->fptrConnect((CPhidgetHandle)phid, phid->fptrConnectptr);
							}
						}
						break;
					case MANAGER:
						{
							CPhidgetManagerHandle phidm = (CPhidgetManagerHandle)list_element;
							CPhidget_setStatusFlag(&phidm->status, PHIDGET_SERVER_CONNECTED_FLAG, &phidm->lock);
							CPhidget_setStatusFlag(&phidm->status, PHIDGET_ATTACHED_FLAG, &phidm->lock);
							if(setupKeysAndListeners_manager(phidm, &phidm->networkInfo->listen_id))
							{
								CPhidget_clearStatusFlag(&phidm->status, PHIDGET_SERVER_CONNECTED_FLAG, &phidm->lock);
								CPhidget_clearStatusFlag(&phidm->status, PHIDGET_ATTACHED_FLAG, &phidm->lock);
								if(phidm->fptrError)
									phidm->fptrError((CPhidgetManagerHandle)phidm, phidm->fptrErrorptr, EEPHIDGET_NETWORK, "Error setting up listeners.");
								removeFromServerInfoList(foundServer, list_element, type);
								remoteInfo->server = NULL;
							}
							else
							{
								if(phidm->fptrConnect)
									phidm->fptrConnect((CPhidgetManagerHandle)phidm, phidm->fptrConnectptr);
							}
						}
						break;
					case DICTIONARY:
						{
							CPhidgetDictionaryHandle dict = (CPhidgetDictionaryHandle)list_element;
							CPhidget_setStatusFlag(&dict->status, PHIDGET_SERVER_CONNECTED_FLAG, &dict->lock);
							CPhidget_setStatusFlag(&dict->status, PHIDGET_ATTACHED_FLAG, &dict->lock);
							if(dict->fptrConnect)
								dict->fptrConnect((CPhidgetDictionaryHandle)dict, dict->fptrConnectptr);
						}
						break;
				}
			}
			break;
		case EPHIDGET_NOTFOUND: //Not Found

			//If the connection fails, this never even makes it into the server list, and the Phidget never sees it's server get initialized
#ifdef ZEROCONF_LOOKUP
			/* this will resolve to an IP address, including .local hostnames (for SBC, because it can't resolve .local hostnames on its own) */
			addr_lookup = mdns_gethostbyname(newServerInfo->server->address);
			if (addr_lookup == NULL) {
				/* didn't work, just use address */
			   addr_lookup_str = newServerInfo->server->address;
			} else {
			   unsigned int i=0;
			   if ( addr_lookup -> h_addr_list[i] != NULL) {
				  addr_lookup_str = inet_ntoa( *( struct in_addr*)( addr_lookup -> h_addr_list[i]));
			   }
			}

			if (!stream_server_connect(addr_lookup_str, newServerInfo->server->port, &newServerInfo->server->socket, &remoteInfo->cancelSocket, errdesc, errlen))
			{
#else
			if (!stream_server_connect(newServerInfo->server->address, newServerInfo->server->port, &newServerInfo->server->socket, &remoteInfo->cancelSocket, errdesc, errlen))
			{
#endif
				LOG(PHIDGET_LOG_DEBUG,"connect(%s:%s): %s", newServerInfo->server->address, newServerInfo->server->port, errdesc);
				CServerInfo_free(newServerInfo); newServerInfo = NULL;
				if(errno==ECANCELED)
					return EPHIDGET_INTERRUPTED;
				return EPHIDGET_NETWORK;
			}
			
			if (!(newServerInfo->server->pdcs = pdc_session_alloc(newServerInfo->server->socket, pu_read, newServerInfo->server->socket, 
				pu_write, pu_close, newServerInfo->server, cleanup_after_socket))) {
				fflush(stderr);
				CServerInfo_free(newServerInfo); newServerInfo = NULL;
				return EPHIDGET_NOTFOUND;
			}

			//set authenticating state
			CPhidget_setStatusFlag(&newServerInfo->server->status, PHIDGETSOCKET_CONNECTING_FLAG, &newServerInfo->server->lock);
			
			//set server for this device to new server
			remoteInfo->server = newServerInfo->server;

			//add it to the list - note we are allowed to have connecting servers in the list!
			if((result = CList_addToList((CListHandle *)&servers, newServerInfo, CServerInfo_areEqual)))
				return result;

			//add device to list in serverInfo here
			if((result = addToServerInfoList(newServerInfo, list_element, type)))
				return result;

			//connection is made - start authorization - this should return as one of two callbacks - success or error
			//TODO: have some sort of timeout for the connection to succeed or fail
			pdc_async_authorize(newServerInfo->server->pdcs, ws_protocol_ver, 
				remoteInfo->password, async_authorization_handler,
				async_authorization_error_handler, newServerInfo);
			
			//Start looking for a timeout
			setTimeNow(&newServerInfo->server->lastHeartbeatTime);
			newServerInfo->server->waitingForHeartbeat = PTRUE;	

			break;
		default:
			return result;
	}
	return EPHIDGET_OK;
}

int MonitorHeartbeats()
{
	CServerList *travServers;
	CPhidgetSocketClientHandle server;
	struct sockaddr name;
	struct sockaddr_in *name_in;
	socklen_t namelen = sizeof(name);

	char key[1024], val[1024];

	CThread_mutex_lock(&serverLockLock);
	CThread_mutex_lock(&serverLock);

start:
	for(travServers = servers; travServers; travServers = travServers->next)
	{
		if(travServers->serverInfo && travServers->serverInfo->server)
		{
			server = travServers->serverInfo->server;

			if(server->waitingForHeartbeat)
			{
				//if we've been waiting too long, then signal disconnect
				double waitTime = timeSince(&server->lastHeartbeatTime);
				//if we haven't recieved any heartbeats, set the timeout high (4*4 = 16 seconds)
				//This is so that really slow connections will get through the auth stage
				double avgPingTime = ((server->avgHeartbeatTimeCount > 0) ? (server->avgHeartbeatTime / server->avgHeartbeatTimeCount) : 4.0);

				//10 times the average ping time, or 2 seconds, whichever is larger
				if(waitTime > (avgPingTime * 10) && waitTime > 2.0)
				{
					server->waitingForHeartbeat = PFALSE;
					server->avgHeartbeatTime = 0;
					server->avgHeartbeatTimeCount = 0;
					//close the socket - this will filter detach events down the chain
					closeServer(travServers->serverInfo, PTRUE);
					//need to exit the loop because we removed the server from it!
					goto start;
				}
			}
			else
			{
				//new heartbeat every 2 seconds
				if(timeSince(&server->lastHeartbeatTime) > 2.0)
				{
					getsockname(server->socket, &name, &namelen);
					name_in = (struct sockaddr_in *)&name;
					snprintf(key, sizeof(key), "/PCK/Heartbeat/%s/%d", inet_ntoa(name_in->sin_addr), (int)name_in->sin_port);
					snprintf(val, sizeof(val), "%d", server->heartbeatCount);
					server->waitingForHeartbeat = PTRUE;
					setTimeNow(&server->lastHeartbeatTime);
					pdc_async_set(server->pdcs, key, val, (int)strlen(val), PTRUE, NULL, NULL);
				}
			}
		}
	}

	CThread_mutex_unlock(&serverLock);
	CThread_mutex_unlock(&serverLockLock);

	return EPHIDGET_OK;
}

// This will try to connect unconnected openRemoteIP devices and find any zeroconf matches
int
FindActiveRemoteDevices()
{
	CPhidgetList *activePhidTrav = 0, *zeroconfPhidTrav;
	CPhidgetManagerList *activeManagerTrav;
	CPhidgetDictionaryList *activeDictTrav;
	CPhidgetRemoteList *zeroconfServerTrav;
	int result = 0;
	char errdesc[1024];
	void *err_device = NULL;

	errdesc[0] = '\0';

	//Zeroconf Phidgets
	CThread_mutex_lock(&activeRemotePhidgetsLock);
	CThread_mutex_lock(&zeroconfPhidgetsLock);

	for (zeroconfPhidTrav=zeroconfPhidgets; zeroconfPhidTrav; zeroconfPhidTrav = zeroconfPhidTrav->next)
	{
		//first look for specific serial numbers
		for (activePhidTrav=activeRemotePhidgets; activePhidTrav; activePhidTrav = activePhidTrav->next)
		{
			if(!CPhidget_statusFlagIsSet(activePhidTrav->phid->status, PHIDGET_INERROREVENT_FLAG)
				&& activePhidTrav->phid->networkInfo->server == NULL
				&& CPhidget_areExtraEqual(activePhidTrav->phid, zeroconfPhidTrav->phid) 
				&& activePhidTrav->phid->networkInfo->requested_address==NULL
				&& (activePhidTrav->phid->networkInfo->requested_serverID == NULL 
					|| !strcmp(activePhidTrav->phid->networkInfo->requested_serverID,zeroconfPhidTrav->phid->networkInfo->zeroconf_server_id)))
			{
				CThread_mutex_lock(&serverLockLock);
				CThread_mutex_lock(&serverLock);

				free(activePhidTrav->phid->networkInfo->zeroconf_name); activePhidTrav->phid->networkInfo->zeroconf_name = strdup(zeroconfPhidTrav->phid->networkInfo->zeroconf_name);
				free(activePhidTrav->phid->networkInfo->zeroconf_type); activePhidTrav->phid->networkInfo->zeroconf_type = strdup(zeroconfPhidTrav->phid->networkInfo->zeroconf_type);
				free(activePhidTrav->phid->networkInfo->zeroconf_domain); activePhidTrav->phid->networkInfo->zeroconf_domain = strdup(zeroconfPhidTrav->phid->networkInfo->zeroconf_domain);

				if((result = connectToServer(activePhidTrav->phid->networkInfo, errdesc, sizeof(errdesc), activePhidTrav->phid, PHIDGET)) != EPHIDGET_OK)
				{
					err_device = activePhidTrav->phid;
					activePhidTrav->phid->networkInfo->server = NULL;
					CThread_mutex_unlock(&serverLock);
					CThread_mutex_unlock(&serverLockLock);
					CThread_mutex_unlock(&zeroconfPhidgetsLock);
					CThread_mutex_unlock(&activeRemotePhidgetsLock);
					goto error_event;
				}

				CThread_mutex_unlock(&serverLock);
				CThread_mutex_unlock(&serverLockLock);

				goto next_zeroconf_phidget;
			}
		}

		//then take care of -1 (any) serial numbers
		for (activePhidTrav=activeRemotePhidgets; activePhidTrav; activePhidTrav = activePhidTrav->next)
		{
			if(!CPhidget_statusFlagIsSet(activePhidTrav->phid->status, PHIDGET_INERROREVENT_FLAG)
				&& !activePhidTrav->phid->networkInfo->server 
				&& CPhidget_areEqual(activePhidTrav->phid, zeroconfPhidTrav->phid) 
				&& activePhidTrav->phid->networkInfo->requested_address==NULL
				&& (activePhidTrav->phid->networkInfo->requested_serverID == NULL 
					|| !strcmp(activePhidTrav->phid->networkInfo->requested_serverID,zeroconfPhidTrav->phid->networkInfo->zeroconf_server_id)))
			{
				CThread_mutex_lock(&serverLockLock);
				CThread_mutex_lock(&serverLock);

				free(activePhidTrav->phid->networkInfo->zeroconf_name); activePhidTrav->phid->networkInfo->zeroconf_name = strdup(zeroconfPhidTrav->phid->networkInfo->zeroconf_name);
				free(activePhidTrav->phid->networkInfo->zeroconf_type); activePhidTrav->phid->networkInfo->zeroconf_type = strdup(zeroconfPhidTrav->phid->networkInfo->zeroconf_type);
				free(activePhidTrav->phid->networkInfo->zeroconf_domain); activePhidTrav->phid->networkInfo->zeroconf_domain = strdup(zeroconfPhidTrav->phid->networkInfo->zeroconf_domain);

				if((result = connectToServer(activePhidTrav->phid->networkInfo, errdesc, sizeof(errdesc), activePhidTrav->phid, PHIDGET)) != EPHIDGET_OK)
				{
					err_device = activePhidTrav->phid;
					activePhidTrav->phid->networkInfo->server = NULL;
					CThread_mutex_unlock(&serverLock);
					CThread_mutex_unlock(&serverLockLock);
					CThread_mutex_unlock(&zeroconfPhidgetsLock);
					CThread_mutex_unlock(&activeRemotePhidgetsLock);
					goto error_event;
				}

				CThread_mutex_unlock(&serverLock);
				CThread_mutex_unlock(&serverLockLock);

				goto next_zeroconf_phidget;
			}
		}
next_zeroconf_phidget:;
	}

	CThread_mutex_unlock(&zeroconfPhidgetsLock);
	CThread_mutex_unlock(&activeRemotePhidgetsLock);

	
	//Zeroconf Dictionaries
	CThread_mutex_lock(&activeRemoteDictionariesLock);
	CThread_mutex_lock(&zeroconfServersLock);
	for (zeroconfServerTrav=zeroconfServers; zeroconfServerTrav; zeroconfServerTrav = zeroconfServerTrav->next)
	{
		for (activeDictTrav=activeRemoteDictionaries; activeDictTrav; activeDictTrav = activeDictTrav->next)
		{
			if(!CPhidget_statusFlagIsSet(activeDictTrav->dict->status, PHIDGET_INERROREVENT_FLAG)
				&& !activeDictTrav->dict->networkInfo->server 
				&& activeDictTrav->dict->networkInfo->requested_address==NULL
				&& (activeDictTrav->dict->networkInfo->requested_serverID == NULL 
					|| !strcmp(activeDictTrav->dict->networkInfo->requested_serverID,zeroconfServerTrav->networkInfo->zeroconf_name)))
			{
				CThread_mutex_lock(&serverLockLock);
				CThread_mutex_lock(&serverLock);
				free(activeDictTrav->dict->networkInfo->zeroconf_name); activeDictTrav->dict->networkInfo->zeroconf_name = strdup(zeroconfServerTrav->networkInfo->zeroconf_name);
				free(activeDictTrav->dict->networkInfo->zeroconf_type); activeDictTrav->dict->networkInfo->zeroconf_type = strdup(zeroconfServerTrav->networkInfo->zeroconf_type);
				free(activeDictTrav->dict->networkInfo->zeroconf_domain); activeDictTrav->dict->networkInfo->zeroconf_domain = strdup(zeroconfServerTrav->networkInfo->zeroconf_domain);
				if((result = connectToServer(activeDictTrav->dict->networkInfo, errdesc, sizeof(errdesc), activeDictTrav->dict, DICTIONARY)) != EPHIDGET_OK)
				{
					err_device = activeDictTrav->dict;
					activeDictTrav->dict->networkInfo->server = NULL;
					CThread_mutex_unlock(&serverLock);
					CThread_mutex_unlock(&serverLockLock);
					CThread_mutex_unlock(&zeroconfServersLock);
					CThread_mutex_unlock(&activeRemoteDictionariesLock);
					goto error_event;
				}
				CThread_mutex_unlock(&serverLock);
				CThread_mutex_unlock(&serverLockLock);
			}
		}
	}
	CThread_mutex_unlock(&zeroconfServersLock);
	CThread_mutex_unlock(&activeRemoteDictionariesLock);

	//IP Phidgets
	CThread_mutex_lock(&activeRemotePhidgetsLock);
	for (activePhidTrav=activeRemotePhidgets; activePhidTrav; activePhidTrav = activePhidTrav->next)
	{
		if(activePhidTrav->phid->networkInfo->requested_address!=NULL)
		{
			if(!CPhidget_statusFlagIsSet(activePhidTrav->phid->status, PHIDGET_INERROREVENT_FLAG)
				&& !activePhidTrav->phid->networkInfo->server)
			{
				CThread_mutex_lock(&serverLockLock);
				CThread_mutex_lock(&serverLock);
				if((result = connectToServer(activePhidTrav->phid->networkInfo, errdesc, sizeof(errdesc), activePhidTrav->phid, PHIDGET)) != EPHIDGET_OK)
				{
					err_device = activePhidTrav->phid;
					activePhidTrav->phid->networkInfo->server = NULL;
					CThread_mutex_unlock(&serverLock);
					CThread_mutex_unlock(&serverLockLock);
					CThread_mutex_unlock(&activeRemotePhidgetsLock);
					goto error_event;
				}
				CThread_mutex_unlock(&serverLock);
				CThread_mutex_unlock(&serverLockLock);
			}
		}
	}
	CThread_mutex_unlock(&activeRemotePhidgetsLock);

	//IP Managers
	CThread_mutex_lock(&activeRemoteManagersLock);
	for (activeManagerTrav=activeRemoteManagers; activeManagerTrav; activeManagerTrav = activeManagerTrav->next)
	{
		if(activeManagerTrav->phidm->networkInfo->requested_address!=NULL)
		{
			if(!CPhidget_statusFlagIsSet(activeManagerTrav->phidm->status, PHIDGET_INERROREVENT_FLAG)
				&& !activeManagerTrav->phidm->networkInfo->server )
			{
				CThread_mutex_lock(&serverLockLock);
				CThread_mutex_lock(&serverLock);
				if((result = connectToServer(activeManagerTrav->phidm->networkInfo, errdesc, sizeof(errdesc), activeManagerTrav->phidm, MANAGER)) != EPHIDGET_OK)
				{
					err_device = activeManagerTrav->phidm;
					activeManagerTrav->phidm->networkInfo->server = NULL;
					CThread_mutex_unlock(&serverLock);
					CThread_mutex_unlock(&serverLockLock);
					CThread_mutex_unlock(&activeRemoteManagersLock);
					goto error_event;
				}
				CThread_mutex_unlock(&serverLock);
				CThread_mutex_unlock(&serverLockLock);
			}
		}
	}
	CThread_mutex_unlock(&activeRemoteManagersLock);

	//IP Dictionaries
	CThread_mutex_lock(&activeRemoteDictionariesLock);
	for (activeDictTrav=activeRemoteDictionaries; activeDictTrav; activeDictTrav = activeDictTrav->next)
	{
		if(activeDictTrav->dict->networkInfo->requested_address!=NULL)
		{
			if(!CPhidget_statusFlagIsSet(activeDictTrav->dict->status, PHIDGET_INERROREVENT_FLAG)
				&& !activeDictTrav->dict->networkInfo->server)
			{
				CThread_mutex_lock(&serverLockLock);
				CThread_mutex_lock(&serverLock);
				if((result = connectToServer(activeDictTrav->dict->networkInfo, errdesc, sizeof(errdesc), activeDictTrav->dict, DICTIONARY)) != EPHIDGET_OK)
				{
					err_device = activeDictTrav->dict;
					activeDictTrav->dict->networkInfo->server = NULL;
					CThread_mutex_unlock(&serverLock);
					CThread_mutex_unlock(&serverLockLock);
					CThread_mutex_unlock(&activeRemoteDictionariesLock);
					goto error_event;
				}
				CThread_mutex_unlock(&serverLock);
				CThread_mutex_unlock(&serverLockLock);
			}
		}
	}
	CThread_mutex_unlock(&activeRemoteDictionariesLock);

	return result;

	//send out any error events that have pilled up...
	//send them out here so we can close, open, etc. from within the error event
error_event:
	//if the error did not originate in a pdc function, we'll fill in our own error description
	if(errdesc[0] == '\0')
		strncpy(errdesc, Phid_ErrorDescriptions[result], sizeof(errdesc));
	//no error event for interrupted - this means that a connect was cancelled by a close
	if(result != EPHIDGET_INTERRUPTED)
	{
		inErrorEvent = PTRUE;
		throw_error_event(err_device, errdesc, EEPHIDGET_NETWORK);
		inErrorEvent = PFALSE;
	}
	return result;
}

//shuts down the central remote thread - we should also make sure zeroconf gets shut down here if it's running
int JoinCentralRemoteThread()
{
	if(CentralRemoteThread.m_ThreadHandle && !CThread_is_my_thread(CentralRemoteThread) && !inErrorEvent)
	{
		CThread_join(&CentralRemoteThread);
		CentralRemoteThread.m_ThreadHandle = 0;
	}
#ifdef USE_ZEROCONF
	if(!activeSBCManagers)
	{
		UninitializeZeroconf();//shut down zeroconf
	}
#endif
	return EPHIDGET_OK;
}

int
StartCentralRemoteThread()
{
	CThread_mutex_lock(&CentralRemoteThreadLock);
#ifdef _WINDOWS
	if (CentralRemoteThread.m_ThreadHandle) {
		int threadStatus = 0;
		int result = 0;
		result = GetExitCodeThread(CentralRemoteThread.m_ThreadHandle,
		    (LPDWORD)&threadStatus);
		if (result) {
			if (threadStatus != STILL_ACTIVE) {
				CloseHandle(CentralRemoteThread.m_ThreadHandle);
				CentralRemoteThread.m_ThreadHandle = 0;
			}
		}
	}
#endif

	if (!CentralRemoteThread.m_ThreadHandle || 
		CentralRemoteThread.thread_status == FALSE)
	{
		if (CThread_create(&CentralRemoteThread, CentralRemoteThreadFunction, 0))
			return EPHIDGET_UNEXPECTED;
		CentralRemoteThread.thread_status = TRUE;
	}
	CThread_mutex_unlock(&CentralRemoteThreadLock);
	return EPHIDGET_OK;
}

CThread_func_return_t CentralRemoteThreadFunction(CThread_func_arg_t lpdwParam)
{
	initialize_locks();
	while(activeRemotePhidgets || activeRemoteManagers || activeRemoteDictionaries) {
		FindActiveRemoteDevices(); //this looks for attached active devices and opens them
		MonitorHeartbeats(); //sends out new heartbeats, detects when a server has gone down
		SLEEP(250);
	}
	CentralRemoteThread.thread_status = FALSE;
	return EPHIDGET_OK;
}

int RegisterRemotePhidget(CPhidgetHandle phid)
{
	int result = EPHIDGET_OK;

	//clear all variables
	phid->fptrClear((CPhidgetHandle)phid);
	phid->initKeys = PUNK_INT;

	CThread_mutex_lock(&activeRemotePhidgetsLock);
	
	result = CList_addToList((CListHandle *)&activeRemotePhidgets, phid, CPhidgetHandle_areEqual);

	if (result)
	{
		CThread_mutex_unlock(&activeRemotePhidgetsLock);
		return result;
	}
	CThread_mutex_unlock(&activeRemotePhidgetsLock);

	result = StartCentralRemoteThread();

	return result;
}

int RegisterRemoteManager(CPhidgetManagerHandle phidm)
{
	int result = EPHIDGET_OK;

	CThread_mutex_lock(&activeRemoteManagersLock);
	
	result = CList_addToList((CListHandle *)&activeRemoteManagers, phidm, CPhidgetHandle_areEqual);

	if (result)
	{
		CThread_mutex_unlock(&activeRemoteManagersLock);
		return result;
	}

	CThread_mutex_unlock(&activeRemoteManagersLock);

	result = StartCentralRemoteThread();

	return result;
}

int RegisterRemoteDictionary(CPhidgetDictionaryHandle dict)
{
	int result = EPHIDGET_OK;

	CThread_mutex_lock(&activeRemoteDictionariesLock);

	if ((result = CList_addToList((CListHandle *)&activeRemoteDictionaries, dict, CPhidgetHandle_areEqual)) != EPHIDGET_OK)
	{
		CThread_mutex_unlock(&activeRemoteDictionariesLock);
		return result;
	}

	CThread_mutex_unlock(&activeRemoteDictionariesLock);

	result = StartCentralRemoteThread();

	return result;
}

int RegisterSBCManager(CPhidgetSBCManagerHandle sbcm)
{
	int result = EPHIDGET_OK;

	CThread_mutex_lock(&activeSBCManagersLock);
	
	result = CList_addToList((CListHandle *)&activeSBCManagers, sbcm, CPhidgetHandle_areEqual);

	if (result)
	{
		CThread_mutex_unlock(&activeSBCManagersLock);
		return result;
	}

	CThread_mutex_unlock(&activeSBCManagersLock);

	result = StartCentralRemoteThread();

	return result;
}

// This will close a server connection (and cleanup the socket, etc) if there are no more
// Phidgets, Managers, or Dictionaries in it's lists
int closeServer(CServerInfoHandle server, unsigned char force)
{
	int result = EPHIDGET_OK;
	char errdesc[1024];
	void *pdcs = server->server->pdcs;
	//no more references to this server
	if(((!server->phidgets && !server->dictionaries && !server->managers) || force) && pdcs)
	{
		CThread_mutex_lock(&server->server->pdc_lock);

		//rather then calling quit - which can easily block, just close the socket!
		if(pu_close(server->server->socket,errdesc,sizeof(errdesc)))
		{
			LOG(PHIDGET_LOG_DEBUG,"pu_close: %s", errdesc);
		}

		CThread_mutex_unlock(&server->server->pdc_lock);
		//We need to wait for the read thread to return
		//unlock serverLock so it can be claimed in the cleanup function
		//don't want the cleanup function to free pdcs while we're waiting on it..
		//this is safe because we don't release serverLockLock, so the only thing that is able to claim serverLock is the cleanup function
		server->server->pdcs = NULL;

		CThread_mutex_unlock(&serverLock);

		pdc_readthread_join(pdcs, NULL);

		CThread_mutex_lock(&serverLock);

		pdc_session_free(pdcs);
	}
	return result;
}

int disconnectRemoteObject(void *object, size_t objectListOffset, int(*compareObjects)(void *, void *))
{
	int result = EPHIDGET_OK;
	CServerInfoHandle foundServer;
	CServerInfo newServerInfo;

	CPhidgetHandle phid = object;

	CThread_mutex_lock(&serverLockLock);
	CThread_mutex_lock(&serverLock);

	if(phid->networkInfo->server)
	{
		newServerInfo.server = phid->networkInfo->server;

		//check to see if there is already a connection to this server
		result = CList_findInList((CListHandle)servers, &newServerInfo, CServerInfo_areEqual, (void **)&foundServer);
		switch(result)
		{
			case EPHIDGET_OK: //Found

				if((result = CList_removeFromList((CListHandle *)((long)foundServer + (long)objectListOffset), phid, compareObjects, PFALSE, NULL)))
				{
					CThread_mutex_unlock(&serverLock);
					CThread_mutex_unlock(&serverLockLock);
					return result;
				}
				
				//stop reports for this Object
				//async so it won't block if the connection is bad
				if(phid->networkInfo->listen_id)
				{
					CThread_mutex_lock(&phid->networkInfo->server->pdc_lock);
					pdc_async_ignore(foundServer->server->pdcs,phid->networkInfo->listen_id,NULL,NULL);
					CThread_mutex_unlock(&phid->networkInfo->server->pdc_lock);
				}

				//closes if there are no more references
				closeServer(foundServer, PFALSE);
				phid->networkInfo->server = NULL;
				
				break;
			case EPHIDGET_NOTFOUND: //Not Found - That's ok, just means it's already closed
				phid->networkInfo->server = NULL;
				CThread_mutex_unlock(&serverLock);
				CThread_mutex_unlock(&serverLockLock);
				return EPHIDGET_OK;
			default:
				phid->networkInfo->server = NULL;
				CThread_mutex_unlock(&serverLock);
				CThread_mutex_unlock(&serverLockLock);
				return result;
		}
	}

	CThread_mutex_unlock(&serverLock);
	CThread_mutex_unlock(&serverLockLock);
	return EPHIDGET_OK;
}

CThread_func_return_t DisconnectPhidgetThreadFunction(CThread_func_arg_t lpdwParam)
{
	CPhidgetHandle phid = (CPhidgetHandle)lpdwParam;
	
	CPhidget_clearStatusFlag(&phid->status, PHIDGET_SERVER_CONNECTED_FLAG, &phid->lock);

	if(phid->fptrDisconnect)
		phid->fptrDisconnect((CPhidgetHandle)phid, phid->fptrDisconnectptr);

	disconnectRemoteObject(phid, offsetof(CServerInfo, phidgets), CPhidget_areEqual);

	return EPHIDGET_OK;
}

int unregisterRemotePhidget(CPhidgetHandle phid)
{
	int result = EPHIDGET_OK;
	
	//cancel a pending connect
	if(phid->networkInfo->cancelSocket != INVALID_SOCKET)
	{
		cancelConnect(phid->networkInfo->cancelSocket);
	}
#ifdef USE_ZEROCONF
	cancelPendingZeroconfLookups(phid->networkInfo);
#endif
	
	CThread_mutex_lock(&activeRemotePhidgetsLock);
	
	result = CList_removeFromList((CListHandle *)&activeRemotePhidgets, phid, CPhidgetHandle_areEqual, FALSE, NULL);

	if (result)
	{
		CThread_mutex_unlock(&activeRemotePhidgetsLock);
		return result;
	}

	CThread_mutex_unlock(&activeRemotePhidgetsLock);

	CPhidget_clearStatusFlag(&phid->status, PHIDGET_SERVER_CONNECTED_FLAG, &phid->lock);
	CPhidget_clearStatusFlag(&phid->status, PHIDGET_ATTACHED_FLAG, &phid->lock);

	result = disconnectRemoteObject(phid, offsetof(CServerInfo, phidgets), CPhidget_areEqual);

	CPhidget_clearStatusFlag(&phid->status, PHIDGET_REMOTE_FLAG, &phid->lock);

	/* don't free this here - it may be referenced elsewhere! */
	phid->networkInfo->server = NULL;
	CPhidgetRemote_free(phid->networkInfo);
	phid->networkInfo = NULL;

	if(!activeRemotePhidgets && !activeRemoteManagers && !activeRemoteDictionaries)
	{
		JoinCentralRemoteThread();
	}

	return result;
}

int unregisterRemoteManager(CPhidgetManagerHandle phidm)
{
	int result = EPHIDGET_OK;
	CServerInfoHandle foundServer;
	CServerInfo newServerInfo;
		
	//cancel a pending connect
	if(phidm->networkInfo->cancelSocket != INVALID_SOCKET)
	{
		cancelConnect(phidm->networkInfo->cancelSocket);
	}
#ifdef USE_ZEROCONF
	cancelPendingZeroconfLookups(phidm->networkInfo);
#endif
	
	CThread_mutex_lock(&activeRemoteManagersLock);
	
	result = CList_removeFromList((CListHandle *)&activeRemoteManagers, phidm, CPhidgetHandle_areEqual, FALSE, NULL);

	if (result)
	{
		CThread_mutex_unlock(&activeRemoteManagersLock);
		return result;
	}
	CThread_mutex_unlock(&activeRemoteManagersLock);
		
	CThread_mutex_lock(&serverLockLock);
	CThread_mutex_lock(&serverLock);
	if(phidm->networkInfo->server)
	{
		newServerInfo.server = phidm->networkInfo->server;

		//check to see if there is already a connection to this server
		result = CList_findInList((CListHandle)servers, &newServerInfo, CServerInfo_areEqual, (void **)&foundServer);
		switch(result)
		{
			case EPHIDGET_OK: //Found

				if((result = CList_removeFromList((CListHandle *)&foundServer->managers, phidm, CPhidgetManager_areEqual, PFALSE, NULL)))
				{
					CThread_mutex_unlock(&serverLock);
					CThread_mutex_unlock(&serverLockLock);
					return result;
				}

				CPhidget_clearStatusFlag(&phidm->status, PHIDGET_SERVER_CONNECTED_FLAG, &phidm->lock);
				CPhidget_clearStatusFlag(&phidm->status, PHIDGET_ATTACHED_FLAG, &phidm->lock);

				//stop reports
				CThread_mutex_lock(&phidm->networkInfo->server->pdc_lock);
				pdc_async_ignore(foundServer->server->pdcs,phidm->networkInfo->listen_id,NULL,NULL);
				CThread_mutex_unlock(&phidm->networkInfo->server->pdc_lock);
				
				closeServer(foundServer, PFALSE);

				CPhidget_clearStatusFlag(&phidm->status, PHIDGET_REMOTE_FLAG, &phidm->lock);

				/* don't free this here - it may be referenced elsewhere! */
				phidm->networkInfo->server = NULL;
				CPhidgetRemote_free(phidm->networkInfo);
				phidm->networkInfo = NULL;
				
				break;
			case EPHIDGET_NOTFOUND: //Not Found - That's ok, just means it's already closed
				CThread_mutex_unlock(&serverLock);
				CThread_mutex_unlock(&serverLockLock);
				return EPHIDGET_OK;
			default:
				CThread_mutex_unlock(&serverLock);
				CThread_mutex_unlock(&serverLockLock);
				return result;
		}
	}
	CThread_mutex_unlock(&serverLock);
	CThread_mutex_unlock(&serverLockLock);

	if(!activeRemotePhidgets && !activeRemoteManagers && !activeRemoteDictionaries)
	{
		JoinCentralRemoteThread();
	}

	return EPHIDGET_OK;
}

int unregisterRemoteDictionary(CPhidgetDictionaryHandle dict)
{
	int result = EPHIDGET_OK;
	CServerInfoHandle foundServer;
	CServerInfo newServerInfo;
	CPhidgetDictionaryListenerListHandle trav;
	
	//cancel a pending connect
	if(dict->networkInfo->cancelSocket != INVALID_SOCKET)
	{
		cancelConnect(dict->networkInfo->cancelSocket);
	}
#ifdef USE_ZEROCONF
	cancelPendingZeroconfLookups(dict->networkInfo);
#endif
	
	CThread_mutex_lock(&activeRemoteDictionariesLock);
	if ((result = CList_removeFromList((CListHandle *)&activeRemoteDictionaries, dict, CPhidgetHandle_areEqual, FALSE, NULL)) != EPHIDGET_OK)
	{
		CThread_mutex_unlock(&activeRemoteDictionariesLock);
		return result;
	}
	CThread_mutex_unlock(&activeRemoteDictionariesLock);

	CThread_mutex_lock(&serverLockLock);
	CThread_mutex_lock(&serverLock);
	CThread_mutex_lock(&dict->lock);
	if(dict->networkInfo && dict->networkInfo->server)
	{
		newServerInfo.server = dict->networkInfo->server;

		//check to see if there is already a connection to this server
		result = CList_findInList((CListHandle)servers, &newServerInfo, CServerInfo_areEqual, (void **)&foundServer);
		switch(result)
		{
			case EPHIDGET_OK: //Found

				if((result = CList_removeFromList((CListHandle *)&foundServer->dictionaries, dict, CPhidgetDictionary_areEqual, PFALSE, NULL)))
				{
					CThread_mutex_unlock(&dict->lock);
					CThread_mutex_unlock(&serverLock);
					CThread_mutex_unlock(&serverLockLock);
					return result;
				}

				//stop reports, remove listeners
				CThread_mutex_lock(&dict->listenersLock);
				for(trav = dict->listeners; trav; trav = trav->next)
				{
					CThread_mutex_lock(&dict->networkInfo->server->pdc_lock);
					pdc_async_ignore(foundServer->server->pdcs,trav->listener->listen_id,NULL,NULL);
					CThread_mutex_unlock(&dict->networkInfo->server->pdc_lock);
				}
				CList_emptyList((CListHandle *)&dict->listeners, PTRUE, CPhidgetDictionaryListener_free);
				CThread_mutex_unlock(&dict->listenersLock);
				
				//closes connection if it's not used anymore
				closeServer(foundServer, PFALSE);

				break;
			case EPHIDGET_NOTFOUND: //Not Found - That's ok, just means it's already closed
				break;
			default: //ERROR
				CThread_mutex_unlock(&dict->lock);
				CThread_mutex_unlock(&serverLock);
				CThread_mutex_unlock(&serverLockLock);
				return result;
		}
		//if server needs to be freed it will already have been
		CPhidget_clearStatusFlag(&dict->status, PHIDGET_SERVER_CONNECTED_FLAG, NULL);
		dict->networkInfo->server = NULL;
		CPhidget_clearStatusFlag(&dict->status, PHIDGET_ATTACHED_FLAG, NULL);
	}

	CPhidgetRemote_free(dict->networkInfo);
	dict->networkInfo = NULL;
	CPhidget_clearStatusFlag(&dict->status, PHIDGET_REMOTE_FLAG, NULL);
	CThread_mutex_unlock(&dict->lock);
	CThread_mutex_unlock(&serverLock);
	CThread_mutex_unlock(&serverLockLock);

	if(!activeRemotePhidgets && !activeRemoteManagers && !activeRemoteDictionaries)
	{
		JoinCentralRemoteThread();
	}

	return EPHIDGET_OK;
}

int unregisterSBCManager(CPhidgetSBCManagerHandle sbcm)
{
	int result = EPHIDGET_OK;

	CThread_mutex_lock(&activeSBCManagersLock);
	
	result = CList_removeFromList((CListHandle *)&activeSBCManagers, sbcm, CPhidgetHandle_areEqual, FALSE, NULL);

	if (result)
	{
		CThread_mutex_unlock(&activeSBCManagersLock);
		return result;
	}
	CThread_mutex_unlock(&activeSBCManagersLock);

#ifdef USE_ZEROCONF
	if(!activeRemotePhidgets && !activeRemoteManagers && !activeRemoteDictionaries && !activeSBCManagers)
	{
		UninitializeZeroconf();//shut down zeroconf
	}
#endif

	return EPHIDGET_OK;
}

int CCONV
CPhidget_openRemoteIP(CPhidgetHandle phid, int serialNumber, const char *address,
    int port, const char *password)
{
	int result = EPHIDGET_OK;
	char portString[6];

	TESTPTR(phid)

	if (serialNumber < -1)
		return EPHIDGET_INVALIDARG;
	
	CThread_mutex_lock(&phid->openCloseLock);
	if (CPhidget_statusFlagIsSet(phid->status, PHIDGET_OPENED_FLAG))
	{
		LOG(PHIDGET_LOG_WARNING, "Open was called on an already opened Phidget handle.");
		CThread_mutex_unlock(&phid->openCloseLock);
		return EPHIDGET_OK;
	}

	if (serialNumber == -1)
		phid->specificDevice = FALSE;
	else
		phid->specificDevice = TRUE;
	phid->serialNumber = serialNumber;

	if((result = CPhidgetRemote_create(&phid->networkInfo)))
	{
		CThread_mutex_unlock(&phid->openCloseLock);
		return result;
	}

	if(password) {
		if (strlen(password) > 255)
		{
			CThread_mutex_unlock(&phid->openCloseLock);
			return EPHIDGET_INVALIDARG;
		}
		if (!(phid->networkInfo->password = strdup(password)))
		{
			CThread_mutex_unlock(&phid->openCloseLock);
			return EPHIDGET_NOMEMORY;
		}
	}

	snprintf(portString, sizeof(portString), "%d", port);
	if(!(phid->networkInfo->requested_port = strdup(portString)))
	{
		CThread_mutex_unlock(&phid->openCloseLock);
		return EPHIDGET_NOMEMORY;
	}
	if(!(phid->networkInfo->requested_address = strdup(address)))
	{
		CThread_mutex_unlock(&phid->openCloseLock);
		return EPHIDGET_NOMEMORY;
	}
	
	initialize_locks();

	result = RegisterRemotePhidget(phid);

	CPhidget_setStatusFlag(&phid->status, PHIDGET_REMOTE_FLAG, &phid->lock);
	CPhidget_setStatusFlag(&phid->status, PHIDGET_OPENED_FLAG, &phid->lock);
	CThread_mutex_unlock(&phid->openCloseLock);

	return result;
}

int CCONV CPhidget_openRemote(CPhidgetHandle phid, int serialNumber, const char *serverID, const char *password)
{
#ifdef USE_ZEROCONF
	int result = EPHIDGET_OK;
	TESTPTR(phid)

	if (serialNumber < -1)
		return EPHIDGET_INVALIDARG;
	
	CThread_mutex_lock(&phid->openCloseLock);
	initialize_locks();

	if((result = InitializeZeroconf())) 
	{
		CThread_mutex_unlock(&phid->openCloseLock);
		if(result == EPHIDGET_TRYAGAIN)
			return EPHIDGET_TIMEOUT;
		return EPHIDGET_UNSUPPORTED;
	}
	
	if (CPhidget_statusFlagIsSet(phid->status, PHIDGET_OPENED_FLAG))
	{
		LOG(PHIDGET_LOG_WARNING, "Open was called on an already opened Phidget handle.");
		CThread_mutex_unlock(&phid->openCloseLock);
		return EPHIDGET_OK;
	}

	if (serialNumber == -1)
		phid->specificDevice = FALSE;
	else
		phid->specificDevice = TRUE;
	phid->serialNumber = serialNumber;

	if((result = CPhidgetRemote_create(&phid->networkInfo)))
	{
		CThread_mutex_unlock(&phid->openCloseLock);
		return result;
	}

	if(password) {
		if (strlen(password) > 255)
		{
			CThread_mutex_unlock(&phid->openCloseLock);
			return EPHIDGET_INVALIDARG;
		}
		if (!(phid->networkInfo->password = strdup(password)))
		{
			CThread_mutex_unlock(&phid->openCloseLock);
			return EPHIDGET_NOMEMORY;
		}
	}
	if(serverID)
	{
		if (!(phid->networkInfo->requested_serverID = strdup(serverID)))
		{
			CThread_mutex_unlock(&phid->openCloseLock);
			return EPHIDGET_NOMEMORY;
		}
	}
	
	phid->networkInfo->mdns = PTRUE;

	result = RegisterRemotePhidget(phid);

	CPhidget_setStatusFlag(&phid->status, PHIDGET_REMOTE_FLAG, &phid->lock);
	CPhidget_setStatusFlag(&phid->status, PHIDGET_OPENED_FLAG, &phid->lock);
	CThread_mutex_unlock(&phid->openCloseLock);
	
	return result;
#else
	return EPHIDGET_UNSUPPORTED;
#endif
}

int CCONV CPhidgetManager_openRemoteIP(CPhidgetManagerHandle phidm, const char *address,
    int port, const char *password)
{
	int result = EPHIDGET_OK;
	char portString[6];

	TESTPTR(phidm)
	
	CThread_mutex_lock(&phidm->openCloseLock);
	if (CPhidget_statusFlagIsSet(phidm->status, PHIDGET_OPENED_FLAG))
	{
		LOG(PHIDGET_LOG_WARNING, "Open was called on an already opened Manager handle.");
		CThread_mutex_unlock(&phidm->openCloseLock);
		return EPHIDGET_OK;
	}

	if((result = CPhidgetRemote_create(&phidm->networkInfo)))
	{
		CThread_mutex_unlock(&phidm->openCloseLock);
		return result;
	}

	if(password) {
		if (strlen(password) > 255)
		{
			CThread_mutex_unlock(&phidm->openCloseLock);
			return EPHIDGET_INVALIDARG;
		}
		if (!(phidm->networkInfo->password = strdup(password)))
		{
			CThread_mutex_unlock(&phidm->openCloseLock);
			return EPHIDGET_NOMEMORY;
		}
	}

	snprintf(portString, sizeof(portString), "%d", port);
	if(!(phidm->networkInfo->requested_port = strdup(portString)))
	{
		CThread_mutex_unlock(&phidm->openCloseLock);
		return EPHIDGET_NOMEMORY;
	}
	if(!(phidm->networkInfo->requested_address = strdup(address)))
	{
		CThread_mutex_unlock(&phidm->openCloseLock);
		return EPHIDGET_NOMEMORY;
	}
	
	phidm->state = PHIDGETMANAGER_ACTIVE;
	
	initialize_locks();

	result = RegisterRemoteManager(phidm);

	CPhidget_setStatusFlag(&phidm->status, PHIDGET_REMOTE_FLAG, &phidm->lock);
	CPhidget_setStatusFlag(&phidm->status, PHIDGET_OPENED_FLAG, &phidm->lock);

	CThread_mutex_unlock(&phidm->openCloseLock);

	return result;
}

int CCONV CPhidgetManager_openRemote(CPhidgetManagerHandle phidm, const char *serverID, const char *password)
{
#ifdef USE_ZEROCONF
	int result = EPHIDGET_OK;
	
	CThread_mutex_lock(&phidm->openCloseLock);

	initialize_locks();

	if((result = InitializeZeroconf())) 
	{
		CThread_mutex_unlock(&phidm->openCloseLock);
		if(result == EPHIDGET_TRYAGAIN)
			return EPHIDGET_TIMEOUT;
		return EPHIDGET_UNSUPPORTED;
	}
	
	if (CPhidget_statusFlagIsSet(phidm->status, PHIDGET_OPENED_FLAG))
	{
		LOG(PHIDGET_LOG_WARNING, "Open was called on an already opened Manager handle.");
		CThread_mutex_unlock(&phidm->openCloseLock);
		return EPHIDGET_OK;
	}

	if((result = CPhidgetRemote_create(&phidm->networkInfo)))
	{
		CThread_mutex_unlock(&phidm->openCloseLock);
		return result;
	}

	if(password) {
		if (strlen(password) > 255)
		{
			CThread_mutex_unlock(&phidm->openCloseLock);
			return EPHIDGET_INVALIDARG;
		}
		if (!(phidm->networkInfo->password = strdup(password)))
		{
			CThread_mutex_unlock(&phidm->openCloseLock);
			return EPHIDGET_NOMEMORY;
		}
	}
	if(serverID)
	{
		if (!(phidm->networkInfo->requested_serverID = strdup(serverID)))
		{
			CThread_mutex_unlock(&phidm->openCloseLock);
			return EPHIDGET_NOMEMORY;
		}
	}

	phidm->networkInfo->mdns = PTRUE;

	//we're not connecting to anything, so - always active :)
	phidm->state = PHIDGETMANAGER_ACTIVATING;

	result = RegisterRemoteManager(phidm);
	if(!result)
	{
		CPhidgetListHandle trav;
		CThread_mutex_lock(&zeroconfPhidgetsLock);
		CThread_mutex_lock(&activeRemoteManagersLock);
		for (trav=zeroconfPhidgets; trav; trav = trav->next)
		{
			if (phidm->fptrAttachChange)
				phidm->fptrAttachChange((CPhidgetHandle)trav->phid, phidm->fptrAttachChangeptr);
		}
		phidm->state = PHIDGETMANAGER_ACTIVE;
		CThread_mutex_unlock(&activeRemoteManagersLock);
		CThread_mutex_unlock(&zeroconfPhidgetsLock);
	}

	CPhidget_setStatusFlag(&phidm->status, PHIDGET_REMOTE_FLAG, &phidm->lock);
	CPhidget_setStatusFlag(&phidm->status, PHIDGET_OPENED_FLAG, &phidm->lock);

	CThread_mutex_unlock(&phidm->openCloseLock);
	return result;
#else
	return EPHIDGET_UNSUPPORTED;
#endif
}

int CCONV CPhidgetDictionary_openRemoteIP(CPhidgetDictionaryHandle dict, const char *address, int port, const char *password)
{
	int result = EPHIDGET_OK;
	char portString[6];
	TESTPTRS(dict,address)

	CThread_mutex_lock(&dict->openCloseLock);
	if (CPhidget_statusFlagIsSet(dict->status, PHIDGET_OPENED_FLAG))
	{
		LOG(PHIDGET_LOG_WARNING, "Open was called on an already opened Dictionary handle.");
		CThread_mutex_unlock(&dict->openCloseLock);
		return EPHIDGET_OK;
	}

	if((result = CPhidgetRemote_create(&dict->networkInfo)) != EPHIDGET_OK)
		goto fail;

	snprintf(portString, sizeof(portString), "%d", port);

	if((dict->networkInfo->requested_port = strdup(portString)) == NULL
		|| (dict->networkInfo->requested_address = strdup(address)) == NULL)
	{
		result = EPHIDGET_NOMEMORY;
		goto fail;
	}

	if(password)
	{
		if (strlen(password) > 255)
		{
			result = EPHIDGET_INVALIDARG;
			goto fail;
		}
		if ((dict->networkInfo->password = strdup(password)) == NULL)
		{
			result = EPHIDGET_NOMEMORY;
			goto fail;
		}
	}
	else
	{
		dict->networkInfo->password = NULL;
	}
	
	initialize_locks();

	if((result = RegisterRemoteDictionary(dict)) != EPHIDGET_OK)
	{
		goto fail;
	}

	CPhidget_setStatusFlag(&dict->status, PHIDGET_REMOTE_FLAG, &dict->lock);
	CPhidget_setStatusFlag(&dict->status, PHIDGET_OPENED_FLAG, &dict->lock);

	CThread_mutex_unlock(&dict->openCloseLock);

	return EPHIDGET_OK;

fail:
	//This will free any memory allocated in this function.
	CPhidgetRemote_free(dict->networkInfo); dict->networkInfo = NULL;
	CThread_mutex_unlock(&dict->openCloseLock);
	return result;
}

/* 
 * dict needs to be valid - should be in opened state
 * serverID can be NULL
 * password can be NULL
 */
int CCONV CPhidgetDictionary_openRemote(CPhidgetDictionaryHandle dict, const char *serverID, const char *password)
{
#ifdef USE_ZEROCONF
	int result = EPHIDGET_OK;
	TESTPTR(dict)

	CThread_mutex_lock(&dict->openCloseLock);

	initialize_locks();

	if((result = InitializeZeroconf())) 
	{
		if(result == EPHIDGET_TRYAGAIN)
			result = EPHIDGET_TIMEOUT;
		else
			result = EPHIDGET_UNSUPPORTED;
		goto fail;
	}

	if (CPhidget_statusFlagIsSet(dict->status, PHIDGET_OPENED_FLAG))
	{
		LOG(PHIDGET_LOG_WARNING, "Open was called on an already opened Dictionary handle.");
		CThread_mutex_unlock(&dict->openCloseLock);
		return EPHIDGET_OK;
	}

	if((result = CPhidgetRemote_create(&dict->networkInfo)) != EPHIDGET_OK)
		goto fail;

	if(password)
	{
		if (strlen(password) > 255)
		{
			result = EPHIDGET_INVALIDARG;
			goto fail;
		}
		if ((dict->networkInfo->password = strdup(password)) == NULL)
		{
			result = EPHIDGET_NOMEMORY;
			goto fail;
		}
	}
	if(serverID)
	{
		if ((dict->networkInfo->requested_serverID = strdup(serverID)) == NULL)
		{
			result = EPHIDGET_NOMEMORY;
			goto fail;
		}
	}

	dict->networkInfo->mdns = PTRUE;

	if((result = RegisterRemoteDictionary(dict)) != EPHIDGET_OK)
	{
		goto fail;
	}

	CPhidget_setStatusFlag(&dict->status, PHIDGET_REMOTE_FLAG, &dict->lock);
	CPhidget_setStatusFlag(&dict->status, PHIDGET_OPENED_FLAG, &dict->lock);

	CThread_mutex_unlock(&dict->openCloseLock);
	return EPHIDGET_OK;

fail:
	//This will free any memory allocated in this function.
	CPhidgetRemote_free(dict->networkInfo); dict->networkInfo = NULL;
	CThread_mutex_unlock(&dict->openCloseLock);
	return result;
#else
	return EPHIDGET_UNSUPPORTED;
#endif
}

int CCONV CPhidgetSBCManager_start(CPhidgetSBCManagerHandle sbcm)
{
#ifdef USE_ZEROCONF
	int result = EPHIDGET_OK;
	
	initialize_locks();

	if((result = InitializeZeroconf())) 
	{
		if(result == EPHIDGET_TRYAGAIN)
			return EPHIDGET_TIMEOUT;
		return EPHIDGET_UNSUPPORTED;
	}

	sbcm->mdns = PTRUE;

	//we're not connecting to anything, so - always active :)
	sbcm->state = PHIDGETMANAGER_ACTIVE;

	result = RegisterSBCManager(sbcm);
	if(!result)
	{
		CPhidgetSBCListHandle trav;
		CThread_mutex_lock(&zeroconfSBCsLock);
		CThread_mutex_lock(&activeSBCManagersLock);
		for (trav=zeroconfSBCs; trav; trav = trav->next)
		{
				if (sbcm->fptrAttachChange)
					sbcm->fptrAttachChange((CPhidgetSBCHandle)trav->sbc, sbcm->fptrAttachChangeptr);
		}
		CThread_mutex_unlock(&activeSBCManagersLock);
		CThread_mutex_unlock(&zeroconfSBCsLock);
	}

	return result;
#else
	return EPHIDGET_UNSUPPORTED;
#endif
}

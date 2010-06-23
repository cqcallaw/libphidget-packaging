#include "stdafx.h"
#include "csocket.h"
#include "csocketevents.h"
#include "cphidgetlist.h"
#include "cphidgetmanager.h"
#include "cphidgetdictionary.h"
#include "cphidgetsbc.h"
#include "zeroconf.h"

#include "avahi-client/client.h"
#include "avahi-client/lookup.h"

#include "avahi-common/thread-watch.h"
#include "avahi-common/malloc.h"
#include "avahi-common/error.h"
#include "avahi-common/domain.h"

struct AvahiThreadedPoll {
    void *simple_poll;
    pthread_t thread_id;
    pthread_mutex_t mutex;
    int thread_running;
    int retval;
} ;

#ifdef ZEROCONF_RUNTIME_LINKING
typedef AvahiClient * (* avahi_client_new_type) (
    const AvahiPoll *poll_api /**< The abstract event loop API to use */,
    AvahiClientFlags flags /**< Some flags to modify the behaviour of  the client library */,
    AvahiClientCallback callback /**< A callback that is called whenever the state of the client changes. This may be NULL */,
    void *userdata /**< Some arbitrary user data pointer that will be passed to the callback function */,
    int *error /**< If creation of the client fails, this integer will contain the error cause. May be NULL if you aren't interested in the reason why avahi_client_new() failed. */);
typedef void (* avahi_client_free_type)(AvahiClient *client);
typedef const char * (* avahi_client_get_host_name_type) (AvahiClient *);
typedef AvahiServiceBrowser * (* avahi_service_browser_new_type) (
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *type,
    const char *domain,
    AvahiLookupFlags flags,
    AvahiServiceBrowserCallback callback,
    void *userdata);
typedef AvahiServiceResolver * (* avahi_service_resolver_new_type)(
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    const char *type,
    const char *domain,
    AvahiProtocol aprotocol,
    AvahiLookupFlags flags,
    AvahiServiceResolverCallback callback,
    void *userdata);
typedef int (* avahi_service_resolver_free_type)(AvahiServiceResolver *r);
typedef AvahiRecordBrowser * (* avahi_record_browser_new_type)(
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *name,
    uint16_t clazz,
    uint16_t type,
    AvahiLookupFlags flags,
    AvahiRecordBrowserCallback callback,
    void *userdata);
typedef int (* avahi_record_browser_free_type)(AvahiRecordBrowser *);
typedef int (* avahi_service_name_join_type)(char *p, size_t size, const char *name, const char *type, const char *domain);
typedef const char *(* avahi_strerror_type)(int error);
typedef int (* avahi_client_errno_type) (AvahiClient*);
typedef AvahiThreadedPoll *(* avahi_threaded_poll_new_type)(void);
typedef void (* avahi_threaded_poll_free_type)(AvahiThreadedPoll *p);
typedef const AvahiPoll* (* avahi_threaded_poll_get_type)(AvahiThreadedPoll *p);
typedef int (* avahi_threaded_poll_start_type)(AvahiThreadedPoll *p);
typedef int (* avahi_threaded_poll_stop_type)(AvahiThreadedPoll *p);
typedef void (* avahi_threaded_poll_quit_type)(AvahiThreadedPoll *p);
typedef void (* avahi_threaded_poll_lock_type)(AvahiThreadedPoll *p);
typedef void (* avahi_threaded_poll_unlock_type)(AvahiThreadedPoll *p);
typedef const char *(* avahi_client_get_version_string_type)(AvahiClient *c);

avahi_service_browser_new_type avahi_service_browser_new_ptr = NULL;
avahi_service_resolver_new_type avahi_service_resolver_new_ptr = NULL;
avahi_service_resolver_free_type avahi_service_resolver_free_ptr = NULL;
avahi_record_browser_new_type avahi_record_browser_new_ptr = NULL;
avahi_record_browser_free_type avahi_record_browser_free_ptr = NULL;
avahi_service_name_join_type avahi_service_name_join_ptr = NULL;
avahi_client_new_type avahi_client_new_ptr = NULL;
avahi_client_free_type avahi_client_free_ptr = NULL;
avahi_strerror_type avahi_strerror_ptr = NULL;
avahi_client_errno_type avahi_client_errno_ptr = NULL;
avahi_threaded_poll_new_type avahi_threaded_poll_new_ptr = NULL;
avahi_threaded_poll_free_type avahi_threaded_poll_free_ptr = NULL;
avahi_threaded_poll_get_type avahi_threaded_poll_get_ptr = NULL;
avahi_threaded_poll_start_type avahi_threaded_poll_start_ptr = NULL;
avahi_threaded_poll_stop_type avahi_threaded_poll_stop_ptr = NULL;
avahi_threaded_poll_quit_type avahi_threaded_poll_quit_ptr = NULL;
avahi_threaded_poll_lock_type avahi_threaded_poll_lock_ptr = NULL;
avahi_threaded_poll_unlock_type avahi_threaded_poll_unlock_ptr = NULL;
avahi_client_get_version_string_type avahi_client_get_version_string_ptr = NULL;
#else
#define avahi_service_browser_new_ptr avahi_service_browser_new
#define avahi_service_resolver_new_ptr avahi_service_resolver_new
#define avahi_service_resolver_free_ptr avahi_service_resolver_free
#define avahi_record_browser_new_ptr avahi_record_browser_new
#define avahi_record_browser_free_ptr avahi_record_browser_free
#define avahi_service_name_join_ptr avahi_service_name_join
#define avahi_client_new_ptr avahi_client_new
#define avahi_client_free_ptr avahi_client_free
#define avahi_strerror_ptr avahi_strerror
#define avahi_client_errno_ptr avahi_client_errno
#define avahi_threaded_poll_new_ptr avahi_threaded_poll_new
#define avahi_threaded_poll_free_ptr avahi_threaded_poll_free
#define avahi_threaded_poll_get_ptr avahi_threaded_poll_get
#define avahi_threaded_poll_start_ptr avahi_threaded_poll_start
#define avahi_threaded_poll_stop_ptr avahi_threaded_poll_stop
#define avahi_threaded_poll_quit_ptr avahi_threaded_poll_quit
#define avahi_threaded_poll_lock_ptr avahi_threaded_poll_lock
#define avahi_threaded_poll_unlock_ptr avahi_threaded_poll_unlock
#define avahi_client_get_version_string_ptr avahi_client_get_version_string
#endif

/* 
 * TXT record version - this should be 1 for a long time
 *  - only need to change if we really change the TXT record format
 */
const char *dnssd_txt_ver = "1";

int Dns_sdInitialized = FALSE; 

static AvahiThreadedPoll *threaded_poll = NULL;
static AvahiClient *client = NULL;

static AvahiServiceBrowser *zeroconf_browse_sbc_ref  = NULL;
static AvahiServiceBrowser *zeroconf_browse_ws_ref  = NULL;
static AvahiServiceBrowser *zeroconf_browse_phidget_ref  = NULL;

//pthread_t dns_thread_ws, dns_thread_phid;

void *avahiLibHandle = NULL;

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) {
    assert(c);

    /* Called whenever the client or server state changes */

    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:
        
            /* The server has startup successfully and registered its host
             * name on the network */
			Dns_sdInitialized = TRUE;
            break;

        case AVAHI_CLIENT_FAILURE:
            
            LOG(PHIDGET_LOG_ERROR, "Client failure: %s", avahi_strerror_ptr(avahi_client_errno_ptr(c)));
            //avahi_threaded_poll_quit_ptr(threaded_poll);
            
            break;

        case AVAHI_CLIENT_S_COLLISION:
        
            /* Let's drop our registered services. When the server is back
             * in AVAHI_SERVER_RUNNING state we will register them
             * again with the new host name. */
            
        case AVAHI_CLIENT_S_REGISTERING:

            /* The server records are now being established. This
             * might be caused by a host name change. We need to wait
             * for our own records to register until the host name is
             * properly esatblished. */
            
            //if (group)
              //  avahi_entry_group_reset_ptr(group);
            
            break;

        case AVAHI_CLIENT_CONNECTING:
            break;
    }
}
	
static uint8_t *InternalTXTRecordSearch
	(
	uint16_t         txtLen,
	const void       *txtRecord,
	const char       *key,
	unsigned long    *keylen
	)
{
	uint8_t *p = (uint8_t*)txtRecord;
	uint8_t *e = p + txtLen;
	*keylen = (unsigned long) strlen(key);
	while (p<e)
		{
		uint8_t *x = p;
		p += 1 + p[0];
		if (p <= e && *keylen <= x[0] && !strncmp(key, (char*)x+1, *keylen))
			if (*keylen == x[0] || x[1+*keylen] == '=') return(x);
		}
	return(NULL);
}

const void * TXTRecordGetValuePtr
	(
	uint16_t         txtLen,
	const void       *txtRecord,
	const char       *key,
	uint8_t          *valueLen
	)
{
	unsigned long keylen;
	uint8_t *item = InternalTXTRecordSearch(txtLen, txtRecord, key, &keylen);
	if (!item || item[0] <= keylen) return(NULL);	// If key not found, or found with no value, return NULL
	*valueLen = (uint8_t)(item[0] - (keylen + 1));
	return (item + 1 + keylen + 1);
}

void PhidFromTXT(CPhidgetHandle phid, uint16_t txtLen, const char *txtRecord)
{
	int i = 0;
	short txtver;
	
	uint8_t valLen = 0;
	const char *valPtr = NULL;
	
	//txt version
	if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "txtvers", &valLen))) return;
	txtver = (short)strtol(valPtr, NULL, 10);
	
	//Serial Number
	if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "serial", &valLen))) return;
	phid->serialNumber = strtol(valPtr, NULL, 10);
	phid->specificDevice = PTRUE;
	
	//version
	if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "version", &valLen))) return;
	phid->deviceVersion = strtol(valPtr, NULL, 10);
	
	//label
	if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "label", &valLen))) return;
	if(valLen > 10) valLen = 10;
	memcpy(phid->label, valPtr, valLen);
	phid->label[valLen] = '\0';
	
	//server_id
	if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "server_id", &valLen))) return;
	free(phid->networkInfo->zeroconf_server_id);
	if(!(phid->networkInfo->zeroconf_server_id = malloc(valLen+1))) return;
	ZEROMEM(phid->networkInfo->zeroconf_server_id, valLen+1);
	memcpy(phid->networkInfo->zeroconf_server_id, valPtr, valLen);
	
	// things added in version 2 of the txt
	if(txtver >= 2)
	{
		//Device ID
		if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "id", &valLen))) return;
		phid->deviceIDSpec = strtol(valPtr, NULL, 10);
		
		for(i = 1;i<PHIDGET_DEVICE_COUNT;i++)
			if(phid->deviceIDSpec == Phid_Device_Def[i].pdd_sdid) break;
		phid->deviceDef = &Phid_Device_Def[i];
		phid->attr = Phid_Device_Def[i].pdd_attr;
		
		//Device Class
		if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "class", &valLen))) return;
		phid->deviceID = strtol(valPtr, NULL, 10);
		phid->deviceType = Phid_DeviceName[phid->deviceID];
	}
	//Old version uses string searching, but some devices have the same name with different IDs
	else
	{
		char *name = NULL;
		char *type = NULL;
		
		//name
		if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "name", &valLen))) return;
		if(!(name = malloc(valLen+1))) return;
		ZEROMEM(name, valLen+1);
		memcpy(name, valPtr, valLen);
		for(i = 0;i<PHIDGET_DEVICE_COUNT;i++)
		{
			if(!strcmp(name, Phid_Device_Def[i].pdd_name))
			{
				phid->deviceIDSpec = Phid_Device_Def[i].pdd_sdid;
				phid->deviceDef = &Phid_Device_Def[i];
				phid->attr = Phid_Device_Def[i].pdd_attr;
				break;
			}
		}
		free(name);
		
		//type
		if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "type", &valLen))) return;
		if(!(type = malloc(valLen+1))) return;
		ZEROMEM(type, valLen+1);
		memcpy(type, valPtr, valLen);
		phid->deviceID = phidget_type_to_id(type);
		phid->deviceType = Phid_DeviceName[phid->deviceID];
		free(type);
	}
	
	phid->networkInfo->mdns = PTRUE;
	
}

void SBCFromTXT(CPhidgetSBCHandle sbc, uint16_t txtLen, const char *txtRecord)
{
	char *hversion = NULL, *txtver = NULL;
	
	uint8_t valLen = 0;
	const char *valPtr = NULL;
	
	//txt version
	if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "txtvers", &valLen))) return;
	if(!(txtver = malloc(valLen+1))) return;
	ZEROMEM(txtver, valLen+1);
	memcpy(txtver, valPtr, valLen);
	sbc->txtver = (short)strtol(txtver, NULL, 10);
	free(txtver);
	
	//Firmware version
	if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "fversion", &valLen))) return;
	if(valLen > 12) valLen = 12;
	memcpy(sbc->fversion, valPtr, valLen);
	sbc->fversion[valLen] = '\0';
	
	//Hardware version
	if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "hversion", &valLen))) return;
	if(!(hversion = malloc(valLen+1))) return;
	ZEROMEM(hversion, valLen+1);
	memcpy(hversion, valPtr, valLen);
	sbc->hversion = (short)strtol(hversion, NULL, 10);
	free(hversion);
	
	// things added in version 2 of the txt
	if(sbc->txtver >= 2)
	{
		//Hostname
		if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "hostname", &valLen))) return;
		if(valLen > 128) valLen = 128;
		memcpy(sbc->hostname, valPtr, valLen);
		sbc->hostname[valLen] = '\0';
	}
	
	// things added in version 3 of the txt
	if(sbc->txtver >= 3)
	{
		//Device Name
		if(!(valPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "name", &valLen))) return;
		if(valLen > 128) valLen = 128;
		memcpy(sbc->deviceName, valPtr, valLen);
		sbc->deviceName[valLen] = '\0';
	}
	else
	{
		sprintf(sbc->deviceName, "PhidgetSBC");
	}
}

void DNSServiceResolve_CallBack(
    AvahiServiceResolver *r,
    AVAHI_GCC_UNUSED AvahiIfIndex interface,
    AVAHI_GCC_UNUSED AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *address,
    uint16_t port,
    AvahiStringList *txt,
    AvahiLookupResultFlags flags,
    AVAHI_GCC_UNUSED void* userdata)
{    

	CPhidgetRemoteHandle networkInfo = (CPhidgetRemoteHandle)userdata;
	switch (event) {
        case AVAHI_RESOLVER_FAILURE:
            LOG(PHIDGET_LOG_ERROR, "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s", name, type, domain, avahi_strerror_ptr(avahi_client_errno_ptr(client)));
			networkInfo->zeroconf_host = strdup("err");
            break;
        case AVAHI_RESOLVER_FOUND: 
		{
			LOG(PHIDGET_LOG_INFO, "DNSServiceResolve_CallBack: %s",name);
			networkInfo->zeroconf_host = strdup(host_name);
			networkInfo->zeroconf_port = malloc(10);
			snprintf(networkInfo->zeroconf_port, 9, "%d", port);
		}
    }

    avahi_service_resolver_free_ptr(r);
}

void DNSServiceQueryRecord_Phidget_CallBack
	(
	AvahiRecordBrowser *b, 
	AvahiIfIndex interface, 
	AvahiProtocol protocol, 
	AvahiBrowserEvent event, 
	const char *name, 
	uint16_t clazz, 
	uint16_t type, 
	const void *rdata, 
	size_t size, 
	AvahiLookupResultFlags flags, 
	void *userdata
	)
{
	CPhidgetHandle phid = (CPhidgetHandle)userdata;
	CPhidgetManagerList *trav;
	
	switch(event)
	{
	case AVAHI_BROWSER_NEW:
		PhidFromTXT(phid, size, rdata);
		LOG(PHIDGET_LOG_INFO, "DNSServiceQueryRecord_Phidget_CallBack: %s",name);
		CThread_mutex_lock(&zeroconfPhidgetsLock);
		CThread_mutex_lock(&activeRemoteManagersLock);
		
		CPhidget_setStatusFlag(&phid->status, PHIDGET_ATTACHED_FLAG, &phid->lock);
		CPhidget_setStatusFlag(&phid->status, PHIDGET_REMOTE_FLAG, &phid->lock);
		CPhidget_setStatusFlag(&phid->status, PHIDGET_SERVER_CONNECTED_FLAG, &phid->lock);

		if(CList_findInList((CListHandle)zeroconfPhidgets, phid, CPhidget_areEqual, NULL))
		{
			CList_addToList((CListHandle *)&zeroconfPhidgets, phid, CPhidget_areEqual);
			//managers
			for (trav=activeRemoteManagers; trav; trav = trav->next)
			{
				if(trav->phidm->networkInfo->requested_address==NULL
					&& (trav->phidm->networkInfo->requested_serverID == NULL || !strcmp(trav->phidm->networkInfo->requested_serverID,phid->networkInfo->zeroconf_server_id)))
				{
					if (trav->phidm->fptrAttachChange && trav->phidm->state == PHIDGETMANAGER_ACTIVE)
						trav->phidm->fptrAttachChange((CPhidgetHandle)phid, trav->phidm->fptrAttachChangeptr);
				}
			}
		}
		CThread_mutex_unlock(&activeRemoteManagersLock);
		CThread_mutex_unlock(&zeroconfPhidgetsLock);
		break;
	case AVAHI_BROWSER_FAILURE:
		LOG(PHIDGET_LOG_ERROR, "DNSServiceQueryRecord_Phidget_CallBack returned error: %s", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
		break;
	case AVAHI_BROWSER_ALL_FOR_NOW:
		avahi_record_browser_free_ptr(b);
	case AVAHI_BROWSER_CACHE_EXHAUSTED:
		LOG(PHIDGET_LOG_INFO, "DNSServiceQueryRecord_Phidget_CallBack %s", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
		break;
	case AVAHI_BROWSER_REMOVE:
		break;
	}
}

void DNSServiceQueryRecord_SBC_CallBack
(
 AvahiRecordBrowser *b, 
 AvahiIfIndex interface, 
 AvahiProtocol protocol, 
 AvahiBrowserEvent event, 
 const char *name, 
 uint16_t clazz, 
 uint16_t type, 
 const void *rdata, 
 size_t size, 
 AvahiLookupResultFlags flags, 
 void *userdata
 )
{
	CPhidgetSBCHandle sbc = (CPhidgetSBCHandle)userdata, found_sbc;
	CPhidgetSBCManagerList *trav;
	
	switch(event)
	{
		case AVAHI_BROWSER_NEW:
			SBCFromTXT(sbc, size, rdata);
			LOG(PHIDGET_LOG_INFO, "DNSServiceQueryRecord_SBC_CallBack: %s",name);
			CThread_mutex_lock(&zeroconfSBCsLock);
			CThread_mutex_lock(&activeSBCManagersLock);
			
			//Check if it's in the list and if it's different, remove it to make way for the new one
			// (Sometimes, we don't get a proper detach notification)
			if(CList_findInList((CListHandle)zeroconfSBCs, sbc, CPhidgetSBC_areEqual, (void **)&found_sbc) == EPHIDGET_OK)
			{
				if(CPhidgetSBC_areExtraEqual(found_sbc, sbc) != PTRUE) //A version number has changed
				{
					//Remove from list - don't free until after detach event
					CList_removeFromList((CListHandle *)&zeroconfSBCs, found_sbc, CPhidgetSBC_areEqual, PFALSE, NULL);
					
					for (trav=activeSBCManagers; trav; trav = trav->next)
					{
						if (trav->sbcm->fptrDetachChange && trav->sbcm->state == PHIDGETMANAGER_ACTIVE)
							trav->sbcm->fptrDetachChange((CPhidgetSBCHandle)found_sbc, trav->sbcm->fptrDetachChangeptr);
					}
					
					CPhidgetSBC_free(found_sbc);
					
					//now we fall through and add back to new one
				}
				else //Nothing has changed, we didn't remove, don't add
				{
					CPhidgetSBC_free(sbc);
					goto dontadd;
				}
			}
			
			//now add it
			CList_addToList((CListHandle *)&zeroconfSBCs, sbc, CPhidgetSBC_areEqual);
			
			//send out events
			for (trav=activeSBCManagers; trav; trav = trav->next)
			{
				if (trav->sbcm->fptrAttachChange && trav->sbcm->state == PHIDGETMANAGER_ACTIVE)
					trav->sbcm->fptrAttachChange((CPhidgetSBCHandle)sbc, trav->sbcm->fptrAttachChangeptr);
				
			}
		dontadd:
			
			CThread_mutex_unlock(&activeSBCManagersLock);
			CThread_mutex_unlock(&zeroconfSBCsLock);
			break;
		case AVAHI_BROWSER_FAILURE:
			LOG(PHIDGET_LOG_ERROR, "DNSServiceQueryRecord_SBC_CallBack returned error: %s", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
			break;
		case AVAHI_BROWSER_ALL_FOR_NOW:
			avahi_record_browser_free_ptr(b);
		case AVAHI_BROWSER_CACHE_EXHAUSTED:
			LOG(PHIDGET_LOG_INFO, "DNSServiceQueryRecord_SBC_CallBack %s", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
			break;
		case AVAHI_BROWSER_REMOVE:
			break;
	}
}

void DNSServiceBrowse_Phidget_CallBack(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void* userdata) 
{

	CPhidgetHandle phid;
	CPhidgetManagerList *trav;
	
	int ret;
	
    switch (event) {

        case AVAHI_BROWSER_FAILURE:
            
            LOG(PHIDGET_LOG_WARNING, "(Browser) %s", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
            avahi_threaded_poll_quit_ptr(threaded_poll);
            return;

        case AVAHI_BROWSER_NEW:
		{
			char fullname[AVAHI_DOMAIN_NAME_MAX];
			
			if((CPhidget_create(&phid))) return;
			if((CPhidgetRemote_create(&phid->networkInfo))) return;

			phid->networkInfo->zeroconf_name = strdup(name);
			phid->networkInfo->zeroconf_type = strdup(type);
			phid->networkInfo->zeroconf_domain = strdup(domain);
			
            LOG(PHIDGET_LOG_INFO, "(Browser) NEW: service '%s' of type '%s' in domain '%s'", name, type, domain);
			
			if((ret = avahi_service_name_join_ptr(fullname, AVAHI_DOMAIN_NAME_MAX, name, type, domain)) != AVAHI_OK)
                LOG(PHIDGET_LOG_ERROR, "Failed avahi_service_name_join_ptr '%s': %s", name, avahi_strerror_ptr(ret));

			if(!(avahi_record_browser_new_ptr(client, interface, protocol, fullname, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_TXT, 0, DNSServiceQueryRecord_Phidget_CallBack, phid)))
                LOG(PHIDGET_LOG_ERROR, "Failed to resolve service '%s': %s", name, avahi_strerror_ptr(avahi_client_errno_ptr(client)));
			//gets added to list in callback
		}
            break;

        case AVAHI_BROWSER_REMOVE:
		{
			if((CPhidget_create(&phid))) return;
			if((CPhidgetRemote_create(&phid->networkInfo))) return;

			phid->networkInfo->zeroconf_name = strdup(name);
			phid->networkInfo->zeroconf_type = strdup(type);
			phid->networkInfo->zeroconf_domain = strdup(domain);
			
            LOG(PHIDGET_LOG_INFO, "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'", name, type, domain);

			//have to fill in phid manually from just the name
			int i;
			CPhidgetHandle found_phid;
			char *name_copy = strdup(name);
			for(i=0;i<strlen(name_copy);i++)
				if(name_copy[i] == '(') break;
			if(i<=1) return;
			name_copy[strlen(name_copy)-1]='\0';
			name_copy[i-1] = '\0';
			phid->serialNumber = strtol(name_copy+i+1, NULL, 10);
			phid->specificDevice = PTRUE;
			for(i = 0;i<PHIDGET_DEVICE_COUNT;i++)
				if(!strcmp(name_copy, Phid_Device_Def[i].pdd_name)) break;
			phid->deviceIDSpec = 0;
			phid->deviceDef = &Phid_Device_Def[i];
			phid->attr = Phid_Device_Def[i].pdd_attr;
			phid->deviceID = Phid_Device_Def[i].pdd_did;
			phid->deviceType = Phid_DeviceName[phid->deviceID];
			phid->networkInfo->mdns = PTRUE;

			CThread_mutex_lock(&zeroconfPhidgetsLock);
			CThread_mutex_lock(&activeRemoteManagersLock);

			CPhidget_clearStatusFlag(&phid->status, PHIDGET_ATTACHED_FLAG, &phid->lock);
			CPhidget_setStatusFlag(&phid->status, PHIDGET_DETACHING_FLAG, &phid->lock);
			CPhidget_setStatusFlag(&phid->status, PHIDGET_REMOTE_FLAG, &phid->lock);
			CPhidget_clearStatusFlag(&phid->status, PHIDGET_SERVER_CONNECTED_FLAG, &phid->lock);

			if(!CList_findInList((CListHandle)zeroconfPhidgets, phid, CPhidget_areEqual, (void **)&found_phid))
			{
				strcpy(phid->label, found_phid->label);
				phid->deviceVersion = found_phid->deviceVersion;
				CList_removeFromList((CListHandle *)&zeroconfPhidgets, phid, CPhidget_areExtraEqual, TRUE, CPhidget_free);
				//managers
				for (trav=activeRemoteManagers; trav; trav = trav->next)
				{
					if(trav->phidm->networkInfo->requested_address==NULL
						&& (trav->phidm->networkInfo->requested_serverID == NULL || !strcmp(trav->phidm->networkInfo->requested_serverID,phid->networkInfo->zeroconf_server_id)))
					{
						if (trav->phidm->fptrDetachChange && trav->phidm->state == PHIDGETMANAGER_ACTIVE)
							trav->phidm->fptrDetachChange((CPhidgetHandle)phid, trav->phidm->fptrDetachChangeptr);
					}
				}
				CPhidget_clearStatusFlag(&phid->status, PHIDGET_DETACHING_FLAG, &phid->lock);
				CPhidget_free(phid);
			}
			CThread_mutex_unlock(&activeRemoteManagersLock);
			CThread_mutex_unlock(&zeroconfPhidgetsLock);
			free(name_copy);
		}
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            LOG(PHIDGET_LOG_INFO, "(Browser) %s", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
            break;
    }
}

void DNSServiceBrowse_SBC_CallBack(
									   AvahiServiceBrowser *b,
									   AvahiIfIndex interface,
									   AvahiProtocol protocol,
									   AvahiBrowserEvent event,
									   const char *name,
									   const char *type,
									   const char *domain,
									   AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
									   void* userdata) 
{
	
	CPhidgetSBCHandle sbc, found_sbc;
	CPhidgetSBCManagerList *trav;
	int ret;
	
    switch (event) {
			
        case AVAHI_BROWSER_FAILURE:
            
            LOG(PHIDGET_LOG_WARNING, "(Browser) %s", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
            avahi_threaded_poll_quit_ptr(threaded_poll);
            return;
			
        case AVAHI_BROWSER_NEW:
		{
			char fullname[AVAHI_DOMAIN_NAME_MAX];
			
			if((CPhidgetSBC_create(&sbc))) return;
			if((CPhidgetRemote_create(&sbc->networkInfo))) return;
			
			sbc->networkInfo->zeroconf_name = strdup(name);
			sbc->networkInfo->zeroconf_type = strdup(type);
			sbc->networkInfo->zeroconf_domain = strdup(domain);
			sbc->networkInfo->mdns = PTRUE;
			
			strncpy(sbc->mac, name+12, 18); //name == 'PhidgetSBC (??:??:??:??:??:??)'
			sbc->mac[17] = '\0';
			
            LOG(PHIDGET_LOG_INFO, "(Browser) NEW: service '%s' of type '%s' in domain '%s'", name, type, domain);
			
			if((ret = avahi_service_name_join_ptr(fullname, AVAHI_DOMAIN_NAME_MAX, name, type, domain)) != AVAHI_OK)
                LOG(PHIDGET_LOG_ERROR, "Failed avahi_service_name_join_ptr '%s': %s", name, avahi_strerror_ptr(ret));
			
			if(!(avahi_record_browser_new_ptr(client, interface, protocol, fullname, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_TXT, 0, DNSServiceQueryRecord_SBC_CallBack, sbc)))
                LOG(PHIDGET_LOG_ERROR, "Failed to resolve service '%s': %s", name, avahi_strerror_ptr(avahi_client_errno_ptr(client)));
			//gets added to list in callback
		}
            break;
			
        case AVAHI_BROWSER_REMOVE:
		{
			if((CPhidgetSBC_create(&sbc))) return;
			if((CPhidgetRemote_create(&sbc->networkInfo))) return;
			
			sbc->networkInfo->zeroconf_name = strdup(name);
			sbc->networkInfo->zeroconf_type = strdup(type);
			sbc->networkInfo->zeroconf_domain = strdup(domain);
			sbc->networkInfo->mdns = PTRUE;
			
			strncpy(sbc->mac, name+12, 18); //name == 'PhidgetSBC (??:??:??:??:??:??)'
			sbc->mac[17] = '\0';
			
            LOG(PHIDGET_LOG_INFO, "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'", name, type, domain);
			
			
			CThread_mutex_lock(&zeroconfSBCsLock);
			CThread_mutex_lock(&activeSBCManagersLock);
			
			if(CList_findInList((CListHandle)zeroconfSBCs, sbc, CPhidgetSBC_areEqual, (void **)&found_sbc) == EPHIDGET_OK)
			{
				CList_removeFromList((CListHandle *)&zeroconfSBCs, found_sbc, CPhidgetSBC_areEqual, PFALSE, NULL);
				//managers
				for (trav=activeSBCManagers; trav; trav = trav->next)
				{
					if (trav->sbcm->fptrDetachChange && trav->sbcm->state == PHIDGETMANAGER_ACTIVE)
						trav->sbcm->fptrDetachChange((CPhidgetSBCHandle)found_sbc, trav->sbcm->fptrDetachChangeptr);
				}
				CPhidgetSBC_free(found_sbc);
			}
			
			CThread_mutex_unlock(&activeSBCManagersLock);
			CThread_mutex_unlock(&zeroconfSBCsLock);
			
			CPhidgetSBC_free(sbc);
		}
            break;
			
        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            LOG(PHIDGET_LOG_INFO, "(Browser) %s", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
            break;
    }
}

void DNSServiceBrowse_ws_CallBack(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void* userdata) 
{

    switch (event) {

        case AVAHI_BROWSER_FAILURE:
            
            LOG(PHIDGET_LOG_ERROR, "(Browser) %s", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
            //avahi_threaded_poll_quit_ptr(threaded_poll);
            return;

        case AVAHI_BROWSER_NEW:
		{
			CPhidgetRemoteHandle networkInfo;
			if((CPhidgetRemote_create(&networkInfo))) return;

			networkInfo->zeroconf_name = strdup(name);
			networkInfo->zeroconf_server_id = strdup(name);
			networkInfo->zeroconf_type = strdup(type);
			networkInfo->zeroconf_domain = strdup(domain);
			
            LOG(PHIDGET_LOG_INFO, "(Browser) NEW: service '%s' of type '%s' in domain '%s'", name, type, domain);

			CThread_mutex_lock(&zeroconfServersLock);
			CList_addToList((CListHandle *)&zeroconfServers, networkInfo, CPhidgetRemote_areEqual);
			CThread_mutex_unlock(&zeroconfServersLock);
		}
            break;

        case AVAHI_BROWSER_REMOVE:
		{
			CPhidgetRemoteHandle networkInfo;
			if((CPhidgetRemote_create(&networkInfo))) return;

			networkInfo->zeroconf_name = strdup(name);
			networkInfo->zeroconf_server_id = strdup(name);
			networkInfo->zeroconf_type = strdup(type);
			networkInfo->zeroconf_domain = strdup(domain);
            LOG(PHIDGET_LOG_INFO, "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'", name, type, domain);

			CThread_mutex_lock(&zeroconfServersLock);
			CList_removeFromList((CListHandle *)&zeroconfServers, networkInfo, CPhidgetRemote_areEqual, TRUE, CPhidgetRemote_free);
			CThread_mutex_unlock(&zeroconfServersLock);
		}
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            LOG(PHIDGET_LOG_INFO, "(Browser) %s", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
            break;
    }
}

//Does nothing in Avahi
int cancelPendingZeroconfLookups(CPhidgetRemoteHandle networkInfo)
{	
	return EPHIDGET_OK;
}

int getZeroconfHostPort(CPhidgetRemoteHandle networkInfo)
{
	int timeout = 200; //2000ms
	
	if(networkInfo->zeroconf_host) free(networkInfo->zeroconf_host);
	networkInfo->zeroconf_host = NULL;
	if(networkInfo->zeroconf_port) free(networkInfo->zeroconf_port);
	networkInfo->zeroconf_port = NULL;
	
	//lock the thread before accessing the client - nope, it messes up if this is from the same thread (via attach/detach)
	//avahi_threaded_poll_lock_ptr(threaded_poll);
	if (!(avahi_service_resolver_new_ptr(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 
					   networkInfo->zeroconf_name, //name
					   networkInfo->zeroconf_type, // service type
					   networkInfo->zeroconf_domain, //domain
					   AVAHI_PROTO_UNSPEC, 0, DNSServiceResolve_CallBack, networkInfo)))
	{
		LOG(PHIDGET_LOG_ERROR, "Failed to resolve service '%s': %s", networkInfo->zeroconf_name, avahi_strerror_ptr(avahi_client_errno_ptr(client)));
		//avahi_threaded_poll_unlock_ptr(threaded_poll);
		return EPHIDGET_UNEXPECTED;
	}
	//avahi_threaded_poll_unlock_ptr(threaded_poll);

	while(!networkInfo->zeroconf_host)
	{
		usleep(10000);
		timeout--;
		if(!timeout)
		{
			LOG(PHIDGET_LOG_ERROR, "getZeroconfHostPort didn't work (timeout)");
			return EPHIDGET_UNEXPECTED;
		}
	}
	
	if(!strcmp(networkInfo->zeroconf_host, "err"))
	{
		LOG(PHIDGET_LOG_ERROR, "getZeroconfHostPort didn't work (error)");
		free(networkInfo->zeroconf_host);
		networkInfo->zeroconf_host = NULL;
		return EPHIDGET_UNEXPECTED;
	}
	
	return EPHIDGET_OK;
}

int refreshZeroconfSBC(CPhidgetSBCHandle sbc)
{
	return EPHIDGET_OK;
}

int refreshZeroconfPhidget(CPhidgetHandle phid)
{
	return EPHIDGET_OK;
}

int InitializeZeroconf()
{
    int error;
    //int ret = 1;
	int timeout = 50; //500ms
	const char *avahiVersion;
	
	CThread_mutex_lock(&zeroconfInitLock);
	if(Dns_sdInitialized) 
	{
		CThread_mutex_unlock(&zeroconfInitLock);
		return EPHIDGET_OK;
	}
	
#ifdef ZEROCONF_RUNTIME_LINKING

	avahiLibHandle = dlopen("libavahi-client.so",RTLD_LAZY);
	if(!avahiLibHandle)
	{
		avahiLibHandle = dlopen("libavahi-client.so.3",RTLD_LAZY);
	}
	if(!avahiLibHandle)
	{
		LOG(PHIDGET_LOG_WARNING, "dlopen failed with error: %s", dlerror());
		LOG(PHIDGET_LOG_WARNING, "Assuming that zeroconf is not supported on this machine.");
		CThread_mutex_unlock(&zeroconfInitLock);
		return EPHIDGET_UNSUPPORTED;
	}

	//These are always in Avahi
	if(!(avahi_client_get_version_string_ptr = (avahi_client_get_version_string_type)dlsym(avahiLibHandle, "avahi_client_get_version_string"))) goto dlsym_err;
	if(!(avahi_service_browser_new_ptr = (avahi_service_browser_new_type)dlsym(avahiLibHandle, "avahi_service_browser_new"))) goto dlsym_err;
	if(!(avahi_service_resolver_new_ptr = (avahi_service_resolver_new_type)dlsym(avahiLibHandle, "avahi_service_resolver_new"))) goto dlsym_err;
	if(!(avahi_service_resolver_free_ptr = (avahi_service_resolver_free_type)dlsym(avahiLibHandle, "avahi_service_resolver_free"))) goto dlsym_err;
	if(!(avahi_record_browser_new_ptr = (avahi_record_browser_new_type)dlsym(avahiLibHandle, "avahi_record_browser_new"))) goto dlsym_err;
	if(!(avahi_record_browser_free_ptr = (avahi_record_browser_free_type)dlsym(avahiLibHandle, "avahi_record_browser_free"))) goto dlsym_err;
	if(!(avahi_service_name_join_ptr = (avahi_service_name_join_type)dlsym(avahiLibHandle, "avahi_service_name_join"))) goto dlsym_err;
	if(!(avahi_client_new_ptr = (avahi_client_new_type)dlsym(avahiLibHandle, "avahi_client_new"))) goto dlsym_err;
	if(!(avahi_client_free_ptr = (avahi_client_free_type)dlsym(avahiLibHandle, "avahi_client_free"))) goto dlsym_err;
	if(!(avahi_strerror_ptr = (avahi_strerror_type)dlsym(avahiLibHandle, "avahi_strerror"))) goto dlsym_err;
	if(!(avahi_client_errno_ptr = (avahi_client_errno_type)dlsym(avahiLibHandle, "avahi_client_errno"))) goto dlsym_err;
	
	//These are in Avahi > 0.6.4
	if(!(avahi_threaded_poll_new_ptr = (avahi_threaded_poll_new_type)dlsym(avahiLibHandle, "avahi_threaded_poll_new"))) goto dlsym_err2;
	if(!(avahi_threaded_poll_free_ptr = (avahi_threaded_poll_free_type)dlsym(avahiLibHandle, "avahi_threaded_poll_free"))) goto dlsym_err2;
	if(!(avahi_threaded_poll_get_ptr = (avahi_threaded_poll_get_type)dlsym(avahiLibHandle, "avahi_threaded_poll_get"))) goto dlsym_err2;
	if(!(avahi_threaded_poll_start_ptr = (avahi_threaded_poll_start_type)dlsym(avahiLibHandle, "avahi_threaded_poll_start"))) goto dlsym_err2;
	if(!(avahi_threaded_poll_stop_ptr = (avahi_threaded_poll_stop_type)dlsym(avahiLibHandle, "avahi_threaded_poll_stop"))) goto dlsym_err2;
	if(!(avahi_threaded_poll_quit_ptr = (avahi_threaded_poll_quit_type)dlsym(avahiLibHandle, "avahi_threaded_poll_quit"))) goto dlsym_err2;
	if(!(avahi_threaded_poll_lock_ptr = (avahi_threaded_poll_lock_type)dlsym(avahiLibHandle, "avahi_threaded_poll_lock"))) goto dlsym_err2;
	if(!(avahi_threaded_poll_unlock_ptr = (avahi_threaded_poll_unlock_type)dlsym(avahiLibHandle, "avahi_threaded_poll_unlock"))) goto dlsym_err2;
	
	goto dlsym_good;
	
dlsym_err:
	LOG(PHIDGET_LOG_WARNING, "dlsym failed with error: %s", dlerror());
	LOG(PHIDGET_LOG_WARNING, "Assuming that zeroconf is not supported on this machine.");
	CThread_mutex_unlock(&zeroconfInitLock);
	return EPHIDGET_UNSUPPORTED;
	
	//Old avahi didn't have the thread functions
dlsym_err2:
	LOG(PHIDGET_LOG_WARNING, "dlsym failed with error: %s", dlerror());
	LOG(PHIDGET_LOG_WARNING, "Avahi is too old, upgrade to at least version 0.6.4.");
	LOG(PHIDGET_LOG_WARNING, "Zeroconf will not be used on this machine.");
	CThread_mutex_unlock(&zeroconfInitLock);
	return EPHIDGET_UNSUPPORTED;
	
dlsym_good:
		
#endif

    /* Allocate main loop object */
    if (!(threaded_poll = avahi_threaded_poll_new_ptr())) {
        LOG(PHIDGET_LOG_ERROR, "Failed to create threaded poll object.");
		CThread_mutex_unlock(&zeroconfInitLock);
        return EPHIDGET_UNEXPECTED;
    }
	
    /* Allocate a new client */
    client = avahi_client_new_ptr(avahi_threaded_poll_get_ptr(threaded_poll), 0, client_callback, NULL, &error);

    /* Check wether creating the client object succeeded */
    if (!client) {
        LOG(PHIDGET_LOG_ERROR, "Failed to create client: %s", avahi_strerror_ptr(error));
		CThread_mutex_unlock(&zeroconfInitLock);
        return EPHIDGET_UNEXPECTED;
    }
	
	//get version
	avahiVersion = avahi_client_get_version_string_ptr(client);
	
	/* Create the service browsers */
    if (!(zeroconf_browse_ws_ref = avahi_service_browser_new_ptr(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_phidget_ws._tcp", NULL, 0, DNSServiceBrowse_ws_CallBack, client))) {
        LOG(PHIDGET_LOG_ERROR, "Failed to create service browser: %s", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
		CThread_mutex_unlock(&zeroconfInitLock);
        return EPHIDGET_UNEXPECTED;
    }
    if (!(zeroconf_browse_phidget_ref = avahi_service_browser_new_ptr(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_phidget._tcp", NULL, 0, DNSServiceBrowse_Phidget_CallBack, client))) {
        LOG(PHIDGET_LOG_ERROR, "Failed to create service browser: %s", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
		CThread_mutex_unlock(&zeroconfInitLock);
        return EPHIDGET_UNEXPECTED;
    }
    if (!(zeroconf_browse_sbc_ref = avahi_service_browser_new_ptr(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_phidget_sbc._tcp", NULL, 0, DNSServiceBrowse_SBC_CallBack, client))) {
        LOG(PHIDGET_LOG_ERROR, "Failed to create service browser: %s", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
		CThread_mutex_unlock(&zeroconfInitLock);
        return EPHIDGET_UNEXPECTED;
    }
	
	if(avahi_threaded_poll_start_ptr(threaded_poll))
	{
		LOG(PHIDGET_LOG_ERROR, "avahi_threaded_poll_start_ptr failed");
		CThread_mutex_unlock(&zeroconfInitLock);
        return EPHIDGET_UNEXPECTED;
	}
	//Thread is started successfully
	else
	{
		//There is a bug in at least Avahi 0.6.16 (Debian Etch default) where thread_running is not set, so quit doesn't work!!!???!?!?!?!?!?!
		//This is fixed in 0.6.24
		//So I'll set it myself here
		if(strcmp(avahiVersion, "avahi 0.6.24") < 0)
		{
			LOG(PHIDGET_LOG_INFO, "Fixing thread_running bug in avahi < 0.6.24");
			threaded_poll->thread_running = 1;
		}
	}
	
	while(!Dns_sdInitialized)
	{
		usleep(10000);
		timeout--;
		if(!timeout)
		{
			UninitializeZeroconf();
			LOG(PHIDGET_LOG_ERROR, "InitializeZeroconf Seems bad... Dns_sdInitialized wasn't set to true.");
			CThread_mutex_unlock(&zeroconfInitLock);
			return EPHIDGET_UNEXPECTED;
		}
	}
	
	LOG(PHIDGET_LOG_INFO, "InitializeZeroconf Seems good... (%s)",avahiVersion);
	
	CThread_mutex_unlock(&zeroconfInitLock);
	return EPHIDGET_OK;
	
}

int UninitializeZeroconf()
{
	int ret;
    /* Cleanup things */
	CThread_mutex_lock(&zeroconfInitLock);
	if(Dns_sdInitialized)
	{
		if (threaded_poll)
		{
			if((ret = avahi_threaded_poll_stop_ptr(threaded_poll)) == -1)
				LOG(PHIDGET_LOG_WARNING, "avahi_threaded_poll_stop failed",ret);
			avahi_client_free_ptr(client);
			avahi_threaded_poll_free_ptr(threaded_poll);
			threaded_poll = NULL;
			client = NULL;
		}
	}
		
	Dns_sdInitialized = FALSE;
	CThread_mutex_unlock(&zeroconfInitLock);
	return EPHIDGET_OK;
}

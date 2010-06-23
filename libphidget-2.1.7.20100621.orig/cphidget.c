#include "stdafx.h"
#include "cphidget.h"
#include "cusb.h"
#include "csocket.h"
#include "cthread.h"
#include "cphidgetlist.h"

int print_debug_messages = FALSE;

CPhidgetListHandle ActiveDevices = 0;
CPhidgetListHandle AttachedDevices = 0;

/* Protects ActiveDevices and AttachedDevices */
int phidgetLocksInitialized = PFALSE;
CThread_mutex_t activeDevicesLock;
CThread_mutex_t attachedDevicesLock;

char
translate_bool_to_ascii(char value)
{
	switch (value) {
	case PTRUE:
		return '1';
	case PFALSE:
		return '0';
	}
	return '?';
}

int CCONV phidget_type_to_id(const char *name)
{
	int i;
	for(i=0;i<PHIDGET_DEVICE_CLASS_COUNT;i++)
	{
		if(Phid_DeviceName[i] != NULL)
		{
			if(!strcmp(Phid_DeviceName[i],name))
				return i;
		}
	}
	return -1;
}

//returns ms
double timestampdiff(CPhidget_Timestamp time1, CPhidget_Timestamp time2)
{
	return ((time1.seconds * 1000) + (double)(time1.microseconds / 1000.0)) - ((time2.seconds * 1000) + (double)(time2.microseconds / 1000.0));
}

double timeSince(TIME *start)
{
#ifdef _WINDOWS
	TIME now;
	FILETIME nowft, oldft, resft;
	ULARGE_INTEGER nowul, oldul, resul;
	double duration;
	GetSystemTime(&now);
	SystemTimeToFileTime(&now, &nowft);
	SystemTimeToFileTime(start, &oldft);

	nowul.HighPart = nowft.dwHighDateTime;
	nowul.LowPart = nowft.dwLowDateTime;
	oldul.HighPart = oldft.dwHighDateTime;
	oldul.LowPart = oldft.dwLowDateTime;

	resul.HighPart = nowul.HighPart - oldul.HighPart;
	resul.LowPart = nowul.LowPart - oldul.LowPart;

	resft.dwHighDateTime = resul.HighPart;
	resft.dwLowDateTime = resul.LowPart;

	duration = (double)(resft.dwLowDateTime/10000000.0);
#else
	struct timeval now;
	gettimeofday(&now, NULL);
	double duration = (now.tv_sec - start->tv_sec) + (double)((now.tv_usec-start->tv_usec)/1000000.0);
#endif
	return duration;
}

void setTimeNow(TIME *now)
{
	#ifdef _WINDOWS
		GetSystemTime(now);
	#else
		gettimeofday(now,NULL);
	#endif
}

/* The is a void pointer that gets malloced only of some platforms */
void CPhidgetFHandle_free(void *arg)
{
	#if defined(_MACOSX) || defined(WINCE)
	return;
	#else
	free(arg); arg = NULL;
	return;
	#endif
}

//Two different Phidget Handles that refer to the same Physical Phidget
int CCONV CPhidget_areEqual(void *arg1, void *arg2)
{
	CPhidgetHandle phid1 = (CPhidgetHandle)arg1;
	CPhidgetHandle phid2 = (CPhidgetHandle)arg2;
	
	TESTPTRS(phid1, phid2)
		
	if(
		(
		(phid1->specificDevice == 0 || phid2->specificDevice == 0)?1:(phid2->serialNumber == phid1->serialNumber))
		&&
		(
		(phid1->deviceIDSpec == 0 || phid2->deviceIDSpec == 0)?1:(phid2->deviceIDSpec == phid1->deviceIDSpec)
		) &&
		(phid2->deviceID == phid1->deviceID))
		return 1;
	return 0;
}
//Two different Phidget Handles that refer to the same Physical Phidget - more stringent
int CCONV CPhidget_areExtraEqual(void *arg1, void *arg2)
{
	CPhidgetHandle phid1 = (CPhidgetHandle)arg1;
	CPhidgetHandle phid2 = (CPhidgetHandle)arg2;
	
	TESTPTRS(phid1, phid2)
		
	if(
		(phid2->serialNumber == phid1->serialNumber) &&
		(phid2->deviceIDSpec == phid1->deviceIDSpec) &&
		(phid2->deviceID == phid1->deviceID))
		return 1;
	return 0;
}

//Two identical Phidget Handles (same pointer)
int CCONV CPhidgetHandle_areEqual(void *arg1, void *arg2)
{
	CPhidgetHandle phid1 = (CPhidgetHandle)arg1;
	CPhidgetHandle phid2 = (CPhidgetHandle)arg2;
	
	TESTPTRS(phid1, phid2)

	if(arg1 == arg2) return 1;
	return 0;
}

void CCONV CPhidget_free(void *arg)
{
	CPhidgetHandle phid = (CPhidgetHandle)arg;
	if (!phid)
		return;

	//Call device specific free function if it exists
	if(phid->fptrFree)
		phid->fptrFree(phid);

	//this is only malloc-ed on windows and linux, not wince or mac
#if (defined(_WINDOWS) && !defined(WINCE)) || defined(_LINUX)
	if (phid->CPhidgetFHandle) {
		CPhidgetFHandle_free(phid->CPhidgetFHandle); phid->CPhidgetFHandle = NULL;
	}
#endif

	CThread_mutex_destroy(&phid->lock);
	CThread_mutex_destroy(&phid->openCloseLock);
	CThread_mutex_destroy(&phid->writelock);
	CThread_mutex_destroy(&phid->outputLock);
	CThread_destroy_event(&phid->writeAvailableEvent);
	CThread_destroy_event(&phid->writtenEvent);

	free(phid); phid = NULL;
	return;
}

int CCONV CPhidget_create(CPhidgetHandle *phid)
{
	CPhidgetHandle temp_phid;

	TESTPTR(phid)
	
	if(!(temp_phid = malloc(sizeof(CPhidget))))
		return EPHIDGET_NOMEMORY;
	ZEROMEM(temp_phid, sizeof(CPhidget));

	CThread_mutex_init(&temp_phid->lock);
	CThread_mutex_init(&temp_phid->openCloseLock);
	CThread_mutex_init(&temp_phid->writelock);
	CThread_mutex_init(&temp_phid->outputLock);
	CThread_create_event(&temp_phid->writeAvailableEvent);
	CThread_create_event(&temp_phid->writtenEvent);

	CPhidget_clearStatusFlag(&temp_phid->status, PHIDGET_ATTACHED_FLAG, &temp_phid->lock);

	*phid = temp_phid;
	 
	 return EPHIDGET_OK;
}

const char *CPhidget_strerror(int error)
{
	if ((error < 0) || (error >= PHIDGET_ERROR_CODE_COUNT))
		return Phid_UnknownErrorDescription;

	return Phid_ErrorDescriptions[error];
}

int CPhidget_statusFlagIsSet(int status, int flag)
{
	if(status & flag) return PTRUE;
	return PFALSE;
}

//status is a flags variable
int CPhidget_setStatusFlag(int *status, int flag, CThread_mutex_t *lock)
{
	TESTPTR(status)

	if(lock != NULL) CThread_mutex_lock(lock);
	*status |= flag;
	if(lock != NULL) CThread_mutex_unlock(lock);

	return EPHIDGET_OK;
}

int CPhidget_clearStatusFlag(int *status, int flag, CThread_mutex_t *lock)
{
	TESTPTR(status)

	if(lock != NULL) CThread_mutex_lock(lock);
	*status &= (~flag);
	if(lock != NULL) CThread_mutex_unlock(lock);

	return EPHIDGET_OK;
}

int CPhidget_read(CPhidgetHandle phid)
{
	int result = EPHIDGET_OK;

	TESTPTR(phid)

	if (CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG)
		|| CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHING_FLAG)) {
		result = CUSBReadPacket((CPhidgetHandle)phid,
		    phid->lastReadPacket);
		if (result) return result;
		if (phid->fptrData)
			result= phid->fptrData((CPhidgetHandle)phid,
			    phid->lastReadPacket, phid->inputReportByteLength);
		return result;
	}
	return EPHIDGET_NOTATTACHED;
}


int CPhidget_write(CPhidgetHandle phid)
{
	unsigned char buffer[MAX_OUT_PACKET_SIZE];
	unsigned int len;
	int result = EPHIDGET_OK;
	
	TESTPTR(phid)
	
	ZEROMEM(buffer, sizeof(buffer));
	
	CThread_reset_event(&phid->writeAvailableEvent);
	
	len = MAX_OUT_PACKET_SIZE;
	if ((result = phid->fptrGetPacket((CPhidgetHandle)phid, buffer, &len))
		!= EPHIDGET_OK)
		goto fail;
	// XXX len ignored
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) !=
		EPHIDGET_OK)
		goto fail;
fail:
	/*
	 * under all circumstances (especially failure), signal
	 * waiting writers to loop around and check device status.
	 */
	CThread_set_event(&phid->writtenEvent);
	return result;
}

//Begin exported functions
int CCONV
CPhidget_open(CPhidgetHandle phid, int serialNumber)
{
	int result = 0;
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

	result = RegisterLocalDevice(phid);
	
	CPhidget_setStatusFlag(&phid->status, PHIDGET_OPENED_FLAG, &phid->lock);
	CThread_mutex_unlock(&phid->openCloseLock);

	return result; 
}

int CCONV
CPhidget_close(CPhidgetHandle phid)
{ 
	int result = EPHIDGET_OK;
	struct sockaddr name;
	struct sockaddr_in *name_in;
	socklen_t namelen = sizeof(name);

	char key[1024], val[6];

	TESTPTR(phid)
	
	CThread_mutex_lock(&phid->openCloseLock);
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_OPENED_FLAG))
	{
		LOG(PHIDGET_LOG_WARNING, "Close was called on an already closed Phidget handle.");
		CThread_mutex_unlock(&phid->openCloseLock);
		return EPHIDGET_OK;
	}

	//Call device specific close function if it exists
	if(phid->fptrClose)
		phid->fptrClose(phid);

	if(CPhidget_statusFlagIsSet(phid->status, PHIDGET_REMOTE_FLAG))
	{
		CThread_mutex_lock(&phid->lock);
		if(CPhidget_statusFlagIsSet(phid->status, PHIDGET_SERVER_CONNECTED_FLAG))
		{
			getsockname(phid->networkInfo->server->socket, &name, &namelen);
			name_in = (struct sockaddr_in *)&name;

			if(phid->specificDevice)
				snprintf(key, sizeof(key), "/PCK/Client/%s/%d/%s/%d", inet_ntoa(name_in->sin_addr), (int)name_in->sin_port, 
					Phid_DeviceName[phid->deviceID], phid->serialNumber);
			else
				snprintf(key, sizeof(key), "/PCK/Client/%s/%d/%s", inet_ntoa(name_in->sin_addr), (int)name_in->sin_port, 
					Phid_DeviceName[phid->deviceID]);
			snprintf(val, sizeof(val), "Close");
			//don't care about errors on this, so we don't add the error handler - just close already!
			pdc_async_set(phid->networkInfo->server->pdcs, key, val, (int)strlen(val), PTRUE, NULL, NULL); 
		}
		CThread_mutex_unlock(&phid->lock);

		result = unregisterRemotePhidget(phid);

		phid->keyCount = 0;
	}
	else
	{
		if(!phidgetLocksInitialized)
		{
			CThread_mutex_init(&activeDevicesLock);
			CThread_mutex_init(&attachedDevicesLock);
			phidgetLocksInitialized = PTRUE;
		}
		CThread_mutex_lock(&activeDevicesLock);
		CList_removeFromList((CListHandle *)&ActiveDevices, phid, CPhidget_areEqual, FALSE, NULL);
		CThread_mutex_unlock(&activeDevicesLock);
		if (CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG)) {
			phid->writeStopFlag = PTRUE;
			CThread_join(&phid->writeThread); //join before closing because we want to wait for outstanding writes to complete
			result = CUSBCloseHandle(phid);
			CThread_join(&phid->readThread);
		}
		
		//BL:What do these constants mean?
		if(phid->specificDevice == 2)
		{
			phid->specificDevice = 0;
			phid->serialNumber = -1;
		}

		//if there are no more active phidgets or managers, wait for the central thread to exit
		if(!ActiveDevices && !ActivePhidgetManagers)
		{
			JoinCentralThread();
		}
	}
	CPhidget_clearStatusFlag(&phid->status, PHIDGET_OPENED_FLAG, &phid->lock);
	CThread_mutex_unlock(&phid->openCloseLock);
	return result;
}

//TODO: maybe this should take (CPhidgetHandle *) so that we can nulify the pointer
// (and handle multiple calls to CPhidget_delete)
int CCONV
CPhidget_delete(CPhidgetHandle phid)
{
	CPhidget_free(phid);
	return EPHIDGET_OK;
}

int CCONV
CPhidget_set_OnDetach_Handler(CPhidgetHandle phid,
    int(CCONV *fptr)(CPhidgetHandle, void *), void *userPtr)
{
	TESTPTR(phid)
	phid->fptrDetach = fptr;
	phid->fptrDetachptr = userPtr;
	return EPHIDGET_OK;
}

int CCONV
CPhidget_set_OnAttach_Handler(CPhidgetHandle phid,
    int(CCONV *fptr)(CPhidgetHandle, void *), void *userPtr)
{
	TESTPTR(phid)
	phid->fptrAttach = fptr;
	phid->fptrAttachptr = userPtr;
	return EPHIDGET_OK;
}

int CCONV
CPhidget_set_OnError_Handler(CPhidgetHandle phid,
    int (CCONV *fptr) (CPhidgetHandle, void *, int, const char *), void *userPtr)
{
	TESTPTR(phid)
	phid->fptrError = fptr;
	phid->fptrErrorptr = userPtr;
	return EPHIDGET_OK;
}
int CCONV CPhidget_set_OnServerConnect_Handler(CPhidgetHandle phid, int (CCONV *fptr)(CPhidgetHandle phid, void *userPtr), void *userPtr)
{
	TESTPTR(phid)
	phid->fptrConnect = fptr; 
	phid->fptrConnectptr = userPtr; 
	return EPHIDGET_OK; 
}
int CCONV CPhidget_set_OnServerDisconnect_Handler(CPhidgetHandle phid, int (CCONV *fptr)(CPhidgetHandle phid, void *userPtr), void *userPtr)
{
	TESTPTR(phid)
	phid->fptrDisconnect = fptr; 
	phid->fptrDisconnectptr = userPtr; 
	return EPHIDGET_OK; 
}

int CCONV
CPhidget_getDeviceName(CPhidgetHandle phid, const char **buffer)
{
	TESTPTRS(phid, buffer)
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG)
		&& !CPhidget_statusFlagIsSet(phid->status, PHIDGET_DETACHING_FLAG))
		return EPHIDGET_NOTATTACHED;

	*buffer = (char *)phid->deviceDef->pdd_name;
	return EPHIDGET_OK;
}

int CCONV
CPhidget_getSerialNumber(CPhidgetHandle phid, int *serialNumber)
{
	TESTPTRS(phid, serialNumber)
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG)
		&& !CPhidget_statusFlagIsSet(phid->status, PHIDGET_DETACHING_FLAG))
		return EPHIDGET_NOTATTACHED;

	*serialNumber = phid->serialNumber;
	return EPHIDGET_OK;
}

int CCONV
CPhidget_getDeviceVersion(CPhidgetHandle phid, int *devVer)
{
	TESTPTRS(phid, devVer)
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG)
		&& !CPhidget_statusFlagIsSet(phid->status, PHIDGET_DETACHING_FLAG))
		return EPHIDGET_NOTATTACHED;

	*devVer = phid->deviceVersion;
	return EPHIDGET_OK;
}

/* for now this just returns the attached bit of the status variable - this function should probably be renamed*/
// This CAN be called on closed devices, this should NOT be called internally as it is confusing
int CCONV
CPhidget_getDeviceStatus(CPhidgetHandle phid, int *status)
{
	TESTPTRS(phid, status)

	*status = CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG);
	return EPHIDGET_OK;
}

/* for now this just returns the attached bit of the status variable */
int CCONV
CPhidget_getServerStatus(CPhidgetHandle phid, int *status)
{
	TESTPTRS(phid, status)

	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_REMOTE_FLAG))
		return EPHIDGET_UNSUPPORTED;

	CThread_mutex_lock(&phid->lock);
	if(CPhidget_statusFlagIsSet(phid->status, PHIDGET_SERVER_CONNECTED_FLAG))
		*status = CPhidget_statusFlagIsSet(phid->networkInfo->server->status, PHIDGETSOCKET_CONNECTED_FLAG);
	else
		*status = PFALSE;
	CThread_mutex_unlock(&phid->lock);

	return EPHIDGET_OK;
}


int CCONV
CPhidget_getLibraryVersion(const char **buffer)
{
	TESTPTR(buffer)
	*buffer = LibraryVersion;
	return EPHIDGET_OK;
}

int CCONV
CPhidget_getDeviceType(CPhidgetHandle phid, const char **buffer)
{
	TESTPTRS(phid, buffer)
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG)
		&& !CPhidget_statusFlagIsSet(phid->status, PHIDGET_DETACHING_FLAG))
		return EPHIDGET_NOTATTACHED;

	*buffer = (char *)Phid_DeviceName[phid->deviceID];
	return EPHIDGET_OK;
}

int CCONV
CPhidget_getDeviceID(CPhidgetHandle phid, CPhidget_DeviceID *deviceID)
{
	TESTPTRS(phid, deviceID)
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG)
		&& !CPhidget_statusFlagIsSet(phid->status, PHIDGET_DETACHING_FLAG))
		return EPHIDGET_NOTATTACHED;

	*deviceID = phid->deviceIDSpec;
	return EPHIDGET_OK;
}

int CCONV
CPhidget_getDeviceClass(CPhidgetHandle phid, CPhidget_DeviceClass *deviceClass)
{
	TESTPTRS(phid, deviceClass)
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG)
		&& !CPhidget_statusFlagIsSet(phid->status, PHIDGET_DETACHING_FLAG))
		return EPHIDGET_NOTATTACHED;

	*deviceClass = phid->deviceID;
	return EPHIDGET_OK;
}

int CCONV
CPhidget_getDeviceLabel(CPhidgetHandle phid, const char **buffer)
{
	TESTPTRS(phid, buffer)
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG)
		&& !CPhidget_statusFlagIsSet(phid->status, PHIDGET_DETACHING_FLAG))
		return EPHIDGET_NOTATTACHED;

	*buffer = (char *)phid->label;
	return EPHIDGET_OK;
}

int CCONV
CPhidget_setDeviceLabel(CPhidgetHandle phid, const char *buffer)
{
	TESTPTRS(phid, buffer)
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG))
		return EPHIDGET_NOTATTACHED;

	if(CPhidget_statusFlagIsSet(phid->status, PHIDGET_REMOTE_FLAG))
	{
		char key[1024];
		snprintf(key, sizeof(key), "/PCK/%s/%d/Label", phid->deviceType, phid->serialNumber);
		CThread_mutex_lock(&phid->lock);
		if(!CPhidget_statusFlagIsSet(phid->status, PHIDGET_SERVER_CONNECTED_FLAG))
		{
			CThread_mutex_unlock(&phid->lock);
			return EPHIDGET_NETWORK_NOTCONNECTED;
		}
		pdc_async_set(phid->networkInfo->server->pdcs, key, buffer, (int)strlen(buffer), PFALSE, internal_async_network_error_handler, phid);
		CThread_mutex_unlock(&phid->lock);
		return EPHIDGET_OK;
	}
	else
	{
#if defined(_WINDOWS) && !defined(WINCE)
	return EPHIDGET_UNSUPPORTED;
#else
	int i;
	char buffer2[22];

	if (strlen(buffer) <= 10) {
		buffer2[0] = (char)strlen(buffer)*2+2;
		buffer2[1] = 3;
		for (i = 0; i < (int)strlen(buffer); i++) {
			buffer2[2+i*2] = buffer[i];
			buffer2[2+i*2+1]=0;
		}
		if (!CUSBSetLabel(phid, buffer2)) {
			CPhidgetHandle foundPhidget;
			strcpy(phid->label, buffer);
			//Also update label in PhidgetManager
			CThread_mutex_lock(&attachedDevicesLock);
			if(CList_findInList((CListHandle)AttachedDevices, phid, CPhidget_areEqual, (void **)&foundPhidget) == EPHIDGET_OK)
			{
				strcpy(foundPhidget->label, buffer);
			}
			CThread_mutex_unlock(&attachedDevicesLock);
			return EPHIDGET_OK;
		}
		return EPHIDGET_UNEXPECTED;
	}
	return EPHIDGET_INVALIDARG;
#endif
	}
}

int CCONV
CPhidget_getServerID(CPhidgetHandle phid, const char **buffer)
{
	TESTPTRS(phid, buffer)

#ifdef USE_ZEROCONF
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_REMOTE_FLAG))
		return EPHIDGET_UNSUPPORTED;

	CThread_mutex_lock(&phid->lock);

	if(!phid->networkInfo->mdns) //not mDNS - not yet supported
	{
		CThread_mutex_unlock(&phid->lock);
		return EPHIDGET_UNSUPPORTED;
	}

	//refresh ONLY if connected - otherwise it might block
	if(CPhidget_statusFlagIsSet(phid->status, PHIDGET_SERVER_CONNECTED_FLAG))
	{
		if(refreshZeroconfPhidget(phid))
		{
			CThread_mutex_unlock(&phid->lock);
			return EPHIDGET_NETWORK;
		}
	}
	if(!phid->networkInfo->zeroconf_server_id)
	{
		CThread_mutex_unlock(&phid->lock);
		return EPHIDGET_UNEXPECTED;
	}
	*buffer = (char *)phid->networkInfo->zeroconf_server_id;

	CThread_mutex_unlock(&phid->lock);
	return EPHIDGET_OK;
#else
	return EPHIDGET_UNSUPPORTED;
#endif
}

int CCONV
CPhidget_getServerAddress(CPhidgetHandle phid, const char **ipAddr, int *port)
{
	TESTPTRS(phid, ipAddr)
	TESTPTR(port)

	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_REMOTE_FLAG))
		return EPHIDGET_UNSUPPORTED;

	CThread_mutex_lock(&phid->lock);
#ifdef USE_ZEROCONF
	if(phid->networkInfo->mdns)
	{
		//Look it up again new EVERY time!
		if(getZeroconfHostPort(phid->networkInfo))
		{
			CThread_mutex_unlock(&phid->lock);
			return EPHIDGET_NETWORK;
		}
		if(!phid->networkInfo->zeroconf_host || !phid->networkInfo->zeroconf_port)
		{
			CThread_mutex_unlock(&phid->lock);
			return EPHIDGET_UNEXPECTED;
		}
		*ipAddr = (char *)phid->networkInfo->zeroconf_host;
		*port = strtol(phid->networkInfo->zeroconf_port, NULL, 10);
	}
	else
#endif
	{
		if(CPhidget_statusFlagIsSet(phid->status, PHIDGET_SERVER_CONNECTED_FLAG))
		{
			if(!phid->networkInfo->server->address || !phid->networkInfo->server->port)
			{
				CThread_mutex_unlock(&phid->lock);
				return EPHIDGET_UNEXPECTED;
			}
			*ipAddr = (char *)phid->networkInfo->server->address;
			*port = strtol(phid->networkInfo->server->port, NULL, 10);
		}
		else
		{
			*ipAddr = (char *)phid->networkInfo->requested_address;
			*port = strtol(phid->networkInfo->requested_port, NULL, 10);
		}
	}
	CThread_mutex_unlock(&phid->lock);
	return EPHIDGET_OK;
}

int CCONV CPhidget_getErrorDescription(int ErrorCode, const char **buf)
{
	TESTPTR(buf)
		if ((ErrorCode < 0) || (ErrorCode >= PHIDGET_ERROR_CODE_COUNT)) {
			*buf = CPhidget_strerror(EPHIDGET_INVALID);
		return EPHIDGET_INVALID;
		}
	*buf = CPhidget_strerror(ErrorCode);
	return EPHIDGET_OK;
}

//expect 6 bytes of data
int CCONV CPhidget_calibrate(CPhidgetHandle phid, unsigned char Offset, unsigned char *data)
{
	int result;
	unsigned char buffer[8];
	TESTPTR(phid)
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG))
		return EPHIDGET_NOTATTACHED;

	ZEROMEM(buffer, sizeof(buffer));
	
	buffer[0] = 0x74;
	buffer[1] = Offset;
	buffer[2] = data[0];
	buffer[3] = data[1];
	buffer[4] = data[2];
	buffer[5] = data[3];
	buffer[6] = data[4];
	buffer[7] = data[5];

	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	return EPHIDGET_OK;
}

int CCONV CPhidget_calibrate_gain2offset(CPhidgetHandle phid, int Index, unsigned short offset, unsigned long gain1, unsigned long gain2)
{
	int result;
	unsigned char buffer[8];
	TESTPTR(phid)
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG))
		return EPHIDGET_NOTATTACHED;

	ZEROMEM(buffer, sizeof(buffer));
	
	buffer[0] = 0x73;
	
	buffer[1] = 0 + Index;
	buffer[2] = (unsigned char)(offset >> 8);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	buffer[1] = 1 + Index;
	buffer[2] = (unsigned char)(offset & 0xff);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	buffer[1] = 2 + Index;
	buffer[2] = (unsigned char)(gain1 >> 16);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	buffer[1] = 3 + Index;
	buffer[2] = (unsigned char)(gain1 >> 8);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	buffer[1] = 4 + Index;
	buffer[2] = (unsigned char)(gain1);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	buffer[1] = 5 + Index;
	buffer[2] = (unsigned char)(gain2 >> 16);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	buffer[1] = 6 + Index;
	buffer[2] = (unsigned char)(gain2 >> 8);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	buffer[1] = 7 + Index;
	buffer[2] = (unsigned char)(gain2);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	return EPHIDGET_OK;
}

int CCONV CPhidget_calibrate_gainoffset(CPhidgetHandle phid, int Index, unsigned short offset, unsigned long gain)
{
	//BL:Not sure what this does
	int result;
	unsigned char buffer[8];
	TESTPTR(phid)
	if (!CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG))
		return EPHIDGET_NOTATTACHED;

	ZEROMEM(buffer, sizeof(buffer));
	
	buffer[0] = 0x72;
	
	//BL: Index can overflow byte
	buffer[1] = 0 + Index;
	buffer[2] = (unsigned char)(offset >> 8);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	buffer[1] = 1 + Index;
	buffer[2] = (unsigned char)(offset & 0xff);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	buffer[1] = 2 + Index;
	buffer[2] = (unsigned char)(gain >> 16);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	buffer[1] = 3 + Index;
	buffer[2] = (unsigned char)(gain >> 8);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	buffer[1] = 4 + Index;
	buffer[2] = (unsigned char)(gain);
	if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
		return result;

	return EPHIDGET_OK;
}

//0 ms = infinite timeout - note that it only checks the attach variable every 10ms
int CCONV CPhidget_waitForAttachment(CPhidgetHandle phid, int milliseconds)
{
	//BL: Should this use constants for infinite timeout rather than just 0?
	long duration = 0;
	TIME	start;

#ifdef _WINDOWS
	TIME now;
	FILETIME nowft, oldft, resft;
	ULARGE_INTEGER nowul, oldul, resul;
#else
	struct timeval now;
#endif

	TESTPTR(phid)
	if(milliseconds)
	{
#ifdef _WINDOWS
		GetSystemTime(&start);
#else
		gettimeofday(&start,NULL);
#endif
	}

	while(!CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG))
	{
		if(!CPhidget_statusFlagIsSet(phid->status, PHIDGET_OPENED_FLAG))
			return EPHIDGET_CLOSED;
		if(milliseconds)
		{
#ifdef _WINDOWS
			GetSystemTime(&now);
			SystemTimeToFileTime(&now, &nowft);
			SystemTimeToFileTime(&start, &oldft);

			nowul.HighPart = nowft.dwHighDateTime;
			nowul.LowPart = nowft.dwLowDateTime;
			oldul.HighPart = oldft.dwHighDateTime;
			oldul.LowPart = oldft.dwLowDateTime;

			resul.HighPart = nowul.HighPart - oldul.HighPart;
			resul.LowPart = nowul.LowPart - oldul.LowPart;

			resft.dwHighDateTime = resul.HighPart;
			resft.dwLowDateTime = resul.LowPart;

			duration = (resft.dwLowDateTime/10000);
#else
			gettimeofday(&now, NULL);
			duration = (now.tv_sec - start.tv_sec)*1000 + ((now.tv_usec-start.tv_usec)/1000);
#endif
			if(duration > milliseconds)
				return EPHIDGET_TIMEOUT;
		}
		SLEEP(10);
	}
	return EPHIDGET_OK;
}

void throw_error_event(CPhidgetHandle phid, const char *error, int errcode)
{
	//BL:didn't add checking here since don't want to mess with error propagation. Should?
	if(phid && phid->fptrError)
	{
		phid->fptrError(phid, phid->fptrErrorptr, errcode, error);
		return;
	}
	LOG(PHIDGET_LOG_WARNING,"Got an async error: %d: %s\n\tTip: Set up an error handler to catch this properly.", errcode, error);
	//abort();
}

//active device is in the list of devices waiting for connections, attachedDevice is the device that was just plugged in
int attachActiveDevice(CPhidgetHandle activeDevice, CPhidgetHandle attachedDevice)
{
	int result = 0;
	TESTPTRS(activeDevice, attachedDevice)

	if(!CPhidget_statusFlagIsSet(activeDevice->status, PHIDGET_OPENED_FLAG))
		return EPHIDGET_UNEXPECTED;

#ifdef _WINDOWS
#ifndef WINCE
	//open uses this so it doesn't have to enumerate all devices - but ONLY ON WINDOWS!
	activeDevice->CPhidgetFHandle = malloc(wcslen(attachedDevice->CPhidgetFHandle)*sizeof(WCHAR)+10);
	wcsncpy((WCHAR *)activeDevice->CPhidgetFHandle, attachedDevice->CPhidgetFHandle, wcslen(attachedDevice->CPhidgetFHandle)+1);
	activeDevice->deviceIDSpec = attachedDevice->deviceIDSpec;
	activeDevice->deviceDef = attachedDevice->deviceDef;
#endif
#endif

	if(activeDevice->specificDevice == 0)
	{
		activeDevice->specificDevice = 2;
		activeDevice->serialNumber = attachedDevice->serialNumber;
	}

	if ((result = CUSBOpenHandle(activeDevice)) !=
		EPHIDGET_OK) {
		LOG(PHIDGET_LOG_WARNING,"unable to open active device: %d", result);
		if(activeDevice->specificDevice == 2)
		{
			activeDevice->specificDevice = 0;
			activeDevice->serialNumber = -1;
		}
		activeDevice->deviceIDSpec = 0;
		return result;
	}
	
	CThread_mutex_lock(&activeDevice->lock);
	CPhidget_setStatusFlag(&activeDevice->status, PHIDGET_ATTACHING_FLAG, NULL);
	if((result = activeDevice->fptrInit((CPhidgetHandle)activeDevice)))
	{
		CPhidget_clearStatusFlag(&activeDevice->status, PHIDGET_ATTACHING_FLAG, NULL);
		CThread_mutex_unlock(&activeDevice->lock);
		if(activeDevice->specificDevice == 2)
		{
			activeDevice->specificDevice = 0;
			activeDevice->serialNumber = -1;
		}
		LOG(PHIDGET_LOG_ERROR, "Device Initialization functions failed: %d", result);
		return result;
	}

	//make sure the write events are in a good state
	activeDevice->writeStopFlag = FALSE;
	CThread_reset_event(&activeDevice->writtenEvent);
	CThread_reset_event(&activeDevice->writeAvailableEvent);

	//set phidget as attached
	CPhidget_clearStatusFlag(&activeDevice->status, PHIDGET_ATTACHING_FLAG, NULL);
	CPhidget_setStatusFlag(&activeDevice->status, PHIDGET_ATTACHED_FLAG, NULL);

	//Start write thread
	if (CThread_create(&activeDevice->writeThread,
		WriteThreadFunction, activeDevice)) {
		LOG(PHIDGET_LOG_WARNING,"unable to create write thread");
		CPhidget_clearStatusFlag(&activeDevice->status, PHIDGET_ATTACHED_FLAG, NULL);
		CThread_mutex_unlock(&activeDevice->lock);
		if(activeDevice->specificDevice == 2)
		{
			activeDevice->specificDevice = 0;
			activeDevice->serialNumber = -1;
		}
		return EPHIDGET_UNEXPECTED;
	}
	activeDevice->writeThread.thread_status = TRUE;
	
	CThread_mutex_unlock(&activeDevice->lock);
	
	//Attach event
	if (activeDevice->fptrAttach)
	{	
		//printf("Throwing read thread\n");
		activeDevice->fptrAttach(activeDevice,
			activeDevice->fptrAttachptr);
	}
	
	activeDevice->fptrEvents((CPhidgetHandle)activeDevice);

	//Start read thread
	//We start this after the attach event returns so that we can guarantee no data events
	CThread_mutex_lock(&activeDevice->lock);
	if (CThread_create(&activeDevice->readThread,
		ReadThreadFunction, activeDevice)) {
		LOG(PHIDGET_LOG_WARNING,"unable to create read thread");
		CPhidget_clearStatusFlag(&activeDevice->status, PHIDGET_ATTACHED_FLAG, NULL);
		CThread_mutex_unlock(&activeDevice->lock);						
		if(activeDevice->specificDevice == 2)
		{
			activeDevice->specificDevice = 0;
			activeDevice->serialNumber = -1;
		}
		return EPHIDGET_UNEXPECTED;
	}
	activeDevice->readThread.thread_status = TRUE;
	CThread_mutex_unlock(&activeDevice->lock);

	return EPHIDGET_OK;
}

double round_double(double x, int decimals)
{
	return (double)((double)round(x * (double)(pow(10, decimals))) / (double)(pow(10, decimals)));
}
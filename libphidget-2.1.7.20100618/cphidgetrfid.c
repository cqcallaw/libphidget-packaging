#include "stdafx.h"
#include "cphidgetrfid.h"
#include "stdio.h"
#include "cusb.h"
#include "csocket.h"
#include "cthread.h"

// === Internal Functions === //
CThread_func_return_t tagTimerThreadFunction(CThread_func_arg_t userPtr);

static int
hexval(unsigned char c)
{
	if (isdigit(c))
		return c - '0';
	c = tolower(c);
	if (c <= 'f')
		return c - 'a' + 10;
	return 0;
}

static int analyze_data(CPhidgetRFIDHandle phid);
static int analyze_data_AC(CPhidgetRFIDHandle phid);
static int sendRAWData(CPhidgetRFIDHandle phid, unsigned char *data, int bitlength);

//Hitag S Commands
static int HitagS_UID_REQUEST(CPhidgetRFIDHandle phid);
static int HitagS_AC_SEQUENCE(CPhidgetRFIDHandle phid, CPhidgetRFID_HitagACHandle ac);

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define pdiff(a, b) ( ABS((a) - (b)) / (double)( ((a) + (b)) / 2.0 ) )

int CPhidgetRFID_Tag_areEqual(void *arg1, void *arg2)
{
	CPhidgetRFID_TagHandle tag1 = (CPhidgetRFID_TagHandle)arg1;
	CPhidgetRFID_TagHandle tag2 = (CPhidgetRFID_TagHandle)arg2;
	
	TESTPTRS(tag1, tag2)
	
	if(!strcmp(tag1->tagString, tag2->tagString)
		&& tag1->tagInfo.tagType == tag2->tagInfo.tagType
		&& tag1->tagInfo.bitRate == tag2->tagInfo.bitRate
		&& tag1->tagInfo.encoding == tag2->tagInfo.encoding)
		return PTRUE;
	else
		return PFALSE;
}

void CPhidgetRFID_Tag_free(void *arg)
{
	CPhidgetRFID_TagHandle tag = (CPhidgetRFID_TagHandle)arg;
	
	if(!tag)
		return;

	free(tag); tag = NULL;
	return;
}

int CPhidgetRFID_HitagAC_areEqual(void *arg1, void *arg2)
{
	CPhidgetRFID_HitagACHandle ac1 = (CPhidgetRFID_HitagACHandle)arg1;
	CPhidgetRFID_HitagACHandle ac2 = (CPhidgetRFID_HitagACHandle)arg2;
	
	TESTPTRS(ac1, ac2)
	
	if(!memcmp(ac1->uid, ac2->uid, 4)
		&& ac1->colPos == ac2->colPos)
		return PTRUE;
	else
		return PFALSE;
}

void CPhidgetRFID_HitagAC_free(void *arg)
{
	CPhidgetRFID_HitagACHandle ac = (CPhidgetRFID_HitagACHandle)arg;
	
	if(!ac)
		return;

	free(ac); ac = NULL;
	return;
}

//clearVars - sets all device variables to unknown state
CPHIDGETCLEARVARS(RFID)
	int i = 0;

	phid->antennaEchoState = PUNI_BOOL;
	phid->ledEchoState = PUNI_BOOL;
	phid->tagPresent = PUNI_BOOL;
	phid->fullStateEcho = PUNK_BOOL;
	phid->ledState = PUNK_BOOL;
	phid->antennaState = PUNK_BOOL;
	phid->lastTagValid = PUNI_BOOL;

	for (i = 0; i<RFID_MAXOUTPUTS; i++)
	{
		phid->outputEchoState[i] = PUNI_BOOL;
	}

	return EPHIDGET_OK;
}

//initAfterOpen - sets up the initial state of an object, reading in packets from the device if needed
//				  used during attach initialization - on every attach
CPHIDGETINIT(RFID)
	int i = 0;
	unsigned char buffer[8] = { 0 };
	int result;
	TESTPTR(phid);

	//setup anything device specific
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_RFID:
			if (phid->phid.deviceVersion < 200)
			{
				phid->fullStateEcho = PFALSE;
				phid->antennaEchoState = PTRUE;
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		case PHIDID_RFID_2OUTPUT: //2-output RFID
			if ((phid->phid.deviceVersion  >= 200) && (phid->phid.deviceVersion  < 201))
			{
				phid->antennaEchoState = PUNK_BOOL;
				phid->fullStateEcho = PFALSE;
			}
			else if ((phid->phid.deviceVersion  >= 201) && (phid->phid.deviceVersion  < 300))
			{
				phid->antennaEchoState = PUNK_BOOL;
				phid->fullStateEcho = PTRUE;
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		case PHIDID_RFID_2OUTPUT_ADVANCED: //2-output RFID (Advanced)
			if ((phid->phid.deviceVersion >= 100) && (phid->phid.deviceVersion  < 200))
			{
				phid->antennaEchoState = PUNK_BOOL;
				phid->fullStateEcho = PTRUE;
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		default:
			return EPHIDGET_UNEXPECTED;
	}

	//set data arrays to unknown
	for (i = 0; i<phid->phid.attr.rfid.numOutputs; i++)
	{
		phid->outputEchoState[i] = PUNK_BOOL;
	}
	phid->ledEchoState = PUNK_BOOL;
	phid->tagPresent = PUNK_BOOL;
	ZEROMEM(phid->lastTag, 10);
	phid->tagEventPending = PFALSE;

	phid->frequencyEcho = PUNK_INT;
	phid->pregapClocksEcho = PUNK_INT;
	phid->postgapClocksEcho = PUNK_INT;
	phid->zeroClocksEcho = PUNK_INT;
	phid->oneClocksEcho = PUNK_INT;
	phid->spaceClocksEcho = PUNK_INT;
	phid->_4097ConfEcho = PUNK_INT;

	phid->dataReadPtr = 0;
	phid->dataWritePtr = 0;
	phid->dataReadACPtr = 0;
	phid->userReadPtr = 0;
	phid->manReadPtr = 0;
	phid->manWritePtr = 0;
	phid->biphaseReadPtr = 0;
	phid->biphaseWritePtr = 0;
	
	phid->ACCodingOK = PFALSE;
	
	phid->tagAdvancedCount = PUNK_INT;
	
	CThread_mutex_lock(&phid->tagthreadlock);
	if(phid->tagAdvancedList)
		CList_emptyList((CListHandle *)&phid->tagAdvancedList, PTRUE, CPhidgetRFID_Tag_free);
	phid->tagAdvancedList = NULL;
	CThread_mutex_unlock(&phid->tagthreadlock);
	phid->lastTagAdvanced.tagString[0] = '\0';

	phid->one = phid->two = phid->oneCount = phid->twoCount = 0;
	
	phid->manLockedIn = 0;
	phid->biphaseLockedIn = 0;
	phid->manShortChange = 0;
	phid->biphaseShortChange = 0;

	//This needs to always have a valid time
#ifdef _WINDOWS
	GetSystemTime(&phid->lastDataTime);
#else
	gettimeofday(&phid->lastDataTime,NULL);
#endif

	//send out any initial pre-read packets
	switch(phid->phid.deviceIDSpec) {
		case PHIDID_RFID:
			if (phid->phid.deviceVersion <= 103)
			{
				ZEROMEM(buffer,8);
				LOG(PHIDGET_LOG_INFO,"Sending workaround startup packet");
				if ((result = CUSBSendPacket((CPhidgetHandle)phid, buffer)) != EPHIDGET_OK)
					return result;
			}
			break;
		case PHIDID_RFID_2OUTPUT:
		default:
			break;
	}

	//issue a read for devices that return output data
	if(phid->fullStateEcho)
	{
		int readtries = 16; //should guarentee a packet with output data - even if a tag is present
		while(readtries-- > 0)
		{
			CPhidget_read((CPhidgetHandle)phid);
			if(phid->outputEchoState[0] != PUNK_BOOL)
					break;
		}
		//one more read guarantees that if there is a tag present, we will see it - output packets only happen every 255ms
		CPhidget_read((CPhidgetHandle)phid);
	}

	//if the antenna is on, and tagPresent is unknown, then it is false
	if(phid->antennaEchoState == PTRUE && phid->tagPresent == PUNK_BOOL)
		phid->tagPresent = PFALSE;

	//if the antenna is on, and tagCount is unknown, then it is false
	if(phid->antennaEchoState == PTRUE && phid->tagAdvancedCount == PUNK_INT)
		phid->tagAdvancedCount = 0;

	//recover what we can - if anything isn't filled out, it's PUNK anyways
	for (i = 0; i<phid->phid.attr.rfid.numOutputs; i++)
	{
		phid->outputState[i] = phid->outputEchoState[i];
	}
	phid->antennaState = phid->antennaEchoState;
	phid->ledState = phid->ledEchoState;
	
	phid->pregapClocks = phid->pregapClocksEcho;
	phid->postgapClocks = phid->postgapClocksEcho;
	phid->zeroClocks = phid->zeroClocksEcho;
	phid->oneClocks = phid->oneClocksEcho;
	phid->spaceClocks = phid->spaceClocksEcho;
	phid->_4097Conf = phid->_4097ConfEcho;

	//make sure the tagTimerThread isn't running
	if (phid->tagTimerThread.thread_status == PTRUE)
	{
		phid->tagTimerThread.thread_status = PFALSE;
		CThread_join(&phid->tagTimerThread);
	}

	return EPHIDGET_OK;
}

//dataInput - parses device packets
CPHIDGETDATA(RFID)
	int i = 0, j = 0;
	unsigned char newTag = PFALSE, newOutputs = PFALSE;
	unsigned char tagData[5];
	unsigned char outputs[RFID_MAXOUTPUTS];
	unsigned char lastOutputs[RFID_MAXOUTPUTS];
	unsigned char antennaState = 0, ledState = 0;
	unsigned char gotData = PFALSE;

	if (length<0) return EPHIDGET_INVALIDARG;
	TESTPTR(phid);
	TESTPTR(buffer);

	ZEROMEM(tagData, sizeof(tagData));
	ZEROMEM(outputs, sizeof(outputs));
	ZEROMEM(lastOutputs, sizeof(lastOutputs));

	//Parse device packets - store data locally
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_RFID:
			if (phid->phid.deviceVersion < 200)
			{
				gotData = PTRUE;
				CThread_mutex_lock(&phid->tagthreadlock);
				setTimeNow(&phid->lastTagTime);
				if(!memcmp(phid->lastTag, buffer+1, 5) && phid->tagPresent == PTRUE)
					newTag = PFALSE;
				else if(!memcmp("\0\0\0\0\0", buffer+1, 5))
					newTag = PFALSE;
				else
				{
					memcpy(tagData, buffer+1, 5);
					newTag = PTRUE;
				}
				CThread_mutex_unlock(&phid->tagthreadlock);
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		case PHIDID_RFID_2OUTPUT:
			if ((phid->phid.deviceVersion  >= 200) && (phid->phid.deviceVersion  < 300))
			{
				switch(buffer[0])
				{
					case RFID_PACKET_TAG:
						gotData = PTRUE;
						CThread_mutex_lock(&phid->tagthreadlock);
						setTimeNow(&phid->lastTagTime);
						if(!memcmp(phid->lastTag, buffer+1, 5) && phid->tagPresent == PTRUE)
							newTag = PFALSE;
						else if(!memcmp("\0\0\0\0\0", buffer+1, 5))
							newTag = PFALSE;
						else
						{
							memcpy(tagData, buffer+1, 5);
							newTag = PTRUE;
						}
						CThread_mutex_unlock(&phid->tagthreadlock);

						break;
					case RFID_PACKET_OUTPUT_ECHO:
						if(phid->fullStateEcho)
						{
							newOutputs = PTRUE;

							for (i = 0, j = 0x01; i < phid->phid.attr.rfid.numOutputs; i++, j<<=1)
							{
								if (buffer[1] & j)
									outputs[i] = PTRUE;
								else
									outputs[i] = PFALSE;
							}

							if(buffer[1] & RFID_LED_FLAG)
								ledState = PTRUE;
							else
								ledState = PFALSE;

							if(buffer[1] & RFID_ANTENNA_FLAG)
								antennaState = PTRUE;
							else
								antennaState = PFALSE;
						}
						break;
					default:
						return EPHIDGET_UNEXPECTED;
				}
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		// RFID with decoding in software and write support
		case PHIDID_RFID_2OUTPUT_ADVANCED:
			if ((phid->phid.deviceVersion  >= 100) && (phid->phid.deviceVersion  < 200))
			{
				int dataLength = 0;
				int data[RFID_MAX_DATA_PER_PACKET];
				switch(buffer[0] & 0x80)
				{
					case RFID_READ_DATA_IN_PACKET:
						gotData = PTRUE;
						phid->frequencyEcho = buffer[1] * 1000;
						
						//move IR data into local storage
						dataLength = buffer[0];
						for(i = 0; i < dataLength; i++)
						{
							data[i] = buffer[i+2];
							phid->dataBuffer[phid->dataWritePtr] = buffer[i+2];

							phid->dataWritePtr++;
							phid->dataWritePtr &= RFID_DATA_ARRAY_MASK;

							//if we run into data that hasn't been read... too bad, we overwrite it and adjust the read pointer
							if(phid->dataWritePtr == phid->dataReadPtr)
							{
								phid->dataReadPtr++;
								phid->dataReadPtr &= RFID_DATA_ARRAY_MASK;
							}
							if(phid->dataWritePtr == phid->dataReadACPtr)
							{
								phid->dataReadACPtr++;
								phid->dataReadACPtr &= RFID_DATA_ARRAY_MASK;
							}
						}

						break;
					case RFID_ECHO_IN_PACKET:
						newOutputs = PTRUE;
						phid->frequencyEcho = buffer[1] * 1000;

						phid->pregapClocksEcho = buffer[2];
						phid->postgapClocksEcho = buffer[3];
						phid->zeroClocksEcho = buffer[4];
						phid->oneClocksEcho = buffer[5];
						phid->spaceClocksEcho = buffer[6];

						for (i = 0, j = 0x01; i < phid->phid.attr.rfid.numOutputs; i++, j<<=1)
						{
							if (buffer[7] & j)
								outputs[i] = PTRUE;
							else
								outputs[i] = PFALSE;
						}

						if(buffer[7] & RFID_LED_FLAG)
							ledState = PTRUE;
						else
							ledState = PFALSE;

						if(buffer[7] & RFID_ANTENNA_FLAG)
							antennaState = PTRUE;
						else
							antennaState = PFALSE;

						phid->_4097ConfEcho = buffer[8];

						//space in data
						if(buffer[9])
						{
							//Add a space in the buffer
							phid->dataBuffer[phid->dataWritePtr] = PUNK_INT;
							phid->dataWritePtr++;
							phid->dataWritePtr &= RFID_DATA_ARRAY_MASK;

							//if we run into data that hasn't been read... too bad, we overwrite it and adjust the read pointer
							if(phid->dataWritePtr == phid->dataReadPtr)
							{
								phid->dataReadPtr++;
								phid->dataReadPtr &= RFID_DATA_ARRAY_MASK;
							}
							if(phid->dataWritePtr == phid->dataReadACPtr)
							{
								phid->dataReadACPtr++;
								phid->dataReadACPtr &= RFID_DATA_ARRAY_MASK;
							}
							dataLength = 1;
							data[0] = PUNK_INT;
						}
						else
							gotData = PTRUE;

						break;
					default:
						return EPHIDGET_UNEXPECTED;
				}

				if(dataLength)
				{
					//send out raw data event
					FIRE(RawData, data, dataLength);

					//analyze data
					analyze_data(phid);
					if(phid->ACCodingOK)
						analyze_data_AC(phid);
					else
						phid->dataReadACPtr = phid->dataWritePtr;
				}
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		default:
			return EPHIDGET_UNEXPECTED;
	}

	if(gotData)
		setTimeNow(&phid->lastDataTime);

	//Make sure values are within defined range, and store to structure
	if(newOutputs)
	{
		for (i = 0; i < phid->phid.attr.rfid.numOutputs; i++)
		{
			lastOutputs[i] = phid->outputEchoState[i];
			phid->outputEchoState[i] = outputs[i];
		}
		phid->ledEchoState = ledState;
		phid->antennaEchoState = antennaState;
	}
	CThread_mutex_lock(&phid->tagthreadlock);
	if(newTag && !phid->tagEventPending)
	{
		memcpy(phid->pendingTag, tagData, 5);
		phid->tagEventPending = PTRUE;
		CThread_set_event(&phid->tagAvailableEvent);
	}
	CThread_mutex_unlock(&phid->tagthreadlock);

	//Events
	if(newOutputs)
	{
		for (i = 0; i < phid->phid.attr.rfid.numOutputs; i++)
		{
			if(lastOutputs[i] != phid->outputEchoState[i])
				FIRE(OutputChange, i, phid->outputEchoState[i]);
		}
	}
	
	return EPHIDGET_OK;
}

//eventsAfterOpen - sends out an event for all valid data, used during attach initialization
CPHIDGETINITEVENTS(RFID)
	int i = 0;

	if(phid->fullStateEcho)
	{
		for (i = 0; i < phid->phid.attr.rfid.numOutputs; i++)
		{
			if(phid->outputEchoState[i] != PUNK_BOOL)
				FIRE(OutputChange, i, phid->outputEchoState[i]);
		}
	}

	//Initial tag events are sent from the tagTimerThread

	//Don't start the tag thread if this is a networked Phidget
	if(!CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
	{
		//Start the tagTimerThread - do it here because we are about to start the read thread, and that will keep it active
		if (CThread_create(&phid->tagTimerThread, tagTimerThreadFunction, phid))
			return EPHIDGET_UNEXPECTED;
	}
	//For remote - if there is a tag present, send the tag event here
	else
	{
		if(phid->tagPresent == PTRUE)
			FIRE(Tag, phid->lastTag);
	}

	return EPHIDGET_OK;
}

//Extra things to do during a close
//This is run before the other things that close does
int CPhidgetRFID_close(CPhidgetHandle phidG)
{
	CPhidgetRFIDHandle phid = (CPhidgetRFIDHandle)phidG;
	//make sure the tagTimerThread isn't running
	if (phid->tagTimerThread.thread_status == PTRUE)
	{
		phid->tagTimerThread.thread_status = PFALSE;
		CThread_join(&phid->tagTimerThread);
	}
	return EPHIDGET_OK;
}

//Extra things to do during a free
//This is run before the other things that free does
int CPhidgetRFID_free(CPhidgetHandle phidG)
{
	CPhidgetRFIDHandle phid = (CPhidgetRFIDHandle)phidG;
	CThread_mutex_destroy(&phid->tagthreadlock);
	CThread_destroy_event(&phid->tagAvailableEvent);
	CThread_destroy_event(&phid->respEvent);
	CThread_destroy_event(&phid->respEvent2);
	return EPHIDGET_OK;
}

//getPacket - used by write thread to get the next packet to send to device
CGETPACKET_BUF(RFID)

//sendpacket - sends a packet to the device asynchronously, blocking if the 1-packet queue is full
CSENDPACKET_BUF(RFID)

//makePacket - constructs a packet using current device state
CMAKEPACKET(RFID)
	int i = 0, j = 0;

	TESTPTRS(phid, buffer);

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_RFID_2OUTPUT: //4-output RFID
			if ((phid->phid.deviceVersion  >= 200) && (phid->phid.deviceVersion  < 300))
			{
				//have to make sure that everything to be sent has some sort of default value if the user hasn't set a value
				for (i = 0; i < phid->phid.attr.rfid.numOutputs; i++)
				{
					if (phid->outputState[i] == PUNK_BOOL)
						phid->outputState[i] = PFALSE;
				}
				if(phid->antennaState == PUNK_BOOL)
					phid->antennaState = PFALSE;
				if(phid->ledState == PUNK_BOOL)
					phid->ledState = PFALSE;

				//construct the packet
				for (i = 0, j = 1; i < phid->phid.attr.rfid.numOutputs; i++, j<<=1)
				{
					if (phid->outputState[i])
						buffer[0] |= j;
				}
				if(phid->ledState == PTRUE)
					buffer[0] |= RFID_LED_FLAG;
				if(phid->antennaState == PTRUE)
					buffer[0] |= RFID_ANTENNA_FLAG;
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		case PHIDID_RFID_2OUTPUT_ADVANCED: //4-output RFID Advanced
			if ((phid->phid.deviceVersion  >= 100) && (phid->phid.deviceVersion  < 200))
			{
				//have to make sure that everything to be sent has some sort of default value if the user hasn't set a value
				for (i = 0; i < phid->phid.attr.rfid.numOutputs; i++)
				{
					if (phid->outputState[i] == PUNK_BOOL)
						phid->outputState[i] = PFALSE;
				}
				if(phid->antennaState == PUNK_BOOL)
					phid->antennaState = PFALSE;
				if(phid->ledState == PUNK_BOOL)
					phid->ledState = PFALSE;

				//construct the packet
				for (i = 0, j = 1; i < phid->phid.attr.rfid.numOutputs; i++, j<<=1)
				{
					if (phid->outputState[i])
						buffer[7] |= j;
				}
				if(phid->ledState == PTRUE)
					buffer[7] |= RFID_LED_FLAG;
				if(phid->antennaState == PTRUE)
					buffer[7] |= RFID_ANTENNA_FLAG;
				
				//TODO: make sure these are actually all valid
				buffer[0] = RFID_CONTROL_OUT_PACKET;
				buffer[1] = phid->pregapClocks;
				buffer[2] = phid->postgapClocks;
				buffer[3] = phid->zeroClocks;
				buffer[4] = phid->oneClocks;
				buffer[5] = phid->spaceClocks;
				buffer[6] = phid->_4097Conf;
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		case PHIDID_RFID:
			return EPHIDGET_UNSUPPORTED; //this version does not have outputs
		default:
			return EPHIDGET_UNEXPECTED;
	}
	return EPHIDGET_OK;
}

//if the time since last tag read > 200ms, fire tagLost event
//NOTE: blocking in data events for too long will cause tagLost events
CThread_func_return_t tagTimerThreadFunction(CThread_func_arg_t userPtr)
{
	CPhidgetRFIDHandle phid = (CPhidgetRFIDHandle)userPtr;
	CPhidgetRFID_TagList *trav = 0, *tagLostList = 0, *tagFoundList = 0;

	if(!phid) return (CThread_func_return_t)EPHIDGET_INVALIDARG;

	LOG(PHIDGET_LOG_INFO,"tagTimerThread running");

	phid->tagTimerThread.thread_status = PTRUE;

	while (CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_ATTACHED_FLAG) && phid->tagTimerThread.thread_status == PTRUE)
	{
		//sleeps for up to 50ms, but can be signalled externally to return immediately
		CThread_wait_on_event(&phid->tagAvailableEvent, 50);
		CThread_reset_event(&phid->tagAvailableEvent);

		//Tag events
		//Old-style
		if(phid->tagEventPending)
		{
			phid->tagPresent = PTRUE;
			FIRE(Tag, phid->pendingTag);
			CThread_mutex_lock(&phid->tagthreadlock);
			setTimeNow(&phid->lastTagTime);

			//so they can access the last tag from this tag event
			memcpy(phid->lastTag, phid->pendingTag, 5);
			phid->tagEventPending = PFALSE;
			CThread_mutex_unlock(&phid->tagthreadlock);
		}
		//New-style
		CThread_mutex_lock(&phid->tagthreadlock);
		for (trav=phid->tagAdvancedList; trav; trav = trav->next)
		{
			if(trav->tag->tagEventPending == PTRUE)
				CList_addToList((CListHandle *)&tagFoundList, trav->tag, CPhidgetRFID_Tag_areEqual);
		}
		for (trav=tagFoundList; trav; trav = trav->next)
		{
			if(phid->tagAdvancedCount == PUNK_INT)
				phid->tagAdvancedCount = 0;
			phid->tagAdvancedCount++;

			CThread_mutex_unlock(&phid->tagthreadlock);
			FIRE(TagAdvanced, trav->tag->tagString, &trav->tag->tagInfo);
			CThread_mutex_lock(&phid->tagthreadlock);
			setTimeNow(&trav->tag->lastTagTime);

			//so they can access the last tag from this tag event
			memcpy(&phid->lastTagAdvanced, trav->tag, sizeof(CPhidgetRFID_Tag));
			trav->tag->tagEventPending = PFALSE;
		}
		CList_emptyList((CListHandle *)&tagFoundList, PFALSE, NULL);
		CThread_mutex_unlock(&phid->tagthreadlock);

		//TAG Lost events
		//Old-style
		if(phid->tagPresent != PFALSE)
		{
			/* check for tag lost */

			CThread_mutex_lock(&phid->tagthreadlock);
			if(timeSince(&phid->lastTagTime) > 0.2)
			{
				if (phid->tagPresent == PTRUE) {
					phid->tagPresent = PFALSE;
					CThread_mutex_unlock(&phid->tagthreadlock);
					FIRE(TagLost, phid->lastTag);
					CThread_mutex_lock(&phid->tagthreadlock);
				}
				else if(phid->antennaEchoState == PTRUE) //could be PUNK_BOOL - don't send event, just set to PFALSE (but only if the antenna is on)
					phid->tagPresent = PFALSE;
			}
			CThread_mutex_unlock(&phid->tagthreadlock);
		}
		//New-style
		if(phid->tagAdvancedCount != 0)
		{
			if(phid->tagAdvancedCount != PUNK_INT)
			{
				CThread_mutex_lock(&phid->tagthreadlock);
				for (trav=phid->tagAdvancedList; trav; trav = trav->next)
				{
					if((timeSince(&trav->tag->lastTagTime) > 0.2 && trav->tag->tagInfo.tagType != PHIDGET_RFID_TAG_HITAGS)
						|| (timeSince(&trav->tag->lastTagTime) > 0.5 && trav->tag->tagInfo.tagType == PHIDGET_RFID_TAG_HITAGS))
						CList_addToList((CListHandle *)&tagLostList, trav->tag, CPhidgetRFID_Tag_areEqual);
				}
				for (trav=tagLostList; trav; trav = trav->next)
				{
					CList_removeFromList((CListHandle *)&phid->tagAdvancedList, trav->tag, CPhidgetRFID_Tag_areEqual, PFALSE, NULL);
					phid->tagAdvancedCount--;
					CThread_mutex_unlock(&phid->tagthreadlock);
					FIRE(TagLostAdvanced, trav->tag->tagString, &trav->tag->tagInfo);
					CThread_mutex_lock(&phid->tagthreadlock);
				}
				CList_emptyList((CListHandle *)&tagLostList, PTRUE, CPhidgetRFID_Tag_free);
				CThread_mutex_unlock(&phid->tagthreadlock);
			}
			else if(phid->antennaEchoState == PTRUE)
				phid->tagAdvancedCount = 0;
		}

		//Actively look for tags if we haven't gotten data for a while (Hitag)
		if(phid->antennaEchoState == PTRUE)
		{
			switch(phid->phid.deviceIDSpec)
			{
				case PHIDID_RFID_2OUTPUT_ADVANCED: //2-output RFID Advanced
					if ((phid->phid.deviceVersion  >= 100) && (phid->phid.deviceVersion  < 200))
					{
						CThread_mutex_lock(&phid->tagthreadlock);
						if(timeSince(&phid->hitagReqTime) > 0.1) //100ms
						{
							if(timeSince(&phid->lastDataTime) > 0.1)  //100ms
							{
								HitagS_UID_REQUEST(phid);
							}
						}
						CThread_mutex_unlock(&phid->tagthreadlock);
					}
					else
						return (CThread_func_return_t)EPHIDGET_UNEXPECTED;
					break;
				//Cannot send data
				case PHIDID_RFID_2OUTPUT:
				case PHIDID_RFID:
					break;
				default:
					return (CThread_func_return_t)EPHIDGET_UNEXPECTED;
			}
		}
	}

	LOG(PHIDGET_LOG_INFO,"tagTimerThread exiting normally");
	phid->tagTimerThread.thread_status = FALSE;
	return (CThread_func_return_t)EPHIDGET_OK;
}

static int sendRAWData(CPhidgetRFIDHandle phid, unsigned char *data, int bitlength)
{
	unsigned char buffer[10];
	int i, j, result, bits;

	int length = bitlength/8 + (bitlength%8 ? 1 : 0);

	if(length > 0xff)
		return EPHIDGET_INVALIDARG;

	bits = 0;
    for (i = 0, j = 1; i < length; i++, j++)
    {
        buffer[j] = data[i];
		if(bitlength < 8)
		{
			bits += bitlength;
			bitlength = 0;
		}
		else
		{
			bits += 8;
			bitlength -= 8;
		}
        if (j == 7 || i == (length-1))
        {
			buffer[0] = RFID_WRITE_DATA_OUT_PACKET | bits;
			if ((result = CPhidgetRFID_sendpacket(phid, buffer)) != EPHIDGET_OK)
			{
				return result;
			}
			j = 0;
			bits = 0;
        }
    }

	return EPHIDGET_OK;
}

static void resetHitagACBuffer(CPhidgetRFIDHandle phid)
{
	phid->dataReadACPtr = phid->dataWritePtr;
	phid->ACCodingOK = PTRUE;
}

//Hitag CRC
#define CRC_PRESET 0xFF
#define CRC_POLYNOM 0x1D
static void calc_crc_hitag(unsigned char * crc,
			  unsigned char data,
			  unsigned char Bitcount)
{ 
	*crc ^= data; //crc = crc (exor) data
	 do
	 {
		 if( *crc & 0x80 ) // if (MSB-CRC == 1)
		 {
			 *crc<<=1; // CRC = CRC Bit-shift left
			 *crc ^= CRC_POLYNOM; // CRC = CRC (exor) CRC_POLYNOM
		 }
		 else
			 *crc<<=1; // CRC = CRC Bit-shift left
	 } while(--Bitcount);
}
static unsigned char hitagCRC8(unsigned char *data, int dataBits)
{
	unsigned char crc;
	int i;
	int dataLength = dataBits / 8;
	if(dataBits%8 != 0)
		dataLength++;
	crc = CRC_PRESET; /* initialize crc algorithm */

	for(i=0; i<dataLength; i++)
	{
		calc_crc_hitag(&crc, data[i], ((dataBits > 8) ? 8 : dataBits));
		dataBits -= 8;
	}

	return crc;
}

static int HitagS_WRITE(CPhidgetRFIDHandle phid, int page, unsigned char *data, unsigned char blockWrite)
{
	int res;
	unsigned char buf[] = {0,0,0};
	unsigned char crc;

	buf[0] = (blockWrite ? 0x90 : 0x80) | page >> 4;
	buf[1] = page << 4;
	crc = hitagCRC8(buf, 12);
	buf[1] |= crc >> 4;
	buf[2] = crc << 4;

	//make sure it's been at least 50ms since last hitag request
	CThread_mutex_lock(&phid->tagthreadlock);
	while(timeSince(&phid->hitagReqTime) < 0.01) //50ms
		SLEEP(10);
						
	phid->hitagState = RFID_HITAG_STATE_WRITE;
	phid->hitagOffset = page;
	
	phid->manShortChange=0;
	phid->manLockedIn = 1;
	phid->manReadPtr = phid->manWritePtr;

	//Send a Hitag S AC Sequence Command
	res = CPhidgetRFID_WriteRaw(phid, buf, 20, 9, 9, 9, 13, 19);
	
	//Don't send it again for at least 100ms
	setTimeNow(&phid->hitagReqTime);
	CThread_mutex_unlock(&phid->tagthreadlock);

	//Wait for ACK

	//Send page data

	return res;
}

static int HitagS_READ(CPhidgetRFIDHandle phid, int page, unsigned char blockRead)
{
	int res;
	unsigned char buf[] = {0,0,0};
	unsigned char crc;

	buf[0] = (blockRead ? 0xD0 : 0xC0) | page >> 4;
	buf[1] = page << 4;
	crc = hitagCRC8(buf, 12);
	buf[1] |= crc >> 4;
	buf[2] = crc << 4;

	//make sure it's been at least 50ms since last hitag request
	CThread_mutex_lock(&phid->tagthreadlock);
	while(timeSince(&phid->hitagReqTime) < 0.01) //50ms
		SLEEP(10);
						
	phid->hitagState = RFID_HITAG_STATE_READ;
	phid->hitagOffset = page;
	
	phid->manShortChange=0;
	phid->manLockedIn = 1;
	phid->manReadPtr = phid->manWritePtr;

	//Send a Hitag S AC Sequence Command
	res = CPhidgetRFID_WriteRaw(phid, buf, 20, 9, 9, 9, 13, 19);
	
	//Don't send it again for at least 100ms
	setTimeNow(&phid->hitagReqTime);
	CThread_mutex_unlock(&phid->tagthreadlock);

	return res;
}

static int HitagS_SELECT(CPhidgetRFIDHandle phid, unsigned char *UID)
{
	int res;
	unsigned char buf[] = {0,0,0,0,0,0};
	unsigned char crc;
	int k,i;

	if(!UID)
		return EPHIDGET_INVALIDARG;
	if(strlen((char *)UID)!=8)
		return EPHIDGET_INVALIDARG;

	for(i=0,k=5;i<32;i++,k++)
	{
		buf[k/8] |= ((hexval(UID[i/4]) >> (3-(i%4))) & 0x01) << (7-(k%8));
	}
	crc = hitagCRC8(buf, 37);
	for(i=0;i<8;i++,k++)
	{
		buf[k/8] |= (crc >> (7-(i%8))) << (7-(k%8));
	}

	//make sure it's been at least 50ms since last hitag request
	CThread_mutex_lock(&phid->tagthreadlock);
	while(timeSince(&phid->hitagReqTime) < 0.05) //50ms
		SLEEP(10);
						
	phid->hitagState = RFID_HITAG_STATE_SELECT;
	
	phid->manShortChange=0;
	phid->manLockedIn = 1;
	phid->manReadPtr = phid->manWritePtr;

	//Send a Hitag S AC Sequence Command
	res = CPhidgetRFID_WriteRaw(phid, buf, k, 9, 9, 9, 13, 19);
	
	//Don't send it again for at least 100ms
	setTimeNow(&phid->hitagReqTime);
	CThread_mutex_unlock(&phid->tagthreadlock);

	return res;
}
static int HitagS_UID_REQUEST(CPhidgetRFIDHandle phid)
{
	int res;
	//Send a Hitag S UID Request Command
	unsigned char buf[] = { 0xC0 };

	//make sure it's been at least 50ms since last hitag request
	CThread_mutex_lock(&phid->tagthreadlock);
	while(timeSince(&phid->hitagReqTime) < 0.05) //50ms
		SLEEP(10);

	phid->hitagState = RFID_HITAG_STATE_UID_REQUEST;

	//Empty AC List
	if(phid->hitagACList)
		CList_emptyList((CListHandle *)&phid->hitagACList, PTRUE, CPhidgetRFID_HitagAC_free);
	phid->hitagACList = NULL;

	resetHitagACBuffer(phid);
	res = CPhidgetRFID_WriteRaw(phid, buf, 5, 9, 9, 9, 13, 19);

	//Don't send it again for at least 100ms
	setTimeNow(&phid->hitagReqTime);
	CThread_mutex_unlock(&phid->tagthreadlock);

	return res;
}
static int HitagS_AC_SEQUENCE(CPhidgetRFIDHandle phid, CPhidgetRFID_HitagACHandle ac)
{
	int res;
	unsigned char buf[] = {0,0,0,0,0,0};
	unsigned char crc;
	int k,i;

	buf[0] = (ac->colPos) << 3;
	for(i=0,k=5;i<ac->colPos;i++,k++)
	{
		buf[k/8] |= (ac->uid[i/8] >> (7-(i%8))) << (7-(k%8));
	}
	crc = hitagCRC8(buf, ac->colPos+5);
	for(i=0;i<8;i++,k++)
	{
		buf[k/8] |= (crc >> (7-(i%8))) << (7-(k%8));
	}

	//make sure it's been at least 50ms since last hitag request
	CThread_mutex_lock(&phid->tagthreadlock);
	while(timeSince(&phid->hitagReqTime) < 0.05) //50ms
		SLEEP(10);

	memcpy(&phid->lastHitagAC, ac, sizeof(CPhidgetRFID_HitagAC));

	//Send a Hitag S AC Sequence Command
	phid->hitagState = RFID_HITAG_STATE_AC_SEQUENCE;

	resetHitagACBuffer(phid);
	res = CPhidgetRFID_WriteRaw(phid, buf, k, 9, 9, 9, 13, 19);
	
	//Don't send it again for at least 100ms
	setTimeNow(&phid->hitagReqTime);
	CThread_mutex_unlock(&phid->tagthreadlock);

	return res;
}

static int decodeEM4102(CPhidgetRFIDHandle phid, unsigned char *data, int *startPtr, int *endPtr, CPhidgetRFID_TagHandle tag)
{
	int i, foundStart, k, j;
	int myReadPtr = *startPtr;
	int em4103data[64];
	unsigned char decodedData[5];
	int bytesInQueue;
	//Look for the starting pattern of 9 Ones
start:
	bytesInQueue = *endPtr - myReadPtr;
	if(myReadPtr > *endPtr)
		bytesInQueue += RFID_DATA_ARRAY_SIZE;

	while(myReadPtr != *endPtr)
	{
		if(bytesInQueue < 64)
			return EPHIDGET_NOTFOUND;
		foundStart = 1;

		for(i=0;i<9;i++)
		{
			if(data[(myReadPtr + i) & RFID_DATA_ARRAY_MASK] != 1)
			{
				foundStart = 0;
				break;
			}
		}
		if(foundStart)
			break;

		myReadPtr++;
		myReadPtr &= RFID_DATA_ARRAY_MASK;

		bytesInQueue--;
	}

	//Got here? - We found the start pattern
	//Now decode the EM4102 data
	for(i=0;i<64;i++)
	{
		em4103data[i] = data[(myReadPtr + i) & RFID_DATA_ARRAY_MASK];
	}

	//last bit should be zero (stop bit)
	if(em4103data[63] != 0)
		goto tryagain;

	//row Parity
	for(i=0;i<10;i++)
	{
		int rowParity = 0;
		for(k=0;k<4;k++)
			rowParity ^= em4103data[9+i*5+k];
		if(rowParity != em4103data[9+i*5+k])
			goto tryagain;
	}
	//column parity
	for(i=0;i<4;i++)
	{
		int colParity = 0;
		for(k=0;k<10;k++)
			colParity ^= em4103data[9+i+k*5];
		if(colParity != em4103data[9+i+k*5])
			goto tryagain;
	}

	//We're good! Strip out data
	memset(decodedData, 0, 5);
	for(i=0,j=9;i<5;i++)
	{
		for(k=7;k>=4;k--,j++)
			decodedData[i] |= em4103data[j] << k;
		j++; //skip row parity bit
		for(k=3;k>=0;k--,j++)
			decodedData[i] |= em4103data[j] << k;
		j++; //skip row parity bit
	}
	
	//Old style Tag event for EM4102
	CThread_mutex_lock(&phid->tagthreadlock);
	setTimeNow(&phid->lastTagTime);
	if((memcmp(phid->lastTag, decodedData, 5) || phid->tagPresent == PFALSE)
		&& memcmp("\0\0\0\0\0", decodedData, 5)
		&& !phid->tagEventPending)
	{
		memcpy(phid->pendingTag, decodedData, 5);
		phid->tagEventPending = PTRUE;
		CThread_set_event(&phid->tagAvailableEvent);
	}
	CThread_mutex_unlock(&phid->tagthreadlock);

	//Update the tag struct for the advanced tag event
	snprintf(tag->tagString, 255, "%02x%02x%02x%02x%02x",decodedData[0],decodedData[1],decodedData[2],decodedData[3],decodedData[4]);
	tag->tagInfo.tagType = PHIDGET_RFID_TAG_EM4102;

	//update master read pointer
	(*startPtr)+=64;
	(*startPtr) &= RFID_DATA_ARRAY_MASK;
	return EPHIDGET_OK;

tryagain:
	myReadPtr++;
	myReadPtr &= RFID_DATA_ARRAY_MASK;
	goto start;
}

//ISO11785 CRC
void
CRC_16_CCITT_update(unsigned short *crc, unsigned char x)
{
	unsigned short crc_new = (unsigned char)((*crc) >> 8) | ((*crc) << 8);
	crc_new ^= x;
	crc_new ^= (unsigned char)(crc_new & 0xff) >> 4;
	crc_new ^= crc_new << 12;
	crc_new ^= (crc_new & 0xff) << 5;
	(*crc) = crc_new;
}

static int decodeISO11785(CPhidgetRFIDHandle phid, unsigned char *data, int *startPtr, int *endPtr, CPhidgetRFID_TagHandle tag)
{
	int i, foundStart, k;
	int myReadPtr = *startPtr;
	unsigned char iso11785data[8];
	unsigned char iso11785dataReversed[8];
	unsigned short iso11785checksum;
	int bytesInQueue;
	unsigned short crc = 0x0000;
	//Look for the starting pattern of 10 zeroes and 1 one
start:
	bytesInQueue = *endPtr - myReadPtr;
	if(myReadPtr > *endPtr)
		bytesInQueue += RFID_DATA_ARRAY_SIZE;

	while(myReadPtr != *endPtr)
	{
		//full sequence is 128 bits
		if(bytesInQueue < 128)
			return EPHIDGET_NOTFOUND;
		foundStart = 1;

		for(i=0;i<10;i++)
		{
			if(data[(myReadPtr + i) & RFID_DATA_ARRAY_MASK] != 0)
			{
				foundStart = 0;
				break;
			}
		}
		if(data[(myReadPtr + 10) & RFID_DATA_ARRAY_MASK] != 1)
		{
			foundStart = 0;
		}
		if(foundStart)
			break;

		myReadPtr++;
		myReadPtr &= RFID_DATA_ARRAY_MASK;

		bytesInQueue--;
	}

	//advance past header
	myReadPtr += 11;
	myReadPtr &= RFID_DATA_ARRAY_MASK;

	//Got here? - We found the start pattern
	//Now decode the ISO11785 data
	//every block of 8 is followed by a '1'
	memset(iso11785data, 0, 8);
	memset(iso11785dataReversed, 0, 8);
	for(i=0,k=0;i<64;i++,k++)
	{
		if(i>0 && i%8 == 0)
		{
			if(data[(myReadPtr + k) & RFID_DATA_ARRAY_MASK] != 1) goto tryagain;
			k++;
		}
		iso11785data[i/8] |= data[(myReadPtr + k) & RFID_DATA_ARRAY_MASK] << (7-(i%8));
		iso11785dataReversed[7-(i/8)] |= data[(myReadPtr + k) & RFID_DATA_ARRAY_MASK] << (i%8);
	}
	if(data[(myReadPtr + k) & RFID_DATA_ARRAY_MASK] != 1) goto tryagain;
	k++;

	//Now the checksum
	iso11785checksum = 0;
	for(i=0;i<16;i++,k++)
	{
		if(i>0 && i%8 == 0)
		{
			if(data[(myReadPtr + k) & RFID_DATA_ARRAY_MASK] != 1) goto tryagain;
			k++;
		}
		iso11785checksum |= data[(myReadPtr + k) & RFID_DATA_ARRAY_MASK] << (15-i);
	}	
	if(data[(myReadPtr + k) & RFID_DATA_ARRAY_MASK] != 1) goto tryagain;
	k++;

	//TODO: there is also a 24 bit trailer which can contain extra info

	//Checksum
	crc = 0x0000;
	for(i=0;i<8;i++)
	{
		CRC_16_CCITT_update(&crc, iso11785data[i]);
	}

	if(crc != iso11785checksum)
	{
		//FAIL
		goto tryagain;
	}

	//We're good!
	//Parse out the different sections
	{
		//These are not used for now
		//unsigned char animal = iso11785dataReversed[0] & 0x80 ? PTRUE : PFALSE; //1 bit - bit 0
		//unsigned char extraData = iso11785dataReversed[1] & 0x01 ? PTRUE : PFALSE; //1 bit - bit 15
		unsigned short countryCode = (iso11785dataReversed[2] << 2 | iso11785dataReversed[3] >> 6) & 0x3ff; //10 bit - bits 16-26
		unsigned long long UID = (
			(((__uint64)iso11785dataReversed[3]) << 32) +
			(((__uint64)iso11785dataReversed[4]) << 24) +
			(((__uint64)iso11785dataReversed[5]) << 16) +
			(((__uint64)iso11785dataReversed[6]) << 8) +
			((__uint64)iso11785dataReversed[7])) & 0x3FFFFFFFFFll;// 38 bit - bits 27-63

		snprintf(tag->tagString, 255, "%03d%012lld",countryCode, UID);
		tag->tagInfo.tagType = PHIDGET_RFID_TAG_ISO11784;
	}

	//update master read pointer
	(*startPtr) += 128;
	(*startPtr) &= RFID_DATA_ARRAY_MASK;
	return EPHIDGET_OK;

tryagain:
	myReadPtr++;
	myReadPtr &= RFID_DATA_ARRAY_MASK;
	goto start;
}

static int decodeHitagUID(CPhidgetRFIDHandle phid, unsigned char *data, int bytesInQueue, CPhidgetRFID_TagHandle tag, int *collisionPos)
{
	int i, k;
	int myReadPtr = 0;
	unsigned int HitagUID;
	int sofBits = 3;
	int expectedBytes = 0;

	if(phid->hitagState == RFID_HITAG_STATE_UID_REQUEST)
		expectedBytes = 35;
	else if(phid->hitagState == RFID_HITAG_STATE_AC_SEQUENCE)
		expectedBytes = 35 - phid->lastHitagAC.colPos;

	*collisionPos = -1;

	//UID is 32 bits, plus SOF == '111'
	if(bytesInQueue != expectedBytes)
		return EPHIDGET_NOTFOUND;

	//verify SOF
	for(i=0;i<sofBits;i++)
	{
		if(data[(myReadPtr + i) & RFID_DATA_ARRAY_MASK] != 1)
			return EPHIDGET_NOTFOUND;
	}

	//advance past SOF
	myReadPtr += sofBits;
	myReadPtr &= RFID_DATA_ARRAY_MASK;

	HitagUID = 0;

	//if AC Sequence, read in bits from last AC
	for(k=0;k<(32-(expectedBytes-sofBits));k++)
	{
		HitagUID |= (phid->lastHitagAC.uid[k/8] >> (7-(k%8))) << (31-k);
	}

	for(i=0;i<(expectedBytes-sofBits);k++,i++)
	{
		//check for a collision
		if(data[(myReadPtr + i) & RFID_DATA_ARRAY_MASK] == PUNK_BOOL)
		{
			*collisionPos = k;
			return EPHIDGET_NOTFOUND;
		}
		HitagUID |= data[(myReadPtr + i) & RFID_DATA_ARRAY_MASK] << (31-k);
	}

	//We're good!
	snprintf(tag->tagString, 255, "%08x",HitagUID);
	tag->tagInfo.tagType = PHIDGET_RFID_TAG_HITAGS;

	return EPHIDGET_OK;
}

static int decodeHitagACKResponse(CPhidgetRFIDHandle phid, unsigned char *data, int *startPtr, int *endPtr)
{
	int myReadPtr = *startPtr;
	int bytesInQueue;
	int i;
	int sofBits = 5; //SOF is really 6-bit but we only see it as 5-bit
	int expectedBytes = 7;

	bytesInQueue = *endPtr - myReadPtr;
	if(myReadPtr > *endPtr)
		bytesInQueue += RFID_DATA_ARRAY_SIZE;

	//printf("maybe(read).. %d\n", bytesInQueue);
	//print_buffer(data, *startPtr, *endPtr);

	if(bytesInQueue != expectedBytes)
		return EPHIDGET_NOTFOUND;

	//verify SOF
	for(i=0;i<sofBits;i++)
	{
		if(data[(myReadPtr + i) & RFID_DATA_ARRAY_MASK] != 1)
		{
			phid->respStatus = EPHIDGET_NOTFOUND;
			goto done;
		}
	}

	//Data should be '01'
	if(data[(myReadPtr + 5) & RFID_DATA_ARRAY_MASK] != 0
		|| data[(myReadPtr + 6) & RFID_DATA_ARRAY_MASK] != 1)
	{
		phid->respStatus = EPHIDGET_NOTFOUND;
		goto done;
	}

	phid->respStatus = EPHIDGET_OK;
done:
	CThread_set_event(&phid->respEvent);
	return phid->respStatus;
}

static int decodeHitagReadResponse(CPhidgetRFIDHandle phid, unsigned char *data, int *startPtr, int *endPtr)
{
	int myReadPtr = *startPtr;
	int bytesInQueue;
	unsigned char buf[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int i,k;
	int sofBits = 5; //SOF is really 6-bit but we only see it as 5-bit
	int expectedBytes = 141;
	unsigned char crcExpected = 0, crcFound = 0;
	CPhidgetRFID_TagHandle tag = phid->respData;

	bytesInQueue = *endPtr - myReadPtr;
	if(myReadPtr > *endPtr)
		bytesInQueue += RFID_DATA_ARRAY_SIZE;

	//printf("maybe(read).. %d\n", bytesInQueue);
	//print_buffer(data, *startPtr, *endPtr);

	if(bytesInQueue != expectedBytes)
		return EPHIDGET_NOTFOUND;

	//verify SOF
	for(i=0;i<sofBits;i++)
	{
		if(data[(myReadPtr + i) & RFID_DATA_ARRAY_MASK] != 1)
		{
			phid->respStatus = EPHIDGET_NOTFOUND;
			goto done;
		}
	}

	//advance past SOF
	myReadPtr += sofBits;
	myReadPtr &= RFID_DATA_ARRAY_MASK;

	for(k=0;k<128;k++)
	{
		buf[k/8] |= data[(myReadPtr + k) & RFID_DATA_ARRAY_MASK] << (7-(k%8));
	}
	for(i=0;i<8;k++,i++)
	{
		crcFound |= data[(myReadPtr + k) & RFID_DATA_ARRAY_MASK] << (7-i);
	}
	crcExpected = hitagCRC8(buf, 128);

	if(crcFound != crcExpected)
	{
		LOG(PHIDGET_LOG_WARNING, "Hitag Read response has bad CRC (%02x, %02x)",crcFound, crcExpected);
		phid->respStatus = EPHIDGET_NOTFOUND;
		goto done;
	}

	//We're good! transfer Read data
	memcpy(tag->tagData + phid->hitagOffset * 4, buf, 16);
	tag->tagDataValid = PTRUE;

	//update timer
	setTimeNow(&tag->lastTagTime);

	phid->respStatus = EPHIDGET_OK;
done:
	CThread_set_event(&phid->respEvent);
	return phid->respStatus;
}

static int decodeHitagSelectResponse(CPhidgetRFIDHandle phid, unsigned char *data, int *startPtr, int *endPtr)
{
	int myReadPtr = *startPtr;
	int bytesInQueue;
	unsigned char buf[] = {0,0,0,0};
	int i,k;
	int sofBits = 5; //SOF is really 6-bit but we only see it as 5-bit
	int expectedBytes = 45;
	unsigned char crcExpected = 0, crcFound = 0;
	CPhidgetRFID_TagHandle tag = phid->respData;

	bytesInQueue = *endPtr - myReadPtr;
	if(myReadPtr > *endPtr)
		bytesInQueue += RFID_DATA_ARRAY_SIZE;

	//printf("maybe.. %d\n", bytesInQueue);
	//print_buffer(data, *startPtr, *endPtr);

	if(bytesInQueue != expectedBytes)
		return EPHIDGET_NOTFOUND;

	//verify SOF
	for(i=0;i<sofBits;i++)
	{
		if(data[(myReadPtr + i) & RFID_DATA_ARRAY_MASK] != 1)
		{
			phid->respStatus = EPHIDGET_NOTFOUND;
			goto done;
		}
	}

	//advance past SOF
	myReadPtr += sofBits;
	myReadPtr &= RFID_DATA_ARRAY_MASK;

	for(k=0;k<32;k++)
	{
		buf[k/8] |= data[(myReadPtr + k) & RFID_DATA_ARRAY_MASK] << (7-(k%8));
	}
	for(i=0;i<8;k++,i++)
	{
		crcFound |= data[(myReadPtr + k) & RFID_DATA_ARRAY_MASK] << (7-i);
	}
	crcExpected = hitagCRC8(buf, 32);

	if(crcFound != crcExpected)
	{
		LOG(PHIDGET_LOG_WARNING, "Hitag Select response has bad CRC (%02x, %02x)",crcFound, crcExpected);
		phid->respStatus = EPHIDGET_NOTFOUND;
		goto done;
	}

	//We're good! fill in structure
	//Memory size
	switch(buf[0] & 0x03)
	{
		case 0x00:
			tag->tagOptions.memSize = 32;
			break;
		case 0x01:
			tag->tagOptions.memSize = 256;
			break;
		case 0x02:
			tag->tagOptions.memSize = 2048;
			break;
		case 0x03:
		default:
			phid->respStatus = EPHIDGET_UNEXPECTED;
			goto done;
	}
	//AUT bit
	if(buf[1] & 0x80)
		tag->tagOptions.encrypted = PTRUE;
	else
		tag->tagOptions.encrypted = PFALSE;
	//LCON bit
	if(buf[1] & 0x02)
		tag->tagOptions.writable = PFALSE;
	else
		tag->tagOptions.writable = PTRUE;

	tag->tagOptionsValid = PTRUE;

	//update timer
	setTimeNow(&tag->lastTagTime);

	phid->respStatus = EPHIDGET_OK;
done:
	CThread_set_event(&phid->respEvent);
	return phid->respStatus;
}

static int add_biphase_data(CPhidgetRFIDHandle phid, int readToPtr, int shortClocks, int longClocks)
{
	int myReadPtr = phid->dataReadPtr;
	while(myReadPtr != readToPtr)
	{
		int clocks = phid->dataBuffer[myReadPtr] & 0x7F;
		//int polarity = (phid->dataBuffer[myReadPtr] & 0x80) ? 1 : 0;

		//1
		if (pdiff(clocks, longClocks) < 0.3) {

			phid->biphaseBuffer[phid->biphaseWritePtr] = 1;

			phid->biphaseWritePtr++;
			phid->biphaseWritePtr &= RFID_DATA_ARRAY_MASK;

			if(phid->biphaseWritePtr == phid->biphaseReadPtr)
			{
				phid->biphaseReadPtr++;
				phid->biphaseReadPtr &= RFID_DATA_ARRAY_MASK;
			}
			
			phid->biphaseLockedIn = 1;
			phid->biphaseShortChange = 0;
		} 
		else if (pdiff(clocks, shortClocks) < 0.3) {
			if (phid->biphaseLockedIn && phid->biphaseShortChange) {
				phid->biphaseBuffer[phid->biphaseWritePtr] = 0;

				phid->biphaseWritePtr++;
				phid->biphaseWritePtr &= RFID_DATA_ARRAY_MASK;

				if(phid->biphaseWritePtr == phid->biphaseReadPtr)
				{
					phid->biphaseReadPtr++;
					phid->biphaseReadPtr &= RFID_DATA_ARRAY_MASK;
				}

				phid->biphaseShortChange=0;
			}
			else	
				phid->biphaseShortChange=1;
		}
		else {
			phid->biphaseLockedIn = 0;
			//invalid
			phid->biphaseReadPtr = phid->biphaseWritePtr;
			//This is not BiPhase encoded data
			return EPHIDGET_NOTFOUND;
		}

		myReadPtr++;
		myReadPtr &= RFID_DATA_ARRAY_MASK;
	}
	return EPHIDGET_OK;
}

static int add_manchester_data(CPhidgetRFIDHandle phid, int readToPtr, int shortClocks, int longClocks)
{
	int myReadPtr = phid->dataReadPtr;
	while(myReadPtr != readToPtr)
	{
		int clocks = phid->dataBuffer[myReadPtr] & 0x7F;
		int polarity = (phid->dataBuffer[myReadPtr] & 0x80) ? 1 : 0;

		if (pdiff(clocks, longClocks) < 0.3) {

			if (polarity)
				phid->manBuffer[phid->manWritePtr] = 1;
			else
				phid->manBuffer[phid->manWritePtr] = 0;

			phid->manWritePtr++;
			phid->manWritePtr &= RFID_DATA_ARRAY_MASK;

			if(phid->manWritePtr == phid->manReadPtr)
			{
				phid->manReadPtr++;
				phid->manReadPtr &= RFID_DATA_ARRAY_MASK;
			}
			
			phid->manLockedIn = 1;
			phid->manShortChange = 0;
		} 
		else if (pdiff(clocks, shortClocks) < 0.3) {
			if (phid->manLockedIn && phid->manShortChange) {
				if (polarity)
					phid->manBuffer[phid->manWritePtr] = 1;
				else
					phid->manBuffer[phid->manWritePtr] = 0;

				phid->manWritePtr++;
				phid->manWritePtr &= RFID_DATA_ARRAY_MASK;

				if(phid->manWritePtr == phid->manReadPtr)
				{
					phid->manReadPtr++;
					phid->manReadPtr &= RFID_DATA_ARRAY_MASK;
				}

				phid->manShortChange=0;
			}
			else	
				phid->manShortChange=1;
		}
		else {
			phid->manLockedIn = 0;
			//invalid
			phid->manReadPtr = phid->manWritePtr;
			//This is not Manchester encoded data
			return EPHIDGET_NOTFOUND;
		}

		myReadPtr++;
		myReadPtr &= RFID_DATA_ARRAY_MASK;
	}
	return EPHIDGET_OK;
}

//Anti-Collision coding for Hitag S
static int decodeACdata(CPhidgetRFIDHandle phid, unsigned char *acBuffer, int *acBufferSize, int readToPtr, int shortClocks, int longClocks)
{
	int myReadPtr = phid->dataReadACPtr;
	int clocks1 = 0;
	int polarity1 = 0;
	int clocks2 = 0;
	int clocks = 0;
	int polarity2 = 0;
	int i;
	int bytesToRead;
	int lastIndex = 0;
	int acWritePtr = 0;
	unsigned char acBitLocation = 0;

	if(phid->hitagState == RFID_HITAG_STATE_UID_REQUEST)
		lastIndex = 34;
	else if(phid->hitagState == RFID_HITAG_STATE_AC_SEQUENCE)
		lastIndex = 34 - phid->lastHitagAC.colPos;

	if(*acBufferSize < lastIndex+1)
		goto fail;

	//if the first pulse is low, we need to add a high pulse before it
	if(!(phid->dataBuffer[phid->dataReadACPtr] & 0x80))
	{
		myReadPtr--;
		myReadPtr &= RFID_DATA_ARRAY_MASK;
		phid->dataBuffer[myReadPtr] = (shortClocks/2) | 0x80;
	}
	
	bytesToRead = readToPtr - myReadPtr;
	if(readToPtr < myReadPtr)
		bytesToRead += RFID_DATA_ARRAY_SIZE;

	bytesToRead &= 0xFFFE; //make LSB == 0

	for(i=0;i<bytesToRead;i+=2)
	{
		clocks1 = phid->dataBuffer[myReadPtr] & 0x7F;
		polarity1 = (phid->dataBuffer[myReadPtr] & 0x80) ? 1 : 0;
		clocks2 = phid->dataBuffer[(myReadPtr+1) & RFID_DATA_ARRAY_MASK] & 0x7F;
		polarity2 = (phid->dataBuffer[(myReadPtr+1) & RFID_DATA_ARRAY_MASK] & 0x80) ? 1 : 0;

		clocks = clocks1 + clocks2;

		if(polarity1 != 1 || polarity2 != 0)
			goto fail;

		//the first pulse can be long
		if (pdiff(clocks, shortClocks) < 0.2 || acWritePtr == 0)
		{			
			if(acBitLocation == 1)
			{
				acBuffer[acWritePtr] = PTRUE;

				acWritePtr++;
			}
			acBitLocation ^= 1;
		}
		else if(pdiff(clocks, longClocks) < 0.2)
		{
			if(acBitLocation == 1)
			{
				//Sometimes we get a too short ending pulse
				if(acWritePtr == lastIndex && myReadPtr == ((readToPtr-1) & RFID_DATA_ARRAY_MASK))
				{
					acBuffer[acWritePtr] = PTRUE;
					acWritePtr++;
				}
				else
					goto fail;
			}
			else
			{
				if(pdiff(clocks1, clocks2) > 0.5)
					acBuffer[acWritePtr] = PUNK_BOOL;
				else
					acBuffer[acWritePtr] = PFALSE;

				acWritePtr++;

				acBitLocation = 0;
			}
		}
		
		myReadPtr+=2;
		myReadPtr &= RFID_DATA_ARRAY_MASK;
	}

	//last low pulse won't be seen because we idle low
	if(acWritePtr == lastIndex && myReadPtr == ((readToPtr-1) & RFID_DATA_ARRAY_MASK))
	{
		clocks = phid->dataBuffer[myReadPtr] & 0x7F;
		polarity1 = (phid->dataBuffer[myReadPtr] & 0x80) ? 1 : 0;

		if(polarity1 != 1)
			goto fail;
		
		if(pdiff(clocks, shortClocks/2) < 0.4)
		{
			if(acBitLocation==1)
			{
				acBuffer[acWritePtr] = PTRUE;
				acWritePtr++;
			}
			//there should still be more data
			else
				acBitLocation ^= 1;
		}
		else if(pdiff(clocks, longClocks/2) < 0.3)
		{
			if(acBitLocation==1)
				goto fail;

			acBuffer[acWritePtr] = PFALSE;
			acWritePtr++;
		}
		else if(pdiff(clocks, (longClocks/4)*3) < 0.25)
		{
			if(acBitLocation==1)
				goto fail;

			acBuffer[acWritePtr] = PUNK_BOOL;
			acWritePtr++;
		}
		else 
			goto fail;

	}

	*acBufferSize = acWritePtr;
	return EPHIDGET_OK;

fail:
	//printf("beep2\n");
	
	acBitLocation = 0;

	//This is not AC encoded data
	return EPHIDGET_NOTFOUND;
}

//NOTE: tag is a local variable, we need to make a copy of it before adding it to lists, etc.
static int advanced_tag_event(CPhidgetRFIDHandle phid, CPhidgetRFID_TagHandle tagPtr)
{
	CPhidgetRFID_TagHandle tag;
	//Add to the tag list here, remove from the tag list in the tagTimerThreadFunction
	//TODO: We need to mutex protect this list, as well as the TIME variables
	CThread_mutex_lock(&phid->tagthreadlock);
	if(CList_findInList((CListHandle)phid->tagAdvancedList, tagPtr, CPhidgetRFID_Tag_areEqual, (void**)&tag) == EPHIDGET_NOTFOUND)
	{
		//make a copy
		tag = (CPhidgetRFID_TagHandle)malloc(sizeof(*tag));
		memcpy(tag, tagPtr, sizeof(*tag));
		tag->tagEventPending = PTRUE;
		CThread_set_event(&phid->tagAvailableEvent);
		CList_addToList((CListHandle *)&phid->tagAdvancedList, tag, CPhidgetRFID_Tag_areEqual);
	}

	setTimeNow(&tag->lastTagTime);
	CThread_mutex_unlock(&phid->tagthreadlock);

	return EPHIDGET_OK;
}

//Analyses streaming data in Manchester or Biphase coding
static int analyze_data(CPhidgetRFIDHandle phid)
{
	int bytesToRead = 0, bytesRead = 0;
	int temp, one, two;
	int myReadPtr;
	CPhidgetRFID_Tag tag;

	//read till we have real data
start:
	while(phid->dataReadPtr != phid->dataWritePtr)
	{
		if(phid->dataBuffer[phid->dataReadPtr] == PUNK_INT)
		{
			phid->dataReadPtr++;
			phid->dataReadPtr &= RFID_DATA_ARRAY_MASK;
			phid->atGap = PTRUE;
			
			phid->one = phid->two = phid->oneCount = phid->twoCount = 0;
		}
		else
			break;
	}
	myReadPtr = phid->dataReadPtr;

	//Make sure we have enough data to do something useful with..
	bytesToRead = phid->dataWritePtr - phid->dataReadPtr;
	if(phid->dataReadPtr > phid->dataWritePtr)
		bytesToRead += RFID_DATA_ARRAY_SIZE;

	if(bytesToRead < 32 && phid->atGap)
		return EPHIDGET_OK;

	//then read till we have a space or run out of data - figure out data rate
	one = two = 0;
	bytesRead = 0;
	while(myReadPtr != phid->dataWritePtr)
	{
		if(phid->dataBuffer[myReadPtr] == PUNK_INT)
			break;

		if(phid->one == 0)
		{
			phid->one = phid->dataBuffer[myReadPtr] & 0x7F;
			phid->oneCount++;
		}
		else
		{
			temp = round((double)((double)phid->one / (double)phid->oneCount));
			if(pdiff(temp, phid->dataBuffer[myReadPtr] & 0x7F) < 0.3)
			{
				phid->one += phid->dataBuffer[myReadPtr] & 0x7F;
				phid->oneCount++;
			}
			else
			{
				if(phid->two == 0)
				{
					temp = round((double)((double)phid->one / (double)phid->oneCount));
					if(pdiff(temp * 2, phid->dataBuffer[myReadPtr] & 0x7F) < 0.3 
						|| pdiff(temp / 2, phid->dataBuffer[myReadPtr] & 0x7F) < 0.3)
					{
						phid->two = phid->dataBuffer[myReadPtr] & 0x7F;
						phid->twoCount++;
					}
					else
					{
						goto update_readPtr_restart;
					}
				}
				else
				{
					temp = round((double)((double)phid->two / (double)phid->twoCount));
					if(pdiff(temp, phid->dataBuffer[myReadPtr] & 0x7F) < 0.3)
					{
						phid->two += phid->dataBuffer[myReadPtr] & 0x7F;
						phid->twoCount++;
					}
					else
					{
						goto update_readPtr_restart;
					}
				}
			}
		}

		myReadPtr++;
		myReadPtr &= RFID_DATA_ARRAY_MASK;
		bytesRead++;
	}
	
	if(bytesRead < bytesToRead)
	{
		goto update_readPtr_restart;
	}

	//don't let the one and two counters get too big
	if(phid->oneCount >= RFID_DATA_ARRAY_SIZE)
	{
		phid->one = round((double)((double)phid->one / 2.0));
		phid->oneCount = phid->oneCount / 2;
	}
	if(phid->twoCount >= RFID_DATA_ARRAY_SIZE)
	{
		phid->two = round((double)((double)phid->two / 2.0));
		phid->twoCount = phid->twoCount / 2;
	}

	if(phid->one)
		one = round((double)((double)phid->one / (double)phid->oneCount));
	if(phid->two)
		two = round((double)((double)phid->two / (double)phid->twoCount));

	//Order them by size
	if(two < one)
	{
		temp = two;
		two = one;
		one = temp;
	}

	//printf("One: %3d Two: %3d Bytes: %4d\n",one, two, bytesToRead);

	ZEROMEM(&tag, sizeof(CPhidgetRFID_Tag));
	//Normalize the data rate we supply to the user to be a power of 2
	//TODO: are there any tags which use a bitrate that's not a power of two?
	temp=1;
	tag.tagInfo.bitRate = two;
	while(temp<two)
	{
		temp*=2;
		if(pdiff(temp, two) < 0.3)
		{
			tag.tagInfo.bitRate = temp;
			break;
		}
	}
	if(tag.tagInfo.bitRate == 128)
		printf("hmm\n"); //TODO: are we actually going to do something about this?

	//Shift data into Manchester and Biphase decoders, update read ptr
	if(phid->hitagState == RFID_HITAG_STATE_SELECT
		|| phid->hitagState == RFID_HITAG_STATE_READ
		|| phid->hitagState == RFID_HITAG_STATE_WRITE)
	{
		if(phid->manReadPtr == phid->manWritePtr)
		{
			//may have to advance one to make things work
			if(phid->dataBuffer[phid->dataReadPtr] & 0x80 && pdiff(phid->dataBuffer[phid->dataReadPtr] & 0x7F, 16) < 0.3)
				phid->dataReadPtr = ((phid->dataReadPtr + 1) & RFID_DATA_ARRAY_MASK);
		}
	}
	if(!add_manchester_data(phid, myReadPtr, one, two))
	{
		tag.tagInfo.encoding = PHIDGET_RFID_ENCODING_MANCHESTER;
		if(phid->hitagState == RFID_HITAG_STATE_SELECT)
		{
			//try to decode a Hitag SELECT response
			decodeHitagSelectResponse(phid, phid->manBuffer, &phid->manReadPtr, &phid->manWritePtr);
		}
		if(phid->hitagState == RFID_HITAG_STATE_READ)
		{
			//try to decode a Hitag READ response
			decodeHitagReadResponse(phid, phid->manBuffer, &phid->manReadPtr, &phid->manWritePtr);
		}
		if(phid->hitagState == RFID_HITAG_STATE_WRITE)
		{
			//try to decode a Hitag WRITE response
			decodeHitagACKResponse(phid, phid->manBuffer, &phid->manReadPtr, &phid->manWritePtr);
		}
		if(!decodeEM4102(phid, phid->manBuffer, &phid->manReadPtr, &phid->manWritePtr, &tag))
			advanced_tag_event(phid, &tag);
		if(!decodeISO11785(phid, phid->manBuffer, &phid->manReadPtr, &phid->manWritePtr, &tag))
			advanced_tag_event(phid, &tag);
	}
	if(!add_biphase_data(phid, myReadPtr, one, two))
	{
		tag.tagInfo.encoding = PHIDGET_RFID_ENCODING_BIPHASE;
		if(!decodeEM4102(phid, phid->biphaseBuffer, &phid->biphaseReadPtr, &phid->biphaseWritePtr, &tag))
			advanced_tag_event(phid, &tag);
		if(!decodeISO11785(phid, phid->biphaseBuffer, &phid->biphaseReadPtr, &phid->biphaseWritePtr, &tag))
			advanced_tag_event(phid, &tag);
	}

	//update read pointer
	phid->dataReadPtr = myReadPtr;
	phid->atGap = PFALSE;

	return EPHIDGET_OK;

	//ran into a bad pulse length or a gap - reset stuff
update_readPtr_restart:
	phid->one = phid->two = phid->oneCount = phid->twoCount = 0;
	phid->dataReadPtr = myReadPtr;
	phid->atGap = PTRUE;
	phid->manReadPtr = 0;
	phid->manWritePtr = 0;
	phid->biphaseReadPtr = 0;
	phid->biphaseWritePtr = 0;
	goto start;
}

//Analyses data for Hitag AC coding
static int analyze_data_AC(CPhidgetRFIDHandle phid)
{
	int bytesToRead = 0;
	unsigned int myReadPtr;
	CPhidgetRFID_Tag tag;
	unsigned char acBuffer[64];
	int acBufferSize = 64;

	ZEROMEM(&tag, sizeof(CPhidgetRFID_Tag));

	//read till we have real data
	while(phid->dataReadACPtr != phid->dataWritePtr)
	{
		if(phid->dataBuffer[phid->dataReadACPtr] == PUNK_INT)
		{
			phid->dataReadACPtr++;
			phid->dataReadACPtr &= RFID_DATA_ARRAY_MASK;
		}
		else
			break;
	}
	myReadPtr = phid->dataReadACPtr;

	if(phid->dataReadACPtr == phid->dataWritePtr)
		return EPHIDGET_OK;

	//read till we find the next gap
	while(myReadPtr != phid->dataWritePtr)
	{
		if(phid->dataBuffer[myReadPtr] != PUNK_INT)
		{
			myReadPtr++;
			myReadPtr &= RFID_DATA_ARRAY_MASK;
		}
		else
			break;
	}

	if(myReadPtr == phid->dataWritePtr)
		return EPHIDGET_OK;

	//We should now have a set of data between two gaps

	//Make sure we have enough data to do something useful with..
	bytesToRead = myReadPtr - phid->dataReadACPtr;
	if(phid->dataReadACPtr > myReadPtr)
		bytesToRead += RFID_DATA_ARRAY_SIZE;

	if(!decodeACdata(phid, acBuffer, &acBufferSize, myReadPtr, 32, 64))
	{
		int collision = -1;

		phid->ACCodingOK = PFALSE;

		//printf("%d ",phid->acWritePtr);
		tag.tagInfo.encoding = PHIDGET_RFID_ENCODING_AC;
		tag.tagInfo.bitRate = 64;
		if(!decodeHitagUID(phid, acBuffer, acBufferSize, &tag, &collision))
		{
			advanced_tag_event(phid, &tag);
			//TODO: select tag?
			//printf("Got Hitag Tag: %s\n",tag.tagString);

			//Any pending AC commands?
			if(phid->hitagACList)
			{
				CPhidgetRFID_HitagACHandle ac = phid->hitagACList[0].hitagAC;
				HitagS_AC_SEQUENCE(phid, ac);
				CList_removeFromList((CListHandle *)&phid->hitagACList, ac, CPhidgetRFID_HitagAC_areEqual, PTRUE, CPhidgetRFID_HitagAC_free);
			}
		}
		else if(collision != -1)
		{
			int k;
			CPhidgetRFID_HitagACHandle ac = (CPhidgetRFID_HitagACHandle)malloc(sizeof(CPhidgetRFID_HitagAC));
			ZEROMEM(ac, sizeof(CPhidgetRFID_HitagAC));
			//printf("Got Hitag Collision: %d\n",collision);

			for(k=0;k<collision;k++)
			{
				ac->uid[k/8] |= acBuffer[k+3] << (7-(k%8));
			}
			//choose 1 for the collision position
			ac->uid[k/8] |= 1 << (7-(k%8));

			ac->colPos = collision+1;

			HitagS_AC_SEQUENCE(phid, ac);

			//add AC with 0 to queue
			ac->uid[k/8] &= ~(1 << (7-(k%8)));
			CList_addToList((CListHandle *)&phid->hitagACList, ac, CPhidgetRFID_HitagAC_areEqual);
		}
	}

	//update read pointer
	phid->dataReadACPtr = myReadPtr;

	return EPHIDGET_OK;
}

// === Exported Functions === //

//create and initialize a device structure
CCREATE_EXTRA(RFID, PHIDCLASS_RFID)
	CThread_mutex_init(&phid->tagthreadlock);
	CThread_create_event(&phid->tagAvailableEvent);
	CThread_create_event(&phid->respEvent);
	CThread_create_event(&phid->respEvent2);
	phid->phid.fptrClose = CPhidgetRFID_close;
	phid->phid.fptrFree = CPhidgetRFID_free;
	return EPHIDGET_OK;
}

//event setup functions
CFHANDLE(RFID, OutputChange, int, int)
CFHANDLE(RFID, Tag, unsigned char *)
CFHANDLE(RFID, TagLost, unsigned char *)
CFHANDLE(RFID, RawData, int *data, int dataLength)
CFHANDLE(RFID, TagAdvanced, char *tagString, CPhidgetRFID_TagInfoHandle tagInfo)
CFHANDLE(RFID, TagLostAdvanced, char *tagString, CPhidgetRFID_TagInfoHandle tagInfo)

CGET(RFID,OutputCount,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED

	MASGN(phid.attr.rfid.numOutputs)
}

CGETINDEX(RFID,OutputState,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED
	TESTINDEX(phid.attr.rfid.numOutputs)
	TESTMASGN(outputState[Index], PUNK_BOOL)

	MASGN(outputState[Index])
}
CSETINDEX(RFID,OutputState,int)
	TESTPTR(phid)
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED
	TESTRANGE(PFALSE, PTRUE)
	TESTINDEX(phid.attr.rfid.numOutputs)

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEYINDEXED(Output, "%d", outputState);
	else
	{
		SENDPACKET(RFID, outputState[Index]);
		//echo back output state if the device doesn't
		//do it here because this is after the packet has been sent off - so blocking in this event will not delay the output
		if (!(phid->fullStateEcho))
		{
			if (phid->outputEchoState[Index] == PUNK_BOOL || phid->outputEchoState[Index] != newVal)
			{
				phid->outputEchoState[Index] = newVal;
				FIRE(OutputChange, Index, newVal);
			}
		}
	}

	return EPHIDGET_OK;
}

CGET(RFID,AntennaOn,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED
	TESTMASGN(antennaEchoState, PUNK_BOOL)

	MASGN(antennaEchoState)
}
CSET(RFID,AntennaOn,int)
	TESTPTR(phid)
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_RFID_2OUTPUT:
			TESTRANGE(PFALSE, PTRUE)

			if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
				ADDNETWORKKEY(AntennaOn, "%d", antennaState);
			else
			{
				SENDPACKET(RFID, antennaState);
				//echo back state if the device doesn't
				if (!(phid->fullStateEcho))
					phid->antennaEchoState = newVal;
			}
			return EPHIDGET_OK;
		case PHIDID_RFID_2OUTPUT_ADVANCED:
			if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
				ADDNETWORKKEY(AntennaOn, "%d", antennaState);
			else
			{
				unsigned char *buffer;
				int ret = 0;
				if(!(buffer = malloc(phid->phid.outputReportByteLength))) return EPHIDGET_NOMEMORY;
				ZEROMEM(buffer, phid->phid.outputReportByteLength);
				CThread_mutex_lock(&phid->phid.writelock);

				phid->antennaState = newVal;
				if(newVal)
				{
					phid->_4097Conf = RFID_4097_AmpDemod | RFID_4097_Active | RFID_4097_DataOut | RFID_4097_IntPLL | RFID_4097_FastStart | RFID_4097_Gain960;
					//phid->_4097Conf = RFID_4097_AmpDemod | RFID_4097_Active | RFID_4097_DataOut | RFID_4097_IntPLL | RFID_4097_FastStart | RFID_4097_Gain120;
				}
				else
				{
					phid->_4097Conf = RFID_4097_PowerDown;
				}

				if((ret = CPhidgetRFID_makePacket(phid, buffer))) goto done2;
				if((ret = CPhidgetRFID_sendpacket(phid, buffer))) goto done2;
			done2:
				CThread_mutex_unlock(&phid->phid.writelock);
				free(buffer);
				if(ret) return ret;
			}
			return EPHIDGET_OK;
		case PHIDID_RFID:
		default:
			return EPHIDGET_UNSUPPORTED;
	}
}

CGET(RFID,LEDOn,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_RFID_2OUTPUT:
		case PHIDID_RFID_2OUTPUT_ADVANCED:
			TESTMASGN(ledEchoState, PUNK_BOOL)
			MASGN(ledEchoState)
		case PHIDID_RFID:
		default:
			return EPHIDGET_UNSUPPORTED;
	}
}
CSET(RFID,LEDOn,int)
	TESTPTR(phid)
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_RFID_2OUTPUT:
		case PHIDID_RFID_2OUTPUT_ADVANCED:
			TESTRANGE(PFALSE, PTRUE)

			if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
				ADDNETWORKKEY(LEDOn, "%d", ledState);
			else
			{
				SENDPACKET(RFID, ledState);
				//echo back state if the device doesn't
				if (!(phid->fullStateEcho))
					phid->ledEchoState = newVal;
			}
			return EPHIDGET_OK;
		case PHIDID_RFID:
		default:
			return EPHIDGET_UNSUPPORTED;
	}
}

CGET(RFID, LastTag, unsigned char)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED

	//if it's all 0's - it's not yet available
	if(!memcmp("\0\0\0\0\0", phid->lastTag, 5))
		return EPHIDGET_UNKNOWNVAL;

	memcpy(pVal, phid->lastTag, 5);

	return EPHIDGET_OK;
}

CGET(RFID, TagStatus, int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED

	if(phid->tagPresent == PUNK_BOOL && phid->tagAdvancedCount == PUNK_INT) 
	{ 
		*pVal = PUNK_BOOL; 
		return EPHIDGET_UNKNOWNVAL;
	}

	if(phid->tagPresent == PTRUE || (phid->tagAdvancedCount > 0 && phid->tagAdvancedCount != PUNK_INT))
		*pVal = PTRUE;
	else
		*pVal = PFALSE;

	return EPHIDGET_OK;
}

PHIDGET21_API int CCONV CPhidgetRFID_WriteRaw(CPhidgetRFIDHandle phid, unsigned char *data, int bitlength, int pregap, int space, int postgap, int zero, int one)
{
	int retval;
	TESTPTR(phid)
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED
	if(pregap < (2) || pregap > (255)) return EPHIDGET_INVALIDARG;
	if(space < (2) || space > (255)) return EPHIDGET_INVALIDARG;
	if(postgap < (2) || postgap > (255)) return EPHIDGET_INVALIDARG;
	if(zero < (5) || zero > (255)) return EPHIDGET_INVALIDARG;
	if(one < (5) || one > (255)) return EPHIDGET_INVALIDARG;

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		;//ADDNETWORKKEYINDEXED(Acceleration, "%lE", motorAcceleration); //TODO
	else
	{
		unsigned char *buffer;
		int ret = 0;
		if(!(buffer = malloc(phid->phid.outputReportByteLength))) return EPHIDGET_NOMEMORY;
		ZEROMEM(buffer, phid->phid.outputReportByteLength);
		CThread_mutex_lock(&phid->phid.writelock);

		phid->pregapClocks = pregap;
		phid->postgapClocks = postgap;
		phid->spaceClocks = space;
		phid->zeroClocks = zero;
		phid->oneClocks = one;

		//Send timing
		if((ret = CPhidgetRFID_makePacket(phid, buffer))) goto done2;
		if((ret = CPhidgetRFID_sendpacket(phid, buffer))) goto done2;

		//send data
		if((retval = sendRAWData(phid, data, bitlength))) goto done2;

	done2:
		CThread_mutex_unlock(&phid->phid.writelock);
		free(buffer);
		if(ret) return ret;
	}

	return EPHIDGET_OK;
}

PHIDGET21_API int CCONV CPhidgetRFID_getRawData(CPhidgetRFIDHandle phid, int *data, int *dataLength)
{
	int i;
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED

	//make sure length is even so we only send out data with starting space and ending pulse
	if((*dataLength % 2) == 1)
		(*dataLength)--;

	for(i=0;i<*dataLength;i++)
	{
		if(phid->userReadPtr == phid->dataWritePtr)
			break;

		data[i] = phid->dataBuffer[phid->userReadPtr];
		phid->userReadPtr = (phid->userReadPtr + 1) & RFID_DATA_ARRAY_MASK;
	}

	//make sure i is even so that we don't end with a pulse
	if((i % 2) == 1)
	{
		//negate the pulse if we added it
		i--;
		phid->userReadPtr = (phid->userReadPtr - 1) & RFID_DATA_ARRAY_MASK;
	}

	*dataLength = i;

	return EPHIDGET_OK;
}

PHIDGET21_API int CCONV CPhidgetRFID_getTagOptions(CPhidgetRFIDHandle phid, char *tagString, CPhidgetRFID_TagOptionsHandle options)
{
	int ret = EPHIDGET_OK, wait_return = 0;
	CPhidgetRFID_TagList *trav = 0;
	CPhidgetRFID_TagHandle tag = 0;
	TESTPTRS(phid,tagString) 
	TESTPTR(options) 
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED

	//1st find it in the list - not there? can't get options.
	CThread_mutex_lock(&phid->tagthreadlock);
	for (trav=phid->tagAdvancedList; trav; trav = trav->next)
	{
		if(!strcmp(trav->tag->tagString, tagString))
			tag = trav->tag;
	}
	if(!tag)
	{
		CThread_mutex_unlock(&phid->tagthreadlock);
		return EPHIDGET_NOTFOUND;
	}

	//Do we need to poll for the options? yes for Hitag
	if(!tag->tagOptionsValid)
	{
		switch(tag->tagInfo.tagType)
		{
			case PHIDGET_RFID_TAG_HITAGS:
				CThread_mutex_unlock(&phid->tagthreadlock);

				phid->respData = tag;
				CThread_reset_event(&phid->respEvent);
				HitagS_SELECT(phid, (unsigned char *)tagString);

				wait_return = CThread_wait_on_event(&phid->respEvent, 500);
				switch (wait_return) {
					case WAIT_TIMEOUT:
						ret = EPHIDGET_TIMEOUT;
						break;
					case WAIT_OBJECT_0:
						ret = phid->respStatus;
						break;
					default:
						ret = EPHIDGET_UNEXPECTED;
						break;
				}

				CThread_reset_event(&phid->respEvent);

				CThread_mutex_lock(&phid->tagthreadlock);
				break;
			case PHIDGET_RFID_TAG_ISO11784:
				break;
			case PHIDGET_RFID_TAG_EM4102:
				break;
			default:
				CThread_mutex_unlock(&phid->tagthreadlock);
				return EPHIDGET_UNEXPECTED;
		}
	}
	//copy over options
	memcpy(options, &tag->tagOptions, sizeof(CPhidgetRFID_TagOptions));

	CThread_mutex_unlock(&phid->tagthreadlock);
	return ret;
}

PHIDGET21_API int CCONV CPhidgetRFID_read(CPhidgetRFIDHandle phid, char *tagString, unsigned char *data, int *dataLength, char *password)
{
	int ret = EPHIDGET_OK, wait_return = 0, i;
	CPhidgetRFID_TagList *trav = 0;
	CPhidgetRFID_TagHandle tag = 0;
	CPhidgetRFID_TagOptions options;
	TESTPTRS(phid,tagString) 
	TESTPTR(data) 
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED

	//1st find it in the list - not there? can't get options.
	CThread_mutex_lock(&phid->tagthreadlock);
	for (trav=phid->tagAdvancedList; trav; trav = trav->next)
	{
		if(!strcmp(trav->tag->tagString, tagString))
			tag = trav->tag;
	}
	if(!tag)
	{
		CThread_mutex_unlock(&phid->tagthreadlock);
		return EPHIDGET_NOTFOUND;
	}

	switch(tag->tagInfo.tagType)
	{
		case PHIDGET_RFID_TAG_HITAGS:
			if(tag->tagOptions.memSize == 32)
			{
				ret = EPHIDGET_UNSUPPORTED;
				break;
			}
			//Do we need to select the tag first?
			if(!tag->tagOptionsValid)
			{
				CThread_mutex_unlock(&phid->tagthreadlock);
				if((ret = CPhidgetRFID_getTagOptions(phid, tagString, &options)))
					return ret;
				CThread_mutex_lock(&phid->tagthreadlock);
			}

			//Now do the reads
			if(!tag->tagDataValid)
			{
				CThread_mutex_unlock(&phid->tagthreadlock);
				phid->respData = tag;
				CThread_reset_event(&phid->respEvent);
				//reading blocks - 4 pages/block, 32 bits/page
				for(i=0;i<tag->tagOptions.memSize/32;i+=4)
				{
					HitagS_READ(phid, i, PTRUE);

					wait_return = CThread_wait_on_event(&phid->respEvent, 500);
					switch (wait_return) {
						case WAIT_TIMEOUT:
							return EPHIDGET_TIMEOUT;
						case WAIT_OBJECT_0:
							if((ret = phid->respStatus))
								return ret;
							break;
						default:
							return EPHIDGET_UNEXPECTED;
					}

					CThread_reset_event(&phid->respEvent);
				}

				CThread_mutex_lock(&phid->tagthreadlock);
			}
			//copy over data - ignore 1st 8 bytes, it's UID and configuration
			//TODO: if Authentication is turned on, we lose even more space
			if(*dataLength > tag->tagOptions.memSize/8 - 8)
				*dataLength = tag->tagOptions.memSize/8 - 8;
			memcpy(data, tag->tagData + 8, *dataLength);

			break;
		case PHIDGET_RFID_TAG_ISO11784:
			ret = EPHIDGET_UNSUPPORTED;
			break;
		case PHIDGET_RFID_TAG_EM4102:
			ret = EPHIDGET_UNSUPPORTED;
			break;
		default:
			ret = EPHIDGET_UNEXPECTED;
			break;
	}

	CThread_mutex_unlock(&phid->tagthreadlock);
	return ret;
}

PHIDGET21_API int CCONV CPhidgetRFID_write(CPhidgetRFIDHandle phid, char *tagString, unsigned char *data, int dataLength, int offset, char *password)
{
	int ret = EPHIDGET_OK, wait_return = 0, i, writePage, writeSize;
	CPhidgetRFID_TagList *trav = 0;
	CPhidgetRFID_TagHandle tag = 0;
	CPhidgetRFID_TagOptions options;
	unsigned char readData[256];
	int readDataSize = 256;
	TESTPTRS(phid,tagString) 
	TESTPTR(data) 
	TESTDEVICETYPE(PHIDCLASS_RFID)
	TESTATTACHED

	//1st find it in the list - not there? can't get options.
	CThread_mutex_lock(&phid->tagthreadlock);
	for (trav=phid->tagAdvancedList; trav; trav = trav->next)
	{
		if(!strcmp(trav->tag->tagString, tagString))
			tag = trav->tag;
	}
	if(!tag)
	{
		CThread_mutex_unlock(&phid->tagthreadlock);
		return EPHIDGET_NOTFOUND;
	}

	switch(tag->tagInfo.tagType)
	{
		case PHIDGET_RFID_TAG_HITAGS:

			if(tag->tagOptions.memSize == 32)
			{
				ret = EPHIDGET_UNSUPPORTED;
				break;
			}
			//Do we need to select the tag first?
			if(!tag->tagOptionsValid)
			{
				CThread_mutex_unlock(&phid->tagthreadlock);
				if((ret = CPhidgetRFID_getTagOptions(phid, tagString, &options)))
					return ret;
				CThread_mutex_lock(&phid->tagthreadlock);
			}

			if(offset+dataLength > (tag->tagOptions.memSize/8)-8)
				return EPHIDGET_INVALIDARG;

			//Need to read in the tag first
			CThread_mutex_unlock(&phid->tagthreadlock);
			if((ret = CPhidgetRFID_read(phid, tagString, readData, &readDataSize, password)))
				return ret;
			CThread_mutex_lock(&phid->tagthreadlock);

			//Now do the writes
			CThread_mutex_unlock(&phid->tagthreadlock);
			phid->respData = tag;
			CThread_reset_event(&phid->respEvent);

			//need to start the write on a page border
			//these are in pages (4 bytes)
			writePage = offset/4;
			writeSize = dataLength/4 + (dataLength%4 ? 1 : 0) + offset%4;
			
			//replace what the user wants to write
			memcpy(readData+offset, data, dataLength);

			for(i=writePage;i<writePage+writeSize;i+=(i%4 ? i%4 : 4))
			{
				HitagS_WRITE(phid, i, readData+i, PTRUE);

				wait_return = CThread_wait_on_event(&phid->respEvent, 500);
				switch (wait_return) {
					case WAIT_TIMEOUT:
						return EPHIDGET_TIMEOUT;
					case WAIT_OBJECT_0:
						if((ret = phid->respStatus))
							return ret;
						break;
					default:
						return EPHIDGET_UNEXPECTED;
				}

				CThread_reset_event(&phid->respEvent);
			}

			CThread_mutex_lock(&phid->tagthreadlock);

			break;
		case PHIDGET_RFID_TAG_ISO11784:
			ret = EPHIDGET_UNSUPPORTED;
			break;
		case PHIDGET_RFID_TAG_EM4102:
			ret = EPHIDGET_UNSUPPORTED;
			break;
		default:
			ret = EPHIDGET_UNEXPECTED;
			break;
	}

	CThread_mutex_unlock(&phid->tagthreadlock);
	return ret;
}

// === Deprecated Functions === //

CGET(RFID,NumOutputs,int)
	return CPhidgetRFID_getOutputCount(phid, pVal);
}

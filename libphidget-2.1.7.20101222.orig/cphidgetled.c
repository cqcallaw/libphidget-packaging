#include "stdafx.h"
#include "cphidgetled.h"
#include "cusb.h"
#include "csocket.h"
#include "cthread.h"

// === Internal Functions === //

//clearVars - sets all device variables to unknown state
CPHIDGETCLEARVARS(LED)
	int i = 0;

	phid->changeRequests=PUNK_BOOL;

	for(i=0;i<LED_MAXLEDS;i++)
	{
		phid->changedLED_Power[i] = PUNK_BOOL;
		phid->LED_Power[i] = PUNI_INT;
		phid->nextLED_Power[i] = PUNK_INT;

		phid->LED_PowerEcho[i] = PUNK_INT;
		phid->outputEnabledEcho[i] = PUNK_BOOL;
		phid->ledOpenDetectEcho[i] = PUNK_BOOL;

		phid->lastLED_Power[i] = PUNK_INT;
	}
	phid->voltage = PHIDGET_LED_VOLTAGE_2_75V;
	phid->currentLimit = PHIDGET_LED_CURRENT_LIMIT_20mA;
	phid->faultEcho = PUNK_BOOL;
	phid->TSDCount=0;
	phid->PGoodErrState = PFALSE;
	phid->powerGoodEcho = PUNK_BOOL;
	phid->outputEnableEcho = PUNK_BOOL;
	phid->currentLimitEcho = -1;
	phid->voltageEcho = -1;

	return EPHIDGET_OK;
}

//initAfterOpen - sets up the initial state of an object, reading in packets from the device if needed
//				  used during attach initialization - on every attach
CPHIDGETINIT(LED)
	int i = 0;

	TESTPTR(phid);

	//set data arrays to unknown
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_LED_64:
			for(i=0;i<phid->phid.attr.led.numLEDs;i++)
			{
				phid->changedLED_Power[i] = PFALSE;
				phid->LED_Power[i] = PUNK_INT;
				phid->nextLED_Power[i] = PUNK_INT;
			}
			break;
		case PHIDID_LED_64_ADV:
			if ((phid->phid.deviceVersion >= 100) && (phid->phid.deviceVersion < 200))
			{
				for(i=0;i<phid->phid.attr.led.numLEDs;i++)
				{
					phid->changedLED_Power[i] = PFALSE;
					phid->LED_Power[i] = PUNK_INT;
					phid->nextLED_Power[i] = PUNK_INT;
					
					phid->LED_PowerEcho[i] = PUNK_INT;
					phid->outputEnabledEcho[i] = PUNK_BOOL;
					phid->ledOpenDetectEcho[i] = PUNK_BOOL;

					phid->lastLED_Power[i] = PUNK_INT;
				}
				phid->voltage = PHIDGET_LED_VOLTAGE_2_75V;
				phid->currentLimit = PHIDGET_LED_CURRENT_LIMIT_20mA;

				phid->faultEcho = PUNK_BOOL;
				phid->powerGoodEcho = PUNK_BOOL;
				phid->PGoodErrState = PFALSE;
				phid->outputEnableEcho = PUNK_BOOL;
				phid->voltageEcho = -1;
				phid->currentLimitEcho = -1;

				phid->TSDCount=0;
				phid->TSDClearCount = 0;
				phid->lastOutputPacket = 0;
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		default:
			return EPHIDGET_UNEXPECTED;
	}
	phid->changeRequests=0;
	phid->controlPacketWaiting = PFALSE;
	
	//issue a read - fill in data
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_LED_64_ADV:
			//need two reads to get the full state
			CPhidget_read((CPhidgetHandle)phid);
			CPhidget_read((CPhidgetHandle)phid);
			for(i=0;i<phid->phid.attr.led.numLEDs;i++)
			{
				if(phid->outputEnabledEcho[i] == PTRUE)
					phid->LED_Power[i] = phid->LED_PowerEcho[i];
				else
					phid->LED_Power[i] = 0;
				
				phid->lastLED_Power[i] = phid->LED_PowerEcho[i];
			}
			if(phid->voltageEcho != -1)
				phid->voltage = phid->voltageEcho;
			if(phid->currentLimitEcho != -1)
				phid->currentLimit = phid->currentLimitEcho;
			break;
		case PHIDID_LED_64:
		default:
			break;
	}

	return EPHIDGET_OK;
}

//dataInput - parses device packets
CPHIDGETDATA(LED)
	int i = 0;
	char error_buffer[50];

	if (length < 0) return EPHIDGET_INVALIDARG;
	TESTPTR(phid);
	TESTPTR(buffer);

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_LED_64_ADV:
			if ((phid->phid.deviceVersion >= 100) && (phid->phid.deviceVersion < 200))
			{
				switch(buffer[0] & 0x80)
				{
					case LED64_IN_LOW_PACKET:
						//PowerGood
						if(buffer[0] & LED64_PGOOD_FLAG)
						{
							phid->PGoodErrState = PFALSE;
							phid->powerGoodEcho = PTRUE;
						}
						else
						{
							phid->powerGoodEcho = PFALSE;
						}

						//all outputs enabled (power on/off)
						if(buffer[0] & LED64_OE_FLAG)
							phid->outputEnableEcho = PTRUE;
						else
							phid->outputEnableEcho = PFALSE;

						//fault
						if(buffer[0] & LED64_FAULT_FLAG)
							phid->faultEcho = PTRUE;
						else
							phid->faultEcho = PFALSE;

						//current limit
						if(buffer[0] & LED64_CURSELA_FLAG)
						{
							if(buffer[0] & LED64_CURSELB_FLAG)
								phid->currentLimitEcho = PHIDGET_LED_CURRENT_LIMIT_80mA;
							else
								phid->currentLimitEcho = PHIDGET_LED_CURRENT_LIMIT_40mA;
						}
						else if (buffer[0] & LED64_CURSELB_FLAG)
							phid->currentLimitEcho = PHIDGET_LED_CURRENT_LIMIT_60mA;
						else
							phid->currentLimitEcho = PHIDGET_LED_CURRENT_LIMIT_20mA;
						
						//voltage
						if(buffer[0] & LED64_PWRSELA_FLAG)
						{
							if(buffer[0] & LED64_PWRSELB_FLAG)
								phid->voltageEcho = PHIDGET_LED_VOLTAGE_5_0V;
							else
								phid->voltageEcho = PHIDGET_LED_VOLTAGE_2_75V;
						}
						else if (buffer[0] & LED64_PWRSELB_FLAG)
							phid->voltageEcho = PHIDGET_LED_VOLTAGE_3_9V;
						else
							phid->voltageEcho = PHIDGET_LED_VOLTAGE_1_7V;

						for(i=0;i<phid->phid.attr.led.numLEDs;i++)
						{
							phid->outputEnabledEcho[i] = (buffer[(i/8)+1] & (1 << (i%8))) ? 1 : 0;
							phid->ledOpenDetectEcho[i] = (buffer[(i/8)+9] & (1 << (i%8))) ? 1 : 0;
						}

						//1st 24 LED powers
						for(i=0;i<24;i++)
						{
							double ledPowerTemp;
							ledPowerTemp = ((double)buffer[i+17] / 127.0) * 100.0;
							phid->LED_PowerEcho[i] = round(ledPowerTemp);
						}

						//We can guess that the fault is a TSD if there is no LOD
						if(phid->faultEcho)
						{
							phid->TSDCount++;
							phid->TSDClearCount = 30; //500ms of no faults before we clear it

							for(i=0;i<phid->phid.attr.led.numLEDs;i++)
							{
								if(phid->ledOpenDetectEcho[i])
									phid->TSDCount = 0;
							}

							//send out some error events on faults
							//TODO: we could also send LED Open Detect?

							//we have counted three fault flags with no LODs - TSD - only one error event is thrown until this is cleared
							//less then 3 counts, and it could be a false positive
							//if outputs are not enabled then the fault should be guaranteed as a TSD
							if(phid->TSDCount == 3 || (phid->TSDCount < 3 && phid->outputEnableEcho == PFALSE))
							{
								phid->TSDCount = 3;
								snprintf(error_buffer,sizeof(error_buffer),"Thermal Shutdown detected.");
								if (phid->phid.fptrError)
									phid->phid.fptrError((CPhidgetHandle)phid, phid->phid.fptrErrorptr, EEPHIDGET_OVERTEMP, error_buffer);
							}
						}
						else
						{
							if(phid->TSDClearCount > 0)
								phid->TSDClearCount--;
							else
								phid->TSDCount=0;
						}
						
						if(!phid->powerGoodEcho && phid->PGoodErrState == PFALSE)
						{
							phid->PGoodErrState = PTRUE;
							snprintf(error_buffer,sizeof(error_buffer),"Bad power supply detected.");
							if (phid->phid.fptrError)
								phid->phid.fptrError((CPhidgetHandle)phid, phid->phid.fptrErrorptr, EEPHIDGET_BADPOWER, error_buffer);
						}

						break;
					case LED64_IN_HIGH_PACKET:
						
						//last 40 LED powers
						for(i=24;i<phid->phid.attr.led.numLEDs;i++)
						{
							double ledPowerTemp;
							ledPowerTemp = ((double)buffer[i-23] / 127.0) * 100.0;
							phid->LED_PowerEcho[i] = round(ledPowerTemp);
						}

						break;
				}
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		case PHIDID_LED_64:
		default:
			return EPHIDGET_UNEXPECTED;
	}

	return EPHIDGET_OK;
}

//eventsAfterOpen - sends out an event for all valid data, used during attach initialization - not used
CPHIDGETINITEVENTS(LED)
	phid = 0;
	return EPHIDGET_OK;
}

//getPacket - used by write thread to get the next packet to send to device
CGETPACKET(LED)
	int i = 0;
	int numLeds = 0;

	CPhidgetLEDHandle phid = (CPhidgetLEDHandle)phidG;

	TESTPTRS(phid, buf)
	TESTPTR(lenp)

	if (*lenp < phid->phid.outputReportByteLength)
		return EPHIDGET_INVALIDARG;

	CThread_mutex_lock(&phid->phid.outputLock);
	
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_LED_64:
			if ((phid->phid.deviceVersion >= 100) && (phid->phid.deviceVersion < 300))
			{

				//construct the packet, with up to 4 LED sets
				for (i = 0; i < phid->phid.attr.led.numLEDs; i++)
				{
					if (phid->changedLED_Power[i] && numLeds < 4) {
						phid->LED_Power[i] = phid->nextLED_Power[i];
						phid->changedLED_Power[i] = PFALSE;
						phid->nextLED_Power[i] = PUNK_INT;
						buf[numLeds*2] = i;
						//0-100 -> 0-63
						buf[numLeds*2+1] = (unsigned char)round((phid->LED_Power[i] / 100.0) * 63.0);
						numLeds++;
						phid->changeRequests--;
					}
				}

				//fill up any remaining buffer space with valid data - sending 0's will mess things up
				for(numLeds=numLeds;numLeds<4;numLeds++)
				{
					buf[numLeds*2] = buf[(numLeds-1)*2];
					buf[numLeds*2+1] = buf[(numLeds-1)*2+1];
				}
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		case PHIDID_LED_64_ADV:
			if ((phid->phid.deviceVersion >= 100) && (phid->phid.deviceVersion < 200))
			{
				//control packet
				if(phid->controlPacketWaiting)
				{

					buf[0] = LED64_CONTROL_PACKET;

					buf[1] = 0;

					switch(phid->currentLimit)
					{
						case PHIDGET_LED_CURRENT_LIMIT_20mA:
							break;
						case PHIDGET_LED_CURRENT_LIMIT_40mA:
							buf[1] |= LED64_CURSELA_FLAG;
							break;
						case PHIDGET_LED_CURRENT_LIMIT_60mA:
							buf[1] |= LED64_CURSELB_FLAG;
							break;
						case PHIDGET_LED_CURRENT_LIMIT_80mA:
							buf[1] |= (LED64_CURSELA_FLAG | LED64_CURSELB_FLAG);
							break;
					}
					
					switch(phid->voltage)
					{
						case PHIDGET_LED_VOLTAGE_1_7V:
							break;
						case PHIDGET_LED_VOLTAGE_2_75V:
							buf[1] |= LED64_PWRSELA_FLAG;
							break;
						case PHIDGET_LED_VOLTAGE_3_9V:
							buf[1] |= LED64_PWRSELB_FLAG;
							break;
						case PHIDGET_LED_VOLTAGE_5_0V:
							buf[1] |= (LED64_PWRSELA_FLAG | LED64_PWRSELB_FLAG);
							break;
					}

					phid->controlPacketWaiting = PFALSE;
				}
				//LED packet
				else
				{
					int bright_packet = PFALSE;
					int output_upper = PFALSE;
					int output_lower = PFALSE;
					//decide if we need to use a normal brightness packet, or if we can use a high efficiency output packet
					for (i = 0; i < phid->phid.attr.led.numLEDs; i++)
					{
						if(phid->changedLED_Power[i])
						{
							if((phid->nextLED_Power[i] != phid->lastLED_Power[i]) && phid->nextLED_Power[i] != 0)
								bright_packet = PTRUE;
							else
							{
								if(i<32)
									output_lower = PTRUE;
								else
									output_upper = PTRUE;
							}
						}
					}

					//only sends brightness changes - not changes between 0 and a brightness
					if(bright_packet)
						{
						//construct the packet, with up to 4 LED sets
						for (i = 0; i < phid->phid.attr.led.numLEDs; i++)
						{
							if (phid->changedLED_Power[i] && numLeds < 4 && phid->nextLED_Power[i] != 0) {
								phid->LED_Power[i] = phid->nextLED_Power[i];
								phid->lastLED_Power[i] = phid->nextLED_Power[i];
								phid->changedLED_Power[i] = PFALSE;
								phid->nextLED_Power[i] = PUNK_INT;
								buf[numLeds*2] = i;
								//0-100 -> 0-127
								buf[numLeds*2+1] = (unsigned char)round((phid->LED_Power[i] / 100.0) * 127.0);
								if(buf[numLeds*2+1])
									buf[numLeds*2+1] |= 0x80; //this turns the LED on when set brightness > 0;
								numLeds++;
								phid->changeRequests--;
							}
						}

						//fill up any remaining buffer space with valid data - sending 0's will mess things up
						//this just replicates data - doesn't send anything
						for(numLeds=numLeds;numLeds<4;numLeds++)
						{
							buf[numLeds*2] = buf[(numLeds-1)*2];
							buf[numLeds*2+1] = buf[(numLeds-1)*2+1];
						}
					}
					else
					{
						//send lower packet
						if((phid->lastOutputPacket == 0 && output_lower) || (phid->lastOutputPacket != 0 && !output_upper))
						{
							buf[0] = LED64_OUTLOW_PACKET;
							for(i = 0;i<32;i++)
							{
								if(phid->changedLED_Power[i])
								{
									phid->changeRequests--;
									phid->LED_Power[i] = phid->nextLED_Power[i];
									phid->changedLED_Power[i] = PFALSE;
									phid->nextLED_Power[i] = PUNK_INT;
								}
								if(phid->LED_Power[i] > 0)
									buf[i/8 + 1] |= (1 << (i%8));
							}
							phid->lastOutputPacket = 1;
						}
						//send upper packet
						else
						{
							buf[0] = LED64_OUTHIGH_PACKET;
							for(i = 32;i<64;i++)
							{
								if(phid->changedLED_Power[i])
								{
									phid->changeRequests--;
									phid->LED_Power[i] = phid->nextLED_Power[i];
									phid->changedLED_Power[i] = PFALSE;
									phid->nextLED_Power[i] = PUNK_INT;
								}
								if(phid->LED_Power[i] > 0)
									buf[i/8 - 3] |= (1 << (i%8));
							}
							phid->lastOutputPacket = 0;
						}
					}
				}
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		default:
			return EPHIDGET_UNEXPECTED;
	}
	
	//if there are still pending sets, signal the event again (which will tell write thread to call this funciton again)
	if(phid->changeRequests)
		CThread_set_event(&phid->phid.writeAvailableEvent);

	*lenp = phid->phid.outputReportByteLength;

	CThread_mutex_unlock(&phid->phid.outputLock);

	return EPHIDGET_OK;
}

//sendpacket - sends a packet to the device asynchronously, blocking if the 1-packet queue is full
//	-every LED has its own 1 state mini-queue
static int CCONV CPhidgetLED_sendpacket(CPhidgetLEDHandle phid,
    unsigned int index, unsigned int power)
{
	int waitReturn;
	CThread_mutex_lock(&phid->phid.writelock);
again:
	if (!CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_ATTACHED_FLAG))
	{
		CThread_mutex_unlock(&phid->phid.writelock);
		return EPHIDGET_NOTATTACHED;
	}
	CThread_mutex_lock(&phid->phid.outputLock);
	//if we have already requested a change on this LED
	if (phid->changedLED_Power[index]) {
		//and it was different then this time
		if (phid->nextLED_Power[index] != power) {
			CThread_mutex_unlock(&phid->phid.outputLock);
			//then wait for it to get written
			waitReturn = CThread_wait_on_event(&phid->phid.writtenEvent, 2500);
			switch(waitReturn)
			{
			case WAIT_OBJECT_0:
				break;
			case WAIT_ABANDONED:
				CThread_mutex_unlock(&phid->phid.writelock);
				return EPHIDGET_UNEXPECTED;
			case WAIT_TIMEOUT:
				CThread_mutex_unlock(&phid->phid.writelock);
				return EPHIDGET_TIMEOUT;
			}
			//and try again
			goto again;
		} else {
			CThread_mutex_unlock(&phid->phid.outputLock);
			CThread_mutex_unlock(&phid->phid.writelock);
			return EPHIDGET_OK;
		}
	//otherwise
	} else {
		//if it's different then current, queue it up
		if (phid->LED_Power[index] != power) {
			phid->changeRequests++;
			phid->changedLED_Power[index] = PTRUE;
			phid->nextLED_Power[index] = power;
			CThread_reset_event(&phid->phid.writtenEvent);
			CThread_mutex_unlock(&phid->phid.outputLock);
			CThread_set_event(&phid->phid.writeAvailableEvent);
		}
		//if it's the same, just return
		else
		{
			CThread_mutex_unlock(&phid->phid.outputLock);
			CThread_mutex_unlock(&phid->phid.writelock);
			return EPHIDGET_OK;
		}
	}
	CThread_mutex_unlock(&phid->phid.writelock);
	return EPHIDGET_OK;
}

// === Exported Functions === //

//create and initialize a device structure
CCREATE(LED, PHIDCLASS_LED)

CGET(LED,LEDCount,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_LED)
	TESTATTACHED

	MASGN(phid.attr.led.numLEDs)
}

CGETINDEX(LED,DiscreteLED,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_LED)
	TESTATTACHED
	TESTINDEX(phid.attr.led.numLEDs)
	TESTMASGN(LED_Power[Index], PUNK_INT)

	MASGN(LED_Power[Index])
}
CSETINDEX(LED,DiscreteLED,int)
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_LED)
	TESTATTACHED
	TESTINDEX(phid.attr.led.numLEDs)
	TESTRANGE(0, 100)

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEYINDEXED(Brightness, "%d", LED_Power);
	else
		return CPhidgetLED_sendpacket(phid, Index, newVal);

	return EPHIDGET_OK;
}

CGET(LED,CurrentLimit,CPhidgetLED_CurrentLimit)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_LED)
	TESTATTACHED

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_LED_64_ADV:
			MASGN(currentLimitEcho)
		case PHIDID_LED_64:
		default:
			return EPHIDGET_UNSUPPORTED;
	}
}
CSET(LED,CurrentLimit,CPhidgetLED_CurrentLimit)
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_LED)
	TESTATTACHED

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_LED_64_ADV:

			TESTRANGE(PHIDGET_LED_CURRENT_LIMIT_20mA, PHIDGET_LED_CURRENT_LIMIT_80mA)

			if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
				ADDNETWORKKEY(CurrentLimit, "%d", currentLimit);
			else
			{
				CThread_mutex_lock(&phid->phid.writelock);
				CThread_mutex_lock(&phid->phid.outputLock);
				phid->currentLimit = newVal;
				phid->controlPacketWaiting = PTRUE;
				CThread_mutex_unlock(&phid->phid.outputLock);
				CThread_set_event(&phid->phid.writeAvailableEvent);
				CThread_mutex_unlock(&phid->phid.writelock);
			}
			break;
		case PHIDID_LED_64:
		default:
			return EPHIDGET_UNSUPPORTED;
	}

	return EPHIDGET_OK;
}

CGET(LED,Voltage,CPhidgetLED_Voltage)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_LED)
	TESTATTACHED

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_LED_64_ADV:
			MASGN(voltageEcho)
		case PHIDID_LED_64:
		default:
			return EPHIDGET_UNSUPPORTED;
	}
}
CSET(LED,Voltage,CPhidgetLED_Voltage)
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_LED)
	TESTATTACHED

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_LED_64_ADV:

			TESTRANGE(PHIDGET_LED_CURRENT_LIMIT_20mA, PHIDGET_LED_CURRENT_LIMIT_80mA)

			if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
				ADDNETWORKKEY(Voltage, "%d", voltage);
			else
			{
				CThread_mutex_lock(&phid->phid.writelock);
				CThread_mutex_lock(&phid->phid.outputLock);
				phid->voltage = newVal;
				phid->controlPacketWaiting = PTRUE;
				CThread_mutex_unlock(&phid->phid.outputLock);
				CThread_set_event(&phid->phid.writeAvailableEvent);
				CThread_mutex_unlock(&phid->phid.writelock);
			}
			break;
		case PHIDID_LED_64:
		default:
			return EPHIDGET_UNSUPPORTED;
	}

	return EPHIDGET_OK;
}

// === Deprecated Functions === //

CGET(LED,NumLEDs,int)
	return CPhidgetLED_getLEDCount(phid, pVal);
}

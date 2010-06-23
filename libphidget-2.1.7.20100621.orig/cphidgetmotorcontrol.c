#include <string.h>
#include <assert.h>
#include "stdafx.h"
#include "cphidgetmotorcontrol.h"
#include "cusb.h"
#include "csocket.h"
#include "cthread.h"

// === Internal Functions === //

//clearVars - sets all device variables to unknown state
CPHIDGETCLEARVARS(MotorControl)
	int i = 0;

	phid->accelerationMax = PUNI_DBL;
	phid->accelerationMin = PUNI_DBL;

	for (i = 0; i<MOTORCONTROL_MAXINPUTS; i++)
	{
		phid->inputState[i] = PUNI_BOOL;
	}
	for (i = 0; i<MOTORCONTROL_MAXMOTORS; i++)
	{
		phid->motorSpeedEcho[i] = PUNI_DBL;
		phid->motorSensedCurrent[i] = PUNI_DBL;
		phid->motorSpeed[i] = PUNK_DBL;
		phid->motorAcceleration[i] = PUNK_DBL;
	}

	return EPHIDGET_OK;
}

//initAfterOpen - sets up the initial state of an object, reading in packets from the device if needed
//				  used during attach initialization - on every attach
CPHIDGETINIT(MotorControl)
	int i = 0;
	int readtries = 0;

	TESTPTR(phid);

	//Setup max/min values
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_MOTORCONTROL_LV_2MOTOR_4INPUT:
		case PHIDID_MOTORCONTROL_HC_2MOTOR:
			if ((phid->phid.deviceVersion >= 100) && (phid->phid.deviceVersion < 200))
			{
				phid->accelerationMax = 100;
				phid->accelerationMin = round_double((1.0 / 10.23), 2);
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		default:
			return EPHIDGET_UNEXPECTED;
	}

	//initialize triggers, set data arrays to unknown
	for (i = 0; i<phid->phid.attr.motorcontrol.numInputs; i++)
	{
		phid->inputState[i] = PUNK_BOOL;
	}
	for (i = 0; i<phid->phid.attr.motorcontrol.numMotors; i++)
	{
		phid->motorSpeedEcho[i] = PUNK_DBL;
		phid->motorSensedCurrent[i] = PUNK_DBL;
	}

	//read in initial state
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_MOTORCONTROL_LV_2MOTOR_4INPUT:
			readtries = 1;
			break;
		case PHIDID_MOTORCONTROL_HC_2MOTOR:
			readtries = phid->phid.attr.motorcontrol.numMotors * 2;
			break;
		default:
			return EPHIDGET_UNEXPECTED;
	}
	while(readtries-- > 0)
	{
		CPhidget_read((CPhidgetHandle)phid);
		for (i = 0; i<phid->phid.attr.motorcontrol.numMotors; i++)
			if(phid->motorSpeedEcho[i] == PUNK_DBL)
				break;
		if(i==phid->phid.attr.motorcontrol.numMotors) break;
	}

	//recover what we can, set others to unknown
	for (i = 0; i<phid->phid.attr.motorcontrol.numMotors; i++)
	{
		phid->motorSpeed[i] = phid->motorSpeedEcho[i];
		phid->motorAcceleration[i] = PUNK_DBL;
	}

	return EPHIDGET_OK;
}

//dataInput - parses device packets
CPHIDGETDATA(MotorControl)
	int i = 0, j = 0;

	if (length < 0) return EPHIDGET_INVALIDARG;
	TESTPTR(phid);
	TESTPTR(buffer);

	switch(phid->phid.deviceIDSpec)
	{
		/* Original Motor controller */
		case PHIDID_MOTORCONTROL_LV_2MOTOR_4INPUT:
			if ((phid->phid.deviceVersion >= 100) && (phid->phid.deviceVersion < 200))
			{
				double speed[MOTORCONTROL_MAXMOTORS];
				double lastSpeed[MOTORCONTROL_MAXMOTORS];
				unsigned char input[MOTORCONTROL_MAXINPUTS];
				unsigned char lastInputState[MOTORCONTROL_MAXINPUTS];
				unsigned char error[MOTORCONTROL_MAXINPUTS];

				ZEROMEM(speed, sizeof(speed));
				ZEROMEM(lastSpeed, sizeof(lastSpeed));
				ZEROMEM(input, sizeof(input));
				ZEROMEM(lastInputState, sizeof(lastInputState));
				ZEROMEM(error, sizeof(error));

				//Parse device packet - store data locally
				for (i = 0, j = 1; i < phid->phid.attr.motorcontrol.numInputs; i++, j<<=1)
				{
					if (buffer[0] & j)
						input[i] = PTRUE;
					else
						input[i] = PFALSE;
				}
				for (i = 0, j = 1; i < phid->phid.attr.motorcontrol.numMotors; i++, j<<=1)
				{
					speed[i] = (char)buffer[4 + i];
					speed[i] = round_double(((speed[i] * 100) / 127.0), 2);

					//errors
					if (buffer[1] & j)
					{
						error[i] = PTRUE;
					}
				}
				
				//Make sure values are within defined range, and store to structure
				for (i = 0; i < phid->phid.attr.motorcontrol.numInputs; i++)
				{
					lastInputState[i] = phid->inputState[i];
					phid->inputState[i] = input[i];
				}
				for (i = 0; i<phid->phid.attr.motorcontrol.numMotors; i++)
				{
					lastSpeed[i] = phid->motorSpeedEcho[i];
					phid->motorSpeedEcho[i] = speed[i];
				}

				//send out any events for changed data
				for (i = 0; i < phid->phid.attr.motorcontrol.numInputs; i++)
				{
					if(phid->inputState[i] != PUNK_BOOL && phid->inputState[i] != lastInputState[i])
						FIRE(InputChange, i, phid->inputState[i]);
				}
				for (i = 0; i<phid->phid.attr.motorcontrol.numMotors; i++)
				{
					if(phid->motorSpeedEcho[i] != PUNK_DBL && phid->motorSpeedEcho[i] != lastSpeed[i])
					{
						FIRE(VelocityChange, i, phid->motorSpeedEcho[i]);
						//Deprecated
						FIRE(MotorChange, i, phid->motorSpeedEcho[i]);
					}
					if(error[i])
					{
						char error_buffer[50];
						snprintf(error_buffer,sizeof(error_buffer),"Motor %d exceeded 1.5 Amp current limit.", i);
						if (phid->phid.fptrError)
							phid->phid.fptrError((CPhidgetHandle)phid, phid->phid.fptrErrorptr, EEPHIDGET_OVERCURRENT, error_buffer);
					}
				}

			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		/* HC motor controller - packets are indexed */
		case PHIDID_MOTORCONTROL_HC_2MOTOR:
			if ((phid->phid.deviceVersion >= 100) && (phid->phid.deviceVersion < 200))
			{
				int index = 0;
				int error[MOTORCONTROL_MAXINPUTS];
				double speed = 0, current = 0;
				double lastSpeed = 0, lastCurrent = 0;

				ZEROMEM(error, sizeof(error));
				
				//Parse device packet - store data locally
				index = buffer[3];

				speed = (signed char)buffer[4];
				speed = round_double(((speed * 100) / 127.0), 2);

				//NOTE: current sense is only accurate at +100 and -100 velocity
				current = (unsigned int)(((unsigned char)buffer[6] << 8) | (unsigned char)buffer[7]);
				current -= 5;
				if(current < 0) current = 0;
				current /= 51.2; //volts
				current = (current * 11370) / 1500; //amps

				if (!(buffer[1] & 0x10)) error[0] |= 0x01;
				if (!(buffer[1] & 0x20)) error[1] |= 0x01;
				if (!(buffer[1] & 0x40)) error[0] |= 0x02;
				if (!(buffer[1] & 0x80)) error[1] |= 0x02;

				//Make sure values are within defined range, and store to structure
				lastSpeed = phid->motorSpeedEcho[index];
				phid->motorSpeedEcho[index] = speed;
				lastCurrent = phid->motorSensedCurrent[index];
				phid->motorSensedCurrent[index] = current;
				
				//send out any events for changed data
				if(phid->motorSpeedEcho[index] != PUNK_DBL && phid->motorSpeedEcho[index] != lastSpeed)
				{
					FIRE(VelocityChange, index, phid->motorSpeedEcho[index]);
					//Deprecated
					FIRE(MotorChange, index, phid->motorSpeedEcho[index]);
				}
				if(phid->motorSensedCurrent[index] != PUNK_DBL && phid->motorSensedCurrent[index] != lastCurrent)
					FIRE(CurrentChange, index, phid->motorSensedCurrent[index]);
				for (i = 0; i<phid->phid.attr.motorcontrol.numMotors; i++)
				{
					char error_buffer[50];
					//There are two error conditions - but as far as I can tell they both mean the same thing
					if(error[i])
					{
						snprintf(error_buffer,sizeof(error_buffer),"Motor %d overtemperature or short detected.", i);
						if (phid->phid.fptrError)
							phid->phid.fptrError((CPhidgetHandle)phid, phid->phid.fptrErrorptr, EEPHIDGET_OVERTEMP, error_buffer);
					}
				}
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		default:
			return EPHIDGET_UNEXPECTED;

	}

	return EPHIDGET_OK;
}

//eventsAfterOpen - sends out an event for all valid data, used during attach initialization
CPHIDGETINITEVENTS(MotorControl)
	int i = 0;

	for (i = 0; i<phid->phid.attr.motorcontrol.numInputs; i++)
	{
		if(phid->inputState[i] != PUNK_BOOL)
			FIRE(InputChange, i, phid->inputState[i]);
	}
	for (i = 0; i<phid->phid.attr.motorcontrol.numMotors; i++)
	{
		if(phid->motorSpeedEcho[i] != PUNK_DBL)
		{
			FIRE(VelocityChange, i, phid->motorSpeedEcho[i]);
			//Deprecated
			FIRE(MotorChange, i, phid->motorSpeedEcho[i]);
		}
		if(phid->motorSensedCurrent[i] != PUNK_DBL)
			FIRE(CurrentChange, i, phid->motorSensedCurrent[i]);
	}

	return EPHIDGET_OK;
}

//getPacket - used by write thread to get the next packet to send to device
CGETPACKET_BUF(MotorControl)

//sendpacket - sends a packet to the device asynchronously, blocking if the 1-packet queue is full
CSENDPACKET_BUF(MotorControl)

//makePacket - constructs a packet using current device state
CMAKEPACKETINDEXED(MotorControl)
	int velocity = 0, accel = 0;

	TESTPTRS(phid, buffer);

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_MOTORCONTROL_LV_2MOTOR_4INPUT:
		case PHIDID_MOTORCONTROL_HC_2MOTOR:
			if ((phid->phid.deviceVersion >= 100) && (phid->phid.deviceVersion < 200))
			{
				//have to make sure that everything to be sent has some sort of default value if the user hasn't set a value
				if(phid->motorSpeed[Index] == PUNK_DBL)
					phid->motorSpeed[Index] = 0; //not moving
				if(phid->motorAcceleration[Index] == PUNK_DBL)
					phid->motorAcceleration[Index] = phid->accelerationMax / 2; //mid-range

				velocity = (int)round((phid->motorSpeed[Index] * 127.0) / 100.0);
				accel = (int)round(phid->motorAcceleration[Index] * 10.23);

				buffer[0] = (unsigned char)Index;
				buffer[1] = (unsigned char)(velocity & 0xff);
				buffer[2] = (unsigned char)((accel >> 8) & 0x0f);
				buffer[3] = (unsigned char)(accel & 0xff);
			}
			else
				return EPHIDGET_UNEXPECTED;
			break;
		default:
			return EPHIDGET_UNEXPECTED;
	}

	return EPHIDGET_OK;
}

// === Exported Functions === //

//create and initialize a device structure
CCREATE(MotorControl, PHIDCLASS_MOTORCONTROL)

//event setup functions
CFHANDLE(MotorControl, InputChange, int, int)
CFHANDLE(MotorControl, VelocityChange, int, double)
CFHANDLE(MotorControl, CurrentChange, int, double)

CGET(MotorControl,MotorCount,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_MOTORCONTROL)
	TESTATTACHED

	MASGN(phid.attr.motorcontrol.numMotors)
}

CGETINDEX(MotorControl,Velocity,double)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_MOTORCONTROL)
	TESTATTACHED
	TESTINDEX(phid.attr.motorcontrol.numMotors)
	TESTMASGN(motorSpeedEcho[Index], PUNK_DBL)

	MASGN(motorSpeedEcho[Index])
}
CSETINDEX(MotorControl,Velocity,double)
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_MOTORCONTROL)
	TESTATTACHED
	TESTINDEX(phid.attr.motorcontrol.numMotors)
	TESTRANGE(-100, 100)

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEYINDEXED(Velocity, "%lE", motorSpeed);
	else
		SENDPACKETINDEXED(MotorControl, motorSpeed[Index], Index);

	return EPHIDGET_OK;
}

CGETINDEX(MotorControl,Acceleration,double)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_MOTORCONTROL)
	TESTATTACHED
	TESTINDEX(phid.attr.motorcontrol.numMotors)
	TESTMASGN(motorAcceleration[Index], PUNK_DBL)

	MASGN(motorAcceleration[Index])
}
CSETINDEX(MotorControl,Acceleration,double)
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_MOTORCONTROL)
	TESTATTACHED
	TESTINDEX(phid.attr.motorcontrol.numMotors)
	TESTRANGE(phid->accelerationMin, phid->accelerationMax)

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEYINDEXED(Acceleration, "%lE", motorAcceleration);
	else
		SENDPACKETINDEXED(MotorControl, motorAcceleration[Index], Index);

	return EPHIDGET_OK;
}

CGETINDEX(MotorControl,AccelerationMax,double)
	TESTPTRS(phid,pVal) 	
	TESTDEVICETYPE(PHIDCLASS_MOTORCONTROL)
	TESTATTACHED
	TESTINDEX(phid.attr.motorcontrol.numMotors)
	TESTMASGN(accelerationMax, PUNK_DBL)

	MASGN(accelerationMax)
}

CGETINDEX(MotorControl,AccelerationMin,double)
	TESTPTRS(phid,pVal) 	
	TESTDEVICETYPE(PHIDCLASS_MOTORCONTROL)
	TESTATTACHED
	TESTINDEX(phid.attr.motorcontrol.numMotors)
	TESTMASGN(accelerationMin, PUNK_DBL)

	MASGN(accelerationMin)
}

CGETINDEX(MotorControl,Current,double)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_MOTORCONTROL)
	TESTATTACHED

	//Only supported on HC
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_MOTORCONTROL_LV_2MOTOR_4INPUT:
			return EPHIDGET_UNSUPPORTED;
		case PHIDID_MOTORCONTROL_HC_2MOTOR:
			TESTINDEX(phid.attr.motorcontrol.numMotors)
			TESTMASGN(motorSensedCurrent[Index], PUNK_DBL)
			MASGN(motorSensedCurrent[Index])
		default:
			return EPHIDGET_UNEXPECTED;
	}
}

CGET(MotorControl,InputCount,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_MOTORCONTROL)
	TESTATTACHED

	MASGN(phid.attr.motorcontrol.numInputs)
}

CGETINDEX(MotorControl,InputState,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_MOTORCONTROL)
	TESTATTACHED
	TESTINDEX(phid.attr.motorcontrol.numInputs)
	TESTMASGN(inputState[Index], PUNK_BOOL)

	MASGN(inputState[Index])
}

// === Deprecated Functions === //

CFHANDLE(MotorControl, MotorChange, int, double)
CGETINDEX(MotorControl,MotorSpeed,double)
	return CPhidgetMotorControl_getVelocity(phid, Index, pVal);
}
CSETINDEX(MotorControl,MotorSpeed,double)
	return CPhidgetMotorControl_setVelocity(phid, Index, newVal);
}
CGET(MotorControl,NumMotors,int)
	return CPhidgetMotorControl_getMotorCount(phid, pVal);
}
CGET(MotorControl,NumInputs,int)
	return CPhidgetMotorControl_getInputCount(phid, pVal);
}

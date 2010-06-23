#include "stdafx.h"
#include "csocket.h"
#include "csocketevents.h"
#include "utils/utils.h"
#include "math.h"

#include "cphidget.h"
#include "cphidgetdictionary.h"
#include "csocket.h"
#include "clog.h"
#include "cphidgetaccelerometer.h"
#include "cphidgetadvancedservo.h"
#include "cphidgetencoder.h"
#include "cphidgetinterfacekit.h"
#include "cphidgetir.h"
#include "cphidgetmanager.h"
#include "cphidgetled.h"
#include "cphidgetmotorcontrol.h"
#include "cphidgetphsensor.h"
#include "cphidgetrfid.h"
#include "cphidgetservo.h"
#include "cphidgetspatial.h"
#include "cphidgetstepper.h"
#include "cphidgettemperaturesensor.h"
#include "cphidgettextlcd.h"
#include "cphidgettextled.h"
#include "cphidgetweightsensor.h"
#include "cphidgetgps.h"
#include "cphidgetgeneric.h"

regex_t phidgetsetex;
regex_t managerex;
regex_t managervalex;

static int hexval(unsigned char c)
{
	if (isdigit(c))
		return c - '0';
	c = tolower(c);
	if (c <= 'f')
		return c - 'a' + 10;
	return 0;
}

int phidget_accelerometer_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetAccelerometerHandle phid = (CPhidgetAccelerometerHandle)generic_phid;
	if(!strncmp(setThing, "NumberOfAxes", sizeof("NumberOfAxes")))
	{
		int value = strtol(state, NULL, 10);
		phid->phid.attr.accelerometer.numAxis = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "Acceleration", sizeof("Acceleration")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.accelerometer.numAxis?
			phid->phid.attr.accelerometer.numAxis:ACCEL_MAXAXES)
		{
			INC_KEYCOUNT(axis[index], PUNI_DBL)
			phid->axis[index] = value;
			if(value != PUNK_DBL)
				FIRE(AccelerationChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "AccelerationMin", sizeof("AccelerationMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(accelerationMin, PUNI_DBL)
		phid->accelerationMin = value;
	}
	else if(!strncmp(setThing, "AccelerationMax", sizeof("AccelerationMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(accelerationMax, PUNI_DBL)
		phid->accelerationMax = value;
	}
	else if(!strncmp(setThing, "Trigger", sizeof("Trigger")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.accelerometer.numAxis?
			phid->phid.attr.accelerometer.numAxis:ACCEL_MAXAXES)
		{
			INC_KEYCOUNT(axisChangeTrigger[index], PUNI_DBL)
			phid->axisChangeTrigger[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for Accelerometer: %s", setThing);
	}
	return ret;
}

int phidget_advancedservo_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetAdvancedServoHandle phid = (CPhidgetAdvancedServoHandle)generic_phid;
	if(!strncmp(setThing, "NumberOfMotors", sizeof("NumberOfMotors")))
	{
		int value = strtol(state, NULL, 10);
		phid->phid.attr.advancedservo.numMotors = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "Position", sizeof("Position")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.advancedservo.numMotors?
			phid->phid.attr.advancedservo.numMotors:ADVSERVO_MAXSERVOS)
		{
			INC_KEYCOUNT(motorPositionEcho[index], PUNI_DBL)
			phid->motorPositionEcho[index] = value;
			if(value != PUNK_DBL)
				FIRE(PositionChange, index, servo_us_to_degrees(phid->servoParams[index], value, PTRUE));
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "PositionMin", sizeof("PositionMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(motorPositionMin[index], PUNI_DBL)
		phid->motorPositionMin[index] = value;
	}
	else if(!strncmp(setThing, "PositionMax", sizeof("PositionMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(motorPositionMax[index], PUNI_DBL)
		phid->motorPositionMax[index] = value;
	}
	else if(!strncmp(setThing, "PositionMinLimit", sizeof("PositionMinLimit")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(motorPositionMinLimit, PUNI_DBL)
		phid->motorPositionMinLimit = value;
	}
	else if(!strncmp(setThing, "PositionMaxLimit", sizeof("PositionMaxLimit")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(motorPositionMaxLimit, PUNI_DBL)
		phid->motorPositionMaxLimit = value;
	}
	// initial acceleration limit always unknown so don't keyCount++
	else if(!strncmp(setThing, "Acceleration", sizeof("Acceleration")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.advancedservo.numMotors?
			phid->phid.attr.advancedservo.numMotors:ADVSERVO_MAXSERVOS)
		{
			phid->motorAcceleration[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "AccelerationMax", sizeof("AccelerationMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(accelerationMax, PUNI_DBL)
		phid->accelerationMax = value;
	}
	else if(!strncmp(setThing, "AccelerationMin", sizeof("AccelerationMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(accelerationMin, PUNI_DBL)
		phid->accelerationMin = value;
	}
	else if(!strncmp(setThing, "Current", sizeof("Current")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.advancedservo.numMotors?
			phid->phid.attr.advancedservo.numMotors:ADVSERVO_MAXSERVOS)
		{
			INC_KEYCOUNT(motorSensedCurrent[index], PUNI_DBL)
			phid->motorSensedCurrent[index] = value;
			if(value != PUNK_DBL)
				FIRE(CurrentChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	// initial velocity limit always unknown so don't keyCount++
	else if(!strncmp(setThing, "VelocityLimit", sizeof("VelocityLimit")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.advancedservo.numMotors?
			phid->phid.attr.advancedservo.numMotors:ADVSERVO_MAXSERVOS)
		{
			phid->motorVelocity[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Velocity", sizeof("Velocity")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.advancedservo.numMotors?
			phid->phid.attr.advancedservo.numMotors:ADVSERVO_MAXSERVOS)
		{
			INC_KEYCOUNT(motorVelocityEcho[index], PUNI_DBL)
			phid->motorVelocityEcho[index] = value;
			if(value != PUNK_DBL)
				FIRE(VelocityChange, index, servo_us_to_degrees_vel(phid->servoParams[index], value, PTRUE));
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "VelocityMax", sizeof("VelocityMax")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.advancedservo.numMotors?
			phid->phid.attr.advancedservo.numMotors:ADVSERVO_MAXSERVOS)
		{
			INC_KEYCOUNT(velocityMax[index], PUNI_DBL)
			phid->velocityMax[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "VelocityMaxLimit", sizeof("VelocityMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(velocityMaxLimit, PUNI_DBL)
		phid->velocityMaxLimit = value;
	}
	else if(!strncmp(setThing, "VelocityMin", sizeof("VelocityMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(velocityMin, PUNI_DBL)
		phid->velocityMin = value;
	}
	else if(!strncmp(setThing, "Engaged", sizeof("Engaged")))
	{
		int value = strtol(state, NULL, 10);
		if(index < phid->phid.attr.advancedservo.numMotors?
			phid->phid.attr.advancedservo.numMotors:ADVSERVO_MAXSERVOS)
		{
			INC_KEYCOUNT(motorEngagedStateEcho[index], PUNI_BOOL)
			phid->motorEngagedStateEcho[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "SpeedRampingOn", sizeof("SpeedRampingOn")))
	{
		int value = strtol(state, NULL, 10);
		if(index < phid->phid.attr.advancedservo.numMotors?
			phid->phid.attr.advancedservo.numMotors:ADVSERVO_MAXSERVOS)
		{
			INC_KEYCOUNT(motorSpeedRampingStateEcho[index], PUNI_BOOL)
			phid->motorSpeedRampingStateEcho[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Stopped", sizeof("Stopped")))
	{
		int value = strtol(state, NULL, 10);
		if(index < phid->phid.attr.advancedservo.numMotors?
			phid->phid.attr.advancedservo.numMotors:ADVSERVO_MAXSERVOS)
		{
			unsigned char lastStoppedState = phid->motorStoppedState[index];
			INC_KEYCOUNT(motorStoppedState[index], PUNI_BOOL)
			phid->motorStoppedState[index] = value;
			//If changed, re-run position/velocity events, so the stopped change will be noticed
			if(lastStoppedState != value)
			{
				if(phid->motorVelocityEcho[index] != PUNK_DBL)
					FIRE(VelocityChange, index, servo_us_to_degrees_vel(phid->servoParams[index], phid->motorVelocityEcho[index], PTRUE));
				if(phid->motorPositionEcho[index] != PUNK_DBL)
					FIRE(PositionChange, index, servo_us_to_degrees(phid->servoParams[index], phid->motorPositionEcho[index], PTRUE));
			}
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "ServoParameters", sizeof("ServoParameters")))
	{
		if(index < phid->phid.attr.advancedservo.numMotors?
			phid->phid.attr.advancedservo.numMotors:ADVSERVO_MAXSERVOS)
		{
			CPhidgetServoParameters params;
			char *endptr;
			params.servoType = strtol(state, &endptr, 10);
			params.min_us = strtod(endptr+1, &endptr);
			params.max_us = strtod(endptr+1, &endptr);
			params.us_per_degree = strtod(endptr+1, &endptr);
			params.max_us_per_s = strtod(endptr+1, NULL);
			params.state = PTRUE;

			INC_KEYCOUNT(servoParams[index].state, PUNI_BOOL)

			phid->servoParams[index] = params;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for Advanced Servo: %s", setThing);
	}
	return ret;
}

int phidget_encoder_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	char *endPtr;
	CPhidgetEncoderHandle phid = (CPhidgetEncoderHandle)generic_phid;
	int value = strtol(state, &endPtr, 10);
	if(!strncmp(setThing, "NumberOfEncoders", sizeof("NumberOfEncoders")))
	{
		phid->phid.attr.encoder.numEncoders = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "NumberOfInputs", sizeof("NumberOfInputs")))
	{
		phid->phid.attr.encoder.numInputs = value;
		phid->phid.keyCount++;
	}
	// initial Input is unknown so don't keyCount++
	else if(!strncmp(setThing, "Input", sizeof("Input")))
	{
		if(index < phid->phid.attr.encoder.numInputs?
			phid->phid.attr.encoder.numInputs:ENCODER_MAXINPUTS)
		{
			phid->inputState[index] = value;
			if(value != PUNK_BOOL)
				FIRE(InputChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	// initial Position is unknown so don't keyCount++
	else if(!strncmp(setThing, "ResetPosition", sizeof("ResetPosition")))
	{
		if(index < phid->phid.attr.encoder.numEncoders?
			phid->phid.attr.encoder.numEncoders:ENCODER_MAXENCODERS)
		{
			phid->encoderPosition[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Position", sizeof("Position")))
	{
		if(index < phid->phid.attr.encoder.numEncoders?
			phid->phid.attr.encoder.numEncoders:ENCODER_MAXENCODERS)
		{
			int posnchange = strtol(endPtr+1, &endPtr, 10);
			int posn = strtol(endPtr+1, &endPtr, 10);
			
			phid->encoderPosition[index] = posn;
			
			FIRE(PositionChange, index, (unsigned short)value, posnchange);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "IndexPosition", sizeof("IndexPosition")))
	{
		if(index < phid->phid.attr.encoder.numEncoders?
			phid->phid.attr.encoder.numEncoders:ENCODER_MAXENCODERS)
		{
			phid->indexPosition[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Enabled", sizeof("Enabled")))
	{
		if(index < phid->phid.attr.encoder.numEncoders?
			phid->phid.attr.encoder.numEncoders:ENCODER_MAXENCODERS)
		{
			INC_KEYCOUNT(enableStateEcho[index], PUNI_BOOL)
			phid->enableStateEcho[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for Encoder: %s", setThing);
	}
	return ret;
}

int phidget_generic_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	//CPhidgetGenericHandle phid = (CPhidgetGenericHandle)generic_phid;
	return ret;
}

int phidget_gps_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetGPSHandle phid = (CPhidgetGPSHandle)generic_phid;
	if(!strncmp(setThing, "Data", sizeof("Data")))
	{
		//TODO: Implement this
		//parse_GPS_data(phid, state);
	}
	else if(!strncmp(setThing, "Trigger", sizeof("Trigger")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(PositionChangeTrigger, PUNI_DBL)
		phid->PositionChangeTrigger = value * 0.0000899928055396;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for GPS: %s", setThing);
	}
	return ret;
}

int phidget_interfacekit_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetInterfaceKitHandle phid = (CPhidgetInterfaceKitHandle)generic_phid;
	int value = strtol(state, NULL, 10);
	if(!strncmp(setThing, "NumberOfSensors", sizeof("NumberOfSensors")))
	{
		phid->phid.attr.ifkit.numSensors = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "NumberOfInputs", sizeof("NumberOfInputs")))
	{
		phid->phid.attr.ifkit.numInputs = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "NumberOfOutputs", sizeof("NumberOfOutputs")))
	{
		phid->phid.attr.ifkit.numOutputs = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "Input", sizeof("Input")))
	{
		if(index < phid->phid.attr.ifkit.numInputs?
			phid->phid.attr.ifkit.numInputs:IFKIT_MAXINPUTS)
		{
			INC_KEYCOUNT(physicalState[index], PUNI_BOOL)
			phid->physicalState[index] = value;
			if(value != PUNK_BOOL)
				FIRE(InputChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Sensor", sizeof("Sensor")))
	{
		if(index < phid->phid.attr.ifkit.numSensors?
			phid->phid.attr.ifkit.numSensors:IFKIT_MAXSENSORS)
		{
			INC_KEYCOUNT(sensorValue[index], PUNI_INT)
			phid->sensorValue[index] = value;
			if(value != PUNK_INT)
				FIRE(SensorChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "RawSensor", sizeof("RawSensor")))
	{
		if(index < phid->phid.attr.ifkit.numSensors?
			phid->phid.attr.ifkit.numSensors:IFKIT_MAXSENSORS)
		{
			INC_KEYCOUNT(sensorRawValue[index], PUNI_INT)
			phid->sensorRawValue[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Output", sizeof("Output")))
	{
		if(index < phid->phid.attr.ifkit.numOutputs?
			phid->phid.attr.ifkit.numOutputs:IFKIT_MAXOUTPUTS)
		{
			INC_KEYCOUNT(outputEchoStates[index], PUNI_BOOL)
			phid->outputEchoStates[index] = value;
			if(value != PUNK_BOOL)
				FIRE(OutputChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Trigger", sizeof("Trigger")))
	{
		if(index < phid->phid.attr.ifkit.numSensors?
			phid->phid.attr.ifkit.numSensors:IFKIT_MAXSENSORS)
		{
			INC_KEYCOUNT(sensorChangeTrigger[index], PUNI_INT)
			phid->sensorChangeTrigger[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "DataRate", sizeof("DataRate")))
	{
		if(index < phid->phid.attr.ifkit.numSensors?
			phid->phid.attr.ifkit.numSensors:IFKIT_MAXSENSORS)
		{
			INC_KEYCOUNT(dataRate[index], PUNI_INT)
			phid->dataRate[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "DataRateMin", sizeof("DataRateMin")))
	{
		INC_KEYCOUNT(dataRateMin, PUNI_INT)
		phid->dataRateMin = value;
	}
	else if(!strncmp(setThing, "DataRateMax", sizeof("DataRateMax")))
	{
		INC_KEYCOUNT(dataRateMax, PUNI_INT)
		phid->dataRateMax = value;
	}
	else if(!strncmp(setThing, "InterruptRate", sizeof("InterruptRate")))
	{
		INC_KEYCOUNT(interruptRate, PUNI_INT)
		phid->interruptRate = value;
	}
	else if(!strncmp(setThing, "Ratiometric", sizeof("Ratiometric")))
	{
		INC_KEYCOUNT(ratiometric, PUNI_BOOL)
		phid->ratiometric = value;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for InterfaceKit: %s", setThing);
	}
	return ret;
}

int phidget_ir_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetIRHandle phid = (CPhidgetIRHandle)generic_phid;
	if(!strncmp(setThing, "Code", sizeof("Code")))
	{
		unsigned char data[IR_MAX_CODE_DATA_LENGTH];
		int bitCount, repeat, length = IR_MAX_CODE_DATA_LENGTH;
		char *endPtr;

		//this will stop at the first ','
		stringToByteArray((char *)state, data, &length);
		bitCount = strtol(state+length*2+1, &endPtr, 10);
		repeat = strtol(endPtr+1, &endPtr, 10);

		//send out the code event!
		FIRE(Code, data, length, bitCount, repeat);

		//store to last code
		ZEROMEM(phid->lastCode, sizeof(phid->lastCode));
		memcpy(phid->lastCode, data, length);
		phid->lastCodeInfo.bitCount = bitCount;
		phid->lastRepeat = repeat;
		phid->lastCodeKnown = PTRUE;
	}
	else if(!strncmp(setThing, "Learn", sizeof("Learn")))
	{
		unsigned char data[IR_MAX_CODE_DATA_LENGTH];
		CPhidgetIR_CodeInfo codeInfo;
		int length=IR_MAX_CODE_DATA_LENGTH;

		stringToCodeInfo((char *)state, &codeInfo);
		stringToByteArray((char *)(state+sizeof(CPhidgetIR_CodeInfo)*2), data, &length);

		//send the event
		FIRE(Learn, data, length, &codeInfo);

		//store to last code
		ZEROMEM(phid->lastLearnedCode, sizeof(phid->lastLearnedCode));
		memcpy(phid->lastLearnedCode, data, length);
		phid->lastLearnedCodeInfo = codeInfo;
		phid->lastLearnedCodeKnown = PTRUE;
	}
	else if(!strncmp(setThing, "RawData", sizeof("RawData")))
	{
		//TODO: 
		//what about multiple clients?
		// -if a faster client is getting all the data and acking it, then the slower client will end up missing chunks...
		// -we are assuming that data comes in in the same order as it is sent, so we don't deal with out of order keys

		//only respond to new raw data
		if(reason != PDR_CURRENT_VALUE)
		{
			int i;
			int data[IR_MAX_CODE_DATA_LENGTH];
			char key[1024], val[1024];
			int rawDataSendCnt, length = IR_MAX_CODE_DATA_LENGTH;
			char *endPtr;

			//this will stop at the first ','
			stringToWordArray((char *)state, data, &length);
			rawDataSendCnt = strtol(state+length*5+1, &endPtr, 10);

			//send an ACK for this count
			CThread_mutex_lock(&phid->phid.lock);
			snprintf(key, sizeof(key), "/PCK/%s/%d/RawDataAck/%d", phid->phid.deviceType, phid->phid.serialNumber, index);
			snprintf(val, sizeof(val), "%d", rawDataSendCnt);
			pdc_async_set(phid->phid.networkInfo->server->pdcs, key, val, (int)strlen(val), PFALSE, internal_async_network_error_handler, &phid->phid);
			CThread_mutex_unlock(&phid->phid.lock);

			//see if we lost a packet
			if(phid->rawDataSendWSCounter != PUNK_INT && phid->rawDataSendWSCounter + 1 != rawDataSendCnt)
			{
				char error_buffer[127];
				snprintf(error_buffer,sizeof(error_buffer),"A piece on PhidgetIR Raw Data was lost. Be careful if decoding RawData manually.");
				if (phid->phid.fptrError)
					phid->phid.fptrError((CPhidgetHandle)phid, phid->phid.fptrErrorptr, EEPHIDGET_PACKETLOST, error_buffer);

				//reset data pointers
				phid->dataWritePtr = 0;
				phid->userReadPtr = 0;
			}
			phid->rawDataSendWSCounter = rawDataSendCnt;

			//send the event
			FIRE(RawData, data, length);

			//TODO: store the raw data array
			for(i=0;i<length;i++)
			{
				phid->dataBuffer[phid->dataWritePtr] = data[i];

				phid->dataWritePtr++;
				phid->dataWritePtr &= IR_DATA_ARRAY_MASK;
				//if we run into data that hasn't been read... too bad, we overwrite it and adjust the read pointer
				if(phid->dataWritePtr == phid->userReadPtr)
				{
					phid->userReadPtr++;
					phid->userReadPtr &= IR_DATA_ARRAY_MASK;
				}
			}

		}
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for IR: %s", setThing);
	}
	return ret;
}

int phidget_led_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetLEDHandle phid = (CPhidgetLEDHandle)generic_phid;
	int value = strtol(state, NULL, 10);
	if(!strncmp(setThing, "NumberOfLEDs", sizeof("NumberOfLEDs")))
	{
		phid->phid.attr.led.numLEDs = value;
		phid->phid.keyCount++;
	}
	// initial brightness is unknown so don't keyCount++
	else if(!strncmp(setThing, "Brightness", sizeof("Brightness")))
	{
		INC_KEYCOUNT(LED_Power[index], PUNI_INT)
		if(index < phid->phid.attr.led.numLEDs?
			phid->phid.attr.led.numLEDs:LED_MAXLEDS)
		{
			phid->LED_Power[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Voltage", sizeof("Voltage")))
	{
		INC_KEYCOUNT(voltageEcho, -1)
		phid->voltageEcho = value;
	}
	else if(!strncmp(setThing, "CurrentLimit", sizeof("CurrentLimit")))
	{
		INC_KEYCOUNT(currentLimitEcho, -1)
		phid->currentLimitEcho = value;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for LED: %s", setThing);
	}
	return ret;
}

int phidget_motorcontrol_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetMotorControlHandle phid = (CPhidgetMotorControlHandle)generic_phid;
	if(!strncmp(setThing, "NumberOfMotors", sizeof("NumberOfMotors")))
	{
		int value = strtol(state, NULL, 10);
		phid->phid.attr.motorcontrol.numMotors = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "NumberOfInputs", sizeof("NumberOfInputs")))
	{
		int value = strtol(state, NULL, 10);
		phid->phid.attr.motorcontrol.numInputs = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "Input", sizeof("Input")))
	{
		int value = strtol(state, NULL, 10);
		if(index < phid->phid.attr.motorcontrol.numInputs?
			phid->phid.attr.motorcontrol.numInputs:MOTORCONTROL_MAXINPUTS)
		{
			INC_KEYCOUNT(inputState[index], PUNI_BOOL)
			phid->inputState[index] = value;
			if(value != PUNK_BOOL)
				FIRE(InputChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Velocity", sizeof("Velocity")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.motorcontrol.numMotors?
			phid->phid.attr.motorcontrol.numMotors:MOTORCONTROL_MAXMOTORS)
		{
			INC_KEYCOUNT(motorSpeedEcho[index], PUNI_DBL)
			phid->motorSpeedEcho[index] = value;
			if(value != PUNK_DBL)
				FIRE(VelocityChange, index, value);
			//Deprecated
			if(value != PUNK_DBL)
				FIRE(MotorChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Current", sizeof("Current")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.motorcontrol.numMotors?
			phid->phid.attr.motorcontrol.numMotors:MOTORCONTROL_MAXMOTORS)
		{
			INC_KEYCOUNT(motorSensedCurrent[index], PUNI_DBL)
			phid->motorSensedCurrent[index] = value;
			if(value != PUNK_DBL)
				FIRE(CurrentChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	// initial acceleration is unknown so don't keyCount++
	else if(!strncmp(setThing, "Acceleration", sizeof("Acceleration")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.motorcontrol.numMotors?
			phid->phid.attr.motorcontrol.numMotors:MOTORCONTROL_MAXMOTORS)
		{
			phid->motorAcceleration[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "AccelerationMin", sizeof("AccelerationMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(accelerationMin, PUNI_DBL)
		phid->accelerationMin = value;
	}
	else if(!strncmp(setThing, "AccelerationMax", sizeof("AccelerationMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(accelerationMax, PUNI_DBL)
		phid->accelerationMax = value;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for MotorControl: %s", setThing);
	}
	return ret;
}

int phidget_phsensor_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetPHSensorHandle phid = (CPhidgetPHSensorHandle)generic_phid;
	double value = strtod(state, NULL);
	if(!strncmp(setThing, "PH", sizeof("PH")))
	{
		INC_KEYCOUNT(PH, PUNI_DBL)
		phid->PH = value;
		if(value != PUNK_DBL)
			FIRE(PHChange, value);
	}
	else if(!strncmp(setThing, "PHMin", sizeof("PHMin")))
	{
		INC_KEYCOUNT(phMin, PUNI_DBL)
		phid->phMin = value;
	}
	else if(!strncmp(setThing, "PHMax", sizeof("PHMax")))
	{
		INC_KEYCOUNT(phMax, PUNI_DBL)
		phid->phMax = value;
	}
	else if(!strncmp(setThing, "Trigger", sizeof("Trigger")))
	{
		INC_KEYCOUNT(PHChangeTrigger, PUNI_DBL)
		phid->PHChangeTrigger = value;
	}
	else if(!strncmp(setThing, "Potential", sizeof("Potential")))
	{
		INC_KEYCOUNT(Potential, PUNI_DBL)
		phid->Potential = value;
	}
	else if(!strncmp(setThing, "PotentialMin", sizeof("PotentialMin")))
	{
		INC_KEYCOUNT(potentialMin, PUNI_DBL)
		phid->potentialMin = value;
	}
	else if(!strncmp(setThing, "PotentialMax", sizeof("PotentialMax")))
	{
		INC_KEYCOUNT(potentialMax, PUNI_DBL)
		phid->potentialMax = value;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for PHSensor: %s", setThing);
	}
	return ret;
}

int phidget_rfid_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetRFIDHandle phid = (CPhidgetRFIDHandle)generic_phid;
	int value = strtol(state, NULL, 10);
	if(!strncmp(setThing, "NumberOfOutputs", sizeof("NumberOfOutputs")))
	{
		phid->phid.attr.rfid.numOutputs = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "LastTag", sizeof("LastTag")))
	{
		unsigned char tagData[5];
		INC_KEYCOUNT(lastTagValid, PUNI_BOOL)
		phid->lastTagValid = PTRUE;
		
		tagData[0] = (hexval(state[0])<<4)|hexval(state[1]);
		tagData[1] = (hexval(state[2])<<4)|hexval(state[3]);
		tagData[2] = (hexval(state[4])<<4)|hexval(state[5]);
		tagData[3] = (hexval(state[6])<<4)|hexval(state[7]);
		tagData[4] = (hexval(state[8])<<4)|hexval(state[9]);
		
		memcpy(phid->lastTag, tagData, 5);
	}
	else if(!strncmp(setThing, "Tag", sizeof("Tag")))
	{
		unsigned char tagData[5];
		//always increment on tagPresent==PUNI_BOOL before setting it!
		INC_KEYCOUNT(tagPresent, PUNI_BOOL)
		phid->tagPresent = 1;
		
		tagData[0] = (hexval(state[0])<<4)|hexval(state[1]);
		tagData[1] = (hexval(state[2])<<4)|hexval(state[3]);
		tagData[2] = (hexval(state[4])<<4)|hexval(state[5]);
		tagData[3] = (hexval(state[6])<<4)|hexval(state[7]);
		tagData[4] = (hexval(state[8])<<4)|hexval(state[9]);

		FIRE(Tag, tagData);
		
		memcpy(phid->lastTag, tagData, 5);
	}
	else if(!strncmp(setThing, "TagLoss", sizeof("TagLoss")))
	{			
		//always increment on tagPresent==PUNI_BOOL before setting it!
		INC_KEYCOUNT(tagPresent, PUNI_BOOL)
		phid->tagPresent = 0;
		FIRE(TagLost, phid->lastTag);
	}
	else if(!strncmp(setThing, "TagState", sizeof("TagState")))
	{			
		//always increment on tagPresent==PUNI_BOOL before setting it!
		INC_KEYCOUNT(tagPresent, PUNI_BOOL)
		phid->tagPresent = value;
	}
	else if(!strncmp(setThing, "Output", sizeof("Output")))
	{
		if(index < phid->phid.attr.rfid.numOutputs?
			phid->phid.attr.rfid.numOutputs:RFID_MAXOUTPUTS)
		{
			INC_KEYCOUNT(outputEchoState[index], PUNI_BOOL)
			phid->outputEchoState[index] = value;
			if(value != PUNK_BOOL)
				FIRE(OutputChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "AntennaOn", sizeof("AntennaOn")))
	{
		INC_KEYCOUNT(antennaEchoState, PUNI_BOOL)
		phid->antennaEchoState = value;
	}
	else if(!strncmp(setThing, "LEDOn", sizeof("LEDOn")))
	{
		INC_KEYCOUNT(ledEchoState, PUNI_BOOL)
		phid->ledEchoState = value;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for RFID: %s", setThing);
	}
	return ret;
}

int phidget_servo_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetServoHandle phid = (CPhidgetServoHandle)generic_phid;
	if(!strncmp(setThing, "NumberOfMotors", sizeof("NumberOfMotors")))
	{
		int value = strtol(state, NULL, 10);
		phid->phid.attr.led.numLEDs = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "Position", sizeof("Position")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.servo.numMotors?
			phid->phid.attr.servo.numMotors:SERVO_MAXSERVOS)
		{
			INC_KEYCOUNT(motorPositionEcho[index], PUNI_DBL)
			phid->motorPositionEcho[index]= value;
			if(value != PUNK_DBL)
				FIRE(PositionChange, index, servo_us_to_degrees(phid->servoParams[index], value, PTRUE));
			//Deprecated
			if(value != PUNK_DBL)
				FIRE(MotorPositionChange, index, servo_us_to_degrees(phid->servoParams[index], value, PTRUE));
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Engaged", sizeof("Engaged")))
	{
		int value = strtol(state, NULL, 10);
		if(index < phid->phid.attr.servo.numMotors?
			phid->phid.attr.servo.numMotors:SERVO_MAXSERVOS)
		{
			INC_KEYCOUNT(motorEngagedStateEcho[index], PUNI_BOOL)
			phid->motorEngagedStateEcho[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "PositionMinLimit", sizeof("PositionMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(motorPositionMinLimit, PUNI_DBL)
		phid->motorPositionMinLimit = value;
	}
	else if(!strncmp(setThing, "PositionMaxLimit", sizeof("PositionMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(motorPositionMaxLimit, PUNI_DBL)
		phid->motorPositionMaxLimit = value;
	}
	else if(!strncmp(setThing, "ServoParameters", sizeof("ServoParameters")))
	{
		if(index < phid->phid.attr.servo.numMotors?
			phid->phid.attr.servo.numMotors:SERVO_MAXSERVOS)
		{
			CPhidgetServoParameters params;
			char *endptr;
			params.servoType = strtol(state, &endptr, 10);
			params.min_us = strtod(endptr+1, &endptr);
			params.max_us = strtod(endptr+1, &endptr);
			params.us_per_degree = strtod(endptr+1, NULL);
			params.state = PTRUE;

			INC_KEYCOUNT(servoParams[index].state, PUNI_BOOL)

			phid->servoParams[index] = params;

			//Set the max/min
			//make sure we don't set max higher then the limit
			if(params.max_us > phid->motorPositionMaxLimit)
				phid->motorPositionMax[index] = phid->motorPositionMaxLimit;
			else
				phid->motorPositionMax[index] = params.max_us;

			phid->motorPositionMin[index] = params.min_us;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for Servo: %s", setThing);
	}
	return ret;
}

int phidget_spatial_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetSpatialHandle phid = (CPhidgetSpatialHandle)generic_phid;
	if(!strncmp(setThing, "AccelerationAxisCount", sizeof("AccelerationAxisCount")))
	{
		int value = strtol(state, NULL, 10);
		phid->phid.attr.spatial.numAccelAxes = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "GyroAxisCount", sizeof("GyroAxisCount")))
	{
		int value = strtol(state, NULL, 10);
		phid->phid.attr.spatial.numGyroAxes = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "CompassAxisCount", sizeof("CompassAxisCount")))
	{
		int value = strtol(state, NULL, 10);
		phid->phid.attr.spatial.numCompassAxes = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "DataRate", sizeof("DataRate")))
	{
		int value = strtol(state, NULL, 10);
		INC_KEYCOUNT(dataRate, PUNI_INT)
		phid->dataRate = value;
	}
	else if(!strncmp(setThing, "DataRateMin", sizeof("DataRateMin")))
	{
		int value = strtol(state, NULL, 10);
		INC_KEYCOUNT(dataRateMin, PUNI_INT)
		phid->dataRateMin = value;
	}
	else if(!strncmp(setThing, "DataRateMax", sizeof("DataRateMax")))
	{
		int value = strtol(state, NULL, 10);
		INC_KEYCOUNT(dataRateMax, PUNI_INT)
		phid->dataRateMax = value;
	}
	else if(!strncmp(setThing, "InterruptRate", sizeof("InterruptRate")))
	{
		int value = strtol(state, NULL, 10);
		INC_KEYCOUNT(interruptRate, PUNI_INT)
		phid->interruptRate = value;
	}
	else if(!strncmp(setThing, "AccelerationMin", sizeof("AccelerationMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(accelerationMin, PUNI_DBL)
		phid->accelerationMin = value;
	}
	else if(!strncmp(setThing, "AccelerationMax", sizeof("AccelerationMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(accelerationMax, PUNI_DBL)
		phid->accelerationMax = value;
	}
	else if(!strncmp(setThing, "AngularRateMin", sizeof("AngularRateMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(angularRateMin, PUNI_DBL)
		phid->angularRateMin = value;
	}
	else if(!strncmp(setThing, "AngularRateMax", sizeof("AngularRateMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(angularRateMax, PUNI_DBL)
		phid->angularRateMax = value;
	}
	else if(!strncmp(setThing, "MagneticFieldMin", sizeof("MagneticFieldMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(magneticFieldMin, PUNI_DBL)
		phid->magneticFieldMin = value;
	}
	else if(!strncmp(setThing, "MagneticFieldMax", sizeof("MagneticFieldMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(magneticFieldMax, PUNI_DBL)
		phid->magneticFieldMax = value;
	}
	else if(!strncmp(setThing, "SpatialData", sizeof("SpatialData")))
	{
		CPhidgetSpatial_SpatialEventDataHandle eventData[1];
		CPhidgetSpatial_SpatialEventData spatialData;
		int i;
		char *endptr = (char *)state-1;
		INC_KEYCOUNT(spatialDataNetwork, PUNI_BOOL)
		
		for(i=0;i<SPATIAL_MAX_ACCELAXES;i++)
		{
			phid->accelAxis[i] = spatialData.acceleration[i] = strtod(endptr+1, &endptr);
		}
		for(i=0;i<SPATIAL_MAX_GYROAXES;i++)
		{
			phid->gyroAxis[i] = spatialData.angularRate[i] = strtod(endptr+1, &endptr);
		}
		for(i=0;i<SPATIAL_MAX_COMPASSAXES;i++)
		{
			phid->compassAxis[i] = spatialData.magneticField[i] = strtod(endptr+1, &endptr);
		}

		spatialData.timestamp.seconds = strtol(endptr+1, &endptr, 10);
		spatialData.timestamp.microseconds = strtol(endptr+1, NULL, 10);
		
		eventData[0] = &spatialData;
		FIRE(SpatialData, eventData, 1);
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for Spatial: %s", setThing);
	}
	return ret;

	return ret;
}

int phidget_stepper_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetStepperHandle phid = (CPhidgetStepperHandle)generic_phid;
	if(!strncmp(setThing, "NumberOfMotors", sizeof("NumberOfMotors")))
	{
		int value = strtol(state, NULL, 10);
		phid->phid.attr.stepper.numMotors = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "NumberOfInputs", sizeof("NumberOfInputs")))
	{
		int value = strtol(state, NULL, 10);
		phid->phid.attr.stepper.numInputs = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "Input", sizeof("Input")))
	{
		int value = strtol(state, NULL, 10);
		if(index < phid->phid.attr.stepper.numInputs?
			phid->phid.attr.stepper.numInputs:STEPPER_MAXINPUTS)
		{
			INC_KEYCOUNT(inputState[index], PUNI_BOOL)
			phid->inputState[index] = value;
			if(value != PUNK_BOOL)
				FIRE(InputChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "CurrentPosition", sizeof("CurrentPosition")))
	{
#if defined(_WINDOWS)
		__int64 value = (__int64)_strtoi64(state, NULL, 10);
#else
		__int64 value = strtoll(state, NULL, 10);
#endif
		if(index < phid->phid.attr.stepper.numMotors?
			phid->phid.attr.stepper.numMotors:STEPPER_MAXSTEPPERS)
		{
			INC_KEYCOUNT(motorPositionEcho[index], PUNI_INT64)
			phid->motorPositionEcho[index] = value;
			if(value != PUNK_INT64)
				FIRE(PositionChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	// initial target position isn't a keyCount++ sort of thing
	else if(!strncmp(setThing, "TargetPosition", sizeof("TargetPosition")))
	{
#if defined(_WINDOWS)
		__int64 value = (__int64)_strtoi64(state, NULL, 10);
#else
		__int64 value = strtoll(state, NULL, 10);
#endif
		if(index < phid->phid.attr.stepper.numMotors?
			phid->phid.attr.stepper.numMotors:STEPPER_MAXSTEPPERS)
		{
			INC_KEYCOUNT(motorPosition[index], PUNI_INT64)
			phid->motorPosition[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "PositionMin", sizeof("PositionMin")))
	{
#if defined(_WINDOWS)
		__int64 value = (__int64)_strtoi64(state, NULL, 10);
#else
		__int64 value = strtoll(state, NULL, 10);
#endif
		INC_KEYCOUNT(motorPositionMin, PUNI_INT64)
		phid->motorPositionMin = value;
	}
	else if(!strncmp(setThing, "PositionMax", sizeof("PositionMax")))
	{
#if defined(_WINDOWS)
		__int64 value = (__int64)_strtoi64(state, NULL, 10);
#else
		__int64 value = strtoll(state, NULL, 10);
#endif
		INC_KEYCOUNT(motorPositionMax, PUNI_INT64)
		phid->motorPositionMax = value;
	}
	// initial acceleration is unknown so dont' keyCount++
	else if(!strncmp(setThing, "Acceleration", sizeof("Acceleration")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.stepper.numMotors?
			phid->phid.attr.stepper.numMotors:STEPPER_MAXSTEPPERS)
		{
			phid->motorAcceleration[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "AccelerationMin", sizeof("AccelerationMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(accelerationMin, PUNI_DBL)
		phid->accelerationMin = value;
	}
	else if(!strncmp(setThing, "AccelerationMax", sizeof("AccelerationMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(accelerationMax, PUNI_DBL)
		phid->accelerationMax = value;
	}
	// initial current limit is unknown so dont' keyCount++
	else if(!strncmp(setThing, "CurrentLimit", sizeof("CurrentLimit")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.stepper.numMotors?
			phid->phid.attr.stepper.numMotors:STEPPER_MAXSTEPPERS)
		{
			phid->motorCurrentLimit[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Current", sizeof("Current")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.stepper.numMotors?
			phid->phid.attr.stepper.numMotors:STEPPER_MAXSTEPPERS)
		{
			INC_KEYCOUNT(motorSensedCurrent[index], PUNI_DBL)
			phid->motorSensedCurrent[index] = value;
			if(value != PUNK_DBL)
				FIRE(CurrentChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "CurrentMin", sizeof("CurrentMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(currentMin, PUNI_DBL)
		phid->currentMin = value;
	}
	else if(!strncmp(setThing, "CurrentMax", sizeof("CurrentMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(currentMax, PUNI_DBL)
		phid->currentMax = value;
	}
	// initial velocity limit is unknown so dont' keyCount++
	else if(!strncmp(setThing, "VelocityLimit", sizeof("VelocityLimit")))
	{
		double value = strtod(state, NULL);
		phid->motorSpeed[index] = value;
	}
	else if(!strncmp(setThing, "Velocity", sizeof("Velocity")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.stepper.numMotors?
			phid->phid.attr.stepper.numMotors:STEPPER_MAXSTEPPERS)
		{
			INC_KEYCOUNT(motorSpeedEcho[index], PUNI_DBL)
			phid->motorSpeedEcho[index] = value;
			if(value != PUNK_DBL)
				FIRE(VelocityChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "VelocityMin", sizeof("VelocityMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(motorSpeedMin, PUNI_DBL)
		phid->motorSpeedMin = value;
	}
	else if(!strncmp(setThing, "VelocityMax", sizeof("VelocityMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(motorSpeedMax, PUNI_DBL)
		phid->motorSpeedMax = value;
	}
	else if(!strncmp(setThing, "Engaged", sizeof("Engaged")))
	{
		int value = strtol(state, NULL, 10);
		if(index < phid->phid.attr.stepper.numMotors?
			phid->phid.attr.stepper.numMotors:STEPPER_MAXSTEPPERS)
		{
			INC_KEYCOUNT(motorEngagedStateEcho[index], PUNI_BOOL)
			phid->motorEngagedStateEcho[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "Stopped", sizeof("Stopped")))
	{
		int value = strtol(state, NULL, 10);
		if(index < phid->phid.attr.stepper.numMotors?
			phid->phid.attr.stepper.numMotors:STEPPER_MAXSTEPPERS)
		{
			unsigned char lastStoppedState = phid->motorStoppedState[index];
			INC_KEYCOUNT(motorStoppedState[index], PUNI_BOOL)
			phid->motorStoppedState[index] = value;
			//If changed, re-run position/velocity events, so the stopped change will be noticed
			if(lastStoppedState != value)
			{
				if(phid->motorSpeedEcho[index] != PUNK_DBL)
					FIRE(VelocityChange, index, phid->motorSpeedEcho[index]);
				if(phid->motorPositionEcho[index] != PUNK_INT64)
					FIRE(PositionChange, index, phid->motorPositionEcho[index]);
			}
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for Stepper: %s", setThing);
	}
	return ret;
}

int phidget_temperaturesensor_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetTemperatureSensorHandle phid = (CPhidgetTemperatureSensorHandle)generic_phid;
	if(!strncmp(setThing, "NumberOfSensors", sizeof("NumberOfSensors")))
	{
		int value = strtol(state, NULL, 10);
		phid->phid.attr.temperaturesensor.numTempInputs = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "Potential", sizeof("Potential")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.temperaturesensor.numTempInputs?
			phid->phid.attr.temperaturesensor.numTempInputs:TEMPSENSOR_MAXSENSORS)
		{
			INC_KEYCOUNT(Potential[index], PUNI_DBL)
			phid->Potential[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "PotentialMin", sizeof("PotentialMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(potentialMin, PUNI_DBL)
		phid->potentialMin = value;
	}
	else if(!strncmp(setThing, "PotentialMax", sizeof("PotentialMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(potentialMax, PUNI_DBL)
		phid->potentialMax = value;
	}
	else if(!strncmp(setThing, "Temperature", sizeof("Temperature")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.temperaturesensor.numTempInputs?
			phid->phid.attr.temperaturesensor.numTempInputs:TEMPSENSOR_MAXSENSORS)
		{
			INC_KEYCOUNT(Temperature[index], PUNI_DBL)
			phid->Temperature[index] = value;
			if(value != PUNK_DBL)
				FIRE(TemperatureChange, index, value);
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else if(!strncmp(setThing, "TemperatureMin", sizeof("TemperatureMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(temperatureMin[index], PUNI_DBL)
		phid->temperatureMin[index] = value;
	}
	else if(!strncmp(setThing, "TemperatureMax", sizeof("TemperatureMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(temperatureMax[index], PUNI_DBL)
		phid->temperatureMax[index] = value;
	}
	else if(!strncmp(setThing, "AmbientTemperature", sizeof("AmbientTemperature")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(AmbientTemperature, PUNI_DBL)
		phid->AmbientTemperature = value;
	}
	else if(!strncmp(setThing, "AmbientTemperatureMin", sizeof("AmbientTemperatureMin")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(ambientTemperatureMin, PUNI_DBL)
		phid->ambientTemperatureMin = value;
	}
	else if(!strncmp(setThing, "AmbientTemperatureMax", sizeof("AmbientTemperatureMax")))
	{
		double value = strtod(state, NULL);
		INC_KEYCOUNT(ambientTemperatureMax, PUNI_DBL)
		phid->ambientTemperatureMax = value;
	}
	else if(!strncmp(setThing, "ThermocoupleType", sizeof("ThermocoupleType")))
	{
		int value = strtol(state, NULL, 10);
		INC_KEYCOUNT(ThermocoupleType[index], -1)
		phid->ThermocoupleType[index] = value;
	}
	else if(!strncmp(setThing, "Trigger", sizeof("Trigger")))
	{
		double value = strtod(state, NULL);
		if(index < phid->phid.attr.temperaturesensor.numTempInputs?
			phid->phid.attr.temperaturesensor.numTempInputs:TEMPSENSOR_MAXSENSORS)
		{
			INC_KEYCOUNT(TempChangeTrigger[index], PUNI_DBL)
			phid->TempChangeTrigger[index] = value;
		}
		else
			ret = EPHIDGET_OUTOFBOUNDS;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for TemperatureSensor: %s", setThing);
	}
	return ret;
}

int phidget_textlcd_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetTextLCDHandle phid = (CPhidgetTextLCDHandle)generic_phid;
	int value = strtol(state, NULL, 10);
	if(!strncmp(setThing, "NumberOfRows", sizeof("NumberOfRows")))
	{
		phid->phid.attr.textlcd.numRows = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "NumberOfColumns", sizeof("NumberOfColumns")))
	{
		phid->phid.attr.textlcd.numColumns = value;
		phid->phid.keyCount++;
	}
	// these are all unknown at attach, so no keyCount++
	else if(!strncmp(setThing, "Backlight", sizeof("Backlight")))
	{
		phid->m_blnBacklight = (unsigned char)value;
	}
	else if(!strncmp(setThing, "CursorOn", sizeof("CursorOn")))
	{
		phid->m_blnCursorOn = (unsigned char)value;
	}
	else if(!strncmp(setThing, "CursorBlink", sizeof("CursorBlink")))
	{
		phid->m_blnCursorBlink = (unsigned char)value;
	}
	else if(!strncmp(setThing, "Contrast", sizeof("Contrast")))
	{
		phid->m_iContrast = (unsigned char)value;
	}
	else if(!strncmp(setThing, "Brightness", sizeof("Brightness")))
	{
		phid->brightness = (unsigned char)value;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for TextLCD: %s", setThing);
	}
	return ret;
}

int phidget_textled_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetTextLEDHandle phid = (CPhidgetTextLEDHandle)generic_phid;
	int value = strtol(state, NULL, 10);
	if(!strncmp(setThing, "NumberOfRows", sizeof("NumberOfRows")))
	{
		phid->phid.attr.textled.numRows = value;
		phid->phid.keyCount++;
	}
	else if(!strncmp(setThing, "NumberOfColumns", sizeof("NumberOfColumns")))
	{
		phid->phid.attr.textled.numColumns = value;
		phid->phid.keyCount++;
	}
	// this is unknown at attach, so no keyCount++
	else if(!strncmp(setThing, "Brightness", sizeof("Brightness")))
	{
		phid->brightness = value;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for TextLED: %s", setThing);
	}
	return ret;
}

int phidget_weightsensor_set(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason)
{
	int ret = EPHIDGET_OK;
	CPhidgetWeightSensorHandle phid = (CPhidgetWeightSensorHandle)generic_phid;
	double value = strtod(state, NULL);
	if(!strncmp(setThing, "Weight", sizeof("Weight")))
	{
		INC_KEYCOUNT(Weight, PUNI_DBL)
		phid->Weight = value;
		if(value != PUNK_DBL)
			FIRE(WeightChange, value);
	}
	else if(!strncmp(setThing, "Trigger", sizeof("Trigger")))
	{
		INC_KEYCOUNT(WeightChangeTrigger, PUNI_DBL)
		phid->WeightChangeTrigger = value;
	}
	else{
		ret = EPHIDGET_INVALIDARG;
		LOG(PHIDGET_LOG_DEBUG,"Bad setType for WeightSensor: %s", setThing);
	}
	return ret;
}

int(*fptrSet[PHIDGET_DEVICE_CLASS_COUNT])(CPhidgetHandle generic_phid, const char *setThing, int index, const char *state, pdict_reason_t reason) = {
NULL,
NULL,
phidget_accelerometer_set,
phidget_advancedservo_set,
phidget_encoder_set,
phidget_gps_set,
NULL, //old gyro
phidget_interfacekit_set,
phidget_led_set,
phidget_motorcontrol_set,
phidget_phsensor_set,
phidget_rfid_set,
phidget_servo_set,
phidget_stepper_set,
phidget_temperaturesensor_set,
phidget_textlcd_set,
phidget_textled_set,
phidget_weightsensor_set,
phidget_generic_set,
phidget_ir_set,
phidget_spatial_set};

void network_phidget_event_handler(const char *key, const char *val, unsigned int len, pdict_reason_t reason, void *ptr)
{
	CPhidgetHandle phid = (CPhidgetHandle)ptr;
	regmatch_t pmatch[6];
	char *setThing = NULL;
	char *index = NULL;
	char *serial = NULL;
	char errbuf[1024];
	
	int serialNumber;

	int res, ind = PUNK_INT, i, ret = EPHIDGET_OK;

	if(!strncmp(val, "\001", 1) && len == 1)
	{
		memset((char *)val,0,1);
	}

	if(reason!=PDR_ENTRY_REMOVING || !strncmp(val, "Detached", sizeof("Detached")))
	{
		if ((res = regexec(&phidgetsetex, key, 6, pmatch, 0)) != 0) {
			LOG(PHIDGET_LOG_DEBUG,"Error in network_phidget_event_handler - pattern not met");
			return;
		}
		getmatchsub(key, &serial, pmatch, 2);
		getmatchsub(key, &setThing, pmatch, 3);
		getmatchsub(key, &index, pmatch, 4);

		serialNumber = strtol(serial, NULL, 10);
			
		if(phid->specificDevice == 0 && strncmp(val, "Detached", sizeof("Detached")))
		{
			phid->specificDevice = 2;
			phid->serialNumber = serialNumber;
		}
		
		if(serialNumber == phid->serialNumber && setThing)
		{
			if(!strncmp(setThing, "Label", sizeof("Label")))
			{
				strncpy(phid->label, val, 11);
				phid->keyCount++;
			}
			else if(!strncmp(setThing, "InitKeys", sizeof("InitKeys")))
			{
				phid->initKeys = strtol(val, NULL, 10);
				phid->keyCount++;
			}
			else if(!strncmp(setThing, "Version", sizeof("Version")))
			{
				phid->deviceVersion = strtol(val, NULL, 10);
				phid->keyCount++;
			}
			else if(!strncmp(setThing, "ID", sizeof("ID")))
			{
				phid->deviceIDSpec = strtol(val, NULL, 10);
				phid->deviceType = Phid_DeviceName[phid->deviceID];
				phid->keyCount++;

				for(i = 1;i<PHIDGET_DEVICE_COUNT;i++)
				{
					if(Phid_Device_Def[i].pdd_sdid == phid->deviceIDSpec)
					{
						phid->deviceDef = &Phid_Device_Def[i];
						phid->attr = Phid_Device_Def[i].pdd_attr;
						break;
					}
				}
			}
			else if(!strncmp(setThing, "Name", sizeof("Name")))
			{
				phid->keyCount++;
			}
			else if(!strncmp(setThing, "Status", sizeof("Status")))
			{
				if(!strncmp(val, "Attached", sizeof("Attached")))
				{
					phid->keyCount++;
				}
				else if(!strncmp(val, "Detached", sizeof("Detached")))
				{
					CThread_mutex_lock(&phid->lock);
					phid->keyCount = 0;
					if(phid->specificDevice == 2)
					{
						phid->specificDevice = 0;
					}
					CPhidget_clearStatusFlag(&phid->status, PHIDGET_ATTACHED_FLAG, NULL);
					CPhidget_setStatusFlag(&phid->status, PHIDGET_DETACHING_FLAG, NULL);
					CThread_mutex_unlock(&phid->lock);

					if (phid->fptrDetach)
						phid->fptrDetach((CPhidgetHandle)phid, phid->fptrDetachptr);

					//clear all variables
					phid->fptrClear((CPhidgetHandle)phid);
					
					//if mDNS & any server, disconnect
					if(!phid->networkInfo->requested_address && !phid->networkInfo->requested_serverID)
					{
						CThread DisconnectPhidgetThread;
						CThread_mutex_lock(&zeroconfPhidgetsLock);
						CList_removeFromList((CListHandle *)&zeroconfPhidgets, phid, CPhidget_areExtraEqual, TRUE, CPhidget_free);
						CThread_mutex_unlock(&zeroconfPhidgetsLock);
						CThread_create(&DisconnectPhidgetThread, DisconnectPhidgetThreadFunction, phid);
					}
					
					CPhidget_clearStatusFlag(&phid->status, PHIDGET_DETACHING_FLAG, NULL);
						
					phid->deviceIDSpec = 0;
					ZEROMEM(&phid->attr, sizeof(CPhidgetAttr));
					ZEROMEM(phid->label, 11);
					phid->deviceVersion = 0;
					phid->initKeys = PUNK_INT;

				}
				else
				{
					throw_error_event(phid, "Bad Message type for Status set", EEPHIDGET_NETWORK);
				}
			}
			else if(fptrSet[phid->deviceID] && setThing)
			{
				if(index)
					ind = strtol(index, NULL, 10);
				ret = fptrSet[phid->deviceID](phid, setThing, ind, val, reason);
			}
			
			if((phid->initKeys != PUNK_INT) && (phid->keyCount >= phid->initKeys) && !CPhidget_statusFlagIsSet(phid->status, PHIDGET_ATTACHED_FLAG))
			{
				CPhidget_setStatusFlag(&phid->status, PHIDGET_ATTACHED_FLAG, &phid->lock);
				if (phid->fptrAttach)
					phid->fptrAttach(phid, phid->fptrAttachptr);
				phid->fptrEvents((CPhidgetHandle)phid);
			}
			//LOG(PHIDGET_LOG_DEBUG, "Message: %s(%s)=%s (%d of %d)", setThing, index, val, phid->keyCount, phid->initKeys);
		}

		free(setThing); setThing = NULL;
		free(index); index = NULL;
		free(serial); serial = NULL;
	}
	
	if(ret)
	{
		snprintf(errbuf, 1024, "Problem during Network set: %s", CPhidget_strerror(ret));
		throw_error_event(phid, errbuf, EEPHIDGET_NETWORK);
	}
}

void network_manager_event_handler(const char *key, const char *val, unsigned int vallen, pdict_reason_t reason, void *ptr)
{
	CPhidgetManagerHandle phidm = (CPhidgetManagerHandle)ptr;
	regmatch_t keymatch[6], valmatch[6];
	char *attachDetach = NULL;
	char *deviceType = NULL;
	char *serial = NULL;
	char *version = NULL;
	char *deviceIDSpec = NULL;
	char *label = NULL;
	char errbuf[1024];
	
	int serialNumber;
	CPhidgetHandle phid;

	//BL:Changed to init Len before use
	int len = 0;

	int res, ret = EPHIDGET_OK;

	int i;
	
	if(!phidm) return;

	if(!strncmp(val, "\001", 1) && (len == 1))
	{
		memset((char *)val,0,1);
	}

	if(reason!=PDR_ENTRY_REMOVING)
	{
		if ((res = regexec(&managerex, key, 3, keymatch, 0)) != 0) {
			LOG(PHIDGET_LOG_DEBUG,"Error in network_manager_event_handler - key pattern not met");
			return;
		}
		if ((res = regexec(&managervalex, val, 5, valmatch, 0)) != 0) {
			LOG(PHIDGET_LOG_DEBUG,"Error in network_manager_event_handler - val pattern not met");
			return;
		}
		getmatchsub(key, &deviceType, keymatch, 1);
		getmatchsub(key, &serial, keymatch, 2);

		getmatchsub(val, &attachDetach, valmatch, 1);
		getmatchsub(val, &version, valmatch, 2);
		getmatchsub(val, &deviceIDSpec, valmatch, 3);
		getmatchsub(val, &label, valmatch, 4);
		
		serialNumber = strtol(serial, NULL, 10);
	
		if((CPhidget_create(&phid))) return;

		phid->deviceID = phidget_type_to_id(deviceType);
		phid->deviceType = Phid_DeviceName[phid->deviceID];
		phid->serialNumber = serialNumber;
		phid->deviceIDSpec = (unsigned short)strtol(deviceIDSpec, NULL, 10);
		phid->deviceVersion = strtol(version, NULL, 10);

		for(i = 1;i<PHIDGET_DEVICE_COUNT;i++)
			if(phid->deviceIDSpec == Phid_Device_Def[i].pdd_sdid) break;
		phid->deviceDef = &Phid_Device_Def[i];
		phid->attr = Phid_Device_Def[i].pdd_attr;

		//so se can get address, etc. from devices.
		phid->networkInfo = phidm->networkInfo;
		CPhidget_setStatusFlag(&phid->status, PHIDGET_REMOTE_FLAG, &phid->lock);
		CPhidget_setStatusFlag(&phid->status, PHIDGET_SERVER_CONNECTED_FLAG, &phid->lock);

		if(label)
		{
			len = (int)strlen(label);
			if(len>10) len = 10;
			for(i=0;i<len;i++)
			{
				phid->label[i] = label[i];
			}
			phid->label[i] = '\0';
		}

		if(!strncmp(attachDetach, "Attached", sizeof("Attached")))
		{
			CPhidget_setStatusFlag(&phid->status, PHIDGET_ATTACHED_FLAG, &phid->lock);
		
			CList_addToList((CListHandle *)&phidm->AttachedPhidgets, phid, CPhidget_areEqual);

			if (phidm->fptrAttachChange && phidm->state == PHIDGETMANAGER_ACTIVE)
				phidm->fptrAttachChange((CPhidgetHandle)phid, phidm->fptrAttachChangeptr);
		}
		
		if(!strncmp(attachDetach, "Detached", sizeof("Attached")))
		{
			CPhidget_clearStatusFlag(&phid->status, PHIDGET_ATTACHED_FLAG, &phid->lock);
			CPhidget_setStatusFlag(&phid->status, PHIDGET_DETACHING_FLAG, &phid->lock);
			if(CList_findInList((CListHandle)phidm->AttachedPhidgets, phid, CPhidget_areEqual, NULL) == EPHIDGET_OK)
			{
				if (phidm->fptrDetachChange && phidm->state == PHIDGETMANAGER_ACTIVE)
					phidm->fptrDetachChange((CPhidgetHandle)phid, phidm->fptrDetachChangeptr);

				CList_removeFromList((CListHandle *)&phidm->AttachedPhidgets, phid, CPhidget_areEqual, PTRUE, CPhidget_free);
			}
			CPhidget_clearStatusFlag(&phid->status, PHIDGET_DETACHING_FLAG, &phid->lock);
			CPhidget_free(phid); phid = NULL;
		}

		free(deviceType); deviceType = NULL;
		free(label); label = NULL;
		free(attachDetach); attachDetach = NULL;
		free(serial); serial = NULL;
		free(version); version = NULL;
		free(deviceIDSpec); deviceIDSpec = NULL;
	}
	
	if(ret)
	{
		snprintf(errbuf, 1024, "Problem during Network set: %s", CPhidget_strerror(ret));
		throw_error_event((CPhidgetHandle)phidm, errbuf, EEPHIDGET_NETWORK);
	}
}

void network_heartbeat_event_handler(const char *key, const char *val, unsigned int len, pdict_reason_t reason, void *ptr)
{
	CPhidgetSocketClientHandle server = (CPhidgetSocketClientHandle)ptr;
	double duration = timeSince(&server->lastHeartbeatTime);

	//Keeps a rolling average of the last 5 times
	if(server->avgHeartbeatTimeCount > 5)
	{
		double avg = server->avgHeartbeatTime / server->avgHeartbeatTimeCount;
		server->avgHeartbeatTime -= avg;
		server->avgHeartbeatTimeCount--;
	}
	server->avgHeartbeatTime += duration;
	server->avgHeartbeatTimeCount++;

	server->heartbeatCount++;

	setTimeNow(&server->lastHeartbeatTime);
	server->waitingForHeartbeat = PFALSE;
}

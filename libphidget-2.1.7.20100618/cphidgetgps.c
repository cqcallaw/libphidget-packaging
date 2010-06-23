#include "stdafx.h"
#include "cphidgetgps.h"
#include "cusb.h"
#include "csocket.h"
#include "cthread.h"
#include <math.h>

// === Internal Functions === //

//Don't compile in any GPS code
static int checkcrc(char *data, int crc);
static int parse_GPS_data(char *data, CPhidgetGPSInfo *phid);
static int parse_GPS_packets(CPhidgetGPSInfo *phid);

//clearVars - sets all device variables to unknown state
CPHIDGETCLEARVARS(GPS)
	TESTPTR(phid);

	phid->PositionChangeTrigger = PUNI_DBL;

	return EPHIDGET_OK;
}

//initAfterOpen - sets up the initial state of an object, reading in packets from the device if needed
//				  used during attach initialization - on every attach
CPHIDGETINIT(GPS)

	TESTPTR(phid);

	phid->sckbuf_read=0;
	phid->sckbuf_write=0;
	phid->PositionChangeTrigger=0;

	return EPHIDGET_OK;
}

//dataInput - parses device packets
CPHIDGETDATA(GPS)
	int i=0;

	if (length < 0) return EPHIDGET_INVALIDARG;
	TESTPTR(phid);
	TESTPTR(buffer);

	/* stick it in a buffer */
	while ((i < 64) && ((unsigned char)(phid->sckbuf_write + 1) != phid->sckbuf_read))
	{
		phid->sckbuf[(unsigned char)(phid->sckbuf_write++)] = buffer[i];
		//LOG(PHIDGET_LOG_DEBUG,"%02x ",buffer[i]);
		//printf("%c",buffer[i]);
		i++;
	}
	parse_GPS_packets(phid);

	return EPHIDGET_OK;
}

//eventsAfterOpen - sends out an event for all valid data, used during attach initialization
CPHIDGETINITEVENTS(GPS)
	phid = 0;
	return EPHIDGET_OK;
}

//getPacket - not used for GPS
CGETPACKET(GPS)
	return EPHIDGET_UNEXPECTED;
}

/* checks a CRC */
static int checkcrc(char *data, int crc) {
	unsigned int i=0;
	unsigned char check=0;
	for(i=1;i<strlen(data);i++)
		check^=data[i];
	if(check == crc)
		return 0;
	return 1;
}

/* this parses a full packet */
static int parse_GPS_data(char *data, CPhidgetGPSInfo *phid) {
	char *dataarray[50];
	int numfields = 0;
	int i,j,crc=0;
	double tempD, longadj;

	char tempbuf[100];

	//LOG(PHIDGET_LOG_DEBUG,"%s\n",data);

	/* fist check CRC if there is one */
	j = (int)strlen(data);
	for(i=0;i<j;i++) {
		if(data[i] == '*') {
			crc = strtol(data+i+1,NULL,16);
			data[i] = '\0';
			if(checkcrc(data, crc))
				return 1;
			break;
		}
	}
	
	/* send raw NMEA data to any handlers that want it */
	FIRE(NMEAData, data);

	/* seperate out by commas */
	dataarray[0] = data;
	j = (int)strlen(data);
	for(i=0;i<j;i++) {
		if(data[i] == ',') {
			numfields++;
			dataarray[numfields] = data+i+1;
			data[i] = '\0';
		}
	}

	/* find the type of sentence */
	if(!strncmp("GGA",dataarray[0]+3,3)) {
		if(strlen(dataarray[1])>=6) {
			sprintf(tempbuf,"%c%c %c%c %c%c",dataarray[1][0],dataarray[1][1],
				dataarray[1][2],dataarray[1][3],dataarray[1][4],dataarray[1][5]);
			phid->GPSData.GGA.time.tm_hour = (short)strtol(tempbuf,NULL,10);
			phid->GPSData.GGA.time.tm_min = (short)strtol(tempbuf+3,NULL,10);
			phid->GPSData.GGA.time.tm_sec = (short)strtol(tempbuf+6,NULL,10);
		}
		/* convert lat/long to signed decimal degree format */
		if(strlen(dataarray[2])) {
			tempD = (strtod((dataarray[2]+2),NULL) / 60);
			sprintf(tempbuf,"%c%f",dataarray[2][0],((dataarray[2][1]-48)+tempD));
			phid->GPSData.GGA.latitude = strtod(tempbuf,NULL);
			if(dataarray[3][0] == 'S')
				phid->GPSData.GGA.latitude = -phid->GPSData.GGA.latitude;
		}

		if(strlen(dataarray[4])) {
			tempD = (strtod((dataarray[4]+3),NULL) / 60);
			sprintf(tempbuf,"%c%c%f",dataarray[4][0],dataarray[4][1],((dataarray[4][2]-48)+tempD));
			phid->GPSData.GGA.longitude = strtod(tempbuf,NULL);
			if(dataarray[5][0] == 'W')
				phid->GPSData.GGA.longitude = -phid->GPSData.GGA.longitude;
		}

		phid->GPSData.GGA.fixQuality = (short)strtol(dataarray[6],NULL,10);
		phid->GPSData.GGA.numSatellites = (short)strtol(dataarray[7],NULL,10);
		phid->GPSData.GGA.horizontalDilution = strtod(dataarray[8],NULL);

		phid->GPSData.GGA.altitude = strtod(dataarray[9],NULL);
		phid->GPSData.GGA.heightOfGeoid = strtod(dataarray[11],NULL);

		/* calculate position change (no altitude) */
		longadj = cos((phid->GPSData.GGA.latitude + phid->lastLatitude)/2);
		tempD = (sqrt(pow(fabs(phid->GPSData.GGA.latitude - phid->lastLatitude),2)+
			pow(fabs(longadj*(phid->GPSData.GGA.longitude - phid->lastLongitude)),2)));

		/* only sends event if the fix is valid, or if PositionChangeTrigger == -1 */
		if (tempD >= phid->PositionChangeTrigger 
			&& (phid->GPSData.GGA.fixQuality > 0 || phid->PositionChangeTrigger < 0))
		{
			FIRE(PositionChange, phid->GPSData.GGA.latitude,phid->GPSData.GGA.longitude,phid->GPSData.GGA.altitude);
			phid->lastLatitude = phid->GPSData.GGA.latitude;
			phid->lastLongitude = phid->GPSData.GGA.longitude;
		}
	}
	else if(!strncmp("GSA",dataarray[0]+3,3)) {
		phid->GPSData.GSA.mode = dataarray[1][0];
		phid->GPSData.GSA.fixType = (short)strtol(dataarray[2],NULL,10);
		for(i=0;i<12;i++)
			phid->GPSData.GSA.satUsed[i] = (short)strtol(dataarray[i+3],NULL,10);
		phid->GPSData.GSA.posnDilution = strtod(dataarray[15],NULL);
		phid->GPSData.GSA.horizDilution = strtod(dataarray[16],NULL);
		phid->GPSData.GSA.vertDilution = strtod(dataarray[17],NULL);
	}
	else if(!strncmp("GSV",dataarray[0]+3,3)) {
		int numSentences, sentenceNumber, numSats;

		numSentences = strtol(dataarray[1],NULL,10);
		sentenceNumber = strtol(dataarray[2],NULL,10);
		numSats = strtol(dataarray[3],NULL,10);

		phid->GPSData.GSV.satsInView = (short)numSats;
		for(i=0;i<(numSentences==sentenceNumber?numSats-(4*(numSentences-1)):4);i++) {
			phid->GPSData.GSV.satInfo[i+((sentenceNumber-1)*4)].ID = (short)strtol(dataarray[4+(i*4)],NULL,10);
			phid->GPSData.GSV.satInfo[i+((sentenceNumber-1)*4)].elevation = (short)strtol(dataarray[5+(i*4)],NULL,10);
			phid->GPSData.GSV.satInfo[i+((sentenceNumber-1)*4)].azimuth = strtol(dataarray[6+(i*4)],NULL,10);
			phid->GPSData.GSV.satInfo[i+((sentenceNumber-1)*4)].SNR = (short)strtol(dataarray[7+(i*4)],NULL,10);
		}
	}
	else if(!strncmp("RMC",dataarray[0]+3,3)) {
		if(strlen(dataarray[1])>=6) {
			sprintf(tempbuf,"%c%c %c%c %c%c",dataarray[1][0],dataarray[1][1],
				dataarray[1][2],dataarray[1][3],dataarray[1][4],dataarray[1][5]);
			phid->GPSData.RMC.time.tm_hour = (short)strtol(tempbuf,NULL,10);
			phid->GPSData.RMC.time.tm_min = (short)strtol(tempbuf+3,NULL,10);
			phid->GPSData.RMC.time.tm_sec = (short)strtol(tempbuf+6,NULL,10);
		}

		phid->GPSData.RMC.status = dataarray[2][0];

		/* convert lat/long to signed decimal degree format */
		if(strlen(dataarray[3])) {
			tempD = (strtod((dataarray[3]+2),NULL) / 60);
			sprintf(tempbuf,"%c%f",dataarray[3][0],((dataarray[3][1]-48)+tempD));
			phid->GPSData.RMC.latitude = strtod(tempbuf,NULL);
			if(dataarray[4][0] == 'S')
				phid->GPSData.RMC.latitude = -phid->GPSData.RMC.latitude;
		}

		if(strlen(dataarray[5])) {
			tempD = (strtod((dataarray[5]+3),NULL) / 0.6);
			sprintf(tempbuf,"%c%c%f",dataarray[5][0],dataarray[5][1],((dataarray[5][2]-48)+tempD));
			phid->GPSData.RMC.longitude = strtod(tempbuf,NULL);
			if(dataarray[6][0] == 'W')
				phid->GPSData.RMC.longitude = -phid->GPSData.RMC.longitude;
		}

		phid->GPSData.RMC.speed = strtod(dataarray[7],NULL);
		phid->GPSData.RMC.heading = strtod(dataarray[8],NULL);

		if(strlen(dataarray[9])>=6) {
			sprintf(tempbuf,"%c%c %c%c %c%c",dataarray[9][0],dataarray[9][1],
				dataarray[9][2],dataarray[9][3],dataarray[9][4],dataarray[9][5]);
			phid->GPSData.RMC.date.tm_mday = (short)strtol(tempbuf,NULL,10);
			phid->GPSData.RMC.date.tm_mon = (short)strtol(tempbuf+3,NULL,10);
			phid->GPSData.RMC.date.tm_year = (short)strtol(tempbuf+6,NULL,10);
		}

		phid->GPSData.RMC.magneticVariation = strtod(dataarray[10],NULL);
		if(dataarray[11][0] == 'W')
			phid->GPSData.RMC.magneticVariation = -phid->GPSData.RMC.magneticVariation;
	}
	else {
		LOG(PHIDGET_LOG_DEBUG,"unrecognized sentence type");
	}

	return 0;
}

/* this parses out the packets */
static int parse_GPS_packets(CPhidgetGPSInfo *phid) {

	char tempbuffer[100];
	unsigned char current_queuesize, packetsize, temp;
	int result, i=0;

	do {		
		result = 0;
		/* Not known if packetsize is valid yet... */

		/* make sure we're at a '$' */
		i=0;
		while ((i < 255) && ((unsigned char)(phid->sckbuf_read) != phid->sckbuf_write))
		{
			if(phid->sckbuf[(unsigned char)(phid->sckbuf_read)] == '$')
				break;
			i++;
			phid->sckbuf_read++;
		}
		
		current_queuesize = phid->sckbuf_write - phid->sckbuf_read;
		
		/* find the end of the packet */
		i=0;
		temp = phid->sckbuf_read;
		packetsize = 0;
		while (i < 255 && ((unsigned char)(temp) != phid->sckbuf_write))
		{
			if(phid->sckbuf[(unsigned char)(temp)] == 0x0a) {
				packetsize = i;
				break;
			}
			temp++;
			i++;
		}
		if(!packetsize) break;

		if ((current_queuesize > 1) && (current_queuesize >= (unsigned char)(packetsize)))
		{
			for (i = 0; i<(packetsize); i++)
				tempbuffer[i] = phid->sckbuf[(unsigned char)(phid->sckbuf_read++)];
			tempbuffer[i] = 0;
			/* We know that we have at least a full packet here... look for another */
			result=1;

			/* here we'll actually parse this packet */
			parse_GPS_data(tempbuffer, phid);
		}	
	}
	while(result);

	return 0;
}

// === Exported Functions === //

//create and initialize a device structure
CCREATE(GPS, PHIDCLASS_GPS)

//event setup functions
CFHANDLE(GPS, PositionChange, double, double, double)
CFHANDLE(GPS, NMEAData, const char *)

CGET(GPS,Latitude,double)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_GPS)
	TESTATTACHED

	MASGN(GPSData.GGA.latitude)
}

CGET(GPS,Longitude,double)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_GPS)
	TESTATTACHED

	MASGN(GPSData.GGA.longitude)
}

CGET(GPS,Altitude,double)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_GPS)
	TESTATTACHED

	MASGN(GPSData.GGA.altitude)
}

CGET(GPS,Time,GPSTime)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_GPS)
	TESTATTACHED

	MASGN(GPSData.GGA.time)
}

CGET(GPS,RawData,GPSInfo)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_GPS)
	TESTATTACHED

	MASGN(GPSData)
}

/* change trigger in meters - 1Deg = 111km */
CGET(GPS,PositionChangeTrigger,double)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_GPS)
	TESTATTACHED

	*pVal = (phid->PositionChangeTrigger / 0.0000899928055396);

	return EPHIDGET_OK;
}
CSET(GPS,PositionChangeTrigger,double)
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_GPS)
	TESTATTACHED

	newVal *= 0.0000899928055396;

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEY(Trigger, "%lE", PositionChangeTrigger);
	else
		phid->PositionChangeTrigger = newVal;

	return EPHIDGET_OK;
}

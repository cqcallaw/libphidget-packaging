#ifndef __CPHIDGETGPS
#define __CPHIDGETGPS
#include "cphidget.h"

/** \defgroup phidgps Phidget GPS 
 * \ingroup phidgets
 * Calls specific to the Phidget GPS. See the product manual for more specific API details, supported functionality, units, etc.
 * @{
 */

/* time and date info in UTC */
struct __GPSTime
{
	short tm_ms, tm_sec, tm_min, tm_hour;
} typedef GPSTime;
struct __GPSDate
{
	short tm_mday, tm_mon, tm_year;
} typedef GPSDate;

struct __GPSSatInfo
{
	short ID;
	short elevation;
	int azimuth;
	short SNR;
} typedef GPSSatInfo;

//sentences
//GGA
struct __GPGGA
{
	GPSTime time;
	double latitude;
	double longitude;
	short fixQuality;
	short numSatellites;
	double horizontalDilution;
	double altitude;
	double heightOfGeoid;
} typedef GPGGA;
//GSA
struct __GPGSA
{
	char mode;
	/* A = auto
	 * M = forced */
	short fixType;
	/* 1 = no fix
	 * 2 = 2D
	 * 3 = 3D */
	short satUsed[12];
	/* IDs of used sats in no real order, 0 means nothing */
	double posnDilution;
	double horizDilution;
	double vertDilution;
} typedef GPGSA;
//GSV
struct __GPGSV
{
	short satsInView;
	GPSSatInfo satInfo[12];
} typedef GPGSV;
//RMC
struct __GPRMC
{
	GPSTime time;
	char status;
	double latitude;
	double longitude;
	double speedKnots;
	double heading;
	GPSDate date;
	double magneticVariation;
	char mode;
} typedef GPRMC;
//VTG
struct __GPVTG
{
	double trueHeading;
	double magneticHeading;
	double speedKnots;
	double speed; //km/hour
	char mode;
} typedef GPVTG;

struct __NMEAData
{
	GPGGA GGA;
	GPGSA GSA;
	GPGSV GSV;
	GPRMC RMC;
	GPVTG VTG;
} typedef NMEAData;

DPHANDLE(GPS)
CHDRSTANDARD(GPS)

CHDRGET(GPS,Latitude,double *latitude)
CHDRGET(GPS,Longitude,double *longitude)
CHDRGET(GPS,Altitude,double *altitude)
CHDRGET(GPS,Heading,double *heading)
CHDRGET(GPS,Velocity,double *velocity)
CHDRGET(GPS,Time,GPSTime *time)
CHDRGET(GPS,Date,GPSDate *date)
CHDRGET(GPS,PositionFixStatus,int *fixStatus)
CHDRGET(GPS,NMEAData,NMEAData *data)

CHDREVENT(GPS,PositionChange,double latitude,double longitude,double altitude)
CHDREVENT(GPS,PositionFixStatusChange,int status)

#ifndef EXTERNALPROTO

struct _CPhidgetGPS {
	CPhidget phid;

	int (CCONV *fptrPositionChange)(CPhidgetGPSHandle, void *, double latitude, double longitude, double altitude);           
	void *fptrPositionChangeptr;
	int (CCONV *fptrPositionFixStatusChange)(CPhidgetGPSHandle, void *, int status);           
	void *fptrPositionFixStatusChangeptr;

	NMEAData GPSData;

	double heading, velocity, altitude, latitude, longitude;
	unsigned char fix;

	unsigned char haveTime, haveDate;

	double lastLongitude, lastLatitude, lastAltitude;
	unsigned char lastFix;

	unsigned char sckbuf[256];
	unsigned char sckbuf_write, sckbuf_read;
} typedef CPhidgetGPSInfo;
#endif

/** @} */

#endif

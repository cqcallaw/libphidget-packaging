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
	short tm_sec,tm_min,tm_hour;
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
	double speed;
	double heading;
	GPSDate date;
	double magneticVariation;
} typedef GPRMC;

struct __GPSInfo
{
	GPGGA GGA;
	GPGSA GSA;
	GPGSV GSV;
	GPRMC RMC;
} typedef GPSInfo;

DPHANDLE(GPS)
CHDRSTANDARD(GPS)


CHDRGET(GPS,Latitude,double *)
CHDRGET(GPS,Longitude,double *)
CHDRGET(GPS,Altitude,double *)
CHDRGET(GPS,Time,GPSTime *)
CHDRGET(GPS,RawData,GPSInfo *)

CHDRGET(GPS,PositionChangeTrigger,double *)
CHDRSET(GPS,PositionChangeTrigger,double)
CHDREVENT(GPS,PositionChange,double latitude,double longitude,double altitude)

CHDREVENT(GPS,NMEAData,const char *)
#endif

#ifndef EXTERNALPROTO

struct _CPhidgetGPS {
	CPhidget phid;

	int (CCONV *fptrPositionChange)(CPhidgetGPSHandle, void *, double latitude, double longitude, double altitude);           
	void *fptrPositionChangeptr;
	
	int (CCONV *fptrNMEAData)(CPhidgetGPSHandle, void *, const char *data);           
	void *fptrNMEADataptr;

	GPSInfo GPSData;

	double lastLongitude, lastLatitude;
	double PositionChangeTrigger;

	char sckbuf[256];
	unsigned char sckbuf_write,sckbuf_read;
} typedef CPhidgetGPSInfo;
#endif

/** @} */


#ifndef __CPHIDGETSPATIAL
#define __CPHIDGETSPATIAL
#include "cphidget.h"

/** \defgroup phidspatial Phidget Spatial 
 * \ingroup phidgets
 * Calls specific to the Phidget Spatial. See the product manual for more specific API details, supported functionality, units, etc.
 * @{
 */

DPHANDLE(Spatial)
CHDRSTANDARD(Spatial)

/**
 * Timestamped position data returned by the \ref CPhidgetSpatial_set_OnSpatialData_Handler event.
 */
typedef struct _CPhidgetSpatial_SpatialEventData
{
	double acceleration[3]; /**< Acceleration data for up to 3 axes. */
	double angularRate[3]; /**< Angular rate data (Gyroscope) for up to 3 axes */
	double magneticField[3]; /**< Magnetic field data (Compass) for up to 3 axes */
	CPhidget_Timestamp timestamp; /**< Hardware timestamp */
} CPhidgetSpatial_SpatialEventData, *CPhidgetSpatial_SpatialEventDataHandle;

/**
 * Gets the number of acceleration axes supplied by this board.
 * @param phid An attached phidget spatial handle.
 * @param count The number of acceleration axes.
 */
CHDRGET(Spatial,AccelerationAxisCount,int *count)
/**
 * Gets the number of gyroscope axes supplied by this board.
 * @param phid An attached phidget spatial handle.
 * @param count The number of gyro axes.
 */
CHDRGET(Spatial,GyroAxisCount,int *count)
/**
 * Gets the number of compass axes supplied by this board.
 * @param phid An attached phidget spatial handle.
 * @param count The number of compass axes.
 */
CHDRGET(Spatial,CompassAxisCount,int *count)

/**
 * Gets the current acceleration of an axis.
 * @param phid An attached phidget spatial handle.
 * @param index The acceleration index.
 * @param acceleration The acceleration in gs.
 */
CHDRGETINDEX(Spatial,Acceleration,double *acceleration)
/**
 * Gets the maximum accleration supported by an axis.
 * @param phid An attached phidget spatial handle.
 * @param index The acceleration index
 * @param max The maximum acceleration
 */
CHDRGETINDEX(Spatial,AccelerationMax,double *max)
/**
 * Gets the minimum acceleration supported by an axis.
 * @param phid An attached phidget spatial handle.
 * @param index The acceleration index
 * @param min The minimum acceleration
 */
CHDRGETINDEX(Spatial,AccelerationMin,double *min)

/**
 * Gets the current angular rate of an axis.
 * @param phid An attached phidget spatial handle.
 * @param index The angular rate index.
 * @param angularRate The angular rate in degrees/second.
 */
CHDRGETINDEX(Spatial,AngularRate,double *angularRate)
/**
 * Gets the maximum angular rate supported by an axis.
 * @param phid An attached phidget spatial handle.
 * @param index The angular rate index
 * @param max The maximum angular rate
 */
CHDRGETINDEX(Spatial,AngularRateMax,double *max)
/**
 * Gets the minimum angular rate supported by an axis.
 * @param phid An attached phidget spatial handle.
 * @param index The angular rate index
 * @param min The minimum angular rate
 */
CHDRGETINDEX(Spatial,AngularRateMin,double *min)

/**
 * Gets the current magnetic field stregth of an axis.
 * @param phid An attached phidget spatial handle.
 * @param index The magnetic field index.
 * @param magneticField The magnetic field strength in Gauss.
 */
CHDRGETINDEX(Spatial,MagneticField,double *magneticField)
/**
 * Gets the maximum magnetic field stregth supported by an axis.
 * @param phid An attached phidget spatial handle.
 * @param index The magnetic field index
 * @param max The maximum magnetic field stregth
 */
CHDRGETINDEX(Spatial,MagneticFieldMax,double *max)
/**
 * Gets the minimum magnetic field stregth supported by an axis.
 * @param phid An attached phidget spatial handle.
 * @param index The magnetic field index
 * @param min The minimum magnetic field stregth
 */
CHDRGETINDEX(Spatial,MagneticFieldMin,double *min)

/**
 * Zeroes the gyroscope. This takes about two seconds and the gyro zxes will report 0 during the process.
 * This should only be called when the board is not moving.
 * @param phid An attached phidget spatial handle.
 */
PHIDGET21_API int CCONV CPhidgetSpatial_zeroGyro(CPhidgetSpatialHandle phid);

/**
 * Get the data rate.
 * @param phid An attached phidget spatial handle.
 * @param milliseconds The data rate in milliseconds.
 */
CHDRGET(Spatial, DataRate, int *milliseconds)
/**
 * Sets the data rate. Note that data at rates faster then 8ms will be delivered to events as an array of data.
 * @param phid An attached phidget spatial handle.
 * @param milliseconds The data rate in milliseconds.
 */
CHDRSET(Spatial, DataRate, int milliseconds)
/**
 * Gets the maximum supported data rate.
 * @param phid An attached phidget spatial handle.
 * @param max Data rate in ms.
 */
CHDRGET(Spatial, DataRateMax, int *max)
/**
 * Gets the minimum supported data rate.
 * @param phid An attached phidget spatial handle.
 * @param min Data rate in ms.
 */
CHDRGET(Spatial, DataRateMin, int *min)

/**
 * Sets the compass correction factors. This can be used to correcting any sensor errors, including hard and soft iron offsets and sensor error factors.
 * @param phid An attached phidget spatial handle.
 * @param magField Local magnetic field strength.
 * @param offset0 Axis 0 offset correction.
 * @param offset1 Axis 1 offset correction.
 * @param offset2 Axis 2 offset correction.
 * @param gain0 Axis 0 gain correction.
 * @param gain1 Axis 1 gain correction.
 * @param gain2 Axis 2 gain correction.
 * @param T0 Non-orthogonality correction factor 0.
 * @param T1 Non-orthogonality correction factor 1.
 * @param T2 Non-orthogonality correction factor 2.
 * @param T3 Non-orthogonality correction factor 3.
 * @param T4 Non-orthogonality correction factor 4.
 * @param T5 Non-orthogonality correction factor 5.
 */
CHDRSET(Spatial, CompassCorrectionParameters, double magField, double offset0, double offset1, double offset2, double gain0, double gain1, double gain2, double T0, double T1, double T2, double T3, double T4, double T5)
/**
 * Resets the compass correction factors. Magnetic field data will be presented directly as reported by the sensor.
 * @param phid An attached phidget spatial handle.
 */
PHIDGET21_API int CCONV CPhidgetSpatial_resetCompassCorrectionParameters(CPhidgetSpatialHandle phid);

/**
 * Set a Data event handler. This is called at /ref CPhidgetSpatial_getDataRate, up to 8ms, for faster then 8ms data, multiple
 * sets of data are supplied in a single event.
 * @param phid An attached phidget spatial handle.
 * @param fptr Callback function pointer.
 * @param userPtr A pointer for use by the user - this value is passed back into the callback function.
 */
CHDREVENT(Spatial,SpatialData,CPhidgetSpatial_SpatialEventDataHandle *data, int dataCount)

#ifndef EXTERNALPROTO

#define SPATIAL_MAX_ACCELAXES 3
#define SPATIAL_MAX_GYROAXES 3
#define SPATIAL_MAX_COMPASSAXES 3

//in milliseconds - this is the fastest hardware rate of any device
#define SPATIAL_MAX_DATA_RATE 1
//1 second is the longest between events that we support
#define SPATIAL_MIN_DATA_RATE 1000
//add 200ms for timing differences (late events, etc) - should be plenty
#define SPATIAL_DATA_BUFFER_SIZE ((SPATIAL_MIN_DATA_RATE + 200)/SPATIAL_MAX_DATA_RATE)
//1 second of data to zero the gyro - make sure DATA_BUFFER_SIZE is big enough to hold this much data
#define SPATIAL_ZERO_GYRO_TIME 2000

//packet types
//IN
#define SPATIAL_PACKET_DATA	0x00
#define SPATIAL_PACKET_CALIB 0x80
//OUT
#define SPATIAL_READCALIB 0x01

struct _CPhidgetSpatial {
	CPhidget phid;
	int (CCONV *fptrSpatialData)(CPhidgetSpatialHandle, void *, CPhidgetSpatial_SpatialEventDataHandle *, int);           
	void *fptrSpatialDataptr;

	double accelAxis[SPATIAL_MAX_ACCELAXES];
	double gyroAxis[SPATIAL_MAX_GYROAXES];
	double compassAxis[SPATIAL_MAX_COMPASSAXES];

	double gryoCorrection[SPATIAL_MAX_GYROAXES];
	unsigned char doZeroGyro;
	int gyroZeroReadPtr;

	CPhidgetSpatial_SpatialEventData dataBuffer[SPATIAL_DATA_BUFFER_SIZE];
	int bufferReadPtr, bufferWritePtr;

	double accelerationMax, accelerationMin;
	double angularRateMax, angularRateMin;
	double magneticFieldMax, magneticFieldMin;

	unsigned char calDataValid;

	double accelGain1[SPATIAL_MAX_ACCELAXES];
	double accelGain2[SPATIAL_MAX_ACCELAXES];
	double accelOffset[SPATIAL_MAX_ACCELAXES];
	double accelFactor1[SPATIAL_MAX_ACCELAXES];
	double accelFactor2[SPATIAL_MAX_ACCELAXES];

	double gyroGain1[SPATIAL_MAX_GYROAXES];
	double gyroGain2[SPATIAL_MAX_GYROAXES];
	double gyroOffset[SPATIAL_MAX_GYROAXES];
	double gyroFactor1[SPATIAL_MAX_GYROAXES];
	double gyroFactor2[SPATIAL_MAX_GYROAXES];

	int compassGain[SPATIAL_MAX_COMPASSAXES];
	int compassOffset[SPATIAL_MAX_COMPASSAXES];

	double userMagField;
	double userCompassGain[SPATIAL_MAX_COMPASSAXES];
	double userCompassOffset[SPATIAL_MAX_COMPASSAXES];
	double userCompassTransform[SPATIAL_MAX_COMPASSAXES*(SPATIAL_MAX_COMPASSAXES-1)];
	
	CPhidget_Timestamp timestamp, lastEventTime, latestDataTime;

	unsigned char lastTimeCounterValid;
	int lastTimeCounter;
	int flip;

	char *compassCorrectionParamsString;
	unsigned char spatialDataNetwork;

	int dataRate, interruptRate;
	int dataRateMax, dataRateMin;

} typedef CPhidgetSpatialInfo;
#endif

/** @} */

#endif

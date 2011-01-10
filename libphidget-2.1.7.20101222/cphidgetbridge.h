#ifndef __CPHIDGETBRIDGE
#define __CPHIDGETBRIDGE
#include "cphidget.h"

/** \defgroup phidbridge Phidget Bridge 
 * \ingroup phidgets
 * Calls specific to the Phidget Bridge. See the product manual for more specific API details, supported functionality, units, etc.
 * @{
 */

typedef enum {
	PHIDGET_BRIDGE_GAIN_1 = 1,
	PHIDGET_BRIDGE_GAIN_8,
	PHIDGET_BRIDGE_GAIN_16,
	PHIDGET_BRIDGE_GAIN_32,
	PHIDGET_BRIDGE_GAIN_64,
	PHIDGET_BRIDGE_GAIN_128,
	PHIDGET_BRIDGE_GAIN_UNKNOWN
}  CPhidgetBridge_Gain;

DPHANDLE(Bridge)
CHDRSTANDARD(Bridge)
CHDRGET(Bridge,InputCount,int *count)
CHDRGETINDEX(Bridge,BridgeValue,double *value)
CHDRGETINDEX(Bridge,BridgeMax,double *max)
CHDRGETINDEX(Bridge,BridgeMin,double *min)
CHDRSETINDEX(Bridge,Enabled,int enabledState)
CHDRGETINDEX(Bridge,Enabled,int *enabledState)
CHDRGETINDEX(Bridge,Gain, CPhidgetBridge_Gain *gain)
CHDRSETINDEX(Bridge,Gain, CPhidgetBridge_Gain gain)
CHDRGET(Bridge,DataRate, int *milliseconds)
CHDRSET(Bridge,DataRate, int milliseconds)
CHDRGET(Bridge,DataRateMax, int *max)
CHDRGET(Bridge,DataRateMin, int *min)
CHDREVENTINDEX(Bridge,BridgeData,double value)

#ifndef EXTERNALPROTO
#define BRIDGE_MAXINPUTS 4
struct _CPhidgetBridge {
	CPhidget phid;
	int (CCONV *fptrBridgeData)(CPhidgetBridgeHandle, void *, int, double);           
	void *fptrBridgeDataptr;

	unsigned char enabled[BRIDGE_MAXINPUTS];
	CPhidgetBridge_Gain gain[BRIDGE_MAXINPUTS];
	int dataRate;

	double bridgeValue[BRIDGE_MAXINPUTS];
	unsigned char enabledEcho[BRIDGE_MAXINPUTS];
	CPhidgetBridge_Gain gainEcho[BRIDGE_MAXINPUTS];
	int dataRateEcho;

	int dataRateMin, dataRateMax;
	double bridgeMin[BRIDGE_MAXINPUTS], bridgeMax[BRIDGE_MAXINPUTS];

	unsigned char outOfRange[BRIDGE_MAXINPUTS], lastOutOfRange[BRIDGE_MAXINPUTS];

	unsigned char outputPacket[8];
	unsigned int outputPacketLen;
} typedef CPhidgetBridgeInfo;
#endif

/** @} */

#endif

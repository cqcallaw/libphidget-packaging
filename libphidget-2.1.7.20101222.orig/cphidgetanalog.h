#ifndef __CPHIDGETANALOG
#define __CPHIDGETANALOG
#include "cphidget.h"

/** \defgroup phidanalog Phidget Analog 
 * \ingroup phidgets
 * Calls specific to the Phidget Analog. See the product manual for more specific API details, supported functionality, units, etc.
 * @{
 */

DPHANDLE(Analog)
CHDRSTANDARD(Analog)

CHDRGET(Analog,OutputCount,int *count)
CHDRGETINDEX(Analog,Voltage,double *voltage)
CHDRSETINDEX(Analog,Voltage,double voltage)
CHDRGETINDEX(Analog,VoltageMax,double *max)
CHDRGETINDEX(Analog,VoltageMin,double *min)
CHDRSETINDEX(Analog,Enabled,int enabledState)
CHDRGETINDEX(Analog,Enabled,int *enabledState)
	
#ifndef EXTERNALPROTO

#define ANALOG_MAXOUTPUTS 4

struct _CPhidgetAnalog 
{
	CPhidget phid;

	double voltage[ANALOG_MAXOUTPUTS];
	unsigned char enabled[ANALOG_MAXOUTPUTS];

	double voltageEcho[ANALOG_MAXOUTPUTS];
	unsigned char enabledEcho[ANALOG_MAXOUTPUTS];
	
	double voltageMax, voltageMin;
	
	double nextVoltage[ANALOG_MAXOUTPUTS];
	double lastVoltage[ANALOG_MAXOUTPUTS];
	unsigned char changedVoltage[ANALOG_MAXOUTPUTS];
	
	unsigned char nextEnabled[ANALOG_MAXOUTPUTS];
	unsigned char lastEnabled[ANALOG_MAXOUTPUTS];
	unsigned char changedEnabled[ANALOG_MAXOUTPUTS];

	unsigned char lastOvercurrent[ANALOG_MAXOUTPUTS];
	unsigned char lastTsd;

	unsigned char controlPacketWaiting;
	unsigned char lastOutputPacket;
} typedef CPhidgetAnalogInfo;

#endif

/** @} */
#endif

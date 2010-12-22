#ifndef __CPHIDGETFREQUENCYCOUNTER
#define __CPHIDGETFREQUENCYCOUNTER
#include "cphidget.h"

/** \defgroup phidtemp Phidget Frequency Counter
 * \ingroup phidgets
 * Calls specific to the Phidget Frequency Counter. See the product manual for more specific API details, supported functionality, units, etc.
 *
 * @{
 */

typedef enum {
	PHIDGET_FREQUENCYCOUNTER_FILTERTYPE_ZERO_CROSSING = 1,
	PHIDGET_FREQUENCYCOUNTER_FILTERTYPE_LOGIC_LEVEL,
	PHIDGET_FREQUENCYCOUNTER_FILTERTYPE_UNKNOWN
}  CPhidgetFrequencyCounter_FilterType;

DPHANDLE(FrequencyCounter)
CHDRSTANDARD(FrequencyCounter)

CHDRGET(FrequencyCounter,FrequencyInputCount,int *count)
CHDRGETINDEX(FrequencyCounter,Frequency,double *frequency)
CHDRGETINDEX(FrequencyCounter,TotalTime,__int64 *time)
CHDRGETINDEX(FrequencyCounter,TotalCount,__int64 *count)
CHDRSETINDEX(FrequencyCounter,Timeout,int timeout)
CHDRGETINDEX(FrequencyCounter,Timeout,int *timeout)
CHDRSETINDEX(FrequencyCounter,Enabled,int enabledState)
CHDRGETINDEX(FrequencyCounter,Enabled,int *enabledState)
CHDRSETINDEX(FrequencyCounter,Filter,CPhidgetFrequencyCounter_FilterType filter)
CHDRGETINDEX(FrequencyCounter,Filter,CPhidgetFrequencyCounter_FilterType *filter)
PHIDGET21_API int CCONV CPhidgetFrequencyCounter_reset(CPhidgetFrequencyCounterHandle phid, int index);
CHDREVENTINDEX(FrequencyCounter,Count,int time,int counts)

#ifndef EXTERNALPROTO
#define FREQCOUNTER_MAXINPUTS 2

#define FREQCOUNTER_TICKS_PER_SEC	100000
#define FREQCOUNTER_MICROSECONDS_PER_TICK	(1000000 / FREQCOUNTER_TICKS_PER_SEC)

//OUT packet flags
#define FREQCOUNTER_FLAG_CH1_LOGIC 0x01
#define FREQCOUNTER_FLAG_CH0_LOGIC 0x02
#define FREQCOUNTER_FLAG_CH1_ENABLE 0x04
#define FREQCOUNTER_FLAG_CH0_ENABLE 0x08

struct _CPhidgetFrequencyCounter {
	CPhidget phid;

	int (CCONV *fptrCount)(CPhidgetFrequencyCounterHandle, void *, int, int, int);
	void *fptrCountptr;

	int timeout[FREQCOUNTER_MAXINPUTS]; //microseconds
	CPhidgetFrequencyCounter_FilterType filter[FREQCOUNTER_MAXINPUTS];
	unsigned char enabled[FREQCOUNTER_MAXINPUTS];

	CPhidgetFrequencyCounter_FilterType filterEcho[FREQCOUNTER_MAXINPUTS];
	unsigned char enabledEcho[FREQCOUNTER_MAXINPUTS];

	double frequency[FREQCOUNTER_MAXINPUTS]; //Hz
	int totalTicksSinceLastCount[FREQCOUNTER_MAXINPUTS]; //ticks

	__int64 totalCount[FREQCOUNTER_MAXINPUTS];
	__int64 totalTime[FREQCOUNTER_MAXINPUTS]; //microseconds

	int flip;
	int lastPacketCount;
	
	CThread_mutex_t resetlock; /* protects reset */

	unsigned char outputPacket[8];
	unsigned int outputPacketLen;
} typedef CPhidgetFrequencyCounterInfo;
#endif

/** @} */

#endif

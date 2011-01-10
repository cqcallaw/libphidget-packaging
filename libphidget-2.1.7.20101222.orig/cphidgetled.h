#ifndef __CPHIDGETLED
#define __CPHIDGETLED
#include "cphidget.h"

/** \defgroup phidled Phidget LED 
 * \ingroup phidgets
 * Calls specific to the Phidget LED. See the product manual for more specific API details, supported functionality, units, etc.
 * @{
 */

DPHANDLE(LED)
CHDRSTANDARD(LED)

/**
 * The Phidget LED supports these current limits
 */
typedef enum {
	PHIDGET_LED_CURRENT_LIMIT_20mA = 1,	/**< 20mA */
	PHIDGET_LED_CURRENT_LIMIT_40mA,		/**< 40mA */
	PHIDGET_LED_CURRENT_LIMIT_60mA,		/**< 60mA */
	PHIDGET_LED_CURRENT_LIMIT_80mA		/**< 80mA */
}  CPhidgetLED_CurrentLimit;
/**
 * The Phidget LED supports these voltages
 */
typedef enum {
	PHIDGET_LED_VOLTAGE_1_7V = 1,	/**< 1.7V */
	PHIDGET_LED_VOLTAGE_2_75V,		/**< 2.75V */
	PHIDGET_LED_VOLTAGE_3_9V,		/**< 3.9V */
	PHIDGET_LED_VOLTAGE_5_0V		/**< 5.0V */
}  CPhidgetLED_Voltage;

/**
 * Gets the number of LEDs supported by this board.
 * @param phid An attached phidget LED handle.
 * @param count The led count.
 */
CHDRGET(LED,LEDCount,int *count)
/**
 * Gets the brightness of an LED.
 * @param phid An attached phidget LED handle.
 * @param index The LED index.
 * @param brightness The LED brightness (0-100).
 */
CHDRGETINDEX(LED,DiscreteLED,int *brightness)
/**
 * Sets the brightness of an LED.
 * @param phid An attached phidget LED handle.
 * @param index The LED index.
 * @param brightness The LED brightness (0-100).
 */
CHDRSETINDEX(LED,DiscreteLED,int brightness)
/**
 * Gets the current limit. This is for all ouputs.
 * @param phid An attached phidget LED handle.
 * @param currentLimit The Current Limit.
 */
CHDRGET(LED,CurrentLimit,CPhidgetLED_CurrentLimit *currentLimit)
/**
 * Sets the current limit. This is for all ouputs.
 * @param phid An attached phidget LED handle.
 * @param currentLimit The Current Limit.
 */
CHDRSET(LED,CurrentLimit,CPhidgetLED_CurrentLimit currentLimit)
/**
 * Gets the output voltate. This is for all ouputs.
 * @param phid An attached phidget LED handle.
 * @param voltage The Output Voltage.
 */
CHDRGET(LED,Voltage,CPhidgetLED_Voltage *voltage)
/**
 * Sets the output voltage. This is for all ouputs.
 * @param phid An attached phidget LED handle.
 * @param voltage The Output Voltage.
 */
CHDRSET(LED,Voltage,CPhidgetLED_Voltage voltage)

#ifndef REMOVE_DEPRECATED
DEP_CHDRGET("Deprecated - use CPhidgetLED_getLEDCount",LED,NumLEDs,int *)
#endif
	
#ifndef EXTERNALPROTO

#define LED_MAXLEDS 64

//OUT Packet Types
#define LED64_NORMAL_PACKET 0x00
#define LED64_CONTROL_PACKET 0x40
#define LED64_OUTLOW_PACKET 0x80
#define LED64_OUTHIGH_PACKET 0xc0

//IN Packet Types
#define LED64_IN_LOW_PACKET 0x00
#define LED64_IN_HIGH_PACKET 0x80

//Flags
#define LED64_PGOOD_FLAG 0x01
#define LED64_CURSELA_FLAG 0x02
#define LED64_CURSELB_FLAG 0x04
#define LED64_PWRSELA_FLAG 0x08
#define LED64_PWRSELB_FLAG 0x10
#define LED64_FAULT_FLAG 0x20
#define LED64_OE_FLAG 0x40

struct _CPhidgetLED 
{
	CPhidget phid;

	int LED_Power[LED_MAXLEDS];
	CPhidgetLED_Voltage voltage;
	CPhidgetLED_CurrentLimit currentLimit;

	int nextLED_Power[LED_MAXLEDS];
	int lastLED_Power[LED_MAXLEDS];
	unsigned char changedLED_Power[LED_MAXLEDS];
	unsigned char changeRequests;

	int LED_PowerEcho[LED_MAXLEDS];
	unsigned char outputEnabledEcho[LED_MAXLEDS];
	unsigned char ledOpenDetectEcho[LED_MAXLEDS];
	unsigned char powerGoodEcho;
	unsigned char outputEnableEcho;
	unsigned char faultEcho;
	CPhidgetLED_Voltage voltageEcho;
	CPhidgetLED_CurrentLimit currentLimitEcho;
	
	unsigned char TSDCount, TSDClearCount, PGoodErrState;
	
	unsigned char controlPacketWaiting;
	unsigned char lastOutputPacket;

} typedef CPhidgetLEDInfo;

#endif

/** @} */
#endif

#ifndef __CPHIDGETTEXTLED
#define __CPHIDGETTEXTLED
#include "cphidget.h"

/** \defgroup phidtextled Phidget TextLED 
 * \ingroup phidgets
 * Calls specific to the Phidget Text LED. See the product manual for more specific API details, supported functionality, units, etc.
 * @{
 */

DPHANDLE(TextLED)
CHDRSTANDARD(TextLED)

/**
 * Gets the number of rows supported by this display.
 * @param phid An attached phidget text led handle.
 * @param count The row count.
 */
CHDRGET(TextLED,RowCount,int *count)
/**
 * Gets the number of columns per supported by this display.
 * @param phid An attached phidget text led handle.
 * @param count The Column count.
 */
CHDRGET(TextLED,ColumnCount,int *count)

/**
 * Gets the last set brightness value.
 * @param phid An attached phidget text led handle.
 * @param brightness The brightness (0-100).
 */
CHDRGET(TextLED,Brightness,int *brightness)
/**
 * Sets the last set brightness value.
 * @param phid An attached phidget text led handle.
 * @param brightness The brightness (0-100).
 */
CHDRSET(TextLED,Brightness,int brightness)

/**
 * Sets a row on the display.
 * @param phid An attached phidget text led handle.
 * @param index The row index.
 * @param displayString The string to display. Make sure this is not longer then \ref CPhidgetTextLED_getColumnCount.
 */
CHDRSETINDEX(TextLED,DisplayString,char *displayString)

#ifndef REMOVE_DEPRECATED
DEP_CHDRGET("Deprecated - use CPhidgetTextLED_getRowCount",TextLED,NumRows,int *)
DEP_CHDRGET("Deprecated - use CPhidgetTextLED_getColumnCount",TextLED,NumColumns,int *)
#endif

#ifndef EXTERNALPROTO
#define TEXTLED_MAXROWS 4
#define TEXTLED_MAXCOLS 8

#define TEXTLED_BRIGHTNESS_PACKET -1
#define TEXTLED_DISPLAYSTRING_PACKET 0

struct _CPhidgetTextLED {
	CPhidget phid;

	char *displayStringPtr[TEXTLED_MAXROWS];
	int brightness;

	unsigned char outputPacket[24];
	unsigned int outputPacketLen;

	char *strings[TEXTLED_MAXROWS];
} typedef CPhidgetTextLEDInfo;
#endif

/** @} */

#endif

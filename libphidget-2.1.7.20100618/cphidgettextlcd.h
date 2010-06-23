#ifndef __CPHIDGETTEXTLCD
#define __CPHIDGETTEXTLCD
#include "cphidget.h"

/** \defgroup phidtextlcd Phidget TextLCD 
 * \ingroup phidgets
 * Calls specific to the Phidget Text LCD. See the product manual for more specific API details, supported functionality, units, etc.
 * @{
 */

DPHANDLE(TextLCD)
CHDRSTANDARD(TextLCD)

/**
 * Gets the number of rows supported by this display.
 * @param phid An attached phidget text lcd handle.
 * @param count The row count.
 */
CHDRGET(TextLCD,RowCount,int *count)
/**
 * Gets the number of columns per supported by this display.
 * @param phid An attached phidget text lcd handle.
 * @param count The Column count.
 */
CHDRGET(TextLCD,ColumnCount,int *count)

/**
 * Gets the state of the backlight.
 * @param phid An attached phidget text lcd handle.
 * @param backlightState The backlight state. Possible values are \ref PTRUE and \ref PFALSE.
 */
CHDRGET(TextLCD,Backlight,int *backlightState)
/**
 * Sets the state of the backlight.
 * @param phid An attached phidget text lcd handle.
 * @param backlightState The backlight state. Possible values are \ref PTRUE and \ref PFALSE.
 */
CHDRSET(TextLCD,Backlight,int backlightState)
/**
 * Gets the brightness of the backlight. Not supported on all TextLCDs
 * @param phid An attached phidget text lcd handle.
 * @param brightness The backlight brightness (0-255).
 */
CHDRGET(TextLCD,Brightness,int *brightness)
/**
 * Sets the brightness of the backlight. Not supported on all TextLCDs
 * @param phid An attached phidget text lcd handle.
 * @param brightness The backlight brightness (0-255).
 */
CHDRSET(TextLCD,Brightness,int brightness)
/**
 * Gets the last set contrast value.
 * @param phid An attached phidget text lcd handle.
 * @param contrast The contrast (0-255).
 */
CHDRGET(TextLCD,Contrast,int *contrast)
/**
 * Sets the last set contrast value.
 * @param phid An attached phidget text lcd handle.
 * @param contrast The contrast (0-255).
 */
CHDRSET(TextLCD,Contrast,int contrast)
/**
 * Gets the cursor visible state.
 * @param phid An attached phidget text lcd handle.
 * @param cursorState The state of the cursor.
 */
CHDRGET(TextLCD,CursorOn,int *cursorState)
/**
 * Sets the cursor visible state.
 * @param phid An attached phidget text lcd handle.
 * @param cursorState The state of the cursor.
 */
CHDRSET(TextLCD,CursorOn,int cursorState)
/**
 * Gets the cursor blink state.
 * @param phid An attached phidget text lcd handle.
 * @param cursorBlinkState The cursor blink state.
 */
CHDRGET(TextLCD,CursorBlink,int *cursorBlinkState)
/**
 * Sets the cursor blink state.
 * @param phid An attached phidget text lcd handle.
 * @param cursorBlinkState The cursor blink state.
 */
CHDRSET(TextLCD,CursorBlink,int cursorBlinkState)

/**
 * Sets a custom character. See the product manual for more information.
 * @param phid An attached phidget text lcd handle.
 * @param index The custom character index (8-15).
 * @param var1 The first part of the custom character.
 * @param var2 The second part of the custom character.
 */
CHDRSETINDEX(TextLCD,CustomCharacter,int var1,int var2)
/**
 * Sets a single character on the display.
 * @param phid An attached phidget text lcd handle.
 * @param index The row index.
 * @param column The column index.
 * @param character The character to display.
 */
CHDRSETINDEX(TextLCD,DisplayCharacter,int column,unsigned char character)
/**
 * Sets a row on the display.
 * @param phid An attached phidget text lcd handle.
 * @param index The row index.
 * @param displayString The string to display. Make sure this is not longer then \ref CPhidgetTextLCD_getColumnCount.
 */
CHDRSETINDEX(TextLCD,DisplayString,char *displayString)

#ifndef REMOVE_DEPRECATED
DEP_CHDRGET("Deprecated - use CPhidgetTextLCD_getRowCount",TextLCD,NumRows,int *)
DEP_CHDRGET("Deprecated - use CPhidgetTextLCD_getColumnCount",TextLCD,NumColumns,int *)
#endif

#ifndef EXTERNALPROTO
#define TEXTLCD_MAXROWS 2
#define TEXTLCD_MAXCOLS 20

#define TEXTLCD_BACKLIGHT_PACKET	0x01
#define TEXTLCD_CONTRAST_PACKET		0x02
#define TEXTLCD_CURSOR_PACKET		0x03

struct _CPhidgetTextLCD {
	CPhidget phid;

	unsigned char m_blnCursorOn,m_blnCursorBlink,m_blnBacklight;
	int m_iContrast, brightness;

	unsigned char outputPacket[8];
	unsigned int outputPacketLen;

	//used for network sets
	char *customs[16];
	char chars[TEXTLCD_MAXROWS * TEXTLCD_MAXCOLS];
	char *strings[TEXTLCD_MAXROWS];
} typedef CPhidgetTextLCDInfo;
#endif

/** @} */

#endif

#include "stdafx.h"
#include "cphidgettextlcd.h"
#include "cusb.h"
#include "csocket.h"
#include "cthread.h"

/*
	Protocol Documentation

	buffer[7] = packet type and screen number
		Top 3 bits are screen number. [SSSCCCCC]
		0x01 - 0x07 = LCD command (data count)
		0x08 - 0x1F = Space for extra packet types:
		0x11 = Backlight, Brightness
			buffer[0] = 0x00 for off, 0x01 for on
			buffer[1] = 0x00 - 0xff (variable brightness where supported)
		0x12 = Contrast
			buffer[0] = 0x00 - 0xff
		0x13 = Init
			re-initializes a display - this can fix a display that's gone wonky, or bring up a display plugged in after power-up
		anything else is ignored

	HD44780-based Character-LCD
	Documentation -		http://home.iae.nl/users/pouweha/lcd/lcd0.shtml#hd44780
						http://www.doc.ic.ac.uk/~ih/doc/lcd/index.html

	buffer[0-6] = LCD command / data
	So sending a packet with any arbitrary set of LCD commands in 0-6 and the command length in 7 is how it works.
	You can combine any number of commands / data in one packet - up to 7 bytes.
		LCD commands - special characters:
			0x00 = escape the next character (can escape 0x00, 0x01, 0x02)
			0x01 = following is commands (clears RS)
			0x02 = following is data (sets RS)

	Always leave in command mode (0x01) when you finish

	So, we can send any command / data, but we can not read back anything (busy flag, CGRAM, DDRAM)

	On our 2x20 display:
		Display Data (DDRAM): Row 0 address 0x00-0x13, Row 1 address 0x40-0x53
		Custom characters (CGRAM): 0x08-0x15 - don't use 0x00-0x07 because 0x00 will terminate displaystring early

*/


// === Internal Functions === //

//clearVars - sets all device variables to unknown state
CPHIDGETCLEARVARS(TextLCD)
	int i;

	for (i = 0; i<TEXTLCD_MAXSCREENS; i++)
	{
		//set data arrays to unknown
		phid->cursorBlink[i] = PUNK_BOOL;
		phid->cursorOn[i] = PUNK_BOOL;
		phid->contrast[i] = PUNK_INT;
		phid->backlight[i] = PUNK_BOOL;
		phid->brightness[i] = PUNK_INT;
		phid->contrastEcho[i] = PUNI_INT;
		phid->backlightEcho[i] = PUNI_BOOL;
		phid->brightnessEcho[i] = PUNI_INT;
		phid->init[i] = 0;
		phid->screenSize[i] = -1;
		phid->rowCount[i] = PUNI_INT;
		phid->columnCount[i] = PUNI_INT;
	}

	return EPHIDGET_OK;
}

//initAfterOpen - sets up the initial state of an object, reading in packets from the device if needed
//				  used during attach initialization - on every attach
CPHIDGETINIT(TextLCD)
	int i;

	TESTPTR(phid);

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_TEXTLCD_2x20:
		case PHIDID_TEXTLCD_2x20_CUSTOM:
		case PHIDID_TEXTLCD_2x20_w_0_8_8:
		case PHIDID_TEXTLCD_2x20_w_8_8_8:
			for (i = 0; i<phid->phid.attr.textlcd.numScreens; i++)
			{
				//set data arrays to unknown
				phid->cursorBlink[i] = PUNK_BOOL;
				phid->cursorOn[i] = PUNK_BOOL;
				phid->contrastEcho[i] = PUNK_INT;
				phid->backlightEcho[i] = PUNK_BOOL;
				phid->brightnessEcho[i] = PUNK_INT;
				phid->screenSize[i] = PHIDGET_TEXTLCD_SCREEN_2x20;
				phid->rowCount[i] = phid->phid.attr.textlcd.numRows;
				phid->columnCount[i] = phid->phid.attr.textlcd.numColumns;
			}
			break;
		case PHIDID_TEXTLCD_ADAPTER:
			for (i = 0; i<phid->phid.attr.textlcd.numScreens; i++)
			{
				//set data arrays to unknown
				phid->cursorBlink[i] = PUNK_BOOL;
				phid->cursorOn[i] = PUNK_BOOL;
				phid->contrastEcho[i] = PUNK_INT;
				phid->backlightEcho[i] = PUNK_BOOL;
				phid->brightnessEcho[i] = PUNK_INT;
				phid->screenSize[i] = PHIDGET_TEXTLCD_SCREEN_UNKNOWN;
				phid->rowCount[i] = 0;
				phid->columnCount[i] = 0;
			}
			break;
		default:
			return EPHIDGET_UNEXPECTED;
	}

	//Make sure no old writes are still pending
	phid->outputPacketLen = 0;

	phid->currentScreen = 0;
	phid->lastScreen = PUNK_INT;

	phid->fullStateEcho = PFALSE;

	//Device specific stuff
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_TEXTLCD_ADAPTER:
			phid->fullStateEcho = PTRUE;
			break;
		case PHIDID_TEXTLCD_2x20:
		case PHIDID_TEXTLCD_2x20_CUSTOM:
		case PHIDID_TEXTLCD_2x20_w_0_8_8:
		case PHIDID_TEXTLCD_2x20_w_8_8_8:
			break;
		default:
			return EPHIDGET_UNEXPECTED;
	}

	//Issue a read if device supports state echo
	if(phid->fullStateEcho)
		CPhidget_read((CPhidgetHandle)phid);

	//set everything to it's echo
	for (i = 0; i<phid->phid.attr.textlcd.numScreens; i++)
	{
		phid->contrast[i] = phid->contrastEcho[i];
		phid->backlight[i] = phid->backlightEcho[i];
		phid->brightness[i] = phid->brightnessEcho[i];
	}

	return EPHIDGET_OK;
}

//dataInput - parses device packets
CPHIDGETDATA(TextLCD)
	int i;

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_TEXTLCD_ADAPTER:
			{
				for (i = 0; i<phid->phid.attr.textlcd.numScreens; i++)
				{
					phid->backlightEcho[i] = buffer[i];
					phid->brightnessEcho[i] = buffer[i+2];
					phid->contrastEcho[i] = buffer[i+4];
				}
			}
			break;
		case PHIDID_TEXTLCD_2x20_w_8_8_8:
		case PHIDID_TEXTLCD_2x20:
		case PHIDID_TEXTLCD_2x20_CUSTOM:
		case PHIDID_TEXTLCD_2x20_w_0_8_8:
		default:
			return EPHIDGET_UNEXPECTED;
	}
	return EPHIDGET_OK;
}

//eventsAfterOpen - sends out an event for all valid data, used during attach initialization - not used
CPHIDGETINITEVENTS(TextLCD)
	phid=0;
	return EPHIDGET_OK;
}

//getPacket - used by write thread to get the next packet to send to device
CGETPACKET_BUF(TextLCD)

//sendpacket - sends a packet to the device asynchronously, blocking if the 1-packet queue is full
CSENDPACKET_BUF(TextLCD)

//makePacket - constructs a packet using current device state
CMAKEPACKETINDEXED(TextLCD)
	int screen = phid->currentScreen;

	TESTPTRS(phid, buffer)

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_TEXTLCD_2x20:
		case PHIDID_TEXTLCD_2x20_CUSTOM:
		case PHIDID_TEXTLCD_2x20_w_0_8_8:
		case PHIDID_TEXTLCD_2x20_w_8_8_8:
		case PHIDID_TEXTLCD_ADAPTER:
			switch(Index)
			{
				case TEXTLCD_BACKLIGHT_PACKET: //backlight

					//if unknown then this must have been called from setBrightness - they still need to enable the backlight
					if (phid->backlight[screen] == PUNK_BOOL)
					{
						phid->backlight[screen] = PFALSE;
						if(!phid->fullStateEcho)
							phid->backlightEcho[screen] = phid->backlight[screen];
					}

					buffer[0] = phid->backlight[screen];

					//Brightness where supported
					switch(phid->phid.deviceIDSpec)
					{
						case PHIDID_TEXTLCD_2x20_w_8_8_8:
							if (phid->phid.deviceVersion < 200)
								break;
						case PHIDID_TEXTLCD_ADAPTER:
							//default the brightness to full
							if (phid->brightness[screen] == PUNK_INT)
							{
								phid->brightness[screen] = 255;
								if(!phid->fullStateEcho)
									phid->brightnessEcho[screen] = phid->brightness[screen];
							}
							buffer[1] = phid->brightness[screen];
						default:
							break;
					}

					buffer[7] = 0x11 | (screen << 5);

					break;
				case TEXTLCD_CONTRAST_PACKET: //contrast
					buffer[0] = (unsigned char)phid->contrast[screen];
					buffer[7] = 0x12 | (screen << 5);

					break;
				case TEXTLCD_INIT_PACKET: //re-init a screen
					if(phid->screenSize[screen] == PHIDGET_TEXTLCD_SCREEN_4x40 && screen == 0)
					{
						buffer[7] = 0x13 | (2 << 5); //screen '2' represents both screens 0 and 1
						phid->cursorOn[1] = PFALSE;
						phid->cursorBlink[1] = PFALSE;
					}
					else
						buffer[7] = 0x13 | (screen << 5);

					phid->lastScreen = PUNK_INT;
					phid->cursorOn[screen] = PFALSE;
					phid->cursorBlink[screen] = PFALSE;

					break;
				case TEXTLCD_CURSOR_PACKET: //LCD commands - Cursor

					if (phid->cursorOn[screen] == PUNK_BOOL)
					{
						phid->cursorOn[screen] = PFALSE;
					}

					if (phid->cursorBlink[screen] == PUNK_BOOL)
					{
						phid->cursorBlink[screen] = PFALSE;
					}

					buffer[0] = 0x0C;
					if (phid->cursorOn[screen]) buffer[0] |= 0x02;
					if (phid->cursorBlink[screen]) buffer[0] |= 0x01;

					//Special case - 4x40, rows 2 and 3
					if(phid->screenSize[0] == PHIDGET_TEXTLCD_SCREEN_4x40 && screen == 0 && phid->lastScreen == 1)
						buffer[7] = 0x01 | (1 << 5);
					else
						buffer[7] = 0x01 | (screen << 5);

					break;
				default:
					return EPHIDGET_UNEXPECTED;
			}
			break;
		default:
			return EPHIDGET_UNEXPECTED;
	}

	return EPHIDGET_OK;
}

/* deals with moving the cursor/blink between the 'screens' on a 4x40 setup */
static void dealWithCursor(CPhidgetTextLCDHandle phid, int screen)
{
	unsigned char buffer[10];
	//Switching between screens for special 4x40 case
	if(phid->screenSize[0] == PHIDGET_TEXTLCD_SCREEN_4x40)
	{
		if(phid->cursorOn[0] == PTRUE || phid->cursorBlink[0] == PTRUE)
		{
			//Move cursor from screen 0 to screen 1
			if(phid->lastScreen == 0 && screen == 1)
			{
				buffer[0] = 0x0C;	
				buffer[7] = 0x01;

				CThread_mutex_lock(&phid->phid.writelock);
				if (CPhidgetTextLCD_sendpacket(phid, buffer) != EPHIDGET_OK)
				{
					CThread_mutex_unlock(&phid->phid.writelock);
					return;
				}

				buffer[0] = 0x0C;
				if (phid->cursorOn[0]) buffer[0] |= 0x02;
				if (phid->cursorBlink[0]) buffer[0] |= 0x01;		
				buffer[7] = 0x01 | (1 << 5);

				CPhidgetTextLCD_sendpacket(phid, buffer);
				CThread_mutex_unlock(&phid->phid.writelock);
			}
			//Move cursor from screen 1 to screen 0
			else if(phid->lastScreen == 1 && screen == 0)
			{
				buffer[0] = 0x0C;	
				buffer[7] = 0x01 | (1 << 5);

				CThread_mutex_lock(&phid->phid.writelock);
				if (CPhidgetTextLCD_sendpacket(phid, buffer) != EPHIDGET_OK)
				{
					CThread_mutex_unlock(&phid->phid.writelock);
					return;
				}

				buffer[0] = 0x0C;
				if (phid->cursorOn[0]) buffer[0] |= 0x02;
				if (phid->cursorBlink[0]) buffer[0] |= 0x01;		
				buffer[7] = 0x01;

				CPhidgetTextLCD_sendpacket(phid, buffer);
				CThread_mutex_unlock(&phid->phid.writelock);
			}
		}
	}
	phid->lastScreen = screen;
}

static int getScreenAndPos(CPhidgetTextLCDHandle phid, int *screen, int row, int col)
{
	int rows = phid->rowCount[*screen];
	int cols = phid->columnCount[*screen];
	int pos = 0;

	switch(row)
	{
		case 0:
			pos = 0x00;
			break;
		case 1:
			pos = 0x40;
			break;
		case 2:
			if(cols == 16)
			{
				pos = 0x10;
			}
			else if(cols == 40)
			{
				pos = 0x00;
				*screen = 1;
			}
			else	
			{
				pos = 0x14;
			}
			break;
		case 3:
			if(cols == 16)
			{
				pos = 0x50;
			}
			else if(cols == 40)
			{
				pos = 0x40;
				*screen = 1;
			}
			else	
			{
				pos = 0x54;
			}
			break;
	}

	return pos+col;
}

// === Exported Functions === //

//create and initialize a device structure
CCREATE(TextLCD, PHIDCLASS_TEXTLCD)

CGET(TextLCD,RowCount,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	MASGN(rowCount[phid->currentScreen])
}

CGET(TextLCD,ColumnCount,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	MASGN(columnCount[phid->currentScreen])
}

CGET(TextLCD,ScreenCount,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	MASGN(phid.attr.textlcd.numScreens)
}

CGET(TextLCD,Backlight,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTMASGN(backlightEcho[phid->currentScreen], PUNK_BOOL)

	MASGN(backlightEcho[phid->currentScreen])
}
CSET(TextLCD,Backlight,int)
	int Index;
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTRANGE(PFALSE, PTRUE)

	Index = phid->currentScreen;

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEYINDEXED(Backlight, "%d", backlight);
	else
	{
		SENDPACKETINDEXED(TextLCD, backlight[Index], TEXTLCD_BACKLIGHT_PACKET);

		if(!phid->fullStateEcho)
			phid->backlightEcho[Index] = phid->backlight[Index];
	}

	return EPHIDGET_OK;
}

CGET(TextLCD,Brightness,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_TEXTLCD_2x20_w_8_8_8:
			if (phid->phid.deviceVersion < 200)
				return EPHIDGET_UNSUPPORTED;
			break;
		case PHIDID_TEXTLCD_ADAPTER:
			break;
		case PHIDID_TEXTLCD_2x20:
		case PHIDID_TEXTLCD_2x20_CUSTOM:
		case PHIDID_TEXTLCD_2x20_w_0_8_8:
			return EPHIDGET_UNSUPPORTED;
		default:
			return EPHIDGET_UNEXPECTED;
	}

	TESTMASGN(brightnessEcho[phid->currentScreen], PUNK_INT)

	MASGN(brightnessEcho[phid->currentScreen])
}
CSET(TextLCD,Brightness,int)
	int Index;
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	Index = phid->currentScreen;
	
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_TEXTLCD_2x20_w_8_8_8:
			if (phid->phid.deviceVersion < 200)
				return EPHIDGET_UNSUPPORTED;
			break;
		case PHIDID_TEXTLCD_ADAPTER:
			break;
		case PHIDID_TEXTLCD_2x20:
		case PHIDID_TEXTLCD_2x20_CUSTOM:
		case PHIDID_TEXTLCD_2x20_w_0_8_8:
			return EPHIDGET_UNSUPPORTED;
		default:
			return EPHIDGET_UNEXPECTED;
	}

	TESTRANGE(0, 255)

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEYINDEXED(Brightness, "%d", brightness);
	else
	{
		SENDPACKETINDEXED(TextLCD, brightness[Index], TEXTLCD_BACKLIGHT_PACKET);

		if(!phid->fullStateEcho)
			phid->brightnessEcho[Index] = phid->brightness[Index];
	}

	return EPHIDGET_OK;
}

CGET(TextLCD,Contrast,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTMASGN(contrastEcho[phid->currentScreen], PUNK_INT)

	MASGN(contrastEcho[phid->currentScreen])
}
CSET(TextLCD,Contrast,int)
	int Index;
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTRANGE(0, 255)

	Index = phid->currentScreen;

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEYINDEXED(Contrast, "%d", contrast);
	else
	{
		SENDPACKETINDEXED(TextLCD, contrast[Index], TEXTLCD_CONTRAST_PACKET);

		if(!phid->fullStateEcho)
			phid->contrastEcho[Index] = phid->contrast[Index];
	}

	return EPHIDGET_OK;
}

CGET(TextLCD,CursorOn,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTMASGN(cursorOn[phid->currentScreen], PUNK_BOOL)

	MASGN(cursorOn[phid->currentScreen])
}
CSET(TextLCD,CursorOn,int)
	int Index;
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTRANGE(PFALSE, PTRUE)

	Index = phid->currentScreen;

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEYINDEXED(CursorOn, "%d", cursorOn);
	else
		SENDPACKETINDEXED(TextLCD, cursorOn[Index], TEXTLCD_CURSOR_PACKET);

	return EPHIDGET_OK;
}

CGET(TextLCD,CursorBlink,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTMASGN(cursorBlink[phid->currentScreen], PUNK_BOOL)

	MASGN(cursorBlink[phid->currentScreen])
}
CSET(TextLCD,CursorBlink,int)
	int Index;
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTRANGE(PFALSE, PTRUE)

	Index = phid->currentScreen;

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEYINDEXED(CursorBlink, "%d", cursorBlink);
	else
		SENDPACKETINDEXED(TextLCD, cursorBlink[Index], TEXTLCD_CURSOR_PACKET);

	return EPHIDGET_OK;
}

CGET(TextLCD,Screen,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	MASGN(currentScreen)
}
CSET(TextLCD,Screen,int)
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTRANGE(0, phid->phid.attr.textlcd.numScreens)

	//This prevents a screen size change during a multi-packet write
	CThread_mutex_lock(&phid->phid.writelock);
	phid->currentScreen = newVal;
	CThread_mutex_unlock(&phid->phid.writelock);

	return EPHIDGET_OK;
}

CGET(TextLCD,ScreenSize,CPhidgetTextLCD_ScreenSize)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTMASGN(screenSize[phid->currentScreen], PHIDGET_TEXTLCD_SCREEN_UNKNOWN)

	MASGN(screenSize[phid->currentScreen])
}
CSET(TextLCD,ScreenSize,CPhidgetTextLCD_ScreenSize)
	int otherScreen;
	int screen;
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_TEXTLCD_ADAPTER:
			break;
		case PHIDID_TEXTLCD_2x20_w_8_8_8:
		case PHIDID_TEXTLCD_2x20:
		case PHIDID_TEXTLCD_2x20_CUSTOM:
		case PHIDID_TEXTLCD_2x20_w_0_8_8:
			return EPHIDGET_UNSUPPORTED;
		default:
			return EPHIDGET_UNEXPECTED;
	}

	TESTRANGE(PHIDGET_TEXTLCD_SCREEN_NONE, PHIDGET_TEXTLCD_SCREEN_4x40)

	screen = phid->currentScreen;

	otherScreen = 1;
	if(screen == 1)
		otherScreen = 0;

	switch(newVal)
	{
		case PHIDGET_TEXTLCD_SCREEN_NONE:
			phid->rowCount[screen] = 0;
			phid->columnCount[screen] = 0;
			break;
		case PHIDGET_TEXTLCD_SCREEN_1x8:
			phid->rowCount[screen] = 1;
			phid->columnCount[screen] = 8;
			break;
		case PHIDGET_TEXTLCD_SCREEN_2x8:
			phid->rowCount[screen] = 2;
			phid->columnCount[screen] = 8;
			break;
		case PHIDGET_TEXTLCD_SCREEN_1x16:
			phid->rowCount[screen] = 1;
			phid->columnCount[screen] = 16;
			break;
		case PHIDGET_TEXTLCD_SCREEN_2x16:
			phid->rowCount[screen] = 2;
			phid->columnCount[screen] = 16;
			break;
		case PHIDGET_TEXTLCD_SCREEN_4x16:
			phid->rowCount[screen] = 4;
			phid->columnCount[screen] = 16;
			break;
		case PHIDGET_TEXTLCD_SCREEN_2x20:
			phid->rowCount[screen] = 2;
			phid->columnCount[screen] = 20;
			break;
		case PHIDGET_TEXTLCD_SCREEN_4x20:
			phid->rowCount[screen] = 4;
			phid->columnCount[screen] = 20;
			break;
		case PHIDGET_TEXTLCD_SCREEN_2x24:
			phid->rowCount[screen] = 2;
			phid->columnCount[screen] = 24;
			break;
		case PHIDGET_TEXTLCD_SCREEN_1x40:
			phid->rowCount[screen] = 1;
			phid->columnCount[screen] = 40;
			break;
		case PHIDGET_TEXTLCD_SCREEN_2x40:
			phid->rowCount[screen] = 2;
			phid->columnCount[screen] = 40;
			break;
		case PHIDGET_TEXTLCD_SCREEN_4x40:
			//Only supported on screen 0
			if(screen != 0)
				return EPHIDGET_UNSUPPORTED;

			phid->rowCount[screen] = 4;
			phid->columnCount[screen] = 40;

			//sets other screen to none
			phid->rowCount[otherScreen] = 0;
			phid->columnCount[otherScreen] = 0;
			//set screen size
			if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
			{
				int newVal = PHIDGET_TEXTLCD_SCREEN_NONE;
				int Index = otherScreen;
				ADDNETWORKKEYINDEXED(ScreenSize, "%d", screenSize);
			}
			else
			{
				phid->screenSize[otherScreen] = PHIDGET_TEXTLCD_SCREEN_NONE;
			}
			break;
		default:
			return EPHIDGET_UNEXPECTED;
	}
	
	phid->lastScreen = PUNK_INT;

	//can't have a 4x40 with anything other then NONE
	if(phid->screenSize[otherScreen] == PHIDGET_TEXTLCD_SCREEN_4x40 && newVal != PHIDGET_TEXTLCD_SCREEN_NONE)
	{
		//sets other screen to none
		phid->rowCount[otherScreen] = 0;
		phid->columnCount[otherScreen] = 0;
		//set screen size
		if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		{
			int newVal = PHIDGET_TEXTLCD_SCREEN_NONE;
			int Index = otherScreen;
			ADDNETWORKKEYINDEXED(ScreenSize, "%d", screenSize);
		}
		else
		{
			phid->screenSize[otherScreen] = PHIDGET_TEXTLCD_SCREEN_NONE;
		}
	}
	
	//set screen size
	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
	{
		int Index = screen;
		ADDNETWORKKEYINDEXED(ScreenSize, "%d", screenSize);
	}
	else
	{
		phid->screenSize[screen] = newVal;
	}
	
	return EPHIDGET_OK;
}


PHIDGET21_API int CCONV CPhidgetTextLCD_setDisplayCharacter (CPhidgetTextLCDHandle phid, int Row, int Column, unsigned char Character)
{
	unsigned char buffer[10];
	int ret;
	int screen;

	TESTPTR(phid)
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	if (Row >= phid->rowCount[phid->currentScreen] || Row < 0) return EPHIDGET_OUTOFBOUNDS;
	if (Column >= phid->columnCount[phid->currentScreen] || Column < 0) return EPHIDGET_OUTOFBOUNDS;

	screen = phid->currentScreen;

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
	{
		char newVal = Character;
		//int Index = (Row + 1) * (Column + 1);
		int Index = (Column << 16) + (Row << 8) + screen;
		ADDNETWORKKEYINDEXED(DisplayCharacter, "%c", chars[screen]);
	}
	else
	{
		switch(phid->phid.deviceIDSpec)
		{
			case PHIDID_TEXTLCD_2x20:
			case PHIDID_TEXTLCD_2x20_CUSTOM:
			case PHIDID_TEXTLCD_2x20_w_0_8_8:
			case PHIDID_TEXTLCD_2x20_w_8_8_8:
			case PHIDID_TEXTLCD_ADAPTER:
				buffer[0] = 0x01;
				buffer[1] = getScreenAndPos(phid, &screen, Row, Column) | 0x80; /* Address */
				buffer[2] = 0x02;
				buffer[3] = Character;
				buffer[4] = 0x01;
				buffer[7] = 0x05 | (screen << 5);

				CThread_mutex_lock(&phid->phid.writelock);
				ret = CPhidgetTextLCD_sendpacket(phid, buffer);
				CThread_mutex_unlock(&phid->phid.writelock);
				dealWithCursor(phid, screen);
				return ret;
				break;
			default:
				return EPHIDGET_UNEXPECTED;
		}
	}
	return EPHIDGET_OK;

}

PHIDGET21_API int CCONV CPhidgetTextLCD_setCustomCharacter (CPhidgetTextLCDHandle phid, int Index, int Val1, int Val2)
{
	unsigned char buffer[10];
	int ret;
	char newVal[50];
	int screen;

	TESTPTR(phid)
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	if ((Index < 8) || (Index > 15)) return EPHIDGET_INVALIDARG;

	screen = phid->currentScreen;

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
	{
		Index = (Index << 8) + screen;
		sprintf(newVal, "%d,%d", Val1, Val2);
		ADDNETWORKKEYINDEXED(CustomCharacter, "%s", customs[screen]);
	}
	else
	{
		switch(phid->phid.deviceIDSpec)
		{
			case PHIDID_TEXTLCD_2x20:
			case PHIDID_TEXTLCD_2x20_CUSTOM:
			case PHIDID_TEXTLCD_2x20_w_0_8_8:
			case PHIDID_TEXTLCD_2x20_w_8_8_8:
			case PHIDID_TEXTLCD_ADAPTER:
				buffer[0] = 0x01;
				buffer[1] = Index * 8;
				buffer[2] = 0x02;
				buffer[3] = (Val1 & 0x1F) | 0x80;
				buffer[4] = ((Val1 >> 5) & 0x1F) | 0x80;
				buffer[5] = ((Val1 >> 10) & 0x1F) | 0x80;
				buffer[6] = ((Val1 >> 15) & 0x1F) | 0x80;
				buffer[7] = 0x07 | (screen << 5);

				CThread_mutex_lock(&phid->phid.writelock);
				if ((ret = CPhidgetTextLCD_sendpacket(phid, buffer)) != EPHIDGET_OK)
				{
					CThread_mutex_unlock(&phid->phid.writelock);
					return ret;
				}

				buffer[0] = (Val2 & 0x1F) | 0x80;
				buffer[1] = ((Val2 >> 5) & 0x1F) | 0x80;
				buffer[2] = ((Val2 >> 10) & 0x1F) | 0x80;
				buffer[3] = ((Val2 >> 15) & 0x1F) | 0x80;
				buffer[4] = 0x01;
				buffer[5] = 0;
				buffer[6] = 0;
				buffer[7] = 0x05 | (screen << 5);

				ret = CPhidgetTextLCD_sendpacket(phid, buffer);
				CThread_mutex_unlock(&phid->phid.writelock);

				//TODO: for 4x40, we need to set the same custom charaster for both 'screens' so users can use them across the entire screen
				if(phid->screenSize[0] == PHIDGET_TEXTLCD_SCREEN_4x40 && screen==0)
				{
					buffer[0] = 0x01;
					buffer[1] = Index * 8;
					buffer[2] = 0x02;
					buffer[3] = (Val1 & 0x1F) | 0x80;
					buffer[4] = ((Val1 >> 5) & 0x1F) | 0x80;
					buffer[5] = ((Val1 >> 10) & 0x1F) | 0x80;
					buffer[6] = ((Val1 >> 15) & 0x1F) | 0x80;
					buffer[7] = 0x07 | (1 << 5);

					CThread_mutex_lock(&phid->phid.writelock);
					if ((ret = CPhidgetTextLCD_sendpacket(phid, buffer)) != EPHIDGET_OK)
					{
						CThread_mutex_unlock(&phid->phid.writelock);
						return ret;
					}

					buffer[0] = (Val2 & 0x1F) | 0x80;
					buffer[1] = ((Val2 >> 5) & 0x1F) | 0x80;
					buffer[2] = ((Val2 >> 10) & 0x1F) | 0x80;
					buffer[3] = ((Val2 >> 15) & 0x1F) | 0x80;
					buffer[4] = 0x01;
					buffer[5] = 0;
					buffer[6] = 0;
					buffer[7] = 0x05 | (1 << 5);

					ret = CPhidgetTextLCD_sendpacket(phid, buffer);
					CThread_mutex_unlock(&phid->phid.writelock);
				}

				return ret;
				break;
			default:
				return EPHIDGET_UNEXPECTED;
		}
	}
	return EPHIDGET_OK;

}

CSETINDEX(TextLCD, DisplayString, char *)
	int i,j, buf_ptr,result=EPHIDGET_OK;
	unsigned int ui;
	size_t len;
	unsigned char form_buffer[250];
	unsigned char buffer[10];
	int screen, screenEnable, pos;

	TESTPTR(phid)
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTINDEX(rowCount[phid->currentScreen])

	if (strlen(newVal) > (size_t)phid->columnCount[phid->currentScreen]) return EPHIDGET_INVALIDARG;

	screen = screenEnable = phid->currentScreen;

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
	{
		Index = (Index << 8) + screen;
		ADDNETWORKKEYINDEXED(DisplayString, "%s", strings[screen]);
	}
	else
	{
		switch(phid->phid.deviceIDSpec)
		{
			case PHIDID_TEXTLCD_2x20:
			case PHIDID_TEXTLCD_2x20_CUSTOM:
			case PHIDID_TEXTLCD_2x20_w_0_8_8:
			case PHIDID_TEXTLCD_2x20_w_8_8_8:
			case PHIDID_TEXTLCD_ADAPTER:
				len = strlen(newVal);
				if (len > phid->columnCount[screen]) len = phid->columnCount[screen];

				pos = getScreenAndPos(phid, &screenEnable, Index, 0);
			
				form_buffer[0] = 0x01; /* Command */
				form_buffer[1] = pos | 0x80; /* Address */
				form_buffer[2] = 0x02; /* Data */
				buf_ptr = 3;
				//Escape 0x01, 0x02
				for (ui = 0; ui<len; ui++)
				{
					if ((newVal[ui] == 0x01) || (newVal[ui] == 0x02))
						form_buffer[buf_ptr++] = 0x00;
					form_buffer[buf_ptr++] = newVal[ui];
				}
				// fill the rest with spaces
				for (ui = 0; ui < (phid->columnCount[screen] - len); ui++)
				{
					form_buffer[buf_ptr++] = 0x20;
				}
				form_buffer[buf_ptr++] = 0x01; /* Back into command mode */
				form_buffer[buf_ptr++] = pos + (unsigned char)strlen(newVal) | 0x80;  /* set cursor position? */

				CThread_mutex_lock(&phid->phid.writelock);

				for (i = 0; i<buf_ptr; i+=7)
				{
					for (j = 0; j<8; j++)
						buffer[j] = 0;
					if ((buf_ptr - i) > 7) len = 7;
					else len = (buf_ptr - i);
					for (ui = 0; ui < len; ui++)
						buffer[ui] = form_buffer[i + ui];
					buffer[7] = (unsigned char)len | (screenEnable << 5);
					
					if ((result = CPhidgetTextLCD_sendpacket(phid, buffer)) != EPHIDGET_OK)
					{
						CThread_mutex_unlock(&phid->phid.writelock);
						return result;
					}
				}
				
				CThread_mutex_unlock(&phid->phid.writelock);
				dealWithCursor(phid, screenEnable);
				return result;
			default:
				return EPHIDGET_UNEXPECTED;
		}
	}
	return EPHIDGET_OK;
}

PHIDGET21_API int CCONV CPhidgetTextLCD_initialize(CPhidgetTextLCDHandle phid)
{
	int Index, newVal=1;
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	Index = phid->currentScreen;
	
	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_TEXTLCD_ADAPTER:
			break;
		case PHIDID_TEXTLCD_2x20:
		case PHIDID_TEXTLCD_2x20_CUSTOM:
		case PHIDID_TEXTLCD_2x20_w_0_8_8:
		case PHIDID_TEXTLCD_2x20_w_8_8_8:
			return EPHIDGET_UNSUPPORTED;
		default:
			return EPHIDGET_UNEXPECTED;
	}

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
	{
		int newVal = phid->init[Index]^1;
		ADDNETWORKKEYINDEXED(Init, "%d", init);
	}
	else
		SENDPACKETINDEXED(TextLCD, init[Index], TEXTLCD_INIT_PACKET);

	return EPHIDGET_OK;
}

// === Deprecated Functions === //

CGET(TextLCD,NumRows,int)
	return CPhidgetTextLCD_getRowCount(phid, pVal);
}
CGET(TextLCD,NumColumns,int)
	return CPhidgetTextLCD_getColumnCount(phid, pVal);
}

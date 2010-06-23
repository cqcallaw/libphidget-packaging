#include "stdafx.h"
#include "cphidgettextlcd.h"
#include "cusb.h"
#include "csocket.h"
#include "cthread.h"

/*
	Protocol Documentation

	buffer[7] = packet type
		0x11 = Backlight
			buffer[0] = 0x00 for off, 0x01 for on
		0x12 = Contrast
			buffer[0] = 0x00 - 0xff
		0x01 - 0x07 = LCD command (data count)
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

	phid->m_blnBacklight = PUNK_BOOL;
	phid->m_blnCursorBlink = PUNK_BOOL;
	phid->m_blnCursorOn = PUNK_BOOL;
	phid->m_iContrast = PUNK_INT;
	phid->brightness = PUNK_INT;

	return EPHIDGET_OK;
}

//initAfterOpen - sets up the initial state of an object, reading in packets from the device if needed
//				  used during attach initialization - on every attach
CPHIDGETINIT(TextLCD)
	TESTPTR(phid);

	//set data arrays to unknown
	phid->m_blnCursorBlink = PUNK_BOOL;
	phid->m_blnCursorOn = PUNK_BOOL;
	phid->m_iContrast = PUNK_INT;
	phid->m_blnBacklight = PUNK_BOOL;

	//Defaults to full brightness
	phid->brightness = 255;

	//make sure there's no outstanding pending packets!
	phid->outputPacketLen=0;

	return EPHIDGET_OK;
}

//dataInput - parses device packets - not used
CPHIDGETDATA(TextLCD)
	phid = 0;
	return EPHIDGET_OK;
}

//eventsAfterOpen - sends out an event for all valid data, used during attach initialization - not used
CPHIDGETINITEVENTS(TextLCD)
	phid = 0;
	return EPHIDGET_OK;
}

//getPacket - used by write thread to get the next packet to send to device
CGETPACKET_BUF(TextLCD)

//sendpacket - sends a packet to the device asynchronously, blocking if the 1-packet queue is full
CSENDPACKET_BUF(TextLCD)

//makePacket - constructs a packet using current device state
CMAKEPACKETINDEXED(TextLCD)

	TESTPTRS(phid, buffer)

	switch(phid->phid.deviceIDSpec)
	{
		case PHIDID_TEXTLCD_2x20:
		case PHIDID_TEXTLCD_2x20_CUSTOM:
		case PHIDID_TEXTLCD_2x20_w_0_8_8:
		case PHIDID_TEXTLCD_2x20_w_8_8_8:
			switch(Index)
			{
				case TEXTLCD_BACKLIGHT_PACKET: //backlight
					buffer[0] = phid->m_blnBacklight;
					if (phid->phid.deviceVersion >= 200 && phid->phid.deviceVersion < 300)
						buffer[1] = phid->brightness;
					buffer[7] = 0x11;

					break;
				case TEXTLCD_CONTRAST_PACKET: //contrast
					buffer[0] = (unsigned char)phid->m_iContrast;
					buffer[7] = 0x12;

					break;
				case TEXTLCD_CURSOR_PACKET: //LCD commands - Cursor
					if (phid->m_blnCursorOn == PUNK_BOOL) phid->m_blnCursorOn = PFALSE;
					if (phid->m_blnCursorBlink == PUNK_BOOL) phid->m_blnCursorBlink = PFALSE;

					buffer[0] = 0x0C;
					if (phid->m_blnCursorOn) buffer[0] |= 0x02;
					if (phid->m_blnCursorBlink) buffer[0] |= 0x01;		
					buffer[7] = 0x01;

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

// === Exported Functions === //

//create and initialize a device structure
CCREATE(TextLCD, PHIDCLASS_TEXTLCD)

CGET(TextLCD,RowCount,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	MASGN(phid.attr.textlcd.numRows)
}

CGET(TextLCD,ColumnCount,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	MASGN(phid.attr.textlcd.numColumns)
}

CGET(TextLCD,Backlight,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTMASGN(m_blnBacklight, PUNK_BOOL)

	MASGN(m_blnBacklight)
}
CSET(TextLCD,Backlight,int)
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTRANGE(PFALSE, PTRUE)

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEY(Backlight, "%d", m_blnBacklight);
	else
		SENDPACKETINDEXED(TextLCD, m_blnBacklight, TEXTLCD_BACKLIGHT_PACKET);

	return EPHIDGET_OK;
}

CGET(TextLCD,Brightness,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	
	if (phid->phid.deviceVersion < 200)
		return EPHIDGET_UNSUPPORTED;

	TESTMASGN(brightness, PUNK_INT)

	MASGN(brightness)
}
CSET(TextLCD,Brightness,int)
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	if (phid->phid.deviceVersion < 200)
		return EPHIDGET_UNSUPPORTED;

	TESTRANGE(0, 255)

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEY(Brightness, "%d", brightness);
	else
		SENDPACKETINDEXED(TextLCD, brightness, TEXTLCD_BACKLIGHT_PACKET);

	return EPHIDGET_OK;
}

CGET(TextLCD,Contrast,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTMASGN(m_iContrast, PUNK_INT)

	MASGN(m_iContrast)
}
CSET(TextLCD,Contrast,int)
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTRANGE(0, 255)

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEY(Contrast, "%d", m_iContrast);
	else
		SENDPACKETINDEXED(TextLCD, m_iContrast, TEXTLCD_CONTRAST_PACKET);

	return EPHIDGET_OK;
}

CGET(TextLCD,CursorOn,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTMASGN(m_blnCursorOn, PUNK_BOOL)

	MASGN(m_blnCursorOn)
}
CSET(TextLCD,CursorOn,int)
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTRANGE(PFALSE, PTRUE)

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEY(CursorOn, "%d", m_blnCursorOn);
	else
		SENDPACKETINDEXED(TextLCD, m_blnCursorOn, TEXTLCD_CURSOR_PACKET);

	return EPHIDGET_OK;
}

CGET(TextLCD,CursorBlink,int)
	TESTPTRS(phid,pVal) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTMASGN(m_blnCursorBlink, PUNK_BOOL)

	MASGN(m_blnCursorBlink)
}
CSET(TextLCD,CursorBlink,int)
	TESTPTR(phid) 
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTRANGE(PFALSE, PTRUE)

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEY(CursorBlink, "%d", m_blnCursorBlink);
	else
		SENDPACKETINDEXED(TextLCD, m_blnCursorBlink, TEXTLCD_CURSOR_PACKET);

	return EPHIDGET_OK;
}

PHIDGET21_API int CCONV CPhidgetTextLCD_setDisplayCharacter (CPhidgetTextLCDHandle phid, int Row, int Column, unsigned char Character)
{
	unsigned char buffer[10];
	int ret;

	TESTPTR(phid)
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	if (Row >= phid->phid.attr.textlcd.numRows || Row < 0) return EPHIDGET_OUTOFBOUNDS;
	if (Column >= phid->phid.attr.textlcd.numColumns || Column < 0) return EPHIDGET_OUTOFBOUNDS;

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
	{
		char newVal = Character;
		int Index = (Row + 1) * (Column +1);
		ADDNETWORKKEYINDEXED(DisplayCharacter, "%c", chars);
	}
	else
	{
		switch(phid->phid.deviceIDSpec)
		{
			case PHIDID_TEXTLCD_2x20:
			case PHIDID_TEXTLCD_2x20_CUSTOM:
			case PHIDID_TEXTLCD_2x20_w_0_8_8:
			case PHIDID_TEXTLCD_2x20_w_8_8_8:
				buffer[0] = 0x01;
				buffer[1] = ((unsigned char)Row) * 0x40 + 0x80 + Column; /* Address */
				buffer[2] = 0x02;
				buffer[3] = Character;
				buffer[4] = 0x01;
				buffer[7] = 0x05;

				CThread_mutex_lock(&phid->phid.writelock);
				ret = CPhidgetTextLCD_sendpacket(phid, buffer);
				CThread_mutex_unlock(&phid->phid.writelock);
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

	TESTPTR(phid)
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED

	if ((Index < 8) || (Index > 15)) return EPHIDGET_INVALIDARG;

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
	{
		sprintf(newVal, "%d,%d", Val1, Val2);
		ADDNETWORKKEYINDEXED(CustomCharacter, "%s", customs);
	}
	else
	{
		switch(phid->phid.deviceIDSpec)
		{
			case PHIDID_TEXTLCD_2x20:
			case PHIDID_TEXTLCD_2x20_CUSTOM:
			case PHIDID_TEXTLCD_2x20_w_0_8_8:
			case PHIDID_TEXTLCD_2x20_w_8_8_8:
				buffer[0] = 0x01;
				buffer[1] = Index * 8;
				buffer[2] = 0x02;
				buffer[3] = (Val1 & 0x1F) | 0x80;
				buffer[4] = ((Val1 >> 5) & 0x1F) | 0x80;
				buffer[5] = ((Val1 >> 10) & 0x1F) | 0x80;
				buffer[6] = ((Val1 >> 15) & 0x1F) | 0x80;
				buffer[7] = 0x07;

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
				buffer[7] = 0x05;

				ret = CPhidgetTextLCD_sendpacket(phid, buffer);
				CThread_mutex_unlock(&phid->phid.writelock);
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
	unsigned char form_buffer[50];
	unsigned char buffer[10];

	TESTPTR(phid)
	TESTDEVICETYPE(PHIDCLASS_TEXTLCD)
	TESTATTACHED
	TESTINDEX(phid.attr.textlcd.numRows)

	if (strlen(newVal) > (size_t)phid->phid.attr.textlcd.numColumns) return EPHIDGET_INVALIDARG;

	if(CPhidget_statusFlagIsSet(phid->phid.status, PHIDGET_REMOTE_FLAG))
		ADDNETWORKKEYINDEXED(DisplayString, "%s", strings);
	else
	{
		switch(phid->phid.deviceIDSpec)
		{
			case PHIDID_TEXTLCD_2x20:
			case PHIDID_TEXTLCD_2x20_CUSTOM:
			case PHIDID_TEXTLCD_2x20_w_0_8_8:
			case PHIDID_TEXTLCD_2x20_w_8_8_8:
				len = strlen(newVal);
				if (len > 20) len = 20;
			
				form_buffer[0] = 0x01; /* Command */
				form_buffer[1] = ((unsigned char)Index) * 0x40 + 0x80; /* Address */
				form_buffer[2] = 0x02; /* Data */
				buf_ptr = 3;
				//Escape 0x01, 0x02
				for (ui = 0; ui<len; ui++)
				{
					if ((newVal[ui] == 0x01) || (newVal[ui] == 0x02))
						form_buffer[buf_ptr++] = 0x00;
					form_buffer[buf_ptr++] = newVal[ui];
				}
				// ?
				for (ui = 0; ui < (20 - len); ui++)
				{
					form_buffer[buf_ptr++] = 0x20;
				}
				form_buffer[buf_ptr++] = 0x01; /* Back into command mode */
				form_buffer[buf_ptr++] = ((unsigned char)Index) * 0x40 + 0x80 + (unsigned char)strlen(newVal);  // ?

				CThread_mutex_lock(&phid->phid.writelock);

				for (i = 0; i<buf_ptr; i+=7)
				{
					for (j = 0; j<8; j++)
						buffer[j] = 0;
					if ((buf_ptr - i) > 7) len = 7;
					else len = (buf_ptr - i);
					for (ui = 0; ui < len; ui++)
						buffer[ui] = form_buffer[i + ui];
					buffer[7] = (unsigned char)len;
					
					if ((result = CPhidgetTextLCD_sendpacket(phid, buffer)) != EPHIDGET_OK)
					{
						CThread_mutex_unlock(&phid->phid.writelock);
						return result;
					}
				}
				
				CThread_mutex_unlock(&phid->phid.writelock);
				return result;
			default:
				return EPHIDGET_UNEXPECTED;
		}
	}
	return EPHIDGET_OK;
}

// === Deprecated Functions === //

CGET(TextLCD,NumRows,int)
	return CPhidgetTextLCD_getRowCount(phid, pVal);
}
CGET(TextLCD,NumColumns,int)
	return CPhidgetTextLCD_getColumnCount(phid, pVal);
}

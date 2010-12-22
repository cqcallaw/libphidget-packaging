#ifndef __CPHIDGETRFID
#define __CPHIDGETRFID
#include "cphidget.h"

/** \defgroup phidrfid Phidget RFID 
 * \ingroup phidgets
 * Calls specific to the Phidget RFID. See the product manual for more specific API details, supported functionality, units, etc.
 * @{
 */

DPHANDLE(RFID)
CHDRSTANDARD(RFID)

/**
 * Gets the number of outputs supported by this board.
 * @param phid An attached phidget rfid handle.
 * @param count The output count.
 */
CHDRGET(RFID,OutputCount,int *count)
/**
 * Gets the state of an output.
 * @param phid An attached phidget rfid handle.
 * @param index The output index.
 * @param outputState The output state. Possible values are \ref PTRUE and \ref PFALSE.
 */
CHDRGETINDEX(RFID,OutputState,int *outputState)
/**
 * Sets the state of an output.
 * @param phid An attached phidget rfid handle.
 * @param index The output index.
 * @param outputState The output state. Possible values are \ref PTRUE and \ref PFALSE.
 */
CHDRSETINDEX(RFID,OutputState,int outputState)
/**
 * Set an output change handler. This is called when an output changes.
 * @param phid An attached phidget rfid handle.
 * @param fptr Callback function pointer.
 * @param userPtr A pointer for use by the user - this value is passed back into the callback function.
 */
CHDREVENTINDEX(RFID,OutputChange,int outputState)

/**
 * Gets the state of the antenna.
 * @param phid An attached phidget rfid handle.
 * @param antennaState The antenna state. Possible values are \ref PTRUE and \ref PFALSE.
 */
CHDRGET(RFID,AntennaOn,int *antennaState)
/**
 * Sets the state of the antenna. Note that the antenna must be enabled before tags will be read.
 * @param phid An attached phidget rfid handle.
 * @param antennaState The antenna state. Possible values are \ref PTRUE and \ref PFALSE.
 */
CHDRSET(RFID,AntennaOn,int antennaState)
/**
 * Gets the state of the onboard LED.
 * @param phid An attached phidget rfid handle.
 * @param LEDState The LED state. Possible values are \ref PTRUE and \ref PFALSE.
 */
CHDRGET(RFID,LEDOn,int *LEDState)
/**
 * Sets the state of the onboard LED.
 * @param phid An attached phidget rfid handle.
 * @param LEDState The LED state. Possible values are \ref PTRUE and \ref PFALSE.
 */
CHDRSET(RFID,LEDOn,int LEDState)

/**
 * Gets the last tag read by the reader. This tag may or may not still be on the reader.
 * @param phid An attached phidget rfid handle.
 * @param tag The tag. This must be an unsigned char array of size 5.
 */
CHDRGET(RFID,LastTag,unsigned char *tag)
/**
 * Gets the tag present status. This is whether or not a tag is being read by the reader.
 * @param phid An attached phidget rfid handle.
 * @param status The tag status. Possible values are \ref PTRUE and \ref PFALSE.
 */
CHDRGET(RFID,TagStatus,int *status)
/**
 * Set a tag handler. This is called when a tag is first detected by the reader.
 * @param phid An attached phidget rfid handle.
 * @param fptr Callback function pointer.
 * @param userPtr A pointer for use by the user - this value is passed back into the callback function.
 */
CHDREVENT(RFID,Tag,unsigned char *tag)
/**
 * Set a tag lost handler. This is called when a tag is no longer detected by the reader.
 * @param phid An attached phidget rfid handle.
 * @param fptr Callback function pointer.
 * @param userPtr A pointer for use by the user - this value is passed back into the callback function.
 */
CHDREVENT(RFID,TagLost,unsigned char *tag)

#ifndef REMOVE_DEPRECATED
DEP_CHDRGET("Deprecated - use CPhidgetRFID_getOutputCount",RFID,NumOutputs,int *)
#endif

//These are for a prototype device - hide until it's released
#if !defined(EXTERNALPROTO) || defined(DEBUG)

typedef enum {
	PHIDGET_RFID_ENCODING_MANCHESTER = 1,
	PHIDGET_RFID_ENCODING_BIPHASE,
	PHIDGET_RFID_ENCODING_AC
} CPhidgetRFID_Encoding;

typedef enum {
	PHIDGET_RFID_TAG_ISO11784 = 1,
	PHIDGET_RFID_TAG_EM4102,
	PHIDGET_RFID_TAG_HITAGS
} CPhidgetRFID_TagType;

typedef struct _CPhidgetRFID_TagInfo
{
	int bitRate;
	CPhidgetRFID_TagType tagType;
	CPhidgetRFID_Encoding encoding;
} CPhidgetRFID_TagInfo, *CPhidgetRFID_TagInfoHandle;

typedef struct _CPhidgetRFID_TagOptions
{
	unsigned char writable;
	unsigned char encrypted;
	int memSize;
} CPhidgetRFID_TagOptions, *CPhidgetRFID_TagOptionsHandle;

PHIDGET21_API int CCONV CPhidgetRFID_WriteRaw(CPhidgetRFIDHandle phid, unsigned char *data, int bitlength, int pregap, int space, int postgap, int zero, int one);
PHIDGET21_API int CCONV CPhidgetRFID_getRawData(CPhidgetRFIDHandle phid, int *data, int *dataLength);

PHIDGET21_API int CCONV CPhidgetRFID_getTagOptions(CPhidgetRFIDHandle phid, char *tagString, CPhidgetRFID_TagOptionsHandle options);
PHIDGET21_API int CCONV CPhidgetRFID_read(CPhidgetRFIDHandle phid, char *tagString, unsigned char *data, int *dataLength, char *password);
PHIDGET21_API int CCONV CPhidgetRFID_write(CPhidgetRFIDHandle phid, char *tagString, unsigned char *data, int dataLength, int offset, char *password);

CHDREVENT(RFID, RawData, int *data, int dataLength)
CHDREVENT(RFID, TagAdvanced, char *tagString, CPhidgetRFID_TagInfoHandle tagInfo)
CHDREVENT(RFID, TagLostAdvanced, char *tagString, CPhidgetRFID_TagInfoHandle tagInfo)

#endif

#ifndef EXTERNALPROTO

#define RFID_PACKET_TAG 0
#define RFID_PACKET_OUTPUT_ECHO 1

#define RFID_LED_FLAG 0x04
#define RFID_ANTENNA_FLAG 0x08

//RFID Advanced Constants
#define RFID_WRITE_DATA_OUT_PACKET	0x00
#define RFID_CONTROL_OUT_PACKET		0x80

#define RFID_READ_DATA_IN_PACKET	0x00
#define RFID_ECHO_IN_PACKET			0x80

//4097 constants
#define RFID_4097_AmpDemod		0x00	//Amplitude demodulation
#define RFID_4097_PhaseDemod	0x01	//Phase demodulation

#define RFID_4097_PowerDown		0x00
#define RFID_4097_Active		0x02

#define RFID_4097_DataOut		0x00	//DATA_OUT is data from the rfid card
#define RFID_4097_ClkOut		0x04	//DATA_OUT is the internal clock/32

#define	RFID_4097_IntPLL		0x00
#define RFID_4097_ExtClk		0x08

#define RFID_4097_FastStart		0x10

#define RFID_4097_Gain960		0x40
#define RFID_4097_Gain480		0x00
#define RFID_4097_Gain240		0x60
#define RFID_4097_Gain120		0x20

#define RFID_4097_TestMode		0x80

#define RFID_MAX_DATA_PER_PACKET	62

#define RFID_DATA_ARRAY_SIZE		2048
#define RFID_DATA_ARRAY_MASK		0x7ff

#define RFID_MAXOUTPUTS 2

typedef enum _CPhidgetRFID_Hitag_State
{
	RFID_HITAG_STATE_NONE = 0,
	RFID_HITAG_STATE_UID_REQUEST,
	RFID_HITAG_STATE_AC_SEQUENCE,
	RFID_HITAG_STATE_SELECT,
	RFID_HITAG_STATE_READ,
	RFID_HITAG_STATE_WRITE

} CPhidgetRFID_Hitag_State;

typedef struct _CPhidgetRFID_Tag
{
	char tagString[256];
	CPhidgetRFID_TagInfo tagInfo;
	TIME	lastTagTime;
	unsigned char tagEventPending;
	unsigned char tagOptionsValid;
	CPhidgetRFID_TagOptions tagOptions;
	unsigned char tagDataValid;
	unsigned char tagData[256];
} CPhidgetRFID_Tag, *CPhidgetRFID_TagHandle;

typedef struct _CPhidgetRFID_TagList
{
	struct _CPhidgetRFID_TagList *next;
	CPhidgetRFID_TagHandle tag;
} CPhidgetRFID_TagList, *CPhidgetRFID_TagListHandle;

typedef struct _CPhidgetRFID_HitagAC
{
	unsigned char uid[4];
	int colPos;
} CPhidgetRFID_HitagAC, *CPhidgetRFID_HitagACHandle;

typedef struct _CPhidgetRFID_HitagACList
{
	struct _CPhidgetRFID_HitagACList *next;
	CPhidgetRFID_HitagACHandle hitagAC;
} CPhidgetRFID_HitagACList, *CPhidgetRFID_HitagACListHandle;

struct _CPhidgetRFID {
	CPhidget phid;

	int (CCONV *fptrOutputChange)(CPhidgetRFIDHandle, void *, int, int);
	int (CCONV *fptrTag)(CPhidgetRFIDHandle, void *, unsigned char *);
	int (CCONV *fptrTagLost)(CPhidgetRFIDHandle, void *, unsigned char *);
	int (CCONV *fptrRawData)(CPhidgetRFIDHandle, void *, int *, int);
	int (CCONV *fptrTagAdvanced)(CPhidgetRFIDHandle, void *, char *, CPhidgetRFID_TagInfoHandle);
	int (CCONV *fptrTagLostAdvanced)(CPhidgetRFIDHandle, void *, char *, CPhidgetRFID_TagInfoHandle);

	void *fptrOutputChangeptr;
	void *fptrTagptr;
	void *fptrTagLostptr;
	void *fptrRawDataptr;
	void *fptrTagAdvancedptr;
	void *fptrTagLostAdvancedptr;

	// Values returned from the device
	unsigned char outputEchoState[RFID_MAXOUTPUTS];
	unsigned char antennaEchoState;
	unsigned char ledEchoState;
	
	unsigned char outputState[RFID_MAXOUTPUTS];
	unsigned char antennaState;
	unsigned char ledState;

	unsigned char lastTag[5];
	unsigned char lastTagValid;
	TIME	lastTagTime;
	unsigned char tagPresent;
	//unsigned char tagEvent;
	unsigned char pendingTag[5];
	unsigned char tagEventPending;

	EVENT tagAvailableEvent;

	void *respData;
	int respStatus;
	EVENT respEvent;
	EVENT respEvent2;

	//Advanced Tag Events
	int tagAdvancedCount;
	CThread_mutex_t tagthreadlock; /* protects tag thread access to things */
	CPhidgetRFID_TagListHandle tagAdvancedList;
	CPhidgetRFID_Tag lastTagAdvanced;
	TIME	lastDataTime;
	TIME	hitagReqTime;

	unsigned char ACCodingOK;

	unsigned char fullStateEcho;
	
	CThread tagTimerThread;

	//RFID Advanced stuff
	int spaceClocks, pregapClocks, postgapClocks, oneClocks, zeroClocks;
	int spaceClocksEcho, pregapClocksEcho, postgapClocksEcho, oneClocksEcho, zeroClocksEcho;
	int _4097Conf, _4097ConfEcho;
	int frequencyEcho;

	int dataBuffer[RFID_DATA_ARRAY_SIZE];
	int dataBufferNormalized[RFID_DATA_ARRAY_SIZE];
	int dataReadPtr, dataWritePtr, dataReadACPtr;
	int userReadPtr; //for the getRawData function
	
	unsigned char manBuffer[RFID_DATA_ARRAY_SIZE];
	int manReadPtr, manWritePtr;

	unsigned char biphaseBuffer[RFID_DATA_ARRAY_SIZE];
	int biphaseReadPtr, biphaseWritePtr;

	unsigned char manLockedIn;
	unsigned char manShortChange;

	unsigned char biphaseLockedIn;
	unsigned char biphaseShortChange;

	int one, two;
	int oneCount, twoCount;

	CPhidgetRFID_Hitag_State hitagState;
	int hitagOffset;

	//Hitag AC queue
	CPhidgetRFID_HitagACListHandle hitagACList;
	CPhidgetRFID_HitagAC lastHitagAC;

	unsigned char atGap;

	unsigned char outputPacket[8];
	unsigned int outputPacketLen;
} typedef CPhidgetRFIDInfo;
#endif

/** @} */

#endif

#include "stdafx.h"
#include "cphidgetconstantsinternal.h"
#include "cphidgetconstants.h"

const char *Phid_ErrorDescriptions[PHIDGET_ERROR_CODE_COUNT] = {
"Function completed successfully.",
"A Phidget matching the type and or serial number could not be found.", 
"Memory could not be allocated.",
"Unexpected Error.  Contact Phidgets Inc. for support.", 
"Invalid argument passed to function.",
"Phidget not physically attached.", 
"Read/Write operation was interrupted.", 
"The Error Code is not defined.",
"Network Error.",
"Value is Unknown (State not yet received from device, or not yet set by user).",
"Authorization Failed.",
"Not Supported.",
"Duplicated request.",
"Given timeout has been exceeded.",
"Index out of Bounds.",
"A non-null error code was returned from an event handler.",
"A connection to the server does not exist.",
"Function is not applicable for this device.",
"Phidget handle was closed.",
"Webservice and Client protocol versions don't match. Update both to newest release."};

const char Phid_UnknownErrorDescription[] = "Unknown Error Code.";

/* enum starts a 1 so we need a blank for 0 
 * Don't  reorder this list */
const char *Phid_DeviceName[PHIDGET_DEVICE_CLASS_COUNT] = {
"ERROR",
"Uninitialized Phidget Handle",
"PhidgetAccelerometer",
"PhidgetAdvancedServo",
"PhidgetEncoder",
"PhidgetGPS",
"ERROR", //Old Gyro
"PhidgetInterfaceKit", 
"PhidgetLED",
"PhidgetMotorControl",
"PhidgetPHSensor",
"PhidgetRFID", 
"PhidgetServo",
"PhidgetStepper",
"PhidgetTemperatureSensor",
"PhidgetTextLCD",
"PhidgetTextLED",
"PhidgetWeightSensor",
"PhidgetGeneric",
"PhidgetIR",
"PhidgetSpatial"};

#ifdef DEBUG
const char LibraryVersion[] = "Phidget21 Debug - Version 2.1.7 - Built " __DATE__
    " " __TIME__;
#else
const char LibraryVersion[] = "Phidget21 - Version 2.1.7 - Built " __DATE__
    " " __TIME__;
#endif

#ifndef _MSC_EXTENSIONS
#define UFINTS(name, initializer...) .name = { initializer }
#else
#define UFINTS(name, ...) { __VA_ARGS__ }
#endif

#define UNUSED_DEVICE(pid) PHIDCLASS_NOTHING, 0x6C2, pid, 0, { UFINTS(dummy, 0, 0, 0) }, ""

// We own Product IDs 0x30 - 0xAF (48-175)
// This needs to start with the device id = 1 invalid device and end in NULL
// This list could be re-ordered, but we leave it in product-id order.
const CPhidgetDeviceDef Phid_Device_Def[PHIDGET_DEVICE_COUNT+1] = {
{ PHIDID_NOTHING,						PHIDCLASS_NOTHING,			0x000, 0x00,	0, { UFINTS(dummy,				0, 0, 0) },	"Uninitialized Phidget Handle"},		//1 start for list logic

/* Very old devices - we don't own these product IDs so don't allocate anymore!! We maintain support for these devices */
{ PHIDID_SERVO_1MOTOR_OLD,				PHIDCLASS_SERVO,			0x925, 0x8101,	0, { UFINTS(servo,				1) },		"Phidget Servo Controller 1-motor"},	//Original 1000
{ PHIDID_SERVO_4MOTOR_OLD,				PHIDCLASS_SERVO,			0x925, 0x8104,	0, { UFINTS(servo,				4) },		"Phidget Servo Controller 4-motor"},	//Original 1001
{ PHIDID_INTERFACEKIT_4_8_8,			PHIDCLASS_INTERFACEKIT,		0x925, 0x8201,	0, { UFINTS(ifkit,				4, 8, 8) },	"Phidget InterfaceKit 4/8/8"},			//Original Ifkit

/* Valid product IDs */
{ PHIDID_RFID,							PHIDCLASS_RFID,				0x6C2, 0x30,	0, { UFINTS(rfid,				0) },		"Phidget RFID"},
{ PHIDID_RFID_2OUTPUT,					PHIDCLASS_RFID,				0x6C2, 0x31,	0, { UFINTS(rfid,				2) },		"Phidget RFID 2-output"},
{ PHIDID_TEMPERATURESENSOR_4,			PHIDCLASS_TEMPERATURESENSOR,0x6C2, 0x32,	0, { UFINTS(temperaturesensor,	4) },		"Phidget Temperature Sensor 4-input"},
{ PHIDID_SPATIAL_ACCEL_GYRO_COMPASS,	PHIDCLASS_SPATIAL,			0x6C2, 0x33,	0, { UFINTS(spatial,			3,3,3) },	"Phidget Spatial 3/3/3"},
{ PHIDID_RFID_2OUTPUT_ADVANCED,			PHIDCLASS_RFID,				0x6C2, 0x34,	0, { UFINTS(rfid,				2) },		"Phidget RFID 2-output Advanced"},
{ PHIDID_SERVO_4MOTOR,					PHIDCLASS_SERVO,			0x6C2, 0x38,	0, { UFINTS(servo,				4) },		"Phidget Servo Controller 4-motor"}, 
{ PHIDID_SERVO_1MOTOR,					PHIDCLASS_SERVO,			0x6C2, 0x39,	0, { UFINTS(servo,				1) },		"Phidget Servo Controller 1-motor"},
{ PHIDID_ADVANCEDSERVO_8MOTOR,			PHIDCLASS_ADVANCEDSERVO,	0x6C2, 0x3A,	0, { UFINTS(advancedservo,		8) },		"Phidget Advanced Servo Controller 8-motor"},

{ PHIDID_INTERFACEKIT_0_0_4,			PHIDCLASS_INTERFACEKIT,		0x6C2, 0x40,	0, { UFINTS(ifkit,				0, 0, 4) },	"Phidget InterfaceKit 0/0/4"},
{ PHIDID_INTERFACEKIT_0_16_16,			PHIDCLASS_INTERFACEKIT,		0x6C2, 0x44,	0, { UFINTS(ifkit,				0, 16, 16) },	"Phidget InterfaceKit 0/16/16"},
{ PHIDID_INTERFACEKIT_8_8_8,			PHIDCLASS_INTERFACEKIT,		0x6C2, 0x45,	0, { UFINTS(ifkit,				8, 8, 8) },	"Phidget InterfaceKit 8/8/8"},
{ PHIDID_TEXTLED_4x8,					PHIDCLASS_TEXTLED,			0x6C2, 0x48,	0, { UFINTS(textled,			4, 8) },	"Phidget TextLED 4x8"},
{ PHIDID_TEXTLED_1x8,					PHIDCLASS_TEXTLED,			0x6C2, 0x49,	0, { UFINTS(textled,			1, 8) },	"Phidget TextLED 1x8"},
{ PHIDID_LED_64,						PHIDCLASS_LED,				0x6C2, 0x4A,	0, { UFINTS(led,				64) },		"Phidget LED 64"},
{ PHIDID_ENCODER_1ENCODER_1INPUT,		PHIDCLASS_ENCODER,			0x6C2, 0x4B,	0, { UFINTS(encoder,			1, 1) },	"Phidget Encoder 1-encoder 1-input"},
{ PHIDID_LED_64_ADV,					PHIDCLASS_LED,				0x6C2, 0x4C,	0, { UFINTS(led,				64) },		"Phidget LED 64 Advanced"},
{ PHIDID_IR,							PHIDCLASS_IR,				0x6C2, 0x4D,	0, { UFINTS(ir,					0) },		"Phidget IR Receiver Transmitter"},
{ PHIDID_ENCODER_HS_4ENCODER_4INPUT,	PHIDCLASS_ENCODER,			0x6C2, 0x4F,	0, { UFINTS(encoder,			4, 4) },	"Phidget High Speed Encoder 4-input"},

{ PHIDID_INTERFACEKIT_0_5_7,			PHIDCLASS_INTERFACEKIT,		0x6C2, 0x51,	0, { UFINTS(ifkit,				0, 5, 7) },	"Phidget InterfaceKit 0/5/7"},			//with TextLCD - Spain
{ PHIDID_TEXTLCD_2x20_CUSTOM,			PHIDCLASS_TEXTLCD,			0x6C2, 0x51,	0, { UFINTS(textlcd,			2, 20) },	"Phidget TextLCD Custom"},				//with 0/5/7 - Spain
{ PHIDID_TEXTLCD_2x20,					PHIDCLASS_TEXTLCD,			0x6C2, 0x52,	0, { UFINTS(textlcd,			2, 20) },	"Phidget TextLCD"},						//no ifkit part
{ PHIDID_INTERFACEKIT_0_8_8_w_LCD,		PHIDCLASS_INTERFACEKIT,		0x6C2, 0x53,	0, { UFINTS(ifkit,				0, 8, 8) },	"Phidget InterfaceKit 0/8/8"},			//with TextLCD
{ PHIDID_TEXTLCD_2x20_w_0_8_8,			PHIDCLASS_TEXTLCD,			0x6C2, 0x53,	0, { UFINTS(textlcd,			2, 20) },	"Phidget TextLCD"},						//with 0/8/8
{ PHIDID_MOTORCONTROL_LV_2MOTOR_4INPUT,	PHIDCLASS_MOTORCONTROL,		0x6C2, 0x58,	0, { UFINTS(motorcontrol,		2, 4) },	"Phidget Low Voltage Motor Controller 2-motor 4-input"},
{ PHIDID_MOTORCONTROL_HC_2MOTOR,		PHIDCLASS_MOTORCONTROL,		0x6C2, 0x59,	0, { UFINTS(motorcontrol,		2, 0) },	"Phidget High Current Motor Controller 2-motor"},

{ PHIDID_TEMPERATURESENSOR,				PHIDCLASS_TEMPERATURESENSOR,0x6C2, 0x70,	0, { UFINTS(temperaturesensor,	1) },		"Phidget Temperature Sensor"},
{ PHIDID_ACCELEROMETER_2AXIS,			PHIDCLASS_ACCELEROMETER,	0x6C2, 0x71,	0, { UFINTS(accelerometer,		2) },		"Phidget Accelerometer 2-axis"},
{ PHIDID_WEIGHTSENSOR,					PHIDCLASS_WEIGHTSENSOR,		0x6C2, 0x72,	0, { UFINTS(weightsensor,		0) },		"Phidget Weight Sensor"},
{ PHIDID_PHSENSOR,						PHIDCLASS_PHSENSOR,			0x6C2, 0x74,	0, { UFINTS(phsensor,			0) },		"Phidget PH Sensor"},
{ PHIDID_LINEAR_TOUCH,					PHIDCLASS_INTERFACEKIT,		0x6C2, 0x76,	0, { UFINTS(ifkit,				1, 2) },	"Phidget Touch Slider"},
{ PHIDID_ROTARY_TOUCH,					PHIDCLASS_INTERFACEKIT,		0x6C2, 0x77,	0, { UFINTS(ifkit,				1, 2) },	"Phidget Touch Rotation"},
{ PHIDID_GPS,							PHIDCLASS_GPS,				0x6C2, 0x79,	0, { UFINTS(gps,				1) },		"Phidget GPS"},							//PROTOTYPE - GPS
{ PHIDID_UNIPOLAR_STEPPER_4MOTOR,		PHIDCLASS_STEPPER,			0x6C2, 0x7A,	0, { UFINTS(stepper,			4) },		"Phidget Unipolar Stepper Controller 4-motor"},
{ PHIDID_BIPOLAR_STEPPER_1MOTOR,		PHIDCLASS_STEPPER,			0x6C2, 0x7B,	0, { UFINTS(stepper,			1, 4) },	"Phidget Bipolar Stepper Controller 1-motor"},
{ PHIDID_INTERFACEKIT_8_8_8_w_LCD,		PHIDCLASS_INTERFACEKIT,		0x6C2, 0x7D,	0, { UFINTS(ifkit,				8, 8, 8) },	"Phidget InterfaceKit 8/8/8"},			//with TextLCD
{ PHIDID_TEXTLCD_2x20_w_8_8_8,			PHIDCLASS_TEXTLCD,			0x6C2, 0x7D,	1, { UFINTS(textlcd,			2, 20) },	"Phidget TextLCD"},						//with 8/8/8
{ PHIDID_ACCELEROMETER_3AXIS,			PHIDCLASS_ACCELEROMETER,	0x6C2, 0x7E,	0, { UFINTS(accelerometer,		3) },		"Phidget Accelerometer 3-axis"},
{ PHIDID_SPATIAL_ACCEL_3AXIS,			PHIDCLASS_SPATIAL,			0x6C2, 0x7F,	0, { UFINTS(spatial,			3,0,0) },	"Phidget Spatial 0/0/3"},

{ PHIDID_ENCODER_HS_1ENCODER,			PHIDCLASS_ENCODER,			0x6C2, 0x80,	0, { UFINTS(encoder,			1) },		"Phidget High Speed Encoder 1-encoder"},
{ PHIDID_INTERFACEKIT_0_0_8,			PHIDCLASS_INTERFACEKIT,		0x6C2, 0x81,	0, { UFINTS(ifkit,				0, 0, 8) },	"Phidget InterfaceKit 0/0/8"},
{ PHIDID_ADVANCEDSERVO_1MOTOR,			PHIDCLASS_ADVANCEDSERVO,	0x6C2, 0x82,	0, { UFINTS(advancedservo,		1) },		"Phidget Advanced Servo Controller 1-motor"},

{ PHIDID_GENERIC,						PHIDCLASS_GENERIC,			0x6C2, 0x99,	0, { UFINTS(dummy,				0, 0, 0) },	"Phidget Generic Device"},				//generic device - used for prototyping

{ 0 } //ending null
};

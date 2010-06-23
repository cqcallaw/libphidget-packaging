#ifndef __CPHIDGETMOTORCONTROL
#define __CPHIDGETMOTORCONTROL
#include "cphidget.h"

/** \defgroup phidmotorcontrol Phidget Motor Control 
 * \ingroup phidgets
 * Calls specific to the Phidget Motor Control. See the product manual for more specific API details, supported functionality, units, etc.
 * @{
 */

DPHANDLE(MotorControl)
CHDRSTANDARD(MotorControl)

/**
 * Gets the number of motors supported by this controller.
 * @param phid An attached phidget motor control handle.
 * @param count The motor count.
 */
CHDRGET(MotorControl,MotorCount,int *count)

/**
 * Gets the current velocity of a motor.
 * @param phid An attached phidget motor control handle.
 * @param index The motor index.
 * @param velocity The current velocity.
 */
CHDRGETINDEX(MotorControl,Velocity,double *velocity)
/**
 * Sets the velocity of a motor.
 * @param phid An attached phidget motor control handle.
 * @param index The motor index.
 * @param velocity The velocity.
 */
CHDRSETINDEX(MotorControl,Velocity,double velocity)
/**
 * Sets a velocity change event handler. This is called when the velocity changes.
 * @param phid An attached phidget motor control handle.
 * @param fptr Callback function pointer.
 * @param userPtr A pointer for use by the user - this value is passed back into the callback function.
 */
CHDREVENTINDEX(MotorControl,VelocityChange,double velocity)

/**
 * Gets the last set acceleration of a motor.
 * @param phid An attached phidget motor control handle.
 * @param index The motor index.
 * @param acceleration The acceleration.
 */
CHDRGETINDEX(MotorControl,Acceleration,double *acceleration)
/**
 * Sets the last set acceleration of a motor.
 * @param phid An attached phidget motor control handle.
 * @param index The motor index.
 * @param acceleration The acceleration.
 */
CHDRSETINDEX(MotorControl,Acceleration,double acceleration)
/**
 * Gets the maximum acceleration supported by a motor
 * @param phid An attached phidget motor control handle
 * @param index The motor index.
 * @param max The maximum acceleration.
 */
CHDRGETINDEX(MotorControl,AccelerationMax,double *max)
/**
 * Gets the minimum acceleration supported by a motor.
 * @param phid An attached phidget motor control handle
 * @param index The motor index.
 * @param min The minimum acceleration
 */
CHDRGETINDEX(MotorControl,AccelerationMin,double *min)


/**
 * Gets the current current draw for a motor.
 * @param phid An attached phidget motor control handle
 * @param index The motor index.
 * @param current The current.
 */
CHDRGETINDEX(MotorControl,Current,double *current)
/**
 * Sets a current change event handler. This is called when the current draw changes.
 * @param phid An attached phidget motor control handle
 * @param fptr Callback function pointer.
 * @param userPtr A pointer for use by the user - this value is passed back into the callback function.
 */
CHDREVENTINDEX(MotorControl,CurrentChange,double current)

/**
 * Gets the number of digital inputs supported by this board.
 * @param phid An attached phidget motor control handle.
 * @param count The ditial input count.
 */
CHDRGET(MotorControl,InputCount,int *count)
/**
 * Gets the state of a digital input.
 * @param phid An attached phidget motor control handle.
 * @param index The input index.
 * @param inputState The input state. Possible values are \ref PTRUE and \ref PFALSE.
 */
CHDRGETINDEX(MotorControl,InputState,int *inputState)
/**
 * Set a digital input change handler. This is called when a digital input changes.
 * @param phid An attached phidget motor control handle.
 * @param fptr Callback function pointer.
 * @param userPtr A pointer for use by the user - this value is passed back into the callback function.
 */
CHDREVENTINDEX(MotorControl,InputChange,int inputState)

#ifndef REMOVE_DEPRECATED
DEP_CHDRGET("Deprecated - use CPhidgetMotorControl_getMotorCount",MotorControl,NumMotors,int *)
DEP_CHDRGET("Deprecated - use CPhidgetMotorControl_getInputCount",MotorControl,NumInputs,int *)
DEP_CHDRGETINDEX("Deprecated - use CPhidgetMotorControl_getVelocity",MotorControl,MotorSpeed,double *)
DEP_CHDRSETINDEX("Deprecated - use CPhidgetMotorControl_setVelocity",MotorControl,MotorSpeed,double)
DEP_CHDREVENTINDEX("Deprecated - use CPhidgetMotorControl_set_OnVelocityChange_Handler",MotorControl,MotorChange,double motorSpeed)
#endif

#ifndef EXTERNALPROTO
#define MOTORCONTROL_MAXMOTORS 2
#define MOTORCONTROL_MAXINPUTS 4
struct _CPhidgetMotorControl {
	CPhidget phid;
   
	int (CCONV *fptrVelocityChange)(CPhidgetMotorControlHandle, void *, int, double);               
	int (CCONV *fptrInputChange)(CPhidgetMotorControlHandle, void *, int, int);     
	int (CCONV *fptrCurrentChange)(CPhidgetMotorControlHandle, void *, int, double);       

	void *fptrInputChangeptr;
	void *fptrVelocityChangeptr;
	void *fptrCurrentChangeptr;

	//Deprecated
	int (CCONV *fptrMotorChange)(CPhidgetMotorControlHandle, void *, int, double);   
	void *fptrMotorChangeptr;

	unsigned char inputState[MOTORCONTROL_MAXINPUTS];
	double motorSpeedEcho[MOTORCONTROL_MAXMOTORS];
	double motorSensedCurrent[MOTORCONTROL_MAXMOTORS];

	double motorSpeed[MOTORCONTROL_MAXMOTORS];
	double motorAcceleration[MOTORCONTROL_MAXMOTORS];

	double accelerationMax, accelerationMin;

	unsigned char outputPacket[8];
	unsigned int outputPacketLen;
} typedef CPhidgetMotorControlInfo;
#endif

/** @} */

#endif

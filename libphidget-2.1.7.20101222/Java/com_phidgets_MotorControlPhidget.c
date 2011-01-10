#include "../stdafx.h"
#include "phidget_jni.h"
#include "com_phidgets_MotorControlPhidget.h"
#include "../cphidgetmotorcontrol.h"

EVENT_VARS(inputChange, InputChange)
EVENT_VARS(motorVelocityChange, MotorVelocityChange)
EVENT_VARS(currentChange, CurrentChange)

JNI_LOAD(motor, MotorControl)
	EVENT_VAR_SETUP(motor, inputChange, InputChange, IZ, V)
	EVENT_VAR_SETUP(motor, motorVelocityChange, MotorVelocityChange, ID, V)
	EVENT_VAR_SETUP(motor, currentChange, CurrentChange, ID, V)
}

EVENT_HANDLER_INDEXED(MotorControl, inputChange, InputChange, 
					  CPhidgetMotorControl_set_OnInputChange_Handler, int)
EVENT_HANDLER_INDEXED(MotorControl, motorVelocityChange, MotorVelocityChange, 
					  CPhidgetMotorControl_set_OnVelocityChange_Handler, double)
EVENT_HANDLER_INDEXED(MotorControl, currentChange, CurrentChange, 
					  CPhidgetMotorControl_set_OnCurrentChange_Handler, double)

JNI_CREATE(MotorControl)
JNI_INDEXED_GETFUNC(MotorControl, Acceleration, Acceleration, jdouble)
JNI_INDEXED_SETFUNC(MotorControl, Acceleration, Acceleration, jdouble)
JNI_INDEXED_GETFUNC(MotorControl, AccelerationMin, AccelerationMin, jdouble)
JNI_INDEXED_GETFUNC(MotorControl, AccelerationMax, AccelerationMax, jdouble)
JNI_INDEXED_GETFUNC(MotorControl, Velocity, Velocity, jdouble)
JNI_INDEXED_SETFUNC(MotorControl, Velocity, Velocity, jdouble)
JNI_INDEXED_GETFUNC(MotorControl, Current, Current, jdouble)
JNI_INDEXED_GETFUNCBOOL(MotorControl, InputState, InputState)
JNI_GETFUNC(MotorControl, MotorCount, MotorCount, jint)
JNI_GETFUNC(MotorControl, InputCount, InputCount, jint)

//Deprecated
JNI_INDEXED_GETFUNC(MotorControl, Speed, Velocity, jdouble)
JNI_INDEXED_SETFUNC(MotorControl, Speed, Velocity, jdouble)

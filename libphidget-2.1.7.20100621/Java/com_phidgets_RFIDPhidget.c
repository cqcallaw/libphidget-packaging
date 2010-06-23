#include "../stdafx.h"
#include "phidget_jni.h"
#include "com_phidgets_RFIDPhidget.h"
#include "../cphidgetrfid.h"

EVENT_VARS(outputChange, OutputChange)
EVENT_VARS(tagLoss, TagLoss)
EVENT_VARS(tagGain, TagGain)

JNI_LOAD(rfid, RFID)
	EVENT_VAR_SETUP(rfid, outputChange, OutputChange, IZ, V)
	EVENT_VAR_SETUP(rfid, tagLoss, TagLoss, Ljava/lang/String;, V)
	EVENT_VAR_SETUP(rfid, tagGain, TagGain, Ljava/lang/String;, V)
}

EVENT_HANDLER_INDEXED(RFID, outputChange, OutputChange, 
					  CPhidgetRFID_set_OnOutputChange_Handler, int)

static int CCONV tagLoss_handler (CPhidgetRFIDHandle h, void *arg, unsigned char *);
JNIEXPORT void JNICALL Java_com_phidgets_RFIDPhidget_enableTagLossEvents (JNIEnv * env, jobject obj, jboolean b)
{
	jlong gr = updateGlobalRef (env, obj, nativeTagLossHandler_fid, b);
	CPhidgetRFIDHandle h = (CPhidgetRFIDHandle) (uintptr_t) (*env)->GetLongField (env, obj, handle_fid);
	CPhidgetRFID_set_OnTagLost_Handler (h, b ? tagLoss_handler : 0, (void *) (uintptr_t) gr);
} static int CCONV
tagLoss_handler (CPhidgetRFIDHandle h, void *arg, unsigned char *v)
{
	JNIEnv *env;
	jobject obj;
	jobject tagLossEv;
	char stringbuffer[20];
	jstring jb;
	if ((*ph_vm)->AttachCurrentThread (ph_vm, (void **) &env, ((void *) 0)))
		abort ();
	obj = (jobject) arg;

	sprintf(stringbuffer, "%02x%02x%02x%02x%02x",v[0],v[1],v[2],v[3],v[4]);
	jb=(*env)->NewStringUTF(env, stringbuffer);

	if (!(tagLossEv = (*env)->NewObject (env, tagLossEvent_class, tagLossEvent_cons, obj, jb)))
		return -1;
	(*env)->CallVoidMethod (env, obj, fireTagLoss_mid, tagLossEv);
	(*env)->DeleteLocalRef (env, tagLossEv);
	(*ph_vm)->DetachCurrentThread (ph_vm);
	return 0;
}

static int CCONV tagGain_handler (CPhidgetRFIDHandle h, void *arg, unsigned char *);
JNIEXPORT void JNICALL Java_com_phidgets_RFIDPhidget_enableTagGainEvents (JNIEnv * env, jobject obj, jboolean b)
{
	jlong gr = updateGlobalRef (env, obj, nativeTagGainHandler_fid, b);
	CPhidgetRFIDHandle h = (CPhidgetRFIDHandle) (uintptr_t) (*env)->GetLongField (env, obj, handle_fid);
	CPhidgetRFID_set_OnTag_Handler (h, b ? tagGain_handler : 0, (void *) (uintptr_t) gr);
} static int CCONV
tagGain_handler (CPhidgetRFIDHandle h, void *arg, unsigned char *v)
{
	JNIEnv *env;
	jobject obj;
	jobject tagGainEv;
	char stringbuffer[20];
	jstring jb;
	if ((*ph_vm)->AttachCurrentThread (ph_vm, (void **) &env, ((void *) 0)))
		abort ();
	obj = (jobject) arg;

	sprintf(stringbuffer, "%02x%02x%02x%02x%02x",v[0],v[1],v[2],v[3],v[4]);
	jb=(*env)->NewStringUTF(env, stringbuffer);

	if (!(tagGainEv = (*env)->NewObject (env, tagGainEvent_class, tagGainEvent_cons, obj, jb)))
		return -1;
	(*env)->CallVoidMethod (env, obj, fireTagGain_mid, tagGainEv);
	(*env)->DeleteLocalRef (env, tagGainEv);
	(*ph_vm)->DetachCurrentThread (ph_vm);
	return 0;
}


JNI_CREATE(RFID)
JNI_INDEXED_GETFUNCBOOL(RFID, OutputState, OutputState)
JNI_INDEXED_SETFUNC(RFID, OutputState, OutputState, jboolean)
JNI_GETFUNCBOOL(RFID, AntennaOn, AntennaOn)
JNI_SETFUNC(RFID, AntennaOn, AntennaOn, jboolean)
JNI_GETFUNCBOOL(RFID, LEDOn, LEDOn)
JNI_GETFUNCBOOL(RFID, TagStatus, TagStatus)
JNI_SETFUNC(RFID, LEDOn, LEDOn, jboolean)
JNI_GETFUNC(RFID, OutputCount, OutputCount, jint)

JNIEXPORT jstring JNICALL
Java_com_phidgets_RFIDPhidget_getLastTag (JNIEnv *env, jobject obj)
{
	CPhidgetRFIDHandle h = (CPhidgetRFIDHandle)(uintptr_t)
	    (*env)->GetLongField( env, obj, handle_fid);
	int error;
	unsigned char buffer[11];
	char stringbuffer[20];
	jstring jb;
	if ((error = CPhidgetRFID_getLastTag(h, (unsigned char *)&buffer)))
		PH_THROW(error);

	sprintf(stringbuffer, "%02x%02x%02x%02x%02x",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]);

	jb=(*env)->NewStringUTF(env, stringbuffer);
	return jb;
}

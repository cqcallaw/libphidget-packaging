#ifndef __CUSB
#define __CUSB

#include "cphidget.h"
#include "cphidgetlist.h"

#ifndef EXTERNALPROTO
int CUSBBuildList(CPhidgetList **curList);
int CUSBOpenHandle(CPhidgetHandle phid);
int CUSBCloseHandle(CPhidgetHandle phid);
int CUSBSetLabel(CPhidgetHandle phid, char *buffer);
void CUSBCleanup();
int CUSBSetupNotifications();
#ifdef _LINUX
int CUSBGetDeviceCapabilities(CPhidgetHandle phid, struct usb_device *dev,
    struct usb_dev_handle *udev);
#else
int CUSBGetDeviceCapabilities(CPhidgetHandle phid, HANDLE DeviceHandle);
#endif
#endif

PHIDGET21_API int CCONV CUSBReadPacket(CPhidgetHandle phidA, unsigned char *buffer);
PHIDGET21_API int CCONV CUSBSendPacket(CPhidgetHandle phidA, unsigned char *buffer);

#endif

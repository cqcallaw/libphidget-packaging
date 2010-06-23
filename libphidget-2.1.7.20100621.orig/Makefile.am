## Process this file with automake to produce Makefile.in

AM_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/utils -I$(top_srcdir)/dict -I$(top_srcdir)/linux -I$(top_srcdir)/include -D_LINUX
#AM_LDFLAGS = -lm -lpthread -ldl -lusb

lib_LTLIBRARIES = libphidget21.la

# Rules for unpdating the libtool version:
# 1. Programs using the previous version may use the new version as drop-in replacement, 
#    and programs using the new version can also work with the previous one. In other words, 
#    no recompiling nor relinking is needed. In this case, bump revision only, don't touch current nor age.
# 2. Programs using the previous version may use the new version as drop-in replacement, 
#    but programs using the new version may use APIs not present in the previous one. In other words, 
#    a program linking against the new version may fail with “unresolved symbols” 
#    if linking against the old version at runtime: set revision to 0, bump current and age.
# 3. Programs may need to be changed, recompiled, relinked in order to use the new version. 
#    Bump current, set revision and age to 0.
libphidget21_la_LDFLAGS = -version-info 0:0:0

libphidget21_la_SOURCES = cphidget.c \
	cphidget.h \
	cphidgetmacros.h \
	cphidgetattr.h \
	cusb.h \
	stdafx.h \
	cphidgetconstantsinternal.h \
	cphidgetaccelerometer.c \
	cphidgetaccelerometer.h \
	cphidgetadvancedservo.c \
	cphidgetadvancedservo.h \
	cphidgetencoder.c \
	cphidgetencoder.h \
	cphidgetgps.c \
	cphidgetgps.h \
	cphidgetinterfacekit.c \
	cphidgetinterfacekit.h \
	cphidgetir.c \
	cphidgetir.h \
	cphidgetled.c \
	cphidgetled.h \
	cphidgetmotorcontrol.c \
	cphidgetmotorcontrol.h \
	cphidgetphsensor.c \
	cphidgetphsensor.h \
	cphidgetrfid.c \
	cphidgetrfid.h \
	cphidgetservo.c \
	cphidgetservo.h \
	cphidgetspatial.c \
	cphidgetspatial.h \
	cphidgetstepper.c \
	cphidgetstepper.h \
	cphidgettemperaturesensor.c \
	cphidgettemperaturesensor.h \
	cphidgettextlcd.c \
	cphidgettextlcd.h \
	cphidgettextled.c \
	cphidgettextled.h \
	cphidgetweightsensor.c \
	cphidgetweightsensor.h \
	cphidgetmanager.c \
	cphidgetmanager.h \
	cphidgetdictionary.c \
	cphidgetdictionary.h \
	cphidgetgeneric.c \
	cphidgetgeneric.h \
	cphidgetsbc.c \
	cphidgetsbc.h \
	clog.c \
	clog.h \
	csocketevents.c \
	csocketevents.h \
	csocketopen.c \
	csocket.h \
	cphidgetconstants.c \
	cphidgetconstants.h \
	cphidgetlist.c \
	cphidgetlist.h \
	cthread.c \
	cthread.h \
	phidget21.c \
	phidget21int.h \
	linux/cusblinux.c \
	dict/pdict.c \
	dict/pdict.h \
	dict/pdictclient.c \
	dict/pdictclient.h \
	dict/pdict-impl.h \
	utils/md5.c \
	utils/md5.h \
	utils/utils.c \
	utils/utils.h \
	utils/ptree.c \
	utils/ptree.h \
	utils/plist.c \
	utils/plist.h

include_HEADERS = phidget21.h

CLEANFILES = phidget21.h phidget21matlab.h phidget21.jar Java/com/phidgets/*.class Java/com/phidgets/event/*.class

distclean-local:
	-rm -rf examples/.deps examples/Makefile
			 
EXTRA_DIST = examples/ifkit.c \
	examples/Makefile.am \
	examples/Makefile.in \
	examples/manager.c \
	examples/phidgetsbclist.c \
	examples/README \
	udev/99-phidgets.rules \
	hotplug/phidgets.usermap \
	hotplug/phidgets \
	version.sh \
	Java/com/phidgets/DictionaryKeyListener.java \
	Java/com/phidgets/TextLCDPhidget.java \
	Java/com/phidgets/SpatialEventData.java \
	Java/com/phidgets/StepperPhidget.java \
	Java/com/phidgets/ServoPhidget.java \
	Java/com/phidgets/PHSensorPhidget.java \
	Java/com/phidgets/AccelerometerPhidget.java \
	Java/com/phidgets/SpatialPhidget.java \
	Java/com/phidgets/AdvancedServoPhidget.java \
	Java/com/phidgets/EncoderPhidget.java \
	Java/com/phidgets/MotorControlPhidget.java \
	Java/com/phidgets/IRLearnedCode.java \
	Java/com/phidgets/IRCodeInfo.java \
	Java/com/phidgets/InterfaceKitPhidget.java \
	Java/com/phidgets/TemperatureSensorPhidget.java \
	Java/com/phidgets/WeightSensorPhidget.java \
	Java/com/phidgets/TextLEDPhidget.java \
	Java/com/phidgets/IRCode.java \
	Java/com/phidgets/Manager.java \
	Java/com/phidgets/PhidgetException.java \
	Java/com/phidgets/IRPhidget.java \
	Java/com/phidgets/RFIDPhidget.java \
	Java/com/phidgets/LEDPhidget.java \
	Java/com/phidgets/Dictionary.java \
	Java/com/phidgets/event/ServoPositionChangeListener.java \
	Java/com/phidgets/event/AccelerationChangeEvent.java \
	Java/com/phidgets/event/AttachListener.java \
	Java/com/phidgets/event/StepperVelocityChangeEvent.java \
	Java/com/phidgets/event/CodeEvent.java \
	Java/com/phidgets/event/ErrorEvent.java \
	Java/com/phidgets/event/TemperatureChangeEvent.java \
	Java/com/phidgets/event/DetachListener.java \
	Java/com/phidgets/event/LearnListener.java \
	Java/com/phidgets/event/ServoVelocityChangeEvent.java \
	Java/com/phidgets/event/PHChangeListener.java \
	Java/com/phidgets/event/RawDataListener.java \
	Java/com/phidgets/event/CurrentChangeListener.java \
	Java/com/phidgets/event/TagGainEvent.java \
	Java/com/phidgets/event/StepperPositionChangeEvent.java \
	Java/com/phidgets/event/WeightChangeListener.java \
	Java/com/phidgets/event/InputChangeEvent.java \
	Java/com/phidgets/event/TagLossListener.java \
	Java/com/phidgets/event/ServerDisconnectEvent.java \
	Java/com/phidgets/event/TemperatureChangeListener.java \
	Java/com/phidgets/event/SpatialDataListener.java \
	Java/com/phidgets/event/SensorChangeEvent.java \
	Java/com/phidgets/event/CodeListener.java \
	Java/com/phidgets/event/ServerConnectEvent.java \
	Java/com/phidgets/event/StepperPositionChangeListener.java \
	Java/com/phidgets/event/SpatialDataEvent.java \
	Java/com/phidgets/event/AttachEvent.java \
	Java/com/phidgets/event/EncoderPositionChangeListener.java \
	Java/com/phidgets/event/EncoderPositionChangeEvent.java \
	Java/com/phidgets/event/DetachEvent.java \
	Java/com/phidgets/event/OutputChangeListener.java \
	Java/com/phidgets/event/TagGainListener.java \
	Java/com/phidgets/event/KeyRemovalListener.java \
	Java/com/phidgets/event/KeyChangeEvent.java \
	Java/com/phidgets/event/KeyRemovalEvent.java \
	Java/com/phidgets/event/OutputChangeEvent.java \
	Java/com/phidgets/event/StepperVelocityChangeListener.java \
	Java/com/phidgets/event/ServerDisconnectListener.java \
	Java/com/phidgets/event/ErrorListener.java \
	Java/com/phidgets/event/SensorChangeListener.java \
	Java/com/phidgets/event/PHChangeEvent.java \
	Java/com/phidgets/event/LearnEvent.java \
	Java/com/phidgets/event/WeightChangeEvent.java \
	Java/com/phidgets/event/MotorVelocityChangeListener.java \
	Java/com/phidgets/event/TagLossEvent.java \
	Java/com/phidgets/event/KeyChangeListener.java \
	Java/com/phidgets/event/ServerConnectListener.java \
	Java/com/phidgets/event/AccelerationChangeListener.java \
	Java/com/phidgets/event/RawDataEvent.java \
	Java/com/phidgets/event/InputChangeListener.java \
	Java/com/phidgets/event/ServoPositionChangeEvent.java \
	Java/com/phidgets/event/CurrentChangeEvent.java \
	Java/com/phidgets/event/ServoVelocityChangeListener.java \
	Java/com/phidgets/event/MotorVelocityChangeEvent.java \
	Java/com/phidgets/Phidget.java

if ZEROCONF
libphidget21_la_SOURCES += zeroconf.h
else
AM_CFLAGS += -DNO_ZEROCONF
endif
	
if ZEROCONF_AVAHI
libphidget21_la_SOURCES += linux/zeroconf_avahi.c \
	linux/avahi-client/client.h \
	linux/avahi-client/lookup.h \
	linux/avahi-client/publish.h \
	linux/avahi-common/address.h \
	linux/avahi-common/alternative.h \
	linux/avahi-common/cdecl.h \
	linux/avahi-common/defs.h \
	linux/avahi-common/domain.h \
	linux/avahi-common/error.h \
	linux/avahi-common/gccmacro.h \
	linux/avahi-common/llist.h \
	linux/avahi-common/malloc.h \
	linux/avahi-common/rlist.h \
	linux/avahi-common/simple-watch.h \
	linux/avahi-common/strlst.h \
	linux/avahi-common/thread-watch.h \
	linux/avahi-common/timeval.h \
	linux/avahi-common/watch.h
endif

if ZEROCONF_BONJOUR
libphidget21_la_SOURCES += zeroconf.c \
	include/dns_sd.h
endif

if ZEROCONF_LOOKUP
libphidget21_la_SOURCES += zeroconf_lookup.c
AM_CFLAGS += -DZEROCONF_LOOKUP
endif

if DEBUG
AM_CFLAGS += -D_DEBUG -DDEBUG
endif

if JNI
AM_CFLAGS += -I$(top_srcdir)/include/jni -I$(top_srcdir)/include/jni/linux
libphidget21_la_SOURCES += 	Java/com_phidgets_Phidget.c \
	Java/com_phidgets_Phidget.h \
	Java/com_phidgets_AccelerometerPhidget.c \
	Java/com_phidgets_AccelerometerPhidget.h \
	Java/com_phidgets_AdvancedServoPhidget.c \
	Java/com_phidgets_AdvancedServoPhidget.h \
	Java/com_phidgets_EncoderPhidget.c \
	Java/com_phidgets_EncoderPhidget.h \
	Java/com_phidgets_InterfaceKitPhidget.c \
	Java/com_phidgets_InterfaceKitPhidget.h \
	Java/com_phidgets_IRPhidget.c \
	Java/com_phidgets_IRPhidget.h \
	Java/com_phidgets_LEDPhidget.c \
	Java/com_phidgets_LEDPhidget.h \
	Java/com_phidgets_MotorControlPhidget.c \
	Java/com_phidgets_MotorControlPhidget.h \
	Java/com_phidgets_PHSensorPhidget.c \
	Java/com_phidgets_PHSensorPhidget.h \
	Java/com_phidgets_RFIDPhidget.c \
	Java/com_phidgets_RFIDPhidget.h \
	Java/com_phidgets_ServoPhidget.c \
	Java/com_phidgets_ServoPhidget.h \
	Java/com_phidgets_SpatialPhidget.c \
	Java/com_phidgets_SpatialPhidget.h \
	Java/com_phidgets_StepperPhidget.c \
	Java/com_phidgets_StepperPhidget.h \
	Java/com_phidgets_TemperatureSensorPhidget.c \
	Java/com_phidgets_TemperatureSensorPhidget.h \
	Java/com_phidgets_TextLCDPhidget.c \
	Java/com_phidgets_TextLCDPhidget.h \
	Java/com_phidgets_TextLEDPhidget.c \
	Java/com_phidgets_TextLEDPhidget.h \
	Java/com_phidgets_Manager.c \
	Java/com_phidgets_Manager.h \
	Java/com_phidgets_Dictionary.c \
	Java/com_phidgets_Dictionary.h \
	Java/com_phidgets_DictionaryKeyListener.c \
	Java/com_phidgets_DictionaryKeyListener.h \
	Java/com_phidgets_WeightSensorPhidget.c \
	Java/com_phidgets_WeightSensorPhidget.h \
	Java/phidget_jni.c \
	Java/phidget_jni.h \
	include/jni/jawt.h \
	include/jni/jni.h \
	include/jni/jvmdi.h \
	include/jni/jvmpi.h \
	include/jni/linux/jawt_md.h \
	include/jni/linux/jni_md.h
endif

#remove pre-autoconf tools install if present
install-exec-local:
	if [ -f /usr/lib/libphidget21.so ]; then \
		rm -f /usr/lib/libphidget21.so; \
		rm -f /usr/include/phidget21.h; \
	fi
	
if RUN_LDCONFIG
install-exec-hook:
	-$(LDCONFIG)
endif

phidget21.h:
	@echo "Making $@"
	@echo "#ifndef PHIDGET_H" > $@
	@echo "#define PHIDGET_H" >> $@
	@echo "#ifdef __cplusplus" >> $@
	@echo "extern \"C\" {" >> $@
	@echo "#endif" >> $@
	@$(CC) -g -E -P $(PHIDGET21FLAGS) -D_LINUX -DEXTERNALPROTO -DREMOVE_DEPRECATED phidget21int.h | sed -e "s/extern \"C\" //g" >> $@
	@cat cphidgetconstants.h >> $@
	@echo "#ifdef __cplusplus" >> $@
	@echo "}" >> $@
	@echo "#endif" >> $@
	@echo "#endif" >> $@
	
phidget21matlab.h:
	@echo "Making $@"
	@echo "/*" > $@
	@echo " * Phidget21 Header for Matlab" >> $@
	@echo " * -All pointers to structs have been changed to long, as Matlab seems" >> $@
	@echo " *  to be unhappy with either the undefined stucts or the double pointers." >> $@
	@echo " */" >> $@
	@$(CC) -g -E -P -D_LINUX -DEXTERNALPROTO -DREMOVE_DEPRECATED phidget21int.h | sed -e "s/extern \"C\" //g" | sed -e "s/struct [a-zA-Z_]* \*/long /" >> $@
	@cat cphidgetconstants.h >> $@

phidget21.jar:
	@echo "Making $@"
	@cd Java ; javac com/phidgets/*.java com/phidgets/event/*.java
	@cd Java ; jar cf ../$@ com/phidgets/event/*.class com/phidgets/*.class
	
jni: all

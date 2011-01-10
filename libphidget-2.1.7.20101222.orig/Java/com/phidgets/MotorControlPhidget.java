
/*
 * Copyright 2006 Phidgets Inc.  All rights reserved.
 */

package com.phidgets;
import java.util.Iterator;
import java.util.LinkedList;
import com.phidgets.event.*;
/**
 * This class represents a Phidget Motor Controller. All methods
 * to to control a motor controller and read back motor data are implemented in this class.
 <p>
 The Motor Control Phidget is able to control 1 or more DC motors. 
 Both speed and acceleration are controllable. Speed is controlled via PWM. The size of the motors
 that can be driven depends on the motor controller. See your hardware documentation for more information.
 <p>
 The motor Controller boards also has 0 or more digital inputs.
 * 
 * @author Phidgets Inc.
 */
public final class MotorControlPhidget extends Phidget
{
	public MotorControlPhidget () throws PhidgetException
	{
		super (create ());
	}
	private static native long create () throws PhidgetException;
	/**
	 * Returns the number of motors supported by this Phidget. This does not neccesarily correspond
	 to the number of motors actually attached to the board.
	 * @return number of supported motors
	 * @throws PhidgetException If this Phidget is not opened and attached. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native int getMotorCount () throws PhidgetException;
	/**
	 * Returns the number of digital inputs. Not all Motor Controllers have digital inputs.
	 * @return number of digital inputs
	 * @throws PhidgetException If this Phidget is not opened and attached. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native int getInputCount () throws PhidgetException;
	/**
	 * Returns the state of a digital input. True means that the input is activated, and False indicated the default state.
	 * @param index index of the input
	 * @return state of the input
	 * @throws PhidgetException If this Phidget is not opened and attached, or if the index is invalid. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native boolean getInputState (int index) throws PhidgetException;
	/**
	 * Returns a motor's acceleration. The valid range is between {@link #getAccelerationMin getAccelerationMin} and {@link #getAccelerationMax getAccelerationMax}, and refers to how fast the Motor Controller
	 will change the speed of a motor.
	 * @param index index of motor
	 * @return acceleration of motor
	 * @throws PhidgetException If this Phidget is not opened and attached, or if the index is invalid. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native double getAcceleration (int index) throws PhidgetException;
	/**
	 * Sets a motor's acceleration. 
	 * Th valid range is between {@link #getAccelerationMin getAccelerationMin} and {@link #getAccelerationMax getAccelerationMax}. This controls how fast the motor changes speed.
	 * @param index index of the motor
	 * @param acceleration requested acceleration for that motor
	 * @throws PhidgetException If this Phidget is not opened and attached, or if the index or acceleration value are invalid. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native void setAcceleration (int index, double acceleration) throws PhidgetException;
	/**
	 * Returns the maximum acceleration that a motor will accept, or return.
	 * @param index Index of the motor
	 * @return Maximum acceleration
	 * @throws PhidgetException If this Phidget is not opened and attached.
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native double getAccelerationMax (int index) throws PhidgetException;
	/**
	 * Returns the minimum acceleration that a motor will accept, or return.
	 * @param index Index of the motor
	 * @return Minimum acceleration
	 * @throws PhidgetException If this Phidget is not opened and attached.
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native double getAccelerationMin (int index) throws PhidgetException;
	/**
	 * Returns a motor's velocity. The valid range is -100 - 100, with 0 being stopped.
	 * @param index index of the motor
	 * @return current velocity of the motor
	 * @throws PhidgetException If this Phidget is not opened and attached, or if the index is invalid. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native double getVelocity (int index) throws PhidgetException;
	/**
	 * @deprecated  Replaced by
	 *              {@link #getVelocity}
	 */
	public native double getSpeed (int index) throws PhidgetException;
	/**
	 * Sets a motor's velocity.
	 * The valid range is from -100 to 100, with 0 being stopped. -100 and 100 both corespond to full voltage,
	 with the value in between corresponding to different widths of PWM.
	 * @param index index of the motor
	 * @param velocity requested velocity for the motor
	 * @throws PhidgetException If this Phidget is not opened and attached, or if the index or speed value are invalid. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native void setVelocity (int index, double velocity) throws PhidgetException;
	/**
	 * @deprecated  Replaced by
	 *              {@link #setVelocity}
	 */
	public native void setSpeed (int index, double speed) throws PhidgetException;
	/**
	 * Returns a motor's current usage. The valid range is 0 - 255. Note that this is not supported on all motor controllers.
	 * @param index index of the motor
	 * @return current usage of the motor
	 * @throws PhidgetException If this Phidget is not opened and attached, or if the index is invalid. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native double getCurrent (int index) throws PhidgetException;

	private final void enableDeviceSpecificEvents (boolean b)
	{
		enableMotorVelocityChangeEvents (b && motorVelocityChangeListeners.size () > 0);
		enableCurrentChangeEvents (b && currentChangeListeners.size () > 0);
		enableInputChangeEvents (b && inputChangeListeners.size () > 0);
	}
	/**
	 * Adds a velocity change listener. The velocity change handler is a method that will be called when the velocity
	 of a motor changes. These velocity changes are reported back from the Motor Controller and so correspond to actual motor speeds
	 over time.
	 * <p>
	 * There is no limit on the number of velocity change handlers that can be registered for a particular Phidget.
	 * 
	 * @param l An implemetation of the {@link com.phidgets.event.MotorVelocityChangeListener MotorVelocityChangeListener} interface
	 */
	public final void addMotorVelocityChangeListener (MotorVelocityChangeListener l)
	{
		synchronized (motorVelocityChangeListeners)
		{
			motorVelocityChangeListeners.add (l);
			enableMotorVelocityChangeEvents (true);
	}} private LinkedList motorVelocityChangeListeners = new LinkedList ();
	private long nativeMotorVelocityChangeHandler = 0;
	public final void removeMotorVelocityChangeListener (MotorVelocityChangeListener l)
	{
		synchronized (motorVelocityChangeListeners)
		{
			motorVelocityChangeListeners.remove (l);
			enableMotorVelocityChangeEvents (motorVelocityChangeListeners.size () > 0);
	}} private void fireMotorVelocityChange (MotorVelocityChangeEvent e)
	{
		synchronized (motorVelocityChangeListeners)
		{
			for (Iterator it = motorVelocityChangeListeners.iterator (); it.hasNext ();)
				((MotorVelocityChangeListener) it.next ()).motorVelocityChanged (e);
		}
	}
	private native void enableMotorVelocityChangeEvents (boolean b);
	/**
	 * Adds a current change listener. The current change handler is a method that will be called when the current
	 consumed by a motor changes. Note that this event is not supported with the current motor controller, but
	 will be supported in the future
	 * <p>
	 * There is no limit on the number of current change handlers that can be registered for a particular Phidget.
	 * 
	 * @param l An implemetation of the {@link com.phidgets.event.CurrentChangeListener CurrentChangeListener} interface
	 */
	public final void addCurrentChangeListener (CurrentChangeListener l)
	{
		synchronized (currentChangeListeners)
		{
			currentChangeListeners.add (l);
			enableCurrentChangeEvents (true);
	}} private LinkedList currentChangeListeners = new LinkedList ();
	private long nativeCurrentChangeHandler = 0;
	public final void removeCurrentChangeListener (CurrentChangeListener l)
	{
		synchronized (currentChangeListeners)
		{
			currentChangeListeners.remove (l);
			enableCurrentChangeEvents (currentChangeListeners.size () > 0);
	}} private void fireCurrentChange (CurrentChangeEvent e)
	{
		synchronized (currentChangeListeners)
		{
			for (Iterator it = currentChangeListeners.iterator (); it.hasNext ();)
				((CurrentChangeListener) it.next ()).currentChanged (e);
		}
	}
	private native void enableCurrentChangeEvents (boolean b);
	/**
	 * Adds an input change listener. The input change handler is a method that will be called when an input on this
	 * Motor Controller board has changed.
	 * <p>
	 * There is no limit on the number of input change handlers that can be registered for a particular Phidget.
	 * 
	 * @param l An implemetation of the {@link com.phidgets.event.InputChangeListener InputChangeListener} interface
	 */
	public final void addInputChangeListener (InputChangeListener l)
	{
		synchronized (inputChangeListeners)
		{
			inputChangeListeners.add (l);
			enableInputChangeEvents (true);
	}} private LinkedList inputChangeListeners = new LinkedList ();
	private long nativeInputChangeHandler = 0;
	public final void removeInputChangeListener (InputChangeListener l)
	{
		synchronized (inputChangeListeners)
		{
			inputChangeListeners.remove (l);
			enableInputChangeEvents (inputChangeListeners.size () > 0);
	}} private void fireInputChange (InputChangeEvent e)
	{
		synchronized (inputChangeListeners)
		{
			for (Iterator it = inputChangeListeners.iterator (); it.hasNext ();)
				((InputChangeListener) it.next ()).inputChanged (e);
		}
	}
	private native void enableInputChangeEvents (boolean b);
}

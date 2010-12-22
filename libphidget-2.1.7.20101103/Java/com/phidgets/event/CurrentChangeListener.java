/*
 * Copyright 2006 Phidgets Inc.  All rights reserved.
 */

package com.phidgets.event;

/**
 * This interface represents a CurrentChangeEvent. This event originates from the Phidget Motor Controller.
 * This event is not currently thrown by any Motor Controllers but will be in the future
 * 
 * @author Phidgets Inc.
 */
public interface CurrentChangeListener
{
	/**
	 * This method is called with the event data when a new event arrives.
	 * 
	 * @param ae the event data object containing event data
	 */
	public void currentChanged(CurrentChangeEvent ae);
}

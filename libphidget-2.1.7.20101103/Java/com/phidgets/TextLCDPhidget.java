
/*
 * Copyright 2006 Phidgets Inc.  All rights reserved.
 */

package com.phidgets;
import java.util.Iterator;
import java.util.LinkedList;
import com.phidgets.event.*;
/**
 * This class represents a Phidget Text LCD. All methods
 * to control the Text LCD are implemented in this class.
 * <p>
 * The TextLCD Phidget consists of a Vacuum Fluorescent display that is capable of
 * displaying Standard as well as custom characters in multiple rows.
 * 
 * @author Phidgets Inc.
 */
public final class TextLCDPhidget extends Phidget
{
	public TextLCDPhidget () throws PhidgetException
	{
		super (create ());
	}
	private static native long create () throws PhidgetException;
	/**
	 * Rwturns the number of rows available on the display. 
	 * @return Number of rows
	 * @throws PhidgetException If this Phidget is not opened and attached. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native int getRowCount () throws PhidgetException;
	/**
	 * Returns the number of columns (characters per row) available on the display. This value is
	 * the same for every row.
	 * @return Number of columns
	 * @throws PhidgetException If this Phidget is not opened and attached. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native int getColumnCount () throws PhidgetException;
	/**
	 * Returns the contrast of the display. This is the contrast of the entire display.
	 * @return Current contrast
	 * @throws PhidgetException If this Phidget is not opened and attached. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native int getContrast () throws PhidgetException;
	/**
	 * Sets the contrast of the display. The valid range is 0-255. Changing the contrast can increase the readability
	 * of the display in certain viewing situation, such as at an odd angle.
	 * @param contrast New contrast to set
	 * @throws PhidgetException If this Phidget is not opened and attached, or the contrast value is out of range. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native void setContrast (int contrast) throws PhidgetException;
	/**
	 * Returns the brightness of the display. This is the brightness of the backlight.
	 * @return Current brightness
	 * @throws PhidgetException If this Phidget is not opened and attached. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached, or brightness is not supported.
	 */
	public native int getBrightness () throws PhidgetException;
	/**
	 * Sets the brightness of the display. The valid range is 0-255. Changing the brightness can increase the readability
	 * of the display in certain viewing situation, such as at an odd angle.
	 * @param brightness New brightness to set
	 * @throws PhidgetException If this Phidget is not opened and attached, or the brightness value is out of range, or brightness is not supported.
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native void setBrightness (int brightness) throws PhidgetException;
	/**
	 * Returns the status of the backlight. True indicated that the backlight is on, False indicated that it is off.
	 * The backlight is by default turned on.
	 * @return Status of the backlight
	 * @throws PhidgetException If this Phidget is not opened and attached. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native boolean getBacklight () throws PhidgetException;
	/**
	 * Sets the status of the backlight. True turns the backlight on, False turns it off.
	 * The backlight is by default turned on.
	 * @param backlight New backlight state
	 * @throws PhidgetException If this Phidget is not opened and attached. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native void setBacklight (boolean backlight) throws PhidgetException;
	/**
	 * Returns the status of the cursor. True turns the cursor is on, False turns it off.
	 * The cursor is an underscore which appears directly to the right of the last entered character
	 * on the display. The cursor is by default disabled.
	 * @return Status of the cursor
	 * @throws PhidgetException If this Phidget is not opened and attached. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native boolean getCursor () throws PhidgetException;
	/**
	 * Sets the state of the cursor. True indicates that the cursor on, False indicates that it is off.
	 * The cursor is an underscore which appears directly to the right of the last entered character
	 * on the display. The cursor is by default disabled.
	 * @param cursor New cursor state
	 * @throws PhidgetException If this Phidget is not opened and attached. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native void setCursor (boolean cursor) throws PhidgetException;
	/**
	 * Returns the status of the cursor blink. True turns the cursor blink on, False turns it off.
	 * The cursor blink is an flashing box which appears directly to the right of the last entered character
	 * on the display, in the same spot as the cursor if it is enabled. The cursor blink is by default disabled.
	 * @return Status of the cursor blink
	 * @throws PhidgetException If this Phidget is not opened and attached. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native boolean getCursorBlink () throws PhidgetException;
	/**
	 * Sets the state of the cursor blink. True indicates that the cursor blink is on, False indicates that it is off.
	 * The cursor blink is an flashing box which appears directly to the right of the last entered character
	 * on the display, in the same spot as the cursor if it is enabled. The cursor blink is by default disabled.
	 * @param cursorblink New cursor blink state
	 * @throws PhidgetException If this Phidget is not opened and attached. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native void setCursorBlink (boolean cursorblink) throws PhidgetException;
	/**
	 * Sets the display string of a certain row.
	 * @param index row
	 * @param text String
	 * @throws PhidgetException If this Phidget is not opened and attached, if the string is too long, or if the row is invalid. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native void setDisplayString (int index, String text) throws PhidgetException;
	/**
	 * Sets the character to display at a certain row and column.
	 * @param row row
	 * @param column column
	 * @param character character
	 * @throws PhidgetException If this Phidget is not opened and attached, or if the row or column is invalid. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native void setDisplayCharacter (int row, int column, char character) throws PhidgetException;
	/**
	 * Sets a custom character. You can set up to 8 custom characters, each one is completely defined by two integers, and gets
	 * stored in the character display until power is removed, whence they must be re-programmed. The characters lie in positions 8-15, and
	 * can be displayed by sending these codes to setDisplayString in amongst standard ASCII characters. See the TextLCD java example for
	 * more information.
	 * @param index position ( 8-15 )
	 * @param param1 first half of the character code
	 * @param param2 second half of the character code
	 * @throws PhidgetException If this Phidget is not opened and attached, or if the index is invalid. 
	 * See {@link com.phidgets.Phidget#open(int) open} for information on determining if a device is attached.
	 */
	public native void setCustomCharacter (int index, int param1, int param2) throws PhidgetException;
	private final void enableDeviceSpecificEvents (boolean b)
	{
	}
}

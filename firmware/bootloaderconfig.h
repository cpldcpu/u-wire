/* Name: bootloaderconfig.h
 * Project: USBaspLoader
 * Author: Christian Starkjohann
 * Creation Date: 2007-12-08
 * Tabsize: 4
 * Copyright: (c) 2007 by OBJECTIVE DEVELOPMENT Software GmbH
 * Portions Copyright: (c) 2012 Louis Beaudoin
 * License: GNU GPL v2 (see License.txt)
 * This Revision: $Id: bootloaderconfig.h 729 2009-03-20 09:03:58Z cs $
 */

#ifndef __bootloaderconfig_h_included__
#define __bootloaderconfig_h_included__

/*
General Description:
This file (together with some settings in Makefile) configures the boot loader
according to the hardware.

This file contains (besides the hardware configuration normally found in
usbconfig.h) two functions or macros: bootLoaderInit() and
bootLoaderCondition(). Whether you implement them as macros or as static
inline functions is up to you, decide based on code size and convenience.

bootLoaderInit() is called as one of the first actions after reset. It should
be a minimum initialization of the hardware so that the boot loader condition
can be read. This will usually consist of activating a pull-up resistor for an
external jumper which selects boot loader mode.

bootLoaderCondition() is called immediately after initialization and in each
main loop iteration. If it returns TRUE, the boot loader will be active. If it
returns FALSE, the boot loader jumps to address 0 (the loaded application)
immediately.

For compatibility with Thomas Fischl's avrusbboot, we also support the macro
names BOOTLOADER_INIT and BOOTLOADER_CONDITION for this functionality. If
these macros are defined, the boot loader uses them.
*/

#define TINY85_HARDWARE_CONFIG_1  1
#define TINY85_HARDWARE_CONFIG_2  2

/* ---------------------------- Hardware Config ---------------------------- */
#define HARDWARE_CONFIG     TINY85_HARDWARE_CONFIG_2

#define USB_CFG_IOPORTNAME      B
  /* This is the port where the USB bus is connected. When you configure it to
   * "B", the registers PORTB, PINB and DDRB will be used.
   */

#ifndef __AVR_ATtiny85__
  # define USB_CFG_DMINUS_BIT      0
  /* This is the bit number in USB_CFG_IOPORT where the USB D- line is connected.
   * This may be any bit in the port.
   */
  #define USB_CFG_DPLUS_BIT       2
  /* This is the bit number in USB_CFG_IOPORT where the USB D+ line is connected.
   * This may be any bit in the port. Please note that D+ must also be connected
   * to interrupt pin INT0!
   */
#endif
 
#if (defined __AVR_ATtiny85__) && (HARDWARE_CONFIG == TINY85_HARDWARE_CONFIG_1)
  #define USB_CFG_DMINUS_BIT      0
  /* This is the bit number in USB_CFG_IOPORT where the USB D- line is connected.
   * This may be any bit in the port.
   */
  #define USB_CFG_DPLUS_BIT       2
  /* This is the bit number in USB_CFG_IOPORT where the USB D+ line is connected.
   * This may be any bit in the port, but must be configured as a pin change interrupt.
   */
#endif
 
#if (defined __AVR_ATtiny85__) && (HARDWARE_CONFIG == TINY85_HARDWARE_CONFIG_2)
#define USB_CFG_DMINUS_BIT      3
/* This is the bit number in USB_CFG_IOPORT where the USB D- line is connected.
 * This may be any bit in the port.
 */
#define USB_CFG_DPLUS_BIT       4
/* This is the bit number in USB_CFG_IOPORT where the USB D+ line is connected.
 * This may be any bit in the port, but must be configured as a pin change interrupt.
 */
#endif

#define USB_CFG_CLOCK_KHZ       (F_CPU/1000)
/* Clock rate of the AVR in kHz. Legal values are 12000, 16000 or 16500.
 * The 16.5 MHz version of the code requires no crystal, it tolerates +/- 1%
 * deviation from the nominal frequency. All other rates require a precision
 * of 2000 ppm and thus a crystal!
 * Default if not specified: 12 MHz
 */

/* ----------------------- Optional Hardware Config ------------------------ */

/* #define USB_CFG_PULLUP_IOPORTNAME   D */
/* If you connect the 1.5k pullup resistor from D- to a port pin instead of
 * V+, you can connect and disconnect the device from firmware by calling
 * the macros usbDeviceConnect() and usbDeviceDisconnect() (see usbdrv.h).
 * This constant defines the port on which the pullup resistor is connected.
 */
/* #define USB_CFG_PULLUP_BIT          4 */
/* This constant defines the bit number in USB_CFG_PULLUP_IOPORT (defined
 * above) where the 1.5k pullup resistor is connected. See description
 * above for details.
 */


/* ------------- Set up interrupt configuration (CPU specific) -------------- */

// setup interrupt for Pin Change for D+
#define USB_INTR_CFG            PCMSK
#define USB_INTR_CFG_SET        (1 << USB_CFG_DPLUS_BIT)
#define USB_INTR_CFG_CLR        0
#define USB_INTR_ENABLE         GIMSK
#define USB_INTR_ENABLE_BIT     PCIE
#define USB_INTR_PENDING        GIFR
#define USB_INTR_PENDING_BIT    PCIF
#define USB_INTR_VECTOR         PCINT0_vect


#endif /* __bootloader_h_included__ */

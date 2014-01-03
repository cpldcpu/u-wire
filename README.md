micro-wire
==========

This is an attempt at the smallest possible USB compliant device with V-USB. It uses an ATtiny85 
to control a single WS2812B LED via USB. 

The source started out as an amalganation of micronucleus and littlewire. All excess functionality was cut away.

Currently the device supports
 * Enumeration.
 * Only SETUP-request can be received.
 * Responses are limited to strings from flash memory or zero sized replies.
 
Memory use
  * 1018 bytes Flash
  * 28 bytes SRAM
  * Uses only regs R16-R31
 





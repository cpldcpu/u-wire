/* 
 * µ-wire
 * cpldcpu Jan 2014
 * 
 * Smallest possible USB Complaint device based on V-USB
 *
 * (Uses little-wire host-code and a stripped down micronucleus)
 */
 
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <util/delay.h>

#include "bootloaderconfig.h"
#include "usbdrv/usbdrv.c"

#define ws2812_mask _BV(PB0)

// Definition of sei and cli without memory barrier keyword to prevent reloading of memory variables
#define sei() asm volatile("sei")
#define cli() asm volatile("cli")
#define nop() asm volatile("nop")

// Use the old delay routines without NOP padding. This saves memory.
#define __DELAY_BACKWARD_COMPATIBLE__  

/* ------------------------------------------------------------------------ */
static uint8_t usbFunctionSetup(uint8_t data[8]);

#define ws2812_port PORTB		// Data port register

inline void ws2812_sendarray_mask(uint8_t *data,uint8_t datlen)
{
	uint8_t curbyte,ctr,masklo,maskhi=ws2812_mask;
	masklo	=~maskhi&ws2812_port;
	maskhi |=ws2812_port;
	
	while (datlen--) {
		curbyte=*data++;
		
		asm volatile(
		
		"		ldi	%0,8		\n\t"		// 0
		"loop%=:out	%2,	%3		\n\t"		// 1
		"		lsl	%1			\n\t"		// 2
		"		dec	%0			\n\t"		// 3

		"		rjmp .+0		\n\t"		// 5
		
		"		brcs .+2		\n\t"		// 6l / 7h
		"		out	%2,	%4		\n\t"		// 7l / -

		"		rjmp .+0		\n\t"		// 9
		
		"		nop				\n\t"		// 10
		"		out	%2,	%4		\n\t"		// 11
		"		breq end%=		\n\t"		// 12      nt. 13 taken

		"		rjmp .+0		\n\t"		// 14
		"		rjmp .+0		\n\t"		// 16
		"		rjmp .+0		\n\t"		// 18
		"		rjmp loop%=		\n\t"		// 20
		"end%=:					\n\t"
		:	"=&d" (ctr)
		:	"r" (curbyte), "I" (_SFR_IO_ADDR(ws2812_port)), "r" (maskhi), "r" (masklo)
		);
		
		// loop overhead including byte load is 6+1 cycles
	}
} 

/* ------------------------------------------------------------------------ */
int main(void) __attribute__((__noreturn__));
int main(void) {
  usbMsgLen_t  usbMsgLen; /* remaining number of bytes */
//  usbMsgPtr_t  usbMsgPtr;  
  usbDeviceDisconnect();  /* do this while interrupts are disabled */
  _delay_ms(500);  
  usbDeviceConnect();

  usbTxLen = USBPID_NAK;
  usbMsgLen = USB_NO_MSG;
  #if USB_INTR_CFG_SET != 0
  USB_INTR_CFG |= USB_INTR_CFG_SET;
  #endif
  #if USB_INTR_CFG_CLR != 0
  USB_INTR_CFG &= ~(USB_INTR_CFG_CLR);
  #endif
  USB_INTR_ENABLE |= (1 << USB_INTR_ENABLE_BIT);
    
  DDRB|=ws2812_mask;
  
  // Dangerous hack:
  // We assume that the next low level on D- is the host-issued reset. This will
  // be detected as the first pulse in the calibrate osc-routine, forcing it to
  // search for and oscall value above 128. This is true for the ATtiny85 and >16Mhz,
  // but may break under other circumstances. 
  // Note: This seems to be a bug in all V-USB implementations with osccal.
  // This approach may also fail if the device is powered while it is reconnected
  // to a new USB host as the usb driver is going to ignore reset.
 
  calibrateOscillatorASM();
  
  sei();   
  do {    
    schar len;
    uchar usbLineStatus;

    len = usbRxLen - 3;
    if(len >= 0){
      uint8_t *data=(uint8_t *)usbRxBuf + USB_BUFSIZE + 1 - usbInputBufOffset;
      usbRequest_t    *rq = (void *)data;   
      /* usbRxToken can be:
       * 0x2d 00101101 (USBPID_SETUP for setup data)
       * 0xe1 11100001 (USBPID_OUT: data phase of setup transfer)
       * 0...0x0f for OUT on endpoint X  <- we do not use this
       * We assume that all packets are setup packets. (usbRxToken == USBPID_SETUP)
       * we also assume that there are no malformed requests - no error handling.
       */
     
        usbMsgLen_t replyLen;
        usbTxBuf[0] = USBPID_DATA0;         /* initialize data toggling */
        usbTxLen = USBPID_NAK;              /* abort pending transmit */
        uchar type = rq->bmRequestType & USBRQ_TYPE_MASK;
        if(type != USBRQ_TYPE_STANDARD){  // All nonstandard setup-requests are updating the LED
          cli();  
          ws2812_sendarray_mask(&data[3],3);        
          sei();
          replyLen=0;
        }else{   // standard requests are handled by driver 
          
          usbMsgLen_t len = 0;
          uchar   *dataPtr = usbTxBuf + 9;    /* there are 2 bytes free space at the end of the buffer */
          uchar   value = rq->wValue.bytes[0];

              dataPtr[0] = 0; /* default reply common to USBRQ_GET_STATUS and USBRQ_GET_INTERFACE */
              SWITCH_START(rq->bRequest)
              SWITCH_CASE(USBRQ_GET_STATUS)           /* 0 */
                  uchar recipient = rq->bmRequestType & USBRQ_RCPT_MASK;  /* assign arith ops to variables to enforce byte size */     
                  dataPtr[1] = 0;
                  len = 2;
              SWITCH_CASE(USBRQ_SET_ADDRESS)          /* 5 */
                  usbNewDeviceAddr = value;
              SWITCH_CASE(USBRQ_GET_DESCRIPTOR)       /* 6 */
                  len = usbDriverDescriptor(rq);
                  goto skipMsgPtrAssignment;
              SWITCH_CASE(USBRQ_GET_CONFIGURATION)    /* 8 */
                  dataPtr = &usbConfiguration;  /* send current configuration value */
                  len = 1;
              SWITCH_CASE(USBRQ_SET_CONFIGURATION)    /* 9 */
                  usbConfiguration = value;
              SWITCH_CASE(USBRQ_GET_INTERFACE)        /* 10 */
                  len = 1;
              SWITCH_DEFAULT                          /* 7=SET_DESCRIPTOR, 12=SYNC_FRAME */
              SWITCH_END
              usbMsgPtr = (usbMsgPtr_t)dataPtr;
            skipMsgPtrAssignment:
            replyLen=len;
          if(replyLen > rq->wLength.bytes[0])   
              replyLen = rq->wLength.bytes[0];
        }
        usbMsgLen = replyLen;
        usbRxLen = 0;       /* mark rx buffer as available */
    }
    
    if(usbTxLen & 0x10){    /* transmit system idle */
        if(usbMsgLen != USB_NO_MSG){    /* transmit data pending? */
          usbMsgLen_t wantLen=usbMsgLen;
          
          if(wantLen > 8)
              wantLen = 8;
              
          usbMsgLen -= wantLen;
          usbTxBuf[0] ^= USBPID_DATA0 ^ USBPID_DATA1; /* DATA toggling */

          {   
            uchar i = wantLen;
            usbMsgPtr_t r = usbMsgPtr;
            uint8_t     *data=usbTxBuf + 1;  
            
            while (i--)  /* don't bother app with 0 sized reads */
            { 
                uchar c = USB_READ_FLASH(r);    /* assign to char size variable to enforce byte ops */
                *data++ = c;
                r++;
            }    
            
            usbMsgPtr = r;
          }
          
          usbCrc16Append(&usbTxBuf[1], wantLen);
          wantLen += 4;           /* length including sync byte */
          if(wantLen < 12)        /* a partial package identifies end of message */
              usbMsgLen = USB_NO_MSG;

          usbTxLen = wantLen;
        }
    }
  } while(1);  
}
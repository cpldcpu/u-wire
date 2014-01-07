/* 
 * µ-wire
 * cpldcpu Jan 2014
 * 
 * Smallest possible USB compliant device based on V-USB
 *
 * (Uses little-wire host-code and a stripped down micronucleus)
 */
 
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <util/delay.h>

#define usbMsgPtr_t uint8_t

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

    "   ldi %0,8		\n\t"		// 0
    "loop%=:out	%2,	%3		\n\t"		// 1
    "   lsl	%1			\n\t"		// 2
    "   dec	%0			\n\t"		// 3

    "   rjmp .+0		\n\t"		// 5

    "   brcs .+2		\n\t"		// 6l / 7h
    "   out	%2,	%4		\n\t"		// 7l / -

    "   rjmp .+0		\n\t"		// 9

    "   nop				\n\t"		// 10
    "   out	%2,	%4		\n\t"		// 11
    "   breq end%=		\n\t"		// 12      nt. 13 taken

    "   rjmp .+0		\n\t"		// 14
    "   rjmp .+0		\n\t"		// 16
    "   rjmp .+0		\n\t"		// 18
    "   rjmp loop%=		\n\t"		// 20
    "end%=:					\n\t"
    :	"=&d" (ctr)
    :	"r" (curbyte), "I" (_SFR_IO_ADDR(ws2812_port)), "r" (maskhi), "r" (masklo)
    );

    // loop overhead including byte load is 6+1 cycles
  }
} 

/* We use if() instead of #if in the macro below because #if can't be used
 * in macros and the compiler optimizes constant conditions anyway.
 * This may cause problems with undefined symbols if compiled without
 * optimizing!
 */
#define GET_DESCRIPTOR(cfgProp, staticName) \
  if(cfgProp){                              \
    len = USB_PROP_LENGTH(cfgProp);         \
    usbMsgPtr = (usbMsgPtr_t)(staticName);  \
  }
    
/* ------------------------------------------------------------------------ */
int main(void) __attribute__((__noreturn__));
int main(void) {
  usbMsgLen_t  usbMsgLen; /* remaining number of bytes */
  usbMsgPtr_t  usbMsgPtr;  
  
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
  DDRB|=_BV(PB1)|_BV(PB2);
  
  // Dangerous hack:
  // We assume that the next low level on D- is the host-issued reset. This will
  // be detected as the first pulse in the calibrate osc-routine, forcing it to
  // search for an osccal value above 128. This is true for the ATtiny85 and >16Mhz,
  // but may break under other circumstances. 
  // Note: This seems to be a bug in all V-USB implementations with osccal.
  // This approach may also fail if the device is powered while it is reconnected
  // to a new USB host as the usb driver is going to ignore reset.
 
  calibrateOscillatorASM();
 //sei();
  USB_INTR_PENDING = 1<<USB_INTR_PENDING_BIT;                   
  do { 

     PORTB|=_BV(PB1);

    while ( !(USB_INTR_PENDING & (1<<USB_INTR_PENDING_BIT)) );
    
    USB_INTR_VECTOR();  
      PORTB&=~_BV(PB1);
    
  //  if ((usbRxBuf[0]==USBPID_SETUP)||(usbRxBuf[0]==USBPID_OUT)) continue;
    
    schar len;
    uchar usbLineStatus;
    
    len = usbRxLen - 3;
    if(len >= 0){
//      uint8_t *data=(uint8_t *)usbRxBuf + USB_BUFSIZE + 1 - usbInputBufOffset;
     uint8_t *data=(uint8_t *)usbRxBuf + 1 ;
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
          ws2812_sendarray_mask(&data[3],3);        
          replyLen=0;
    /*      little-wire version reply
          	if( req == 34 ) // This has to be hardcoded to 34!
              {
                data[0]=LITTLE_WIRE_VERSION;
                usbMsgPtr = data;
                return 1;		
              } 
  */
        }else{   // standard requests are handled by driver 
          
          usbMsgLen_t len = 0;
          uchar   value = rq->wValue.bytes[0];

              SWITCH_START(rq->bRequest)
              SWITCH_CASE(USBRQ_GET_STATUS)           /* 0 */
                  uchar recipient = rq->bmRequestType & USBRQ_RCPT_MASK;  /* assign arith ops to variables to enforce byte size */     
                  len = 2;
              SWITCH_CASE(USBRQ_SET_ADDRESS)          /* 5 */
                  usbNewDeviceAddr = value;
              SWITCH_CASE(USBRQ_GET_DESCRIPTOR)       /* 6 */
                SWITCH_START(rq->wValue.bytes[1])
                SWITCH_CASE(USBDESCR_DEVICE)    /* 1 */
                    GET_DESCRIPTOR(USB_CFG_DESCR_PROPS_DEVICE, usbDescriptorDevice)
                SWITCH_CASE(USBDESCR_CONFIG)    /* 2 */
                    GET_DESCRIPTOR(USB_CFG_DESCR_PROPS_CONFIGURATION, usbDescriptorConfiguration)
                SWITCH_CASE(USBDESCR_STRING)    /* 3 */
                    SWITCH_START(rq->wValue.bytes[0])
                    SWITCH_CASE(0)
                        GET_DESCRIPTOR(USB_CFG_DESCR_PROPS_STRING_0, usbDescriptorString0)
                    SWITCH_CASE(1)
                        GET_DESCRIPTOR(USB_CFG_DESCR_PROPS_STRING_VENDOR, usbDescriptorStringVendor)
                    SWITCH_CASE(2)
                        GET_DESCRIPTOR(USB_CFG_DESCR_PROPS_STRING_PRODUCT, usbDescriptorStringDevice)
                    SWITCH_CASE(3)
                        GET_DESCRIPTOR(USB_CFG_DESCR_PROPS_STRING_SERIAL_NUMBER, usbDescriptorStringSerialNumber)
                    SWITCH_DEFAULT
                    SWITCH_END
                SWITCH_DEFAULT
                SWITCH_END
              SWITCH_CASE(USBRQ_GET_INTERFACE)        /* 10 */
                  len = 1;
              SWITCH_DEFAULT                          /* 7=SET_DESCRIPTOR, 12=SYNC_FRAME */
              SWITCH_END
            replyLen=len;
          if(replyLen > rq->wLength.bytes[0])   
              replyLen = rq->wLength.bytes[0];
        }
        usbMsgLen = replyLen;
        usbRxLen = 0;       /* mark rx buffer as available */
    }
    
    if(usbTxLen & 0x10)   // transmit system is always idle in sequential mode
    {  
        if(usbMsgLen != USB_NO_MSG){    /* transmit data pending? */
          usbMsgLen_t wantLen=usbMsgLen;
          
          if(wantLen > 8) {
              wantLen = 8;
              usbMsgLen -= wantLen;
          }
          else {  // end of message reached
            usbMsgLen = USB_NO_MSG;
          }
          

          {   
            uint8_t i,c;
            usbMsgPtr_t r = usbMsgPtr;             
            uint8_t     *data=usbTxBuf + 1;

        // *data++ ^= USBPID_DATA0 ^ USBPID_DATA1; // DATA toggling 
        //  AVR-GCC 4.7.2 is too stupid to optimize this
        
            asm volatile(
            "         ld %0,-X    \n\t"        
            "         eor %0,%2   \n\t"        
            "         st  X+,%0  \n\t"   
            : "=&d" (c)
            :  "x" (data), "r" ((uint8_t)(USBPID_DATA0 ^ USBPID_DATA1))
            );            

            i=wantLen;
            while (i--)  // don't bother app with 0 sized reads 
            { 
                c = USB_READ_FLASH(r);    // assign to char size variable to enforce byte ops 
                *data++ = c;
                r++;
            }    
          }
          
          usbMsgPtr += wantLen;            
          
          usbCrc16Append(&usbTxBuf[1], wantLen);
          wantLen += 4;           /* length including sync byte */

          usbTxLen = wantLen;
        }
    }
    
        if (USB_INTR_PENDING & (1<<USB_INTR_PENDING_BIT))  // Usbpoll() collided with data packet
       {        
          uint8_t ctr;
         
          // loop takes 5 cycles
          asm volatile(      
          "         ldi  %0,%1 \n\t"        
          "loop%=:  sbic %2,%3  \n\t"        
          "         ldi  %0,%1  \n\t"
          "         subi %0,1   \n\t"        
          "         brne loop%= \n\t"   
          : "=&d" (ctr)
          :  "M" ((uint8_t)(10.0f*(F_CPU/1.0e6f)/5.0f+0.5)), "I" (_SFR_IO_ADDR(USBIN)), "M" (USB_CFG_DPLUS_BIT)
          );       
         USB_INTR_PENDING = 1<<USB_INTR_PENDING_BIT;                   
       }      
  } while(1);  
}

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

void ws2812_sendarray_mask(void)
{
  uint8_t curbyte=0,ctr,masklo,maskhi=ws2812_mask;
  masklo=~maskhi;
  uint8_t datlen=3;
  uint8_t *data=usbRxBuf + 4;

  asm volatile(
  "   in    %0,%2     \n\t"
  "   or    %3,%0     \n\t"
  "   and   %4,%0     \n\t"
  "olop%=:subi  %1,1 \n\t"		// 12
  "   brcs  exit%=    \n\t"		// 14
  "   ld    %6,z+     \n\t"		// 15
  "   ldi   %0,8      \n\t"		// 16
  "loop%=:out %2,%3   \n\t"		// 1
  "   rjmp .+0        \n\t"		// 3
  "   sbrs  %6,7      \n\t"  // 4nt / 5t
  "   out   %2, %4    \n\t"		// 5
  "   dec   %0        \n\t"		// 6
  "   rjmp .+0        \n\t"		// 8	
  "   out   %2, %4    \n\t"		// 9
  "   breq  olop%=    \n\t"		// 10nt  / 11t
  "   lsl   %6        \n\t"		// 11
  "   rjmp .+0        \n\t"		// 13
  "   rjmp  loop%=    \n\t"		// 15
  "exit%=:            \n\t"		//
  :	"=&d" (ctr)
  :	"r" (datlen), "I" (_SFR_IO_ADDR(ws2812_port)), "r" (maskhi), "r" (masklo), "z" (data), "r" (curbyte)
  );

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
void USB_INTR_VECTOR(void); 
int main(void) __attribute__((__noreturn__));
int main(void) {
  usbMsgLen_t  usbMsgLen; /* remaining number of bytes */
  usbMsgPtr_t  usbMsgPtr;  

#ifdef __AVR_ATtiny10__
    CCP=0xD8;   // configuration change protection, write signature
    CLKPSR=0;   // set cpu clock prescaler =1 (8Mhz) (attiny 4/5/9/10)
#endif    
 // usbDeviceDisconnect();  /* do this while interrupts are disabled */
 // _delay_ms(500);  
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
 
  calibrateOscillatorASM();
  USB_INTR_PENDING = 1<<USB_INTR_PENDING_BIT;                   
  do { 

    while ( !(USB_INTR_PENDING & (1<<USB_INTR_PENDING_BIT)) );
 //   PORTB|=ws2812_mask;  
    USB_INTR_VECTOR();      
 //   PORTB&=~ws2812_mask;
    
    schar len;
    uchar usbLineStatus;
    
    len = usbRxLen - 3;
    if(len >= 0){
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
    //  usbTxLen = USBPID_NAK;              /* abort pending transmit */
        uint8_t   type = rq->bmRequestType & USBRQ_TYPE_MASK;
        uint8_t   request= rq->bRequest;
        if(type != USBRQ_TYPE_STANDARD){  // All nonstandard setup-requests are updating the LED
          if (request == 34) { // little-wire version reply
            usbMsgPtr = (usbMsgPtr_t)(&usbDescriptorDevice[12]); // Version from usb descriptor
            replyLen=1;          
          }
          else {
            ws2812_sendarray_mask();
            replyLen=0;
          }
        }else{   // standard requests are handled by driver 

          usbMsgLen_t len = 0;
    
          uint8_t   request= rq->bRequest;
          uint8_t   value0 = rq->wValue.bytes[0];
          uint8_t   value1 = rq->wValue.bytes[1];
          
              SWITCH_START(request)
              SWITCH_CASE(USBRQ_GET_STATUS)           /* 0 */
                  len = 2;
              SWITCH_CASE(USBRQ_SET_ADDRESS)          /* 5 */
                  usbNewDeviceAddr = value0;
              SWITCH_CASE(USBRQ_GET_DESCRIPTOR)       /* 6 */
                SWITCH_START(value1)
                SWITCH_CASE(USBDESCR_DEVICE)    /* 1 */
                    GET_DESCRIPTOR(USB_CFG_DESCR_PROPS_DEVICE, usbDescriptorDevice)
                SWITCH_CASE(USBDESCR_CONFIG)    /* 2 */
                    GET_DESCRIPTOR(USB_CFG_DESCR_PROPS_CONFIGURATION, usbDescriptorConfiguration)
                SWITCH_CASE(USBDESCR_STRING)    /* 3 */
                    SWITCH_START(value0)
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
     //         SWITCH_CASE(USBRQ_GET_INTERFACE)        /* 10 */  We do not support an alternative interface
     //             len = 1;
              SWITCH_DEFAULT                          /* 7=SET_DESCRIPTOR, 12=SYNC_FRAME */
              SWITCH_END
            replyLen=len;
          if(replyLen > rq->wLength.bytes[0])   
              replyLen = rq->wLength.bytes[0];
        }
        usbMsgLen = replyLen;
        usbRxLen = 0;       /* mark rx buffer as available */
    }    
 //   if(usbTxLen & 0x10)   // transmit system is always idle in polled mode
    { usbMsgLen_t wantLen=USBPID_NAK;

        if(usbMsgLen != USB_NO_MSG){    /* transmit data pending? */
          wantLen=usbMsgLen;
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
#ifdef __AVR_ATtiny10__         
            asm volatile(
            "         ld %0,-Z    \n\t"        
            "         eor %0,%2   \n\t"        
            "         st  Z+,%0  \n\t"   
            : "=&d" (c)
            :  "z" (data), "r" ((uint8_t)(USBPID_DATA0 ^ USBPID_DATA1))
            );            
#else
            asm volatile(
            "         ld %0,-x    \n\t"        
            "         eor %0,%2   \n\t"        
            "         st  x+,%0  \n\t"   
            : "=&d" (c)
            :  "x" (data), "r" ((uint8_t)(USBPID_DATA0 ^ USBPID_DATA1))
            );            
#endif
            i=wantLen;
            while (i--)  // don't bother app with 0 sized reads 
            { 
#ifdef __AVR_ATtiny10__               
                uint8_t *flashbase=(uint8_t*)0x4000+r;
                c = *flashbase; 
#else
                c = USB_READ_FLASH(r);    // assign to char size variable to enforce byte ops 
#endif                
                *data++ = c;
                r++;
            }    
          }
          
          usbMsgPtr += wantLen;            
          
          usbCrc16Append(&usbTxBuf[1], wantLen);
          wantLen += 4;           /* length including sync byte */

        }
      usbTxLen = wantLen;
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

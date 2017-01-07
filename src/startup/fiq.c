/*
 * irq.c
 *
 *  Created on: 2016-10-26
 *      Author: lkuta, pblazinski
 */

#include "fiq.h"
#include "framework.h"
#include "lpc2xxx.h"
#include "printf_P.h"

void
initMyFiq(){

	  //install own FIQ handler (other than default handler)
	  pISR_FIQ = (unsigned int)_fiqHandler;

	  //initialize P0.14 to EINT1 (active falling edge)
	  EXTMODE  = 0x00000002;   //EINT1 is edge sensitive
	  EXTPOLAR = 0x00000000;   //EINT1 is falling edge sensitive
	  PINSEL0 &= ~0x30000000;
	  PINSEL0 |=  0x20000000;
	  EXTINT   = 0x00000002;   //reset EINT1 IRQ flag

	  //initialize VIC for EINT1 interrupts (as FIQ)
	  VICIntSelect |= 0x00008000;      //EINT1 interrupt is assigned to FIQ (not IRQ)
	  VICIntEnable  = 0x00008000;      //enable eint1 interrupt

}

void
_fiqHandler(void)
{
	printf("\nFIQ interrupt!");

	TIMER1_TCR = 0x00;

  //end interrupt
  EXTINT = 0x00000002;       //reset IRQ flag
  VICVectAddr = 0x00;        //dummy write to VIC to signal end of interrupt
}

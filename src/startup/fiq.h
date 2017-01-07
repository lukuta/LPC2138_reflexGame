/*
 * irq.h
 *
 *  Created on: 2016-10-26
 *      Author: lkuta, pblazinski
 */

#ifndef FIQ_H_
#define FIQ_H_

void initMyIrq();
void _fiqHandler(void) __attribute__((interrupt("FIQ")));

#endif /* FIQ_H_ */

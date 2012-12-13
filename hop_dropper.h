/*
 * hop_dropper.h
 *
 *  Created on: Dec 14, 2012
 *      Author: brad
 */

#ifndef HOP_DROPPER_H_
#define HOP_DROPPER_H_

#define HOP_DROPPER_DRIVE_PORT GPIOB
#define HOP_DROPPER_DRIVE_PIN GPIO_Pin_13

#define HOP_DROPPER_LIMIT_PORT GPIOA
#define HOP_DROPPER_LIMIT_PIN GPIO_Pin_11

void vHopDropperApplet(int init);
int iHopDropperKey(int xx, int yy);


#endif /* HOP_DROPPER_H_ */

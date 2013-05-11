/*
 * stir.h
 *
 *  Created on: Dec 15, 2012
 *      Author: brad
 */

#ifndef STIR_H_
#define STIR_H_

//---STIR CONTROL PORTS/PINS
#define STIR_CONTROL_PORT GPIOA
#define STIR_ENABLE_PIN GPIO_Pin_12
#define STIR_DIR_PIN GPIO_Pin_1

//---STIR STEPPER DRIVE PORT/PIN
#define STIR_DRIVE_PORT GPIOC
#define STIR_STEP_PIN GPIO_Pin_9

int iStirKey(int xx, int yy);
void vStirApplet(int init);
void vStirInit(void);
#endif /* STIR_H_ */

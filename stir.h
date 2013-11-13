/*
 * stir.h
 *
 *  Created on: Dec 15, 2012
 *      Author: brad
 */

#ifndef STIR_H_
#define STIR_H_

//---STIR CONTROL PORTS/PINS


#define STIR_PORT PORTU
#define STIR_PIN PCF_PIN0

void vStirInit(void);
void vStirApplet(int init);
int iStirKey(int xx, int yy);


#endif /* STIR_H_ */

/*
 * mill.h
 *
 *  Created on: Dec 13, 2012
 *      Author: brad
 */

#ifndef MILL_H_
#define MILL_H_

#define MILL_PORT GPIOE
#define MILL_PIN GPIO_Pin_6

void vMillInit(void);
void vMillApplet(int init);
int iMillKey(int xx, int yy);


#endif /* MILL_H_ */

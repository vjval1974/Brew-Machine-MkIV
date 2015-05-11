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
typedef enum
{
  MILL_DRIVING=1,
  MILL_STOPPED=0
}MillState;


void vMillInit(void);
void vMillApplet(int init);
int iMillKey(int xx, int yy);
void vMill( MillState state );




#endif /* MILL_H_ */

/*
 * brew.h
 *
 *  Created on: 14/11/2013
 *      Author: brad
 */

#ifndef BREW_H_
#define BREW_H_


void vTaskBrew(void * pvParameters);
void vBrewApplet(int init);
int iBrewKey(int xx, int yy);
#endif /* BREW_H_ */

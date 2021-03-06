/*
 * message.h
 *
 *  Created on: 26/11/2013
 *      Author: brad
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_

// Task Identifiers
#define TASK_BASE  100
#define BREW_TASK  100
#define BOIL_TASK  101
#define HLT_TASK   102
#define CRANE_TASK 103
#define BOIL_VALVE_TASK 104
#define CHILL_TASK 105 // Not a task, but for chiller purposes it is ok for now.
#define BREW_TASK_RESET 106
#define BREW_TASK_BRING_TO_BOIL 107
#define BREW_TASK_SPARGESETUP 108
#define BREW_TASK_CHECK_BOIL_VALVE 109
#define BREW_TASK_NO_REPLY 110



struct HLTMsg {
  const char * pcMsgText;
  uint32_t uState;
  double  dData1;
  double  dData2;
};


struct TextMsg {
  const char * pcMsgText;
  unsigned char ucLine;

};






#endif /* MESSAGE_H_ */

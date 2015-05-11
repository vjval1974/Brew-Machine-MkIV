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


struct GenericMessage
{
  unsigned char ucFromTask;
  unsigned char ucToTask;
  unsigned int uiStepNumber;
  void * pvMessageContent;
};

struct HLTMsg {
  const char * pcMsgText;
  uint32_t uState;
  double  uData1;
  double  uData2;
};


struct TextMsg {
  const char * pcMsgText;
  unsigned char ucLine;

};






#endif /* MESSAGE_H_ */

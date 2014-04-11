///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2007, Matthew Pratt, All Rights Reserved.
//
// Authors: Matthew Pratt
//
// Date:  8 Jun 2012
//
///////////////////////////////////////////////////////////////////////////////

#ifndef ADC_H
#define ADC_H
// ADC channels for the ST Dev board are 10 and 11... PC0 and PC1 respectively.

void adc_init (void);
unsigned short read_adc(int channel);

#endif

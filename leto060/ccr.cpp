#include <stdio.h>
#include "ccr.h"

uint8_t
CCR1::get()
{
	return ((FlagX & 0x80) ? CCR_X : 0)
	     | ((FlagS & 0x80) ? CCR_N : 0)
	     | (FlagNZ ? 0 : CCR_Z)
	     | ((FlagV & 0x80) ? CCR_V : 0)
	     | ((FlagC & 0x80) ? CCR_C : 0);
}

uint8_t
CCR5::get()
{
	return (FlagX() ? CCR_X : 0)
	     | (FlagN() ? CCR_N : 0)
	     | (FlagZ() ? CCR_Z : 0)
	     | (FlagV() ? CCR_V : 0)
	     | (FlagC() ? CCR_C : 0);
}

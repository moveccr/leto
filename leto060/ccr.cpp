#include <stdio.h>
#include "ccr.h"

void
CCR1::eval()
{
}

void
CCR1::print()
{
	printf("XNZVC=%c%c%c%c%c\n",
		FlagX ? '1' : '0',
		FlagS ? '1' : '0',
		FlagNZ ? '0' : '1',
		FlagV ? '1' : '0',
		FlagC ? '1' : '0');
}

void
CCR2::eval()
{
}

void
CCR2::print()
{
	printf("XNZVC=%c%c%c%c%c\n",
		CCR & 0x10 ? '1' : '0',
		CCR & 0x8 ? '1' : '0',
		CCR & 0x4 ? '1' : '0',
		CCR & 0x2 ? '1' : '0',
		CCR & 0x1 ? '1' : '0');
}

void
CCR3::eval()
{
}

void
CCR3::print()
{
	printf("XNZVC=%c%c%c%c%c\n",
		FlagX ? '1' : '0',
		FlagN ? '1' : '0',
		FlagZ ? '1' : '0',
		FlagV ? '1' : '0',
		FlagC ? '1' : '0');
}

void
CCR4::eval()
{
}

void
CCR4::print()
{
	printf("XNZVC=%c%c%c%c%c\n",
		FlagX & 0x1 ? '1' : '0',
		FlagNZ & 0x80000000 ? '1' : '0',
		!FlagNZ ? '1' : '0',
		FlagVC & 0x2 ? '1' : '0',
		FlagVC & 0x1 ? '1' : '0');
}

void
CCR5::eval()
{
}

void
CCR5::print()
{
	printf("XNZVC=%c%c%c%c%c\n",
		FlagX ? '1' : '0',
		FlagNZVC & 0x8000 ? '1' : '0',
		FlagNZVC & 0x4000 ? '1' : '0',
		FlagNZVC & 0x0001 ? '1' : '0',
		FlagNZVC & 0x0100 ? '1' : '0');
}

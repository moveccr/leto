#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include "stopwatch.h"
#include "ccr.h"

int
main()
{
	Stopwatch sw;

	volatile int N = 1000000000;

	sw.Start();
	for (int i = 0; i < N; i++) { }
	sw.Stop();
	printf("LOOP = %d\n", sw.msec());

	sw.Reset();

	{
		CCR1 ccr;
		sw.Start();
		for (int i = 0; i < N; i++) {
			// or.b エミュ
			uint8_t d = (uint8_t)i;
			ccr.FlagNZ = d;
			ccr.FlagS = d >> 7;
			ccr.FlagV = 0;
			ccr.FlagC = 0;
			ccr.eval();
		}
		sw.Stop();
		ccr.print();
	}
	printf("CCR1 = %d\n", sw.msec());

	sw.Reset();

	{
		CCR2 ccr;
		sw.Start();
		for (int i = 0; i < N; i++) {
			// or.b エミュ
			uint8_t d = (uint8_t)i;
			ccr.CCR =
				(ccr.CCR & 0x10)			// X は保存
				| ((d & 0x80) >> 4)			// N
				| (d ? 0 : 0x4)				// Z
			;								// V,C =0
			ccr.eval();
		}
		sw.Stop();
		ccr.print();
	}
	printf("CCR2 = %d\n", sw.msec());

	sw.Reset();

	{
		CCR3 ccr;
		sw.Start();
		for (int i = 0; i < N; i++) {
			// or.b エミュ
			uint8_t d = (uint8_t)i;
			ccr.FlagZ = (d == 0);
			ccr.FlagN = d & 0x80;
			ccr.FlagV = 0;
			ccr.FlagC = 0;
			ccr.eval();
		}
		sw.Stop();
		ccr.print();
	}
	printf("CCR3 = %d\n", sw.msec());

	sw.Reset();

	{
		CCR4 ccr;
		sw.Start();
		for (int i = 0; i < N; i++) {
			// or.b エミュ
			uint8_t d = (uint8_t)i;
			ccr.FlagNZ = d << 24;
			ccr.FlagVC = 0;
			ccr.eval();
		}
		sw.Stop();
		ccr.print();
	}
	printf("CCR4 = %d\n", sw.msec());

	return 0;
}


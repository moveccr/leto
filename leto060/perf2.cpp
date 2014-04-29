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

#if 1
	sw.Reset();
	{
		CCR1 ccr;
		sw.Start();
		for (int i = 0; i < N; i++) {
			eval();
		}
		sw.Stop();
	}
	printf("EVAL LOOP = %d\n", sw.msec());
#endif
	sw.Reset();

	{
		CCR5 ccr;
		sw.Start();
		for (int i = 0; i < N; i++) {
			// add.b エミュ
			uint8_t d = (uint8_t)i;
			uint8_t s = (uint8_t)10;
			__asm__(
				"add %3,%0\n\t"
				"lahf\n\t"
				"seto %%al\n\t"
				"mov %%ax,%1\n\t"
				"mov %%ax,%2\n\t"
				: "+r"(d), "=g"(ccr.FlagNZVC), "=g"(ccr.FlagX)
				: "r"(s)
				: "%eax"
			);
		}
		sw.Stop();
		ccr.print();
	}
	printf("CCR5 = %d\n", sw.msec());


	sw.Reset();

	{
		CCR1 ccr;
		sw.Start();
		for (int i = 0; i < N; i++) {
			// add.b エミュ
			uint8_t d = (uint8_t)i;
			uint8_t s = (uint8_t)10;
			uint32_t r;
			r = (uint32_t)d + s;
			ccr.FlagNZ = r;
			ccr.FlagS = r >> 7;
			ccr.FlagV = (s ^ r) & (d ^ r);
			ccr.FlagC = r >> 1;
			ccr.FlagX = ccr.FlagC;
			eval();
		}
		sw.Stop();
		ccr.print();
	}
	printf("CCR1 = %d\n", sw.msec());


	return 0;
}


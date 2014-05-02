//
// CCR のテスト
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include "ccr.h"
#include "stopwatch.h"

#define countof(x)	(sizeof(x) / sizeof(x[0]))

// パフォーマンス測定
int
perf()
{
	Stopwatch sw;
	volatile int N = 1000000000 / 2;
	volatile uint32_t src;
	volatile uint32_t dst;
	volatile uint64_t res;

	// 空ループにかかる時間を測定。
	// 最適化によってループが取り除かれてしまうのを防ぐため nop を実行。
	sw.Start();
	for (int i = 0; i < N; i++) {
		__asm__("nop");
	}
	sw.Stop();
	printf("nop only = %d\n", sw.msec());

	sw.Reset();
	{
		CCR1 ccr;
		sw.Start();
		__asm__("// CCR1 performance START");
		for (int i = -N; i < N; i++) {
			res = i;
			ccr.setMOVEL(res);
			__asm__("" : : "m"(ccr));
		}
		__asm__("// CCR1 performance END");
		sw.Stop();
	}
	printf("CCR1 = %d\n", sw.msec());

	sw.Reset();
	{
		CCR2 ccr;
		sw.Start();
		__asm__("// CCR2 performance START");
		for (int i = -N; i < N; i++) {
			res = i;
			ccr.setMOVEL(res);
			__asm__("" : : "m"(ccr));
		}
		__asm__("// CCR2 performance END");
		sw.Stop();
	}
	printf("CCR2 = %d\n", sw.msec());

	return 0;
}

int
main()
{
	perf();
	return 0;
}

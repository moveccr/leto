#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "stopwatch.h"
#include "ccr.h"

#define countof(x)	(sizeof(x) / sizeof(x[0]))

// CCR1
inline uint8_t
add1(CCR1& ccr, uint8_t d, uint8_t s)
{
	uint32_t r;
	r = (uint32_t)d + s;
	ccr.FlagNZ = r & 0xff;
	ccr.FlagS = r;
	ccr.FlagV = (s ^ r) & (d ^ r);
	ccr.FlagC = r >> 1;
	ccr.FlagX = ccr.FlagC;

	return r;
}

// CCR2
inline uint8_t
add2(CCR2& ccr, uint8_t d, uint8_t s)
{
/* ADD.B Dq,<ea>
 30908:    wb (a, z = (byte) ((x = rbs (a)) + (y = (byte) r[op >> 9 & 7])));
 30909:    ccr = ((z > 0 ? 0 : z < 0 ? CCR_N : CCR_Z) |
 30910:           ((x ^ z) & (y ^ z)) >>> 31 << 1 |
 30911:           ((x | y) ^ (x ^ y) & z) >> 31 & (CCR_X | CCR_C));  //ccr_add
*/
	// Java の byte は符号付き
	// >>> は算術シフト。とかそういう辺を気をつけて移植しないといけない

	typedef int8_t byte;
	byte x;
	byte y;
	byte z;

	z = (byte)((x = (byte)d) + (y = (byte)s));
	ccr.CCR = ((z > 0 ? 0 : z < 0 ? CCR_N : CCR_Z) |
		(((uint32_t)((x ^ z) & (y ^ z)) >> 31) << 1) |
		((((x | y) ^ (x ^ y) & z) >> 31) & (CCR_X | CCR_C)));  //ccr_add

	return z;
}

// CCR5
inline uint8_t
add5(CCR5& ccr, uint8_t d, uint8_t s)
{
	__asm__(
		"addb %3,%0\n\t"
		"lahf\n\t"
		"seto %%al\n\t"
		"mov %%ax,%1\n\t"
		"mov %%ax,%2\n\t"
		: "+r"(d), "=g"(ccr.FlagNZVC), "=g"(ccr.X)
		: "r"(s)
		: "%eax"
	);
	return d;
}

// 動作確認
int
regress()
{
	struct {
		uint8_t dst, src, exp_val, exp_ccr;
	} list[] = {
		{ 0x00, 0x00, 0x00, CCR_Z, },
		{ 0x00, 0x80, 0x80, CCR_N, },
		{ 0x7f, 0x01, 0x80, CCR_N | CCR_V },
		{ 0x80, 0x80, 0x00, CCR_X | CCR_Z | CCR_V | CCR_C, },
	};
	uint8_t r;
	int failed = 0;

	{
		CCR1 ccr;
		for (int i = 0; i < countof(list); i++) {
			r = add1(ccr, list[i].dst, list[i].src);

			if (r != list[i].exp_val) {
				printf("CCR1 test%d: expected result=$%x but $%x\n",
					i, list[i].exp_val, r);
				failed++;
			}
			if (ccr.get() != list[i].exp_ccr) {
				printf("CCR1 test%d: expected CCR=$%x but $%x\n",
					i, list[i].exp_ccr, ccr.get());
				failed++;
			}
		}
		if (failed > 0) {
			exit(1);
		}
		printf("CCR1 test ok\n");
	}

	{
		CCR2 ccr;
		for (int i = 0; i < countof(list); i++) {
			r = add2(ccr, list[i].dst, list[i].src);

			if (r != list[i].exp_val) {
				printf("CCR2 test%d: expected result=$%x but $%x\n",
					i, list[i].exp_val, r);
				failed++;
			}
			if (ccr.get() != list[i].exp_ccr) {
				printf("CCR2 test%d: expected CCR=$%x but $%x\n",
					i, list[i].exp_ccr, ccr.get());
				failed++;
			}
		}
		if (failed > 0) {
			exit(1);
		}
		printf("CCR2 test ok\n");
	}

	{
		CCR5 ccr;
		for (int i = 0; i < countof(list); i++) {
			r = add5(ccr, list[i].dst, list[i].src);

			if (r != list[i].exp_val) {
				printf("CCR5 test%d: expected result=$%x but $%x\n",
					i, list[i].exp_val, r);
				failed++;
			}
			if (ccr.get() != list[i].exp_ccr) {
				printf("CCR5 test%d: expected CCR=$%x but $%x\n",
					i, list[i].exp_ccr, ccr.get());
				failed++;
			}
		}
		if (failed > 0) {
			exit(1);
		}
		printf("CCR5 test ok\n");
	}

	return 0;
}

// パフォーマンス測定
int
perf()
{
	Stopwatch sw;
	volatile int N = 1000000000;
	volatile uint8_t r;

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
		for (int i = 0; i < N; i++) {
			r = add1(ccr, i, 10);
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
		for (int i = 0; i < N; i++) {
			r = add2(ccr, i, 10);
			__asm__("" : : "m"(ccr));
		}
		__asm__("// CCR2 performance END");
		sw.Stop();
	}
	printf("CCR2 = %d\n", sw.msec());

	sw.Reset();
	{
		CCR5 ccr;
		sw.Start();
		__asm__("// CCR5 performance START");
		for (int i = 0; i < N; i++) {
			r = add5(ccr, i, 10);
			__asm__("" : : "m"(ccr));
		}
		__asm__("// CCR5 performance END");
		sw.Stop();
	}
	printf("CCR5 = %d\n", sw.msec());

	return 0;
}

int
main()
{
	regress();
	perf();
	return 0;
}

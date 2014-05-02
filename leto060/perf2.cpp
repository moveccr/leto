//
// CCR のテスト

// CCRをどう実装したらいいかテストしたい。

// 1. mame みたいに独立したバイト変数として実装。
// 2. XEiJ みたいに複合したバイト変数として実装。
// 3. ビットフィールドを使う。
// 5. アセンブラでの演算結果のフラグを使用する。

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include "stopwatch.h"

#define countof(x)	(sizeof(x) / sizeof(x[0]))

#define CCR_X 0x0010
#define CCR_N 0x0008
#define CCR_Z 0x0004
#define CCR_V 0x0002
#define CCR_C 0x0001

class CCR1
{
	// これが MPU だと思いねえ。

 public:
	CCR1()
		: FlagNZ(0), FlagC(0), FlagV(0), FlagS(0), FlagX(0)
	{
	}

	uint8_t get()
	{
		return ((FlagX & 0x80) ? CCR_X : 0)
		     | ((FlagS & 0x80) ? CCR_N : 0)
		     | (FlagNZ ? 0 : CCR_Z)
		     | ((FlagV & 0x80) ? CCR_V : 0)
		     | ((FlagC & 0x80) ? CCR_C : 0);
	}

	uint32_t FlagNZ;	// 値が == 0 なら Z=1
	// 以下いずれも 0x80 のビットで判定する
	uint8_t FlagC;
	uint8_t FlagV;
	uint8_t FlagS;	// N フラグ。Z を NZ としているので S に変えとく。
	uint8_t FlagX;
};


class CCR2
{
	// これが MPU だと思いねえ。
 public:
	CCR2()
		: CCR(0)
	{
	}

	uint8_t get() { return CCR; }

	uint8_t CCR;	// XNZVC
};

class CCR3
{
	// これが MPU だと思いねえ。
 public:
	CCR3()
		: FlagZ(0), FlagC(0), FlagV(0), FlagN(0), FlagX(0)
	{
	}

	bool FlagZ: 1;
	bool FlagC: 1;
	bool FlagV: 1;
	bool FlagN: 1;
	bool FlagX: 1;
};

class CCR5
{
	// x64 の FLAG レジスタ
	static const int FLAG_CF = 0x0001;	// Carry
	static const int FLAG_ZF = 0x0040;	// Zero
	static const int FLAG_SF = 0x0080;	// Sign

 public:
	CCR5()
		: FlagNZVC(0), X(0)
	{
	}

	uint8_t get()
	{
		return (FlagX() ? CCR_X : 0)
		     | (FlagN() ? CCR_N : 0)
		     | (FlagZ() ? CCR_Z : 0)
		     | (FlagV() ? CCR_V : 0)
		     | (FlagC() ? CCR_C : 0);
	}

	// LAHF 命令でフラグは AH (AX の上位バイト) にロードされる。
	bool FlagN() { return (FlagNZVC & (FLAG_SF << 8)); }
	bool FlagZ() { return (FlagNZVC & (FLAG_ZF << 8)); }
	bool FlagC() { return (FlagNZVC & (FLAG_CF << 8)); }

	// Vフラグに相当する OV フラグは LAHF 命令では取り出せないので
	//  SETO 命令を使って AX のビット 0 を立てている。
	bool FlagV() { return (FlagNZVC & 0x0001); }

	// XフラグのビットはCフラグのそれと同じ位置にしておく
	bool FlagX() { return (X & (FLAG_CF << 8)); }

	uint16_t FlagNZVC;	// N,Z,V,C
	uint16_t X;
};



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
	// >> は算術シフト。
	// >>> は論理シフト。とかそういう辺を気をつけて移植しないといけない

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
		: "+r"(d), "=m"(ccr.FlagNZVC), "=m"(ccr.X)
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
			r = add1(ccr, r, i);
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
			r = add2(ccr, r, i);
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
			r = add5(ccr, r, i);
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

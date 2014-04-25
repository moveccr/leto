//
// CCR のテスト

// CCRをどう実装したらいいかテストしたい。

// 1. mame みたいに独立したバイト変数として実装。
// 2. XEiJ みたいに複合したバイト変数として実装。
// 3. ビットフィールドを使う。

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include "stopwatch.h"

class CCR1
{
	// これが MPU だと思いねえ。

 public:
	CCR1()
		: FlagC(0), FlagV(0), FlagNZ(0), FlagS(0), FlagX(0)
	{
	}

	void print();
	void eval();

	uint32_t FlagNZ;
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

	void print();
	void eval();
	uint8_t CCR;	// XNZVC
};

class CCR3
{
	// これが MPU だと思いねえ。
 public:
	CCR3()
		: FlagC(0), FlagV(0), FlagZ(0), FlagN(0), FlagX(0)
	{
	}

	void print();
	void eval();
	bool FlagZ: 1;
	bool FlagC: 1;
	bool FlagV: 1;
	bool FlagN: 1;
	bool FlagX: 1;
};

class CCR4
{
	// これが MPU だと思いねえ。

 public:
	CCR4()
		: FlagVC(0), FlagNZ(0), FlagX(0)
	{
	}

	void print();
	void eval();

	uint32_t FlagNZ;	// N + (NZ)
	uint8_t FlagVC;		// V + C
	uint8_t FlagX;
};


//
// CCR のテスト

// CCRをどう実装したらいいかテストしたい。

// 1. mame みたいに独立したバイト変数として実装。
// 2. XEiJ みたいに複合したバイト変数として実装。
// 3. ビットフィールドを使う。
// 5. アセンブラでの演算結果のフラグを使用する。

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

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

	uint8_t get();

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

	uint8_t get();

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


//
// CCR のテスト

// CCRをどう実装したらいいかテストしたい。

// 1. mame みたいに独立したバイト変数として実装。
// 2. XEiJ みたいに複合したバイト変数として実装。
// 3. mame 方式だが変数を bool で実装。
// 5. アセンブラでの演算結果のフラグを使用する。

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

// MC680x0 の CCR ビット位置
#define CCR_X 0x0010
#define CCR_N 0x0008
#define CCR_Z 0x0004
#define CCR_V 0x0002
#define CCR_C 0x0001

// CCR クラスが持っているインタフェース。
// ただし、呼び出しコストを小さくするために、一般的な C++ で
// 行われるような、継承による動的呼び出しは行わず、ダックタイピングとして
// コーディングすること。
class CCR_DESIGN
{
 public:

	// MC680x0 の CCR の値を返す。
	// 下位 5 ビットにのみ有効な値を入れて返すこと。
	uint8_t get();

	// CCR の全ビットを一度に設定する。
	void set(uint8_t value);

	// 各、フラグ 1 bit 分を返す。
	bool getX();
	bool getN();
	bool getZ();
	bool getV();
	bool getC();

	// 特定のフラグ変化形式は、これらの関数で設定すること。
	// ここに無い変化をする形式は命令側で実装し、set() で設定すること。
	// MOVE 系。move, 論理演算など。
	void setMOVEB(uint8_t res);
	void setMOVEW(uint16_t res);
	void setMOVEL(uint32_t res); 
	// ADD 系。
	// src, dst は命令の計算前のオペランド。
	void setADDB(uint8_t src, uint8_t dst, uint32_t res);
	void setADDW(uint16_t src, uint16_t dst, uint32_t res);
	void setADDL(uint32_t src, uint32_t dst, uint64_t res);
	// Sub 系。
	// src, dst は命令の計算前のオペランド。
	void setSUBB(uint8_t src, uint8_t dst, uint32_t res);
	void setSUBW(uint16_t src, uint16_t dst, uint32_t res);
	void setSUBL(uint32_t src, uint32_t dst, uint64_t res);
	// Cmp 系。
	// src, dst は命令の計算前のオペランド。
	void setCMPB(uint8_t src, uint8_t dst, uint32_t res);
	void setCMPW(uint16_t src, uint16_t dst, uint32_t res);
	void setCMPL(uint32_t src, uint32_t dst, uint64_t res);
	// シフト命令
	// asl, asr, lsl, lsr, roxl, roxr は、命令側で全フラグを評価すること。
	// ROL 系。rol, ror 
	// res は命令の計算後のオペランド。
	void setROLB(uint8_t res, bool flag_c);
	void setROLW(uint16_t res, bool flag_c);
	void setROLL(uint32_t res, bool flag_c);
	// Btst 系。btst, bset 等。
	// bit は取り出したビット。
	void setBTST(bool bit);

};


// 1. mame みたいに独立したバイト変数として実装。
class CCR1
{
 public:
	CCR1()
	{
		NotZ = C = V = N = X = 0;
	}

	// MC680x0 の CCR の値を返す。
	// 下位 5 ビットにのみ有効な値を入れて返すこと。
	uint8_t get() const
	{
		return ((X & 0x80) ? CCR_X : 0)
		     | ((N & 0x80) ? CCR_N : 0)
		     | (NotZ ? 0 : CCR_Z)
		     | ((V & 0x80) ? CCR_V : 0)
		     | ((C & 0x80) ? CCR_C : 0);
	}

	// CCR の全ビットを一度に設定する。
	void set(uint8_t value)
	{
		NotZ = (value & CCR_Z);
		C = (((value & CCR_C) / CCR_C) << 7);
		V = (((value & CCR_V) / CCR_V) << 7);
		N = (((value & CCR_N) / CCR_N) << 7);
		X = (((value & CCR_X) / CCR_X) << 7);
	}

	// 各、フラグ 1 bit 分を返す。
	bool getX() const
	{
		return (X & 0x80);
	}

	bool getN() const
	{
		return (N & 0x80);
	}

	bool getZ() const
	{
		return !NotZ;
	}

	bool getV() const
	{
		return (V & 0x80);
	}

	bool getC() const
	{
		return (C & 0x80);
	}

	// 特定のフラグ変化形式は、これらの関数で設定すること。
	// ここに無い変化をする形式は命令側で実装し、set() で設定すること。
	// MOVE 系。move, 論理演算など。
	void setMOVEB(uint8_t res)
	{
		// X はそのまま
		// V, C は 0
		// Z は res が 0 なら立てる、ので NotZ は !0 なら立てる
		// N は 最上位ビット
		V = C = 0;
		NotZ = res;
		N = res;
	}

	void setMOVEW(uint16_t res)
	{
		V = C = 0;
		NotZ = res;
		N = res >> 8;
	}

	void setMOVEL(uint32_t res)
	{
		V = C = 0;
		NotZ = res;
		N = res >> 24;
	} 

	// ADD 系。
	void setADDB(uint8_t src, uint8_t dst, uint32_t res)
	{
		N = res;
		NotZ = res & 0xff;
		V = ((src ^ res) & (dst ^ res));
		X = C = (res >> 1);
	}

	void setADDW(uint16_t src, uint16_t dst, uint32_t res)
	{
		N = res >> 8;
		NotZ = res & 0xffff;
		V = ((src ^ res) & (dst ^ res)) >> 8;
		X = C = (res >> 9);
	}

	void setADDL(uint32_t src, uint32_t dst, uint64_t res)
	{
		N = res >> 24;
		NotZ = res;
		V = ((src ^ res) & (dst ^ res)) >> 24;
		X = C = (res >> 25);
	}

	// Sub 系。
	void setSUBB(uint8_t src, uint8_t dst, uint32_t res)
	{
		N = res;
		NotZ = res & 0xff;
		V = ((src ^ dst) & (res ^ dst));
		X = C = (res >> 1);
	}

	void setSUBW(uint16_t src, uint16_t dst, uint32_t res)
	{
		N = res >> 8;
		NotZ = res & 0xffff;
		V = ((src ^ dst) & (res ^ dst)) >> 8;
		X = C = (res >> 9);
	}

	void setSUBL(uint32_t src, uint32_t dst, uint64_t res)
	{
		N = res >> 24;
		NotZ = res;
		V = ((src ^ dst) & (res ^ dst)) >> 24;
		X = C = (res >> 25);
	}

	// Cmp 系。
	// SUB と同じだが、X フラグが変化しない。
	void setCMPB(uint8_t src, uint8_t dst, uint32_t res)
	{
		N = res;
		NotZ = res & 0xff;
		V = ((src ^ dst) & (res ^ dst));
		C = (res >> 1);
	}

	void setCMPW(uint16_t src, uint16_t dst, uint32_t res)
	{
		N = res >> 8;
		NotZ = res & 0xffff;
		V = ((src ^ dst) & (res ^ dst)) >> 8;
		C = (res >> 9);
	}

	void setCMPL(uint32_t src, uint32_t dst, uint64_t res)
	{
		N = res >> 24;
		NotZ = res;
		V = ((src ^ dst) & (res ^ dst)) >> 24;
		C = (res >> 25);
	}

	// シフト命令
	// asl, asr, lsl, lsr, roxl, roxr は、命令側で全フラグを評価すること。
	// ROL 系。rol, ror 
	void setROLB(uint8_t res, bool flag_c)
	{
		N = res;
		NotZ = res;
		V = 0;
		C = flag_c;
	}

	void setROLW(uint16_t res, bool flag_c)
	{
		N = res >> 8;
		NotZ = res;
		V = 0;
		C = flag_c;
	}

	void setROLL(uint32_t res, bool flag_c)
	{
		N = res >> 24;
		NotZ = res;
		V = 0;
		C = flag_c;
	}

	// Btst 系。btst, bset 等。
	void setBTST(bool bit)
	{
		NotZ = value;
	}

 private:
	uint32_t NotZ;	// 値が == 0 なら Z=1
	// 以下いずれも 0x80 のビットで判定する
	uint8_t C;
	uint8_t V;
	uint8_t N;
	uint8_t X;
};

// 2. XEiJ みたいに複合したバイト変数として実装。
class CCR2
{
 public:
	CCR2()
	{
		ccr = 0;
	}

	// MC680x0 の CCR の値を返す。
	// 下位 5 ビットにのみ有効な値を入れて返すこと。
	uint8_t get() const
	{
		return ccr;
	}

	// CCR の全ビットを一度に設定する。
	void set(uint8_t value)
	{
		ccr = value;
	}

	// 各、フラグ 1 bit 分を返す。
	bool getX() const
	{
		return (ccr & CCR_X);
	}

	bool getN() const
	{
		return (ccr & CCR_N);
	}

	bool getZ() const
	{
		return (ccr & CCR_Z);
	}

	bool getV() const
	{
		return (ccr & CCR_V);
	}

	bool getC() const
	{
		return (ccr & CCR_C);
	}

	// 特定のフラグ変化形式は、これらの関数で設定すること。
	// ここに無い変化をする形式は命令側で実装し、set() で設定すること。
	// MOVE 系。move, 論理演算など。
	void setMOVEB(uint8_t res)
	{
		// X はそのまま
		// V, C は 0
		// Z は res が 0 なら立てる
		// N は 最上位ビット
		int8_t r = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) | (ccr & CCR_X);
	}

	void setMOVEW(uint16_t res)
	{
		int16_t r = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) | (ccr & CCR_X);
	}

	void setMOVEL(uint32_t res)
	{
		int32_t r = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) | (ccr & CCR_X);
	} 

	// ADD 系。
	void setADDB(uint8_t src, uint8_t dst, uint32_t res)
	{
		int8_t r = res;
		uint8_t ur = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) |
			((((src ^ ur) & (dst ^ ur)) >> 7) << 1) |
			((((int8_t)(res >> 1)) >> 7) & (CCR_C | CCR_X));
	}

	void setADDW(uint16_t src, uint16_t dst, uint32_t res)
	{
		int16_t r = res;
		uint16_t ur = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) |
			((((src ^ ur) & (dst ^ ur)) >> 15) << 1) |
			((((int8_t)(res >> 9)) >> 7) & (CCR_C | CCR_X));
	}

	void setADDL(uint32_t src, uint32_t dst, uint64_t res)
	{
		int32_t r = res;
		uint32_t ur = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) |
			((((src ^ ur) & (dst ^ ur)) >> 31) << 1) |
			((((int8_t)(res >> 25)) >> 7) & (CCR_C | CCR_X));
	}

	// Sub 系。
	void setSUBB(uint8_t src, uint8_t dst, uint32_t res)
	{
		int8_t r = res;
		uint8_t ur = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) |
			((((src ^ dst) & (ur ^ dst)) >> 7) << 1) |
			((((int8_t)(res >> 1)) >> 7) & (CCR_C | CCR_X));
	}

	void setSUBW(uint16_t src, uint16_t dst, uint32_t res)
	{
		int16_t r = res;
		uint16_t ur = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) |
			((((src ^ dst) & (ur ^ dst)) >> 15) << 1) |
			((((int8_t)(res >> 9)) >> 7) & (CCR_C | CCR_X));
	}

	void setSUBL(uint32_t src, uint32_t dst, uint64_t res)
	{
		int32_t r = res;
		uint32_t ur = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) |
			((((src ^ dst) & (ur ^ dst)) >> 31) << 1) |
			((((int8_t)(res >> 25)) >> 7) & (CCR_C | CCR_X));
	}

	// Cmp 系。
	// SUB と同じだが、X フラグが変化しない。
	void setCMPB(uint8_t src, uint8_t dst, uint32_t res)
	{
		int8_t r = res;
		uint8_t ur = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) |
			((((src ^ dst) & (ur ^ dst)) >> 7) << 1) |
			((((int8_t)(res >> 1)) >> 7) & CCR_C) |
			(ccr & CCR_X);
	}

	void setCMPW(uint16_t src, uint16_t dst, uint32_t res)
	{
		int16_t r = res;
		uint16_t ur = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) |
			((((src ^ dst) & (ur ^ dst)) >> 15) << 1) |
			((((int8_t)(res >> 9)) >> 7) & CCR_C) | 
			(ccr & CCR_X);
	}

	void setCMPL(uint32_t src, uint32_t dst, uint64_t res)
	{
		int32_t r = res;
		uint32_t ur = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) |
			((((src ^ dst) & (ur ^ dst)) >> 31) << 1) |
			((((int8_t)(res >> 25)) >> 7) & CCR_C) |
			(ccr & CCR_X);
	}

	// シフト命令
	// asl, asr, lsl, lsr, roxl, roxr は、命令側で全フラグを評価すること。
	// ROL 系。rol, ror 
	void setROLB(uint8_t res, bool flag_c)
	{
		int8_t r = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) |
			(flag_c * CCR_C) |
			(ccr & CCR_X);
	}

	void setROLW(uint16_t res, bool flag_c)
	{
		int16_t r = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) |
			(flag_c * CCR_C) |
			(ccr & CCR_X);
	}

	void setROLL(uint32_t res, bool flag_c)
	{
		int32_t r = res;
		ccr = (r > 0 ? 0 : r < 0 ? CCR_N : CCR_Z) |
			(flag_c * CCR_C) |
			(ccr & CCR_X);
	}

	// Btst 系。btst, bset 等。
	void setBTST(bool bit)
	{
		ccr = (ccr & ~CCR_Z) | (!bit * CCR_Z);
	}

 private:
	uint8_t ccr;
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


/*
class CCR4
{
 public:
	// オペレーション
	static const uint8_t CCR_OP_None   = 0x00;
	static const uint8_t CCR_OP_ADD_8  = 0x01;
	static const uint8_t CCR_OP_MOVE_8 = 0x02;
	// オペレーションを同定するためのマスク
	static const uint8_t CCR_OP_MASK = 0x0f;
	// X フラグが遅延評価されるとき、bit 7 が立つ。
	static const uint8_t CCR_OP_X = 0x80;
	// CV フラグが遅延評価されるとき、bit 6 が立つ。
	static const uint8_t CCR_OP_CV = 0x40;
	// CCR_OP_CV が立っている時有効で、
	// CV フラグが ともに 0 と評価する。(MOVE系)
	static const uint8_t CCR_OP_CV0 = 0x20;


// X, V, C が全て変化する命令
// add, sub,...
// -> オペランドを記憶してXCV遅延評価しよう
// ccr(NZ), ccr_OP, src, dst を書き換える。

// X が変化せず、V=C=0 に評価される命令。
// move, not, mul,...
//  A: オペランドは記憶せず、CVを 0 で評価するように遅延評価する。
//    ccr(NZ--), ccr_OPのビット
//  B: オペランドは記憶せず、CV を 0 で即時評価して、X を遅延評価
//     するように記録しておく。
//    ccr(NZ00), ccr_OPのビット

// X が変化せず、V,C が変化する命令
// ror,...
//  A: 命令前に全フラグ(X)を確定しておいて、命令を実行する。
//     ror 自身は遅延評価しても良い。
//  B: X だけ遅延評価するように記録しておいて、
//     CV は即時評価する。


	CCR4()
		: src(0), dst(0), ccr_OP(CCR_OP_None)
	{
	}

	uint32_t src, dst;
	// N, Z は即時評価
	// C, V, X は遅延評価
	// XNZVC
	uint8_t ccr;
	uint8_t ccr_OP;

	uint8_t get()
	{
		if (ccr_OP != CCR_OP_None) {
			CalcCCR();
		}
		return ccr;
	}

	bool FlagN()
	{ 
		// N は即時評価されている
		return ccr & CCR_N;
	}

	typedef int8_t byte;
	void CalcCCR()
	{
		byte x, y, z;

		// CV0 評価
		if (ccr_OP & CCR_OP_CV0) {
		} else {


		if (((ccr_OP & 0x80) == 0) && (ccrx_OP & 0x80)) {
			switch (ccrx_OP) {
			 case CCR_OP_ADD_8:
				z = (byte)((x = (byte)dstx) + (y = (byte)srcx));
				ccr = (ccr & ~CCR_X) | 
					((((x | y) ^ (x ^ y) & z) >> 31) & CCR_X);  //ccr_add
				break;
			 case CCR_OP_MOVE_8:
			 default:
				break;
			}
		}
		ccrx_OP = CCR_OP_None;

		switch (ccr_OP) {
		 case CCR_OP_ADD_8:
			z = (byte)((x = (byte)dst) + (y = (byte)src));
			ccr = ((z > 0 ? 0 : z < 0 ? CCR_N : CCR_Z) |
				(((uint32_t)((x ^ z) & (y ^ z)) >> 31) << 1) |
				((((x | y) ^ (x ^ y) & z) >> 31) & (CCR_X | CCR_C)));  //ccr_add
			break;
		 case CCR_OP_MOVE_8:
			z = (byte)dst;
			ccr = (z > 0 ? 0 : z < 0 ? CCR_N : CCR_Z) | (ccr & CCR_X);
			break;
		 default:
			break;
		}
		ccr_OP = CCR_OP_None;
	}

	void CalcCCRX()
	{
		byte x, y, z;
	}

	// Move 命令系
	void LazyCCR_Move()
	{
		ccr_OP = ccr_OP | CCR_OP_CV | CCR_OP_CV0;
	}

	// Add 命令系
	void LazyCCR(uint8_t OP, uint32_t s, uint32_t d)
	{
		ccr_OP = OP | CCR_OP_X | CCR_OP_CV & ~CCR_OP_CV0;
		src = s;
		dst = d;
	}
};
*/


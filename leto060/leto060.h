/*
 * leto060.h
 *
 * (C) 2014 Y.Sugahara
 */
#ifndef LETO060_H_
#define LETO060_H_

#include <stdint.h>

class Leto060FPR
{
};

class Leto060FPU
{
public:
	Leto060FPR FP[8];
	uint32_t FPCR;
	uint32_t FPSR;
	uint32_t FPIAR;
};

class Leto060ATC
{
};

class Leto060IATC
 : public Leto060ATC
{
};

class Leto060DATC
 : public Leto060ATC
{
	// 4way set associative cache

};

class Leto060CACHE
{
};

class Leto060ICACHE
 : public Leto060CACHE
{
};

class Leto060DCACHE
 : public Leto060CACHE
{
};

// バスアクセスのトークンのようなもの
class Leto060BusToken
{
 public:
	uint32_t laddr;		// 論理アドレス。IEU から MMU で使用。
	uint32_t paddr;		// 物理アドレス。キャッシュから外部バスで使用。
	int length;			// 転送バイト数
	int fc;				// tm
	uint32_t data;		// データ
	enum {				// バスエラー区別用(ログ出力用)
		PhysicalBusErr = 0,
		VirtualBusErr,
	} err;

 public:
	Leto060BusToken() {
		laddr = 0;
		paddr = 0;
		length = 0;
		fc = 0;
		data = 0;
		err = PhysicalBusErr;
	}
};
typedef class Leto060BusToken BUS;

// MMU
class Leto060MMU
{
 public:
	bool enable;
	uint32_t pgmask;

	bool translate_fetch(BUS& bus);
	bool translate_read(BUS& bus);
	bool translate_write(BUS& bus);
};

// 物理バスインタフェース
class Leto060BusIF
{
 public:
	bool (*read_byte)(BUS& bus);
	bool (*read_word)(BUS& bus);
	bool (*read_long)(BUS& bus);
	bool (*write_byte)(BUS& bus);
	bool (*write_word)(BUS& bus);
	bool (*write_long)(BUS& bus);
};

extern Leto060BusIF busif_060;

// Memory Unit + Bus Unit
// マニュアルの、ブロック図の右側の3ユニット。
class Leto060MU
{
public:
	// キャッシュコントロール (CACR)
	uint32_t CACR;

	uint32_t URP;
	uint32_t SRP;
	uint16_t TC;
	uint32_t DTT[2];
	uint32_t ITT[2];

	Leto060IATC IATC;
	Leto060ICACHE ICACHE;

	Leto060DATC DATC;
	Leto060DCACHE DCACHE;

	Leto060MMU mmu;

	bool icache_enable;
	bool dcache_enable;

	// メモリアクセス
	// IEU からの通常アクセスはこのルーチンを使用すること。
	// MMU が有効ならアドレス変換を行い、キャッシュが有効ならキャッシュを
	// 通過する。
	bool fetch_long(BUS& bus);
	bool mu_read(BUS& bus);
	bool mu_write(BUS& bus);

	// キャッシュアクセス。たぶん内部関数
	bool cache_fetch_long(BUS& bus);
	bool cache_read(BUS& bus);
	bool cache_write(BUS& bus);

	// 外部バスアクセス。ミスアライン処理も行う。
	// サイズが実行時決定であるか、ミスアラインを処理する必要がある場合は
	// こちらを使用。そうでなければ次のインタフェース関数を使用のこと。
	bool bus_read(BUS& bus);
	bool bus_write(BUS& bus);

	// 外部バスアクセスインタフェース。
	// サイズがコンパイル時確定で、アラインされている場合はこちらを使用できる。
	Leto060BusIF busif;
};

class Leto060
{
public:
	Leto060();
	virtual ~Leto060();

// ** 実レジスタ **
	// PC レジスタ
	uint32_t PC;
	uint32_t PPC;
			// (現在実行中の命令先頭PC)

	// D0-D7 A0-A7
	uint32_t R[16];
#define D(x)	R[x]
#define A(x)	R[(x) + 8]

	// スタックポインタのスタンバイ側
	// USP/SSP のうち現在アクティブなものは常に R[15] で参照できる。
	// アクティブでないほうを USP または SSP に格納する。
	uint32_t USP;
	uint32_t SSP;

	// CCR
	bool CCR_C;
	bool CCR_V;
	bool CCR_NZ;
	bool CCR_N;
	bool CCR_X;

	// FPU
	Leto060FPU FPU;

	// プロセッサコンフィギュレーションレジスタ
	uint32_t PCR;

	// SR
	uint8_t SR_I;
	bool SR_M;
	bool SR_S;
	bool SR_T;

	int prog_fc;
	int data_fc;

	// ベクタベースレジスタ
	uint32_t VBR;

	// ファンクションコードレジスタ
	uint32_t SFC;
	uint32_t DFC;

	// Instruction/Data Memory Unit
	Leto060MU MU;

	// バスコントロールレジスタ
	uint32_t BUSCR;

// ** 特殊レジスタへのアクセス **
	uint8_t get_CCR();
	void set_CCR(uint8_t v);
	uint16_t get_SR();
	void set_SR(uint16_t v);

// ** 内部デバッグ **
	void DEBUG_Dump();

	// リセット
	bool Reset();

	// 実行
	int Execute(int cycle);

// ここから IEU
 public:
	// フェッチ
	bool IEU_fetch_word(BUS& bus);
	bool IEU_fetch_long(BUS& bus);

 private:
	bool IEU_fill_ipipe();

	// ロングワードプリフェッチ用バッファ
	uint16_t prefetch_stage[1];
	int prefetch_count;
};

extern bool busif_init(const char *);

#endif	/* LETO060_H_ */

// o 物理バスの命令フェッチはロングワードしか現実にはないので、
//   それには、合わせる。
//   実際の MPU は 96 Bytes のバッファを持っているとデータシートには
//   あるけど、スーパースカラまで実装する気はないので、
//   1ロングワード分の命令プリフェッチバッファを実装して、
//   1ワードごとの命令フェッチを実現する。
// o 060 では PC 相対はデータATC,データキャッシュ,プログラム空間アクセス
//   なので、fetch_* は使ってはいけない。
// o CPUから見た外部バスアクセスルーチンはミスアラインドを処理しない。
//   呼び出し側がアラインドされたアクセスごとに分割してコールすること。
// o 現実の MPU が入出力で使用するバス信号線上のビット位置は
//   エミュレートせずに、常に下位に詰めた形でアクセスする。

//	実行ユニット
//	命令フェッチユニット
//		<論理アドレスの世界>
//	MMU
//		<物理アドレスの世界>
//	CACHE
//	物理バス
//
// MMU はテーブルサーチとかで、CACHE を飛び越えて直接物理バスへアクセス
// したりします。

// o 実行ユニットからは、命令はワードアライン、データは任意のアラインで
//   アクセス出来る。
// o MMU でのミスアラインドアクセスについての記述は見当たらないので、
//   好きにやっていいんだとすると、後述のキャッシュと揃えておくとか、
//   ページ境界をまたぐ(可能性のある)時だけ分割するなど、いくつか取りうる
//   手はある。
// o キャッシュは16バイト1ライン(ここではSETという用語か)を一つの単位とする
//   ため、ミスアラインドアクセスは最大でも2回の分割になる。例えば
//   +1 バイト目からのロングワードは +1から3バイトと、+3からの1バイト。
// o 物理バスアクセスは適切にアラインするため、
//   例えば +1 バイト目からのロングワードは3回に分割される。

// アドレスエラーについて
// アドレスエラーは実際の MPU ではおそらく命令フェッチごとに PC の bit0 が
// エラーを発生させるかどうかを決めているんじゃないかと思われるが、
// エミュレータでは、PC が任意の値に変更された時(ジャンプ、コール、リターン、
// トラップなど)にチェックすれば事足りる。
// 例外をどのタイミングで発行するかは別途検討。


#include <assert.h>
#include <stdio.h>
#include "leto060.h"

Leto060::Leto060()
{
}

Leto060::~Leto060()
{
}

// リセット例外
bool
Leto060::Reset()
{
	BUS bus;

	MU.mmu.enable = false;
	MU.icache_enable = false;
	MU.dcache_enable = false;

	// ここは物理アドレスと分かっている
	bus.paddr = 0;
	MU.busif.read_long(bus);
	R[15] = bus.data;

	bus.paddr = 4;
	MU.busif.read_long(bus);
	PC = bus.data;

	printf("PC=%08x SP=%08x\n", PC, R[15]);

	return true;
}

// 命令実行
int
Leto060::Execute(int cycle)
{
	BUS bus;
	uint16_t ir;

	PPC = PC;

	if (IEU_fetch_word(bus) == false) {
		// バスエラー
		// XXX 未実装
		return 0;
	}
	ir = bus.data;

	printf("PPC=%08x PC=%08x IR=%04x\n", PPC, PC, ir);
	return 0;
}

bool
Leto060::IEU_fill_ipipe()
{
	BUS bus;

	// 奇数ワード目なので、ここを含むロングワードをフェッチ
	bus.laddr = PC - 2;

	if (MU.fetch_long(bus) == false) {
		// バスエラーになったら PC を元に戻してバスエラー報告
		// XXX ↑要確認
		return false;
	}

	// XXX ここの bus はローカル変数なので PC を戻さなくていい

	// 後半ワードだけが必要
	prefetch_stage[0] = bus.data;
	prefetch_count = 1;
}

// IEU でのワードフェッチ。
// bus には何もセットしなくてよい。現在の PC、FC を使用する。
// 成功すれば取得したワードを bus.data に格納して true を返す。
// バスエラーなら、bus.err にコードを格納して false を返す。
bool
Leto060::IEU_fetch_word(BUS& bus)
{
	bus.laddr = PC;
	bus.fc = prog_fc;

printf("IEU_fetch_word: PC=%08x\n", bus.laddr);
	if ((bus.laddr & 2) == 0) {
		// 偶数ワード目なら新たにロングワードフェッチ
		if (MU.fetch_long(bus) == false) {
			return false;
		}

		// 後半ワードはキャッシュしておく
		prefetch_stage[0] = bus.data;
		prefetch_count = 1;
		bus.data >>= 16;
printf("IEU_fetch_word: data=%04x stage=%04x\n", bus.data, prefetch_stage[0]);
	} else {
		// 奇数ワード目なら
		if (prefetch_count == 0) {
			// パイプが未充填ならここで充填 */
			if (IEU_fill_ipipe() == false) {
				return false;
			}
		}

		// で、プリフェッチステージのワードを使用
		bus.data = prefetch_stage[0];
		prefetch_count = 0;
printf("IEU_fethc_word: data=%04x stage=xxxx\n", bus.data);
	}

	PC += 2;
	return true;
}

// IEU でのロングワードフェッチ。
// bus には何もセットしなくてよい。現在の PC、FC を使用する。
// 成功すれば取得したロングワードを bus.data に格納して true を返す。
// バスエラーなら、bus.err にコードを格納して false を返す。
bool
Leto060::IEU_fetch_long(BUS& bus)
{
	uint32_t data;

	bus.laddr = PC;
	bus.fc = prog_fc;

	if ((bus.laddr & 2) == 0) {
		// 偶数ワード目ならロングワードフェッチしてそのまま使う
		if (MU.fetch_long(bus) == false) {
			return false;
		}
	} else {
		// 奇数ワード目

		// 前半ワードはステージにある
		if (prefetch_count == 0) {
			if (IEU_fill_ipipe() == false) {
				return false;
			}
		}
		data = prefetch_stage[0] << 16;

		// 後半ワードを取得
		bus.laddr += 2;
		if (MU.mmu.enable) {
			if (MU.mmu.translate_fetch(bus) == false) {
				return false;
			}
		} else {
			bus.paddr = bus.laddr;
		}

		if (MU.cache_fetch_long(bus) == false) {
			return false;
		}

		data |= bus.data >> 16;
		prefetch_stage[0] = bus.data;
		prefetch_count = 1;
	}

	PC += 4;
	return true;
}

// IEU から使用するフェッチ。
// MMU とキャッシュはそれぞれ有効なら適切に通過する。
bool
Leto060MU::fetch_long(BUS& bus)
{
printf("MU::fetch_long laddr=%08x\n", bus.laddr);
	if (mmu.enable) {
		if (mmu.translate_fetch(bus) == false) {
			return false;
		}
	} else {
		bus.paddr = bus.laddr;
	}
printf("MU::fetch_long paddr=%08x\n", bus.paddr);

	if (icache_enable) {
		if (cache_fetch_long(bus) == false) {
			return false;
		}
	} else {
		if (busif.read_long(bus) == false) {
			return false;
		}
	}
printf("MU::fetch_long data=%08x\n", bus.data);
	return true;
}

// 命令キャッシュからのフェッチ
bool
Leto060MU::cache_fetch_long(BUS& bus)
{
	return false;
}

// IEU からの Byte/Word/Long リード。
//
// src のうち以下をセットして呼び出す。
//  src.laddr  .. 論理アドレス (MMUオフでも論理アドレスを使用)
//  src.length .. バイト数 (1〜4)
//  src.fc     .. FC(TM)
//
// 読み込みが成功すれば true を返し、src.data に読み込んだ値が返る。
// src.data の上位不要ビットはクリアされている。src.err は不定。
//
// 読み込みがバスエラーになれば src.err にエラー内容を格納して false を返す。
// src.data は不定。
//
bool
Leto060MU::mu_read(BUS& bus0)
{
	BUS bus1;
	int n;
	uint32_t end;

	n = 1;

	// MMU
	if (mmu.enable) {
		// MMU 有効

		// bus0 については必ず1回は行う
		if (mmu.translate_read(bus0) == false) {
			return false;
		}

		// ページ境界をまたいでいたら、バストークンを分割
		end = (bus0.laddr & mmu.pgmask) + bus0.length;
		if (end > mmu.pgmask) {
			bus1 = bus0;
			bus1.length = end - (mmu.pgmask + 1);
			bus0.length -= bus1.length;
			bus1.laddr += bus0.length;
			n++;

			if (mmu.translate_read(bus1) == false) {
				return false;
			}
		}
	} else {
		// MMU 無効
		bus0.paddr = bus0.laddr;
	}

	// データキャッシュ
	if (dcache_enable) {
		// データキャッシュ有効

		if (n == 1) {
			// キャッシュラインをまたいでいたら、バストークンを分割
			end = (bus0.paddr & 15) + bus0.length;
			if (end > 15) {
				bus1 = bus0;
				bus1.length = end - 16;
				bus0.length -= bus1.length;
				bus1.laddr += bus0.length;
				bus1.paddr += bus0.length;
				n++;
			}
		}

		// キャッシュ経由でリード
		if (cache_read(bus0) == false) {
			return false;
		}
		if (n == 1) {
			return true;
		}
		// トークンが2つなら、もう一回リードしてデータを合成
		if (cache_read(bus1) == false) {
			bus0.err = bus1.err;
			return false;
		}
		bus0.data <<= bus1.length * 8;
		bus0.data |= bus0.data;
		return true;
	} else {
		// データキャッシュ無効

		if (bus_read(bus0) == false) {
			return false;
		}
		if (n == 1) {
			return true;
		}
		// トークンが2つなら、もう一回リードしてデータを合成
		if (bus_read(bus1) == false) {
			bus0.err = bus1.err;
			return false;
		}
		bus0.data <<= bus1.length * 8;
		bus0.data |= bus0.data;
		return true;
	}
}

// 1回のキャッシュリードアクセス。
// bus は4バイト境界に収まるものを指定される前提。ミスアラインでも可。
// キャッシュミスすればここで充填。
bool
Leto060MU::cache_read(BUS& bus)
{
	return false;
}

// ミスアライン処理付きの物理バスリード。
// bus は4バイト境界に収まるものを指定される前提。
// アラインされてることが分かっているものは直接 bus_read_*() を呼ぶべし。
bool
Leto060MU::bus_read(BUS& bus)
{
	uint32_t data;

#define SIZE_BYTE	(1 << 2)
#define SIZE_WORD	(2 << 2)
#define SIZE_3BYTES	(3 << 2)
#define SIZE_LONG	(4 << 2)

	switch ((bus.paddr & 3) | (bus.length << 2)) {
	 case 0 | SIZE_BYTE:
	 case 1 | SIZE_BYTE:
	 case 2 | SIZE_BYTE:
	 case 3 | SIZE_BYTE:
		if (busif.read_byte(bus) == false) {
			return false;
		}
		break;

	 case 0 | SIZE_WORD:
	 case 2 | SIZE_WORD:
		if (busif.read_word(bus) == false) {
			return false;
		}
		break;

	 case 1 | SIZE_WORD:
		if (busif.read_byte(bus) == false) {
			return false;
		}
		data = bus.data;

		bus.laddr += 1;
		bus.paddr += 1;
		if (busif.read_byte(bus) == false) {
			return false;
		}
		bus.data |= data << 8;
		break;

	 case 0 | SIZE_3BYTES:
		if (busif.read_word(bus) == false) {
			return false;
		}
		data = bus.data;

		bus.laddr += 2;
		bus.paddr += 2;
		if (busif.read_byte(bus) == false) {
			return false;
		}
		bus.data |= data << 8;
		break;

	 case 1 | SIZE_3BYTES:
		if (busif.read_byte(bus) == false) {
			return false;
		}
		data = bus.data;

		bus.laddr += 1;
		bus.paddr += 1;
		if (busif.read_word(bus) == false) {
			return false;
		}
		bus.data |= data << 16;
		break;

	 case 0 | SIZE_LONG:
		if (busif.read_long(bus) == false) {
			return false;
		}
		break;

	 default:
		assert(false);
		break;
	}

	return true;
}

// IEU からの Byte/Word/Long ライト。
//
// src のうち以下をセットして呼び出す。
//  src.laddr  .. 論理アドレス (MMUオフでも論理アドレスを使用)
//  src.length .. バイト数 (1〜4)
//  src.data   .. データ (上位の不要バイトは無視する)
//  src.fc     .. FC(TM)
//
// 書き込みが成功すれば true を返し、この場合 src.err は不定。
//
// 書き込みがバスエラーになれば src.err にエラー内容を格納して false を返す。
bool
Leto060MU::mu_write(BUS& bus0)
{
	BUS bus1;
	int n;
	uint32_t end;

	n = 1;

	// MMU
	if (mmu.enable) {
		// MMU 有効

		// bus0 については必ず1回行う
		if (mmu.translate_write(bus0) == false) {
			return false;
		}

		// ページ境界をまたいでいたら、バストークンを分割
		end = (bus0.laddr & mmu.pgmask) + bus0.length;
		if (end > mmu.pgmask) {
			bus1 = bus0;
			bus1.length = end - (mmu.pgmask + 1);
			bus0.length -= bus1.length;
			bus1.laddr += bus0.length;
			bus0.data >>= bus1.length * 8;
			bus1.data &= (1 << bus1.length * 8) - 1;
			n++;

			if (mmu.translate_write(bus1) == false) {
				return false;
			}
		}
	} else {
		// MMU 無効
		bus0.paddr = bus0.laddr;
	}

	// データキャッシュ
	if (dcache_enable) {
		// データキャッシュ有効

		if (n == 1) {
			// キャッシュラインをまたいでいたら、バストークンを分割
			end = (bus0.paddr & 15) + bus0.length;
			if (end > 15) {
				bus1 = bus0;
				bus1.length = end - 16;
				bus0.length -= bus1.length;
				bus1.laddr += bus0.length;
				bus1.paddr += bus0.length;
				n++;
			}
		}

		// キャッシュにライト
		if (cache_write(bus0) == false) {
			return false;
		}
		if (n == 1) {
			return true;
		}
		// トークンが2つなら、もう一回ライト
		if (cache_write(bus1) == false) {
			bus0.err = bus1.err;
			return false;
		}
		return true;
	} else {
		// データキャッシュ無効

		if (bus_write(bus0) == false) {
			return false;
		}
		if (n == 1) {
			return true;
		}
		// トークンが2つなら、もう一回ライト
		if (bus_write(bus1) == false) {
			bus0.err = bus1.err;
			return false;
		}
		return true;
	}
}

bool
Leto060MU::cache_write(BUS& bus)
{
	return false;
}

bool
Leto060MU::bus_write(BUS& bus)
{
	uint32_t data;

	switch ((bus.paddr & 3) | (bus.length  << 2)) {
	 case 0 | SIZE_BYTE:
	 case 1 | SIZE_BYTE:
	 case 2 | SIZE_BYTE:
	 case 3 | SIZE_BYTE:
		if (busif.write_byte(bus) == false) {
			return false;
		}
		break;

	 case 0 | SIZE_WORD:
	 case 2 | SIZE_WORD:
		if (busif.write_word(bus) == false) {
			return false;
		}
		break;

	 case 1 | SIZE_WORD:
		data = bus.data;
		bus.data >>= 8;
		if (busif.write_byte(bus) == false) {
			return false;
		}

		bus.laddr += 1;
		bus.paddr += 1;
		bus.data = data;
		if (busif.write_byte(bus) == false) {
			return false;
		}
		break;

	 case 0 | SIZE_3BYTES:
		data = bus.data;
		bus.data >>= 8;
		if (busif.write_word(bus) == false) {
			return false;
		}

		bus.laddr += 2;
		bus.paddr += 2;
		bus.data = data;
		if (busif.write_byte(bus) == false) {
			return false;
		}
		break;

	 case 1 | SIZE_3BYTES:
		data = bus.data;
		bus.data >>= 16;
		if (busif.write_byte(bus) == false) {
			return false;
		}

		bus.laddr += 1;
		bus.paddr += 1;
		bus.data = data;
		if (busif.write_word(bus) == false) {
			return false;
		}
		break;

	 case 0 | SIZE_LONG:
		if (busif.write_long(bus) == false) {
			return false;
		}
		break;

	 default:
		assert(false);
		break;
	}

	return true;
}

bool
Leto060MMU::translate_fetch(BUS& bus)
{
	return false;
}

bool
Leto060MMU::translate_read(BUS& bus)
{
	return false;
}

bool
Leto060MMU::translate_write(BUS& bus)
{
	return false;
}

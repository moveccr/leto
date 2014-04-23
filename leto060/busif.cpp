#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include "leto060.h"

// RAM および ROM のようなメモリデバイスの内部でのメモリの持ち方考察。
//
// offset  : +00 +01 +02 +03 +04 +05 +06 +07
// m68k    :  00  01  02  03  04  05  06  07
// BUSIF_16:  01  00  03  02  05  04  07  06
// BUSIF_32:  03  02  01  00  07  06  05  04
//
// o BUSIF_16 はワードで拾った時にホストエンディアンになっている。
//   68000 エミュレーションで最高のパフォーマンスが出るんじゃないか。
// o BUSIF_32 はロングワードで拾った時にホストエンディアンになっている。
//   試しにやってみよう
// -> BUSIF_32 が優位に速い。
// 68030 以降では外部バスに出るフェッチやキャッシュの充填が必ずロング
// ワードで行われることになっており、ロングワード最適化したほうがよい。

#define BUSIF_32

#if defined(BUSIF_32)
const char *BUSIF_NAME = "BUSIF_32";
#else
const char *BUSIF_NAME = "BUSIF_16";
#endif

static bool bus_read_byte(BUS& bus);
static bool bus_read_word(BUS& bus);
static bool bus_read_long(BUS& bus);
static bool bus_write_byte(BUS& bus);
static bool bus_write_word(BUS& bus);
static bool bus_write_long(BUS& bus);

// とりあえず
Leto060BusIF busif_060 = {
	bus_read_byte,
	bus_read_word,
	bus_read_long,
	bus_write_byte,
	bus_write_word,
	bus_write_long,
};

uint32_t memsize;
uint8_t *memory;
uint8_t *iplrom1;

#if defined(BUSIF_32)
static inline uint32_t
betoh32(uint32_t a)
{
#if 1	/* HOST_IS_LE */
	return
	   ((a & 0xff) << 24)
	 | ((a & 0xff00) << 8)
	 | ((a & 0xff0000) >> 8)
	 | ((a & 0xff000000) >> 24);
#else
	return a;
#endif
}

static inline uint32_t
read_byte(uint8_t *ptr0, uint32_t byte_offset)
{
#if 1	/* HOST_IS_LE */
	// $0 -> $3  %00 -> %11
	// $1 -> $2  %01 -> %10
	// $2 -> $1  %10 -> %01
	// $3 -> $0  %11 -> %00
	return (uint32_t) (ptr0[byte_offset ^ 3]);
#else
	return (uint32_t) (ptr0[byte_offset]);
#endif
}

static inline uint32_t
read_word(uint8_t *ptr0, uint32_t byte_offset)
{
#if 1	/* HOST_IS_LE */
	// $0 -> $2  %00 -> %10
	// $2 -> $0  %10 -> %00
	return (uint32_t) *(uint16_t*)(&ptr0[byte_offset ^ 2]);
#else
	return (uint32_t) *(uint16_t*)(&ptr0[byte_offset]);
#endif
}

static inline uint32_t
read_long(uint8_t *ptr0, uint32_t byte_offset)
{
	return (uint32_t) *(uint32_t*)(&ptr0[byte_offset]);
}

static inline void
write_byte(uint8_t *ptr0, uint32_t byte_offset, uint8_t data)
{
#if 1
	ptr0[byte_offset ^ 3] = data;
#else
	ptr0[byte_offset] = data;
#endif
}

static inline void
write_word(uint8_t *ptr0, uint32_t byte_offset, uint16_t data)
{
#if 1	/* HOST_IS_LE */
	*(uint16_t*)(&ptr0[byte_offset ^ 2]) = data;
#else
	*(uint16_t*)(&ptr0[byte_offset]) = data;
#endif
}

static inline void
write_long(uint8_t *ptr0, uint32_t byte_offset, uint32_t data)
{
	*(uint32_t*)(&ptr0[byte_offset]) = data;
}
#endif	/* BUSIF_32 */


// とりあえず
bool
busif_init(const char *iplrom30_pathname)
{
	// メインメモリ
	memsize = 2 * 1024 * 1024;
	memory = (uint8_t *)malloc(memsize);
	if (memory == NULL) {
		warn("malloc failed");
		return false;
	}

	iplrom1 = (uint8_t *)malloc(128 * 1024);
	if (iplrom1 == NULL) {
		warn("malloc2 failed");
		return false;
	}

	// IPLROM30 をとりあえず
	if (iplrom30_pathname != NULL) {
		int fd = open(iplrom30_pathname, O_RDONLY);
		if (fd == -1) {
			warn("open failed");
			return false;
		}
		if (read(fd, iplrom1, 128 * 1024) < 128 * 1024) {
			warn("read failed");
			return false;
		}
	}

#if defined(BUSIF_32)
	// ROM ファイルが BIG ENDIAN なのでホストエンディアンに変換
	for (int i = 0; i < 128 * 1024; i += 4) {
		write_long(iplrom1, i, betoh32(read_long(iplrom1, i)));
	}
#else
	for (int i = 0; i < 128 * 1024; i += 2) {
		uint8_t t;
		t = iplrom1[i];
		iplrom1[i] = iplrom1[i + 1];
		iplrom1[i + 1] = t;
	}
#endif

	// リセットベクタ
#if defined(BUSIF_32)
	write_long(memory, 0, read_long(iplrom1, 0x10000));
	write_long(memory, 4, read_long(iplrom1, 0x10004));
#else
	*(uint32_t *)&memory[0] = *(uint32_t *)&iplrom1[0x10000];
	*(uint32_t *)&memory[4] = *(uint32_t *)&iplrom1[0x10004];
#endif

	return true;
}

// 物理バスからのバイトリード
// bus.paddr  .. 物理アドレス
// bus.fc     .. FC(TM)
// (bus.length は参照しない)
// 読み込みが成功すれば bus.data にデータを格納して true を返す。
// bus.data の上位不要バイトは 0。
// 読み込みがバスエラーになれば bus.err に PhysicalBusErr を格納して
// false を返す。

static bool
bus_read_byte(BUS& bus)
{
	if (bus.paddr < memsize) {
#if defined(BUSIF_32)
		bus.data = read_byte(memory, bus.paddr);
#else
		bus.data = memory[bus.paddr ^ 1];
#endif
	} else if ((bus.paddr & 0xffffff) >= 0xfe0000) {
#if defined(BUSIF_32)
		bus.data = read_byte(iplrom1, (bus.paddr & 0xffffff) - 0xfe0000);
#else
		bus.data = iplrom1[((bus.paddr & 0xffffff) - 0xfe0000) ^ 1];
#endif
	} else {
		return false;
	}
	return true;
}

static bool
bus_read_word(BUS& bus)
{
	if (bus.paddr < memsize) {
#if defined(BUSIF_32)
		bus.data = read_word(memory, bus.paddr);
#else
		bus.data = *(uint16_t *)&memory[bus.paddr];
#endif
	} else if ((bus.paddr & 0xffffff) >= 0xfe0000) {
#if defined(BUSIF_32)
		bus.data = read_word(iplrom1, (bus.paddr & 0xffffff) - 0xfe0000);
#else
		bus.data = *(uint16_t *)&iplrom1[(bus.paddr & 0xffffff) - 0xfe0000];
#endif
	} else {
		return false;
	}
#if defined(BUSIF_LOG)
	printf("bus_read_word %08x -> %04x\n", bus.paddr, bus.data);
#endif
	return true;
}

static bool
bus_read_long(BUS& bus)
{
	if (bus.paddr < memsize) {
#if defined(BUSIF_32)
		bus.data = read_long(memory, bus.paddr);
#else
		bus.data = *(uint16_t *)&memory[bus.paddr];
		bus.data <<= 16;
		bus.data |= *(uint16_t *)&memory[bus.paddr + 2];
#endif
	} else if ((bus.paddr & 0xffffff) >= 0xfe0000) {
#if defined(BUSIF_32)
		bus.data = read_long(iplrom1, (bus.paddr & 0xffffff) - 0xfe0000);
#else
		bus.data = *(uint16_t *)&iplrom1[(bus.paddr & 0xffffff) - 0xfe0000];
		bus.data <<= 16;
		bus.data |= *(uint16_t *)&iplrom1[((bus.paddr + 2) & 0xffffff) - 0xfe0000];
#endif
	} else {
		return false;
	}
#if defined(BUSIF_LOG)
	printf("bus_read_long %08x -> %08x\n", bus.paddr, bus.data);
#endif
	return true;
}

// 物理バスへのバイトライト
// bus.paddr  .. 物理アドレス
// bus.fc     .. FC(TM)
// bus.data   .. データ。上位の不要バイトはここでカットする?
// (bus.length は参照しない)
// 書き込みが成功すれば true を返す。
// 書き込みがバスエラーになれば bus.err に PhysicalBusErr を格納して
// false を返す。

static bool
bus_write_byte(BUS& bus)
{
	if (bus.paddr < memsize) {
#if defined(BUSIF_32)
		write_byte(memory, bus.paddr, bus.data);
#else
		memory[bus.paddr ^ 1] = bus.data & 0xff;
#endif
	} else {
		return false;
	}
	return true;
}

static bool
bus_write_word(BUS& bus)
{
	if (bus.paddr < memsize) {
#if defined(BUSIF_32)
		write_word(memory, bus.paddr, bus.data);
#else
		*(uint16_t *)&memory[bus.paddr] = bus.data & 0xffff;
#endif
	} else {
		return false;
	}
	return false;
}

static bool
bus_write_long(BUS& bus)
{
	if (bus.paddr < memsize) {
#if defined(BUSIF_32)
		write_long(memory, bus.paddr, bus.data);
#else
		*(uint16_t *)&memory[bus.paddr] = bus.data >> 16;
		*(uint16_t *)&memory[bus.paddr + 2] = bus.data & 0xffff;
#endif
	} else {
		return false;
	}
	return true;
}

#include <stdio.h>
#include <sys/time.h>
#include "leto060.h"
#include "stopwatch.h"

extern const char *BUSIF_NAME;

int
print_result(const char *msg, Stopwatch& sw)
{
	int msec;

	msec = sw.msec();
	printf("%s %4d msec\n", msg, msec);

	return msec;
}

int
main()
{
	Stopwatch sw;
	BUS bus;
	int i;
	int r;
	uint64_t N, NR;
	int score = 0;
	float realscore;

	printf("BUSIF = %s\n", BUSIF_NAME);
	busif_init(NULL);

	/* まずざっと時間を測定。さすがに1秒かからないと想定 */
	sw.Start();
	for (i = 0; i < 10000; i++) {
		busif_060.read_long(bus);
	}
	sw.Stop();
	/* 1秒程度になるようなループ回数 N を求める */
	NR = ((uint64_t)1000 * 1000 * 10000 / sw.usec());
	/* 10進 リスケーリング。計算結果が人間に分かりやすい。 */
	N = 1;
	while (NR > 10) { N *= 10; NR /= 10; }
	printf("N=%qd\n", N);

	sw.Reset();
	sw.Start();
	for (i = 0; i < N; i++) {
		busif_060.read_long(bus);
	}
	sw.Stop();
	r = print_result("long", sw);
	score += r * 76;

	sw.Reset();
	sw.Start();
	for (i = 0; i < N; i++) {
		busif_060.read_byte(bus);
	}
	sw.Stop();
	r = print_result("byte", sw);
	score += r * 12;

	sw.Reset();
	sw.Start();
	for (i = 0; i < N; i++) {
		busif_060.read_word(bus);
	}
	sw.Stop();
	r = print_result("word", sw);
	score += r * 12;

	/*
	 * score の単位は N * 100 * msec の BWL平準化アクセス時間。
	 * 一般化するため、1/100 msec 間に BWL平準化アクセス可能な回数に
	 * 変換する。
	 */
	realscore = (float)N / score;
	printf("score: %f\n", realscore);
	return 0;
}

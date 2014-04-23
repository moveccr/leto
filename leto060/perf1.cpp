#include <stdio.h>
#include <sys/time.h>
#include "leto060.h"

extern const char *BUSIF_NAME;

int
print_result(const char *msg, timeval& start, timeval& end)
{
	timeval tv;
	int msec;

	timersub(&end, &start, &tv);
	msec = (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
	printf("%s %4d msec\n", msg, msec);

	return msec;
}

int
main()
{
	timeval start, end, result;
	BUS bus;
	int i;
	int r;
	uint64_t N, NR;
	int score = 0;
	float realscore;

	printf("BUSIF = %s\n", BUSIF_NAME);
	busif_init(NULL);

	/* まずざっと時間を測定。さすがに1秒かからないと想定 */
	gettimeofday(&start, NULL);
	for (i = 0; i < 10000; i++) {
		busif_060.read_long(bus);
	}
	gettimeofday(&end, NULL);
	timersub(&end, &start, &result);
	/* 1秒程度になるようなループ回数 N を求める */
	NR = ((uint64_t)1000 * 1000 * 10000 / (uint64_t)result.tv_usec);
	/* 10進 リスケーリング。計算結果が人間に分かりやすい。 */
	N = 1;
	while (NR > 10) { N *= 10; NR /= 10; }
	printf("N=%qd\n", N);

	gettimeofday(&start, NULL);
	for (i = 0; i < N; i++) {
		busif_060.read_long(bus);
	}
	gettimeofday(&end, NULL);
	r = print_result("long", start, end);
	score += r * 76;

	gettimeofday(&start, NULL);
	for (i = 0; i < N; i++) {
		busif_060.read_byte(bus);
	}
	gettimeofday(&end, NULL);
	r = print_result("byte", start, end);
	score += r * 12;

	gettimeofday(&start, NULL);
	for (i = 0; i < N; i++) {
		busif_060.read_word(bus);
	}
	gettimeofday(&end, NULL);
	r = print_result("word", start, end);
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

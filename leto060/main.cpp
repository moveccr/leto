#include <stdio.h>
#include "leto060.h"

int
main(int ac, char *av[])
{
	Leto060 cpu;

	if (ac < 2) {
		printf("usage: %s <path_of_iplrom30.dat>\n", av[0]);
		return 1;
	}

	// とりあえず第一引数を IPLROM30.DAT のパスとする。
	busif_init(av[1]);
	cpu.MU.busif = busif_060;

	cpu.Reset();
	cpu.Execute(0/*dummy*/);

	return 0;
}


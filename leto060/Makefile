CFLAGS = -O2
LDFLAGS =

a.out:	leto060.o busif.o main.o
	${CXX} ${LDFLAGS} -o a.out $>

perf1:	perf1.o busif.o
	${CXX} ${LDFLAGS} -o $@ $>

perf2:	perf2.o ccr.o
	${CXX} ${LDFLAGS} -o $@ $>

clean:
	rm -rf *.o a.out perf1 perf2

perf2.o:	perf2.cpp ccr.h
ccr.o:	ccr.cpp ccr.h

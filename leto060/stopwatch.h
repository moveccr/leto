
#ifndef STOPWATCH_H_
#define STOPWATCH_H_

class Stopwatch
{
 public:
	Stopwatch()
	{
		ElapsedMicroSeconds = 0;
	}

	void Start()
	{
		gettimeofday(&start, NULL);
	}

	void Stop()
	{
		gettimeofday(&end, NULL);
		timeval tv;
		timersub(&end, &start, &tv);
		ElapsedMicroSeconds += (uint64_t)tv.tv_sec * 1000000UL
			 + (uint64_t)tv.tv_usec;
	}

	void Reset()
	{
		ElapsedMicroSeconds = 0;
	}

	uint64_t usec() const
	{
		return ElapsedMicroSeconds;
	}

	int msec() const
	{
		return (int)(ElapsedMicroSeconds / 1000UL);
	}

 private:
	uint64_t ElapsedMicroSeconds;
	timeval start;
	timeval end;

};

#endif


#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

class Timer
{
private:
	timeval startTime;

public:
	void start(){gettimeofday(&startTime, NULL);}

	double stop()
	{
		timeval e;
		long seconds, uSeconds;
		double duration;

		gettimeofday(&e, NULL);

		seconds = e.tv_sec - startTime.tv_sec;
		uSeconds = e.tv_usec - startTime.tv_usec;

		duration = seconds + uSeconds / 1000000.0;

		return duration;
	}

	static void print(double duration){printf("%5.6f seconds\n", duration);}
};

typedef Timer i;
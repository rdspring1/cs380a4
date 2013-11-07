#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#define TIMES 100
#define SIZE 10000

struct stuff
{
	int data[SIZE];
};

int main(int argc, char* argv[])
{
	struct timeval start_time;
	struct timeval end_time;
	gettimeofday(&start_time, NULL);

	for(unsigned n = 0; n < TIMES; ++n)
	{
		static struct stuff s;
		for(unsigned i = 0; i < SIZE; ++i)
		{
			s.data[i] = i;
		}
	}
	gettimeofday(&end_time, NULL);
	double seconds = difftime(end_time.tv_sec, start_time.tv_sec);
	printf("%f seconds\n", seconds);

	while(true)
		;
}

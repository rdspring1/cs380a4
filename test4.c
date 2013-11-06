#include <stdio.h>
#include <malloc.h>

#define SIZE 1000000

struct stuff
{
	int data[1000];
};

struct stuff s;


int main(int argc, char* argv[])
{
	//static struct stuff s;
	int* values = (int*) malloc(sizeof(int) * SIZE);
	for(unsigned i = 0; i < SIZE; ++i)
	{
		values[i] = i;
		printf("%d\n", values[i]);
	}

	while(true)
		;
}

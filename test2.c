#include <stdio.h>
#include <malloc.h>

#define SIZE 10

int main(int argc, char* argv[])
{
	int* values = (int*) malloc(sizeof(int) * SIZE);
	for(unsigned i = 0; i < SIZE; ++i)
	{
		values[i] = i;
		printf("%d\n", values[i]);
	}
}

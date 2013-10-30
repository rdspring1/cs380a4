#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
	execve(argv[1], NULL, NULL);
	perror("execve");
	exit(EXIT_FAILURE);
}

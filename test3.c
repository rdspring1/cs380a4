#include <stdio.h>
#include <elf.h>

unsigned int sys;

int main(int argc, char **argv, char **envp) {
	char** sp = argv;
	printf("argv\n");
	/* walk past all argv pointers */
	while (*sp++ != NULL)
	{
		if(*sp != NULL)
			printf("%s\n", *sp);
	}

	printf("envp\n");
	/* walk past all env pointers */
	while (*sp++ != NULL)
	{
		if(*sp != NULL)
			printf("%s\n", *sp);
	}

        /* and find ELF auxiliary vectors (if this was an ELF binary) */
	int i = 0;
        for (Elf64_auxv_t *auxv = (Elf64_auxv_t *) sp; auxv->a_type != AT_NULL; ++auxv) {
		printf("%d : ", ++i);
		switch (auxv->a_type)
		{
			case AT_NULL:
			{
				printf("AT_NULL\n");
				break;
			}
			case AT_IGNORE:
			{
				printf("AT_IGNORE\n");
				break;
			}
			case AT_EXECFD:
			{
				printf("AT_EXECFD : %lu\n", auxv->a_un.a_val);
				break;
			}
			case AT_PHDR:
			{
				printf("AT_PHDR : %p\n", (void*) auxv->a_un.a_val);
				break;
			}
			case AT_PHENT:
			{
				printf("AT_PHENT : %lu\n", auxv->a_un.a_val);
				break;
			}
			case AT_PHNUM:
			{
				printf("AT_PHNUM : %lu\n", auxv->a_un.a_val);
				break;
			}
			case AT_PAGESZ:
			{
				printf("AT_PAGESZ : %lu\n", auxv->a_un.a_val);
				break;
			}
			case AT_BASE:
			{
				printf("AT_BASE : %p\n", (void*) auxv->a_un.a_val);
				break;
			}
			case AT_FLAGS:
			{
				printf("AT_FLAGS : %lu\n", auxv->a_un.a_val);
				break;
			}
			case AT_ENTRY:
			{
				printf("AT_ENTRY : %p\n", (void*) auxv->a_un.a_val);
				break;
			}
			case AT_NOTELF:
			{
				printf("AT_NOTELF : %lu\n", auxv->a_un.a_val);
				break;
			}
			case AT_UID:
			{
				printf("AT_UID : %lu\n", auxv->a_un.a_val);
				break;
			}
			case AT_EUID:
			{
				printf("AT_EUID : %lu\n", auxv->a_un.a_val);
				break;
			}
			case AT_GID:
			{
				printf("AT_GID : %lu\n", auxv->a_un.a_val);
				break;
			}
			case AT_EGID:
			{
				printf("AT_EGID : %lu\n", auxv->a_un.a_val);
				break;
			}	
			case AT_CLKTCK:
			{
				printf("AT_CLKTCK : %lu\n", auxv->a_un.a_val);
				break;
			}	
			case AT_PLATFORM:
			{
				printf("AT_PLATFORM : %lu\n", auxv->a_un.a_val);
				break;
			}	
			case AT_HWCAP:
			{
				printf("AT_HWCAP : %lu\n", auxv->a_un.a_val);
				break;
			}
		}
        }
}

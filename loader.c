#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <elf.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>

void load_program (char* filename)
{
	Elf64_Ehdr      ehdr;
	char            *taddr   = NULL;
	void            *entry_addr   = NULL;
	char            *addr    = NULL;
	size_t size;

	{
		int fd = open(filename, O_RDONLY);
		if(fd == ERROR)
		{
			printf("open failed, errno: %s\n", strerror(errno));
			EXIT(EXIT_FAILURE);
		}

		{
			ssize_t r = read(fd, (void*) &ehdr, sizeof(Elf64_Ehdr));

			if(r == ERROR)
			{
				printf("read failed, errno: %s\n", strerror(errno));
				EXIT(EXIT_FAILURE);
			}
		}
		close(fd);
	}

	{
		struct stat sb;
		if (stat(filename, &sb) == -1) 
		{
			printf("stat failed, errno: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		size = sb.st_size;
	}

	// Setup memory for ELF Binary
	addr = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if(addr == MAP_FAILED)
	{
		printf("map failed, errno: %s\n", strerror(errno));
		EXIT(EXIT_FAILURE);
	}
	memset(addr,0x0,size);
	entry_addr = addr + ehdr.e_entry;
	{
		int fd = open(filename, O_RDONLY);
		if(fd == ERROR)
		{
			printf("open failed, errno: %s\n", strerror(errno));
			EXIT(EXIT_FAILURE);
		}

		off_t offset = lseek(fd, ehdr.e_phoff, SEEK_SET); 
		for(Elf64_Half i = 1; i <= ehdr.e_phnum; ++i)
		{
			Elf64_Phdr  phdr;
			ssize_t s = read(fd, (void*) &phdr, sizeof(phdr));
			offset = lseek(fd, offset + sizeof(phdr), SEEK_SET); 

			if(phdr.p_type != PT_LOAD || !phdr.p_filesz) 
			{
				continue;
			}

			// p_filesz can be smaller than p_memsz
			char buf[phdr.p_filesz];
			// Read data into buffer and then copy data from buffer to mmap
			{
				int readfd = open(filename, O_RDONLY);
				ssize_t r = read(readfd, buf, phdr.p_filesz);

				if(r == ERROR)
				{
					printf("read failed, errno: %s\n", strerror(errno));
					EXIT(EXIT_FAILURE);
				}
				close(readfd);
			}

			taddr = phdr.p_vaddr + addr;
			memmove(taddr,buf,phdr.p_filesz);

			if(phdr.p_flags & PF_R) 
			{
				mprotect((unsigned char *) taddr, phdr.p_memsz, PROT_READ);
			}

			if(phdr.p_flags & PF_W) 
			{
				mprotect((unsigned char *) taddr, phdr.p_memsz, PROT_WRITE);
			}

			if(phdr.p_flags & PF_X) 
			{
				mprotect((unsigned char *) taddr, phdr.p_memsz, PROT_EXEC);
			}
		}
		close(fd);
	}
	return entry_addr;
}

void setup_stack(char* filename, void* entry_addr)
{
	// Allocate Anonomous mmap for stack
	size_t size = NUM_STACK_PAGES *; 
	void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	// Add argc and argc to stack

	// Add envp to stack

	// Add ELF AUXV to stack
}

int main(int argc, char** argv, char** envp)
{
	// Static Pager
	void* entry_addr = load_program(argv[1]);

	// Setup Stack
	setup_stack(argv[1], entry_addr);

	// Clear Registers
	asm("movq $0, %rbx");
	asm("movq $0, %rcx");
	asm("movq $0, %rdx");
	asm("movq $0, %rsi");
	asm("movq $0, %rdi");
	asm("movq $0, %rbp");
	// Jump to test program
}

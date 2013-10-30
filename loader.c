#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <elf.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>
#include <iostream>

#define ERROR -1
#define NUM_STACK_PAGES 10

void* entry_addr = NULL;
void* phdr_addr = NULL;
size_t phnum;
size_t phent;

void load_program (char* filename)
{
	Elf64_Ehdr      ehdr;
	char            *taddr   = NULL;
	char            *addr    = NULL;
	size_t size;

	{
		int fd = open(filename, O_RDONLY);
		if(fd == ERROR)
		{
			printf("open failed, errno: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		{
			ssize_t r = read(fd, (void*) &ehdr, sizeof(Elf64_Ehdr));

			if(r == ERROR)
			{
				printf("read failed, errno: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
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
	addr = (char*) mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if(addr == MAP_FAILED)
	{
		printf("map failed, errno: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	memset(addr,0x0,size);
	entry_addr = addr + ehdr.e_entry;
	phdr_addr = addr;
	phnum = ehdr.e_phnum;
	phent = ehdr.e_phentsize;
	{
		int fd = open(filename, O_RDONLY);
		if(fd == ERROR)
		{
			printf("open failed, errno: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
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
					exit(EXIT_FAILURE);
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
}

void new_aux_ent(uint64_t* auxv_ptr, uint64_t val, uint64_t id)
{
	*(--auxv_ptr) = val;
	*(--auxv_ptr) = id;
}

void* setup_stack(char* filename, void* entry_addr, int argc, char** argv, char** envp)
{
	// Allocate Anonomous mmap for stack
	size_t size = NUM_STACK_PAGES * getpagesize(); 
	void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	char* stack_ptr = (char*) addr + size;

	// Add ELF AUXV to stack
	// AT_NULL
	stack_ptr -= sizeof(Elf64_auxv_t);
	memset(stack_ptr, 0, sizeof(Elf64_auxv_t));

	uint64_t* auxv_ptr = (uint64_t*) stack_ptr;
	new_aux_ent(auxv_ptr, getegid(), AT_EGID);
	new_aux_ent(auxv_ptr, getgid(), AT_GID);
	new_aux_ent(auxv_ptr, geteuid(), AT_EUID);
	new_aux_ent(auxv_ptr, getuid(), AT_UID);
	new_aux_ent(auxv_ptr, (uint64_t) entry_addr, AT_ENTRY);
	new_aux_ent(auxv_ptr, 0, AT_ENTRY);
	new_aux_ent(auxv_ptr, phnum, AT_PHNUM);
	new_aux_ent(auxv_ptr, phent, AT_PHENT);
	new_aux_ent(auxv_ptr, (uint64_t) phdr_addr, AT_PHDR);
	new_aux_ent(auxv_ptr, CLOCKS_PER_SEC, AT_CLKTCK);
	new_aux_ent(auxv_ptr, getpagesize(), AT_PAGESZ);

	// Add envp to stack
	char** char_ptr = (char**) auxv_ptr;
	{
		size_t count = 0;
		char** env;
		for(env = envp; *env != 0; ++env)
		{
			++count;
		}

		memset(--char_ptr, 0, sizeof(char**));
		for(size_t i = 0; i < count; ++i)
		{
			*(--char_ptr) = *(--env);
		}
	}
	// Add argc and argc to stack
	{
		char** arg = argv + sizeof(char**) * argc;
		memset(--char_ptr, 0, sizeof(char**));
		for(size_t i = 0; i < argc; ++i)
		{
			*(--char_ptr) = *(--arg);
		}
	}
	int* int_ptr = (int*) char_ptr;
	*(--int_ptr) = argc;
	return (void*) int_ptr;
}

int main(int argc, char** argv, char** envp)
{
	// Static Pager
	load_program(argv[1]);

	// Setup Stack
	void* stack_ptr = setup_stack(argv[1], entry_addr, argc, argv, envp);

	asm("movq %0, %%rsp\n\t" : "g" ((uint64_t) stack_ptr));

	// Clear Registers
	asm("movq $0, %rbx");
	asm("movq $0, %rcx");
	asm("movq $0, %rdx");
	asm("movq $0, %rsi");
	asm("movq $0, %rdi");
	asm("movq $0, %rbp");

	// Jump to test program
	asm("movq %0, %%rax\n\t" : "g" ((uint64_t) entry_addr));
	asm("jmp %rax\n\t");	
}

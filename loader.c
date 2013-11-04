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

using namespace std;

void* entry_addr = NULL;
void* phdr_addr = NULL;
size_t phnum;
size_t phent;

void load_program (char* filename)
{
	Elf64_Ehdr      ehdr;
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

	entry_addr = (void*) ehdr.e_entry;
	phnum = ehdr.e_phnum;
	phent = ehdr.e_phentsize;
	{
		bool first = true;
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

			if(phdr.p_type != PT_LOAD || !phdr.p_memsz) 
			{
				continue;
			}
	
			if(first)
			{
				phdr_addr = (char*) phdr.p_vaddr;
				first = false;
			}

			int prot = 0;
			if(phdr.p_flags & PF_R) 
			{
				prot |= PROT_READ;
			}

			if(phdr.p_flags & PF_W) 
			{
				prot |= PROT_WRITE;
			}

			if(phdr.p_flags & PF_X) 
			{
				prot |= PROT_EXEC;
			}

			// Setup memory for ELF Binary
			size_t page_align = phdr.p_offset % sysconf(_SC_PAGE_SIZE);
			size_t aligned_vaddr = phdr.p_vaddr - page_align;
			size_t aligned_offset = phdr.p_offset - page_align;

			char* addr = (char*) mmap((void*) aligned_vaddr, page_align + phdr.p_memsz, prot, MAP_PRIVATE | MAP_FIXED, fd, aligned_offset);
			if(addr == MAP_FAILED)
			{
				printf("map failed, errno: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}

			// Clear Page Aligned Offset
			memset(addr, 0x0, page_align);

			// If memsz is greater than filesz, clear remaining memory address
			if(phdr.p_memsz > phdr.p_filesz)
			{
				memset((void*) (addr + page_align + phdr.p_filesz), 0x0, phdr.p_memsz - phdr.p_filesz);
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
		size_t envc = 0;
		for(char** env = envp; *env != 0; ++env)
		{
			++envc;
		}
		memset(--char_ptr, 0, sizeof(char**));
		for(int i = envc - 1; i >= 0; --i)
		{
			*(--char_ptr) = envp[i];
		}
	}
	// Add argc and argc to stack
	{
		memset(--char_ptr, 0, sizeof(char**));
		for(int i = argc - 1; i > 0; --i)
		{
			*(--char_ptr) = argv[i];
		}
	}
	long* long_ptr = (long*) char_ptr;
	*(--long_ptr) = argc - 1;
	return (void*) long_ptr;
}

int main(int argc, char** argv, char** envp)
{
	// Static Pager
	load_program(argv[1]);

	// Setup Stack
	void* stack_ptr = setup_stack(argv[1], entry_addr, argc, argv, envp);

	// Clear Registers
	//asm("movq $0, %rbx");
	//asm("movq $0, %rcx");
	//asm("movq $0, %rdx");
	//asm("movq $0, %rsi");
	//asm("movq $0, %rdi");
	//asm("movq $0, %rbp");

	printf("stack_ptr: %p\n", stack_ptr);	
	printf("entry_ptr: %p\n", entry_addr);	

	// Jump to test program
	asm("movq %0, %%rsp\n\t" : "+r" ((uint64_t) stack_ptr));
	asm("movq %0, %%rax\n\t" : "+r" ((uint64_t) entry_addr));
	asm("jmp *%rax\n\t");	
}

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
#include <signal.h>
#include <math.h>

#define ERROR -1
#define NUM_STACK_PAGES 32
#define BSSPAGES 100
#define ADD 1

using namespace std;

char* filename;
Elf64_Ehdr ehdr;
void* entry_addr = NULL;
void* stack_ptr = NULL; 
void* phdr_addr = NULL;
size_t phnum;
size_t phent;
int elf_fd;

size_t bss_addr[BSSPAGES];
int addr_offset = -1;

static void segreturn(uint64_t addr)
{
	// Return to program after loading page
	//printf("Return from SIGSEGV: %p\n", (void*) addr);
	asm("movq %0, %%rsp\n\t" : "+r" ((uint64_t) stack_ptr));
	asm("movq %0, %%rax\n\t" : "+r" ((uint64_t) entry_addr));
	asm("movq $0, %rdx");
	asm("movq $0, %rbp");
	asm("jmp *%rax\n\t");	
}

static void handler(int sig, siginfo_t* si, void* unused)
{
	printf("SIGSEGV at address: %p\n", (void*) si->si_addr);
	// Load Anonymous MMAP for BSS
	size_t size_offset = (uint64_t) si->si_addr % sysconf(_SC_PAGE_SIZE);
	size_t aligned_vaddr = (uint64_t) si->si_addr - size_offset;
	char* addr = (char*) mmap((void*) aligned_vaddr, sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(addr == MAP_FAILED)
	{
		printf("map failed, errno: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	bss_addr[++addr_offset] = aligned_vaddr;
	size_t next_vaddr = aligned_vaddr + sysconf(_SC_PAGE_SIZE);


	for(unsigned n = 0; n < ADD; ++n)
	{
		size_t last_vaddr = 0;
		bool avail = true;
		if(next_vaddr == bss_addr[n])
		{
			avail = false;
		}
		last_vaddr = max(last_vaddr, bss_addr[n]);

		if(!avail)
		{
			next_vaddr = last_vaddr + sysconf(_SC_PAGE_SIZE);
		}
		char* addr1 = (char*) mmap((void*) next_vaddr, sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if(addr1 == MAP_FAILED)
		{
			printf("map failed, errno: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		bss_addr[++addr_offset] = next_vaddr;
		next_vaddr += sysconf(_SC_PAGE_SIZE);
	}
	segreturn((uint64_t) si->si_addr);
}

void print_elf_auxv(char** sp)
{
	/* walk past all argv pointers */
	while (*sp++ != NULL)
		;

	/* walk past all env pointers */
	while (*sp++ != NULL)
		;

	{
		Elf64_auxv_t *auxv = (Elf64_auxv_t *) sp;
		if(auxv->a_type == AT_NULL)
		{
			printf("AT_NULL %p\n", auxv);
		}
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
			read(fd, (void*) &phdr, sizeof(phdr));
			offset = lseek(fd, offset + sizeof(phdr), SEEK_SET); 

			if(phdr.p_type != PT_LOAD || !phdr.p_memsz) 
			{
				continue;
			}

			if(first)
			{
				phdr_addr = (char*) phdr.p_vaddr + ehdr.e_phoff;
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

			char* addr = (char*) mmap((void*) aligned_vaddr, page_align + phdr.p_filesz, prot, MAP_PRIVATE | MAP_FIXED, fd, aligned_offset);
			if(addr == MAP_FAILED)
			{
				printf("map failed, errno: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}

			// Clear Page Aligned Offset
			memset(addr, 0x0, page_align);
		}
		close(fd);
	}
}

void new_aux_ent(uint64_t*& auxv_ptr, uint64_t val, uint64_t id)
{
	*(--auxv_ptr) = val;
	*(--auxv_ptr) = id;
}

void* setup_stack(char* filename, void* entry_addr, int argc, char** argv, char** envp)
{
	// Allocate Anonomous mmap for stack
	size_t size = NUM_STACK_PAGES * getpagesize(); 
	void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	char* stack_ptr = (char*) addr + size;

	// Add ELF AUXV to stack
	// AT_NULL
	stack_ptr -= sizeof(Elf64_auxv_t);
	memset(stack_ptr, 0, sizeof(Elf64_auxv_t));

	uint64_t* auxv_ptr = (uint64_t*) stack_ptr;
	new_aux_ent(auxv_ptr, 0, AT_IGNORE);
	new_aux_ent(auxv_ptr, elf_fd, AT_EXECFD);
	new_aux_ent(auxv_ptr, 0, AT_NOTELF);
	new_aux_ent(auxv_ptr, getegid(), AT_EGID);
	new_aux_ent(auxv_ptr, getgid(), AT_GID);
	new_aux_ent(auxv_ptr, geteuid(), AT_EUID);
	new_aux_ent(auxv_ptr, getuid(), AT_UID);
	new_aux_ent(auxv_ptr, (uint64_t) entry_addr, AT_ENTRY);
	new_aux_ent(auxv_ptr, 0, AT_FLAGS);
	new_aux_ent(auxv_ptr, 0, AT_BASE);
	new_aux_ent(auxv_ptr, phnum, AT_PHNUM);
	new_aux_ent(auxv_ptr, phent, AT_PHENT);
	new_aux_ent(auxv_ptr, (uint64_t) phdr_addr, AT_PHDR);
	new_aux_ent(auxv_ptr, sysconf(_SC_PAGE_SIZE), AT_PAGESZ);

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
	// Add argv to stack
	{
		memset(--char_ptr, 0, sizeof(char**));
		for(int i = argc - 1; i > 0; --i)
		{
			*(--char_ptr) = argv[i];
		}
	}
	long* long_ptr = (long*) char_ptr;
	*(--long_ptr) = argc - 1;
	// Debug Purposes - Print ELF AUXV vector
	//print_elf_auxv((char**) long_ptr);
	return (void*) long_ptr;
}


int main(int argc, char** argv, char** envp)
{
	filename = argv[1];

	// Install SIGSEGV Handler
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO | SA_NODEFER;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = handler;
	if (sigaction(SIGSEGV, &sa, NULL) == ERROR)
	{
		printf("SIGACTION FAILURE\n");
		exit(EXIT_FAILURE);
	}

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


	elf_fd = open(argv[1], O_RDONLY);
	if(elf_fd == ERROR)
	{
		printf("open failed, errno: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// Static Pager
	load_program(argv[1]);

	// Setup Stack
	stack_ptr = setup_stack(argv[1], entry_addr, argc, argv, envp);

	//printf("stack_ptr: %p\n", stack_ptr);	
	//printf("entry_ptr: %p\n", entry_addr);	

	// Jump to test program
	asm("movq %0, %%rsp\n\t" : "+r" ((uint64_t) stack_ptr));
	asm("movq %0, %%rax\n\t" : "+r" ((uint64_t) entry_addr));
	asm("movq $0, %rdx");
	asm("movq $0, %rbp");
	asm("jmp *%rax\n\t");	
}

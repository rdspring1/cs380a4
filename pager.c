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


void printk(const char* msg)
{
	fputs(msg, stderr);
}

int is_image_valid(Elf64_Ehdr *hdr)
{
	return 1;
}

void* find_sym(const char* name, Elf64_Shdr* shdr, const char* strings, const char* src, char* dst)
{
	Elf64_Sym* syms = (Elf64_Sym*)(src + shdr->sh_offset);
	int i;
	for(i = 0; i < shdr->sh_size / sizeof(Elf64_Sym); i += 1) {
		if (strcmp(name, strings + syms[i].st_name) == 0) {
			return dst + syms[i].st_value;
		}
	}
	return NULL;
}

void *image_load (char *elf_start, unsigned int size)
{
	Elf64_Ehdr      *hdr     = NULL;
	Elf64_Phdr      *phdr    = NULL;
	Elf64_Shdr      *shdr    = NULL;
	Elf64_Sym       *syms    = NULL;
	char            *strings = NULL;
	char            *start   = NULL;
	char            *taddr   = NULL;
	void            *entry   = NULL;
	int i = 0;
	char *exec = NULL;

	hdr = (Elf64_Ehdr *) elf_start;

	if(!is_image_valid(hdr)) {
		printk("image_load:: invalid ELF image\n");
		return 0;
	}

	taddr = exec = (char*) mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

	if(!exec) {
		printk("image_load:: error allocating memory\n");
		return 0;
	}

	// Start with clean memory.
	memset(exec,0x0,size);

	phdr = (Elf64_Phdr *)(elf_start + hdr->e_phoff);

	for(i=0; i < hdr->e_phnum; ++i)
	{
		if(phdr[i].p_type != PT_LOAD) {
			continue;
		}
		if(phdr[i].p_filesz > phdr[i].p_memsz) {
			printk("image_load:: p_filesz > p_memsz\n");
			munmap(exec, size);
			return 0;
		}
		if(!phdr[i].p_filesz) {
			continue;
		}

		// p_filesz can be smaller than p_memsz,
		// the difference is zeroe'd out.
		start = elf_start + phdr[i].p_offset;
		//taddr = exec + phdr[i].p_vaddr;
		//memmove(taddr,start,phdr[i].p_filesz);

		if(!(phdr[i].p_flags & PF_W)) {
			// Read-only.
			mprotect((unsigned char *) taddr,
					phdr[i].p_memsz,
					PROT_READ);
		}

		if(phdr[i].p_flags & PF_X) {
			// Executable.
			mprotect((unsigned char *) taddr,
					phdr[i].p_memsz,
					PROT_EXEC);
		}

		for(unsigned n = 0; n < phdr[i].p_filesz; ++n)
		{
			*taddr = start[n];
			++taddr;
		}
	}

	shdr = (Elf64_Shdr *)(elf_start + hdr->e_shoff);

	for(i=0; i < hdr->e_shnum; ++i) {
		if (shdr[i].sh_type == SHT_DYNSYM) {
			syms = (Elf64_Sym*)(elf_start + shdr[i].sh_offset);
			strings = elf_start + shdr[shdr[i].sh_link].sh_offset;
			entry = find_sym("main", shdr + i, strings, elf_start, exec);
			break;
		}
	}

	return entry;

}/* image_load */

int main(int argc, char** argv, char** envp)
{
	FILE* elf = fopen(argv[1], "rb");

	fseek(elf, 0, SEEK_END);
	size_t size = ftell(elf);
	rewind(elf);

	char* buffer = (char*) malloc (sizeof(char) * size);

	size_t s = fread(buffer, 1, size, elf);
	return ((int (*)(int, char**, char**)) image_load(buffer, size))(argc,argv,envp);
}

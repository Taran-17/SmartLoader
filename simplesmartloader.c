#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <elf.h>
#include <stdint.h>

#define PAGE_SIZE 4096

int fd;
int page_fault_count = 0;
int page_allocation_count = 0;
size_t total_internal_fragmentation = 0;

Elf64_Ehdr *ehdr; // Support for 64-bit ELF
Elf64_Phdr *phdr;

void page_fault_handler(int signum, siginfo_t *info, void *context) {
    void *fault_addr = info->si_addr;
    uintptr_t fault_page = (uintptr_t)fault_addr & ~(PAGE_SIZE - 1);

    printf("Segmentation fault at address %p, aligning to page %p\n", fault_addr, (void *)fault_page);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        uintptr_t seg_start = phdr[i].p_vaddr & ~(PAGE_SIZE - 1);
        uintptr_t seg_end = (phdr[i].p_vaddr + phdr[i].p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        printf("Checking segment %d: start = %p, end = %p\n", i, (void *)seg_start, (void *)seg_end);

        if (phdr[i].p_type == PT_LOAD &&
            fault_page >= seg_start &&
            fault_page < seg_end) {

            printf("Mapping page at address %p for segment %d\n", (void *)fault_page, i);

            // Set permissions based on segment flags
            int prot = 0;
            if (phdr[i].p_flags & PF_R) prot |= PROT_READ;
            if (phdr[i].p_flags & PF_W) prot |= PROT_WRITE;
            if (phdr[i].p_flags & PF_X) prot |= PROT_EXEC;

            // Map the page with correct permissions
            void *segment = mmap((void *)fault_page, PAGE_SIZE, prot,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (segment == MAP_FAILED) {
                perror("Failed to mmap page");
                exit(EXIT_FAILURE);
            }

            // Calculate the offset within the segment
            size_t offset_in_segment = fault_page - phdr[i].p_vaddr;
            size_t bytes_to_copy = PAGE_SIZE;

            // Adjust bytes_to_copy to stay within segment file size
            if (offset_in_segment + PAGE_SIZE > phdr[i].p_filesz) {
                bytes_to_copy = phdr[i].p_filesz - offset_in_segment;
            }

            if (bytes_to_copy > 0) {
                memcpy(segment, (char *)ehdr + phdr[i].p_offset + offset_in_segment, bytes_to_copy);
            }

            // Zero out remaining memory if beyond file-backed segment size
            if (offset_in_segment + PAGE_SIZE > phdr[i].p_filesz) {
                memset((char *)segment + bytes_to_copy, 0, PAGE_SIZE - bytes_to_copy);
            }

            page_fault_count++;
            page_allocation_count++;
            total_internal_fragmentation += PAGE_SIZE - bytes_to_copy;

            return; // Successfully handled this page fault
        }
    }

    fprintf(stderr, "Unhandled segmentation fault at address %p\n", fault_addr);
    exit(EXIT_FAILURE);
}


void load_and_run_elf(char **exe) {
    fd = open(exe[1], O_RDONLY);
    if (fd < 0) {
        perror("Failed to open ELF file");
        exit(EXIT_FAILURE);
    }

    // Map ELF file into memory
    struct stat st;
    if (fstat(fd, &st) != 0) {
        perror("Failed to get file status");
        close(fd);
        exit(EXIT_FAILURE);
    }

    ehdr = (Elf64_Ehdr *)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ehdr == MAP_FAILED) {
        perror("Failed to mmap ELF file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Verify ELF Magic Number
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Not a valid ELF file\n");
        munmap(ehdr, st.st_size);
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Locate program headers
    phdr = (Elf64_Phdr *)((char *)ehdr + ehdr->e_phoff);

    // Set up signal handling for segmentation faults
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = page_fault_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, NULL) < 0) {
        perror("Failed to set signal handler");
        munmap(ehdr, st.st_size);
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Pre-map the segment containing the entry point
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD &&
            ehdr->e_entry >= phdr[i].p_vaddr &&
            ehdr->e_entry < phdr[i].p_vaddr + phdr[i].p_memsz) {

            int prot = 0;
            if (phdr[i].p_flags & PF_R) prot |= PROT_READ;
            if (phdr[i].p_flags & PF_W) prot |= PROT_WRITE;
            if (phdr[i].p_flags & PF_X) prot |= PROT_EXEC;

            // Align the start address for mapping
            uintptr_t aligned_vaddr = phdr[i].p_vaddr & ~(PAGE_SIZE - 1);
            size_t map_size = phdr[i].p_memsz + (phdr[i].p_vaddr - aligned_vaddr);

            void *mapped_segment = mmap((void *)aligned_vaddr, map_size, prot,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mapped_segment == MAP_FAILED) {
                perror("Failed to mmap entry point segment");
                munmap(ehdr, st.st_size);
                close(fd);
                exit(EXIT_FAILURE);
            }
            break;
        }
    }

    // Jump to the entry point
    int (*entrypoint)() = (int (*)())(intptr_t)ehdr->e_entry;
    int result = entrypoint();

    // Cleanup
    printf("User _start return value = %d\n", result);
    munmap(ehdr, st.st_size);
    close(fd);

    // Report statistics
    printf("Total page faults: %d\n", page_fault_count);
    printf("Total pages allocated: %d\n", page_allocation_count);
    printf("Internal fragmentation: %zu bytes\n", total_internal_fragmentation);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <elf_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    load_and_run_elf(argv);
    return 0;
}


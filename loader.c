#include "loader.h"
#include <signal.h>
Elf32_Ehdr *ehdr;
Elf32_Phdr *phdr;
int fd , i ,min_entrypoint;
Elf32_Addr entry_pt = 0 ;
void *virtual_mem = NULL;
int no_of_faults = 0, pages = 0;
size_t total_size = 0, fragmentation = 0, PAGE_SIZE = 4096; 

size_t roundup(size_t size) {
  return (size/PAGE_SIZE + 1) * PAGE_SIZE;
}

// Release memory and perform other cleanups
void free_space(){
    free(ehdr);
    free(phdr);
}

// Unmap virtual memory and close the file descriptor
void unmapping_virtual_memory(){
    if (virtual_mem != NULL) {
      if( munmap(virtual_mem, phdr[i].p_memsz)  == -1){
        printf("error doing munmap\n");
        exit(1);
      }
    }
    close(fd);
}

//Check if the ELF file can be opened for reading
void check_file_read(const char* exe){
  int fd = open(exe, O_RDONLY);
  if (fd < 0) {
    printf("Error opening ELF file\n");
    exit(1);
  }
}

// Check if offset seeking was successful
void check_offset( off_t new_position ){
  if ( new_position == -1 )
  {
    printf("Failed to seek offset\n");
    exit(1);
  }
}

// Load program headers into memory
void load_phdr( size_t size_of_phdr ){
  phdr = ( Elf32_Phdr* )malloc( size_of_phdr * ehdr->e_phnum); 
  
  if (phdr == NULL) {
        printf("Failed to allocate memory for program headers.\n");
        exit(1);
  }
  
  check_offset(lseek(fd, 0, SEEK_SET));
  check_offset( lseek(fd , ehdr -> e_phoff , SEEK_SET ) );
  
  if ( read( fd , phdr , size_of_phdr * ehdr -> e_phnum) !=  size_of_phdr * ehdr -> e_phnum)
  {
    printf("Failed to load program headers properly\n");
    exit(1);
  }
  return;
}

// Load ELF header into memory and perform necessary checks
void load_ehdr( size_t size_of_ehdr ){
  ehdr = ( Elf32_Ehdr* )malloc(size_of_ehdr);
  
  if (ehdr == NULL) {
        printf("Failed to allocate memory for ELF header.\n");
        exit(1);
  }

  check_offset( lseek(fd, 0, SEEK_SET) ); 
  if (read(fd, ehdr, size_of_ehdr) != size_of_ehdr)
  {
    printf("Failed to load ELF header properly\n");
    exit(1);
  }
  if (ehdr -> e_ident[EI_CLASS] != ELFCLASS32) {
    printf("Not a 32-bit ELF file\n");
    exit(1);
  }
  return;
}

void open_elf( char* exe ){
  fd = open(exe, O_RDONLY);
  
  if (fd < 0) {
    printf("Failed to open ELF file\n");
    exit(1);
  }
}

void add_fragmentation(size_t bytes_read){

  if (bytes_read < PAGE_SIZE) {
    fragmentation = fragmentation + (PAGE_SIZE - bytes_read);
  }
}

void SIGSEGV_handler(int signum, siginfo_t *sig, void *context) {//warning was being displayed if i removed context
  if (signum == SIGSEGV) {
    no_of_faults++;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if ((sig -> si_addr) >= (phdr[i].p_vaddr) && (sig->si_addr) < phdr[i].p_vaddr + phdr[i].p_memsz) {
          //printf("Fault address is : %p\n", sig->si_addr);

          // Attempt to allocate memory using mmap
          virtual_mem = mmap(sig->si_addr, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);

          if (virtual_mem == MAP_FAILED) {
            // mmap failed
            printf("mmap failed\n");
            exit(1);
          }
          check_offset(lseek(fd, 0, SEEK_SET) );
          check_offset(lseek(fd, phdr[i].p_offset, SEEK_SET));
          ssize_t bytes_read = read(fd, virtual_mem, PAGE_SIZE);

          //printf("size of phdr segment is %d\n", phdr[i].p_memsz);
          //printf("Number of bytes read: %d\n", bytes_read);

          if (bytes_read < 0) {
            printf("Less than 0 bytes read\n");
            exit(1);
          }
          add_fragmentation(bytes_read);
          pages++;
          break;
        }
      }
    }
  }


void setup_signal_handler() {
    struct sigaction sig;
    memset(&sig, 0, sizeof(sig));
    sig.sa_flags = SA_SIGINFO;
    sig.sa_sigaction = SIGSEGV_handler;
    if(sigaction(SIGSEGV, &sig, NULL) == -1){
      printf("Not able to setup signal handler properly\n");
    }
}

// Load and execute the ELF executable
void load_and_run_elf(char* exe) {
  open_elf(exe);

  size_t size_of_phdr = sizeof( Elf32_Phdr ) ,size_of_ehdr = sizeof( Elf32_Ehdr); // size of one program header and the ehdr

  load_ehdr( size_of_ehdr );
  load_phdr( size_of_phdr );
  entry_pt = ehdr -> e_entry;
  //typecasting the entry point of the start function
  int (*_start)() = (int(*)())entry_pt;
  int result = _start();

  printf("User _start return value = %d\n", result);
  printf("no of faults: %d\n", no_of_faults);
  printf("Number of pages used: %d\n", pages);
  printf("Total internal fragmentation is : %d\n", fragmentation);

}

int main(int argc, char** argv)
{
  if(argc != 2) {
    printf("Usage: %s <ELF Executable> \n",argv[0]);
    exit(1);
  }
  setup_signal_handler();
  check_file_read(argv[1]);

  load_and_run_elf(argv[1]);

  free_space();
  unmapping_virtual_memory();

  return 0;
}

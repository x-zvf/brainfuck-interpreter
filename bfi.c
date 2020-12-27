#include<stdio.h>
#include<stdint.h>
#include<sys/mman.h>
#include<string.h>
#include<stdlib.h>
#include<errno.h>

//this would be better, if determined dynamically.
//That is too much effort though.
#define PAGE_SIZE 4096
#define CELLS_PER_MEM PAGE_SIZE-sizeof(int)-sizeof(void *)-sizeof(void *)
#define STACK_SIZE PAGE_SIZE*8
struct mem { // this should be one memory page
    struct mem *next;
    struct mem *prev;
    int offset;
    uint8_t cells[CELLS_PER_MEM];
};
struct mem *cmem;
size_t *stack;
unsigned int stackoffs = 0;

uint8_t get_cell() {
    fflush(stdout);
    return cmem->cells[cmem->offset];
}

void inc_cell() {
    cmem->cells[cmem->offset]++;
}

void dec_cell() {
    cmem->cells[cmem->offset]--;
}
struct mem *alloc_mem() {
    struct mem *new = (struct mem*)
        mmap(NULL, sizeof(struct mem),
            PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if(new == MAP_FAILED){
        fprintf(stderr,"Failed to alloc memory:%s\n",strerror(errno));
        exit(1);
    }
    new->offset = 0;
    new->next = NULL;
    new->prev = NULL;
    memset(new->cells, 0, CELLS_PER_MEM);
    fflush(stderr);
    return new;
}

void inc_ptr() {
    cmem->offset++;
    if(cmem->offset >= CELLS_PER_MEM) {
        if(cmem->next == NULL) {
            cmem->next = alloc_mem();
            cmem->next->prev = cmem;
        }
        cmem = cmem->next;
    }
}
void dec_ptr() {
    cmem->offset--;
    if(cmem->offset < 0) {
        if(cmem->prev == NULL) {
            cmem->prev = alloc_mem();
            cmem->prev->next = cmem;
        }
        cmem = cmem->prev;
    }
}

void read_cell() {
    cmem->cells[cmem->offset] = getchar();
}
void print_cell() {
    putchar((char) cmem->cells[cmem->offset]);
}

int main(int argc, char **argv){
    if(argc != 2) {
        printf("Usage: ./bfi <file>\n");
        return 1;
    }
    FILE *program;
    fflush(stderr);
    if((program = fopen(argv[1],"r")) == NULL) {
        fprintf(stderr,"Failed to open file %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    cmem = alloc_mem();
    stack = mmap(NULL, STACK_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if(stack == MAP_FAILED){
        fprintf(stderr,"Failed to allocate stack memory: %s\n", strerror(errno));
        return 1;
    }

    char c;
    // if this is non-zero instructions are not executed.
    unsigned int noexec = 0;
    while((c = fgetc(program)) != EOF) {
        switch(c) {
            case '[':
                stack[stackoffs++] = ftell(program);
                if(!noexec && !get_cell()){
                    noexec = stackoffs;
                }

                if(stackoffs >= STACK_SIZE) {
                    exit(1);
                }
                break;
            case ']':
                if(noexec){
                    stackoffs--;
                    if(noexec == stackoffs+1) {
                        noexec = 0;
                    }
                    break;
                }
                if(get_cell()) {
                    fseek(program, stack[stackoffs-1], SEEK_SET);
                } else {
                    stackoffs--;
                }
                break;
        }
        if(noexec)
            continue;

        switch(c) {
            case '>':
                inc_ptr();
                break;
            case '<':
                dec_ptr();
                break;
            case '+':
                inc_cell();
                break;
            case '-':
                dec_cell();
                break;
            case '.':
                print_cell();
                break;
            case ',':
                read_cell();
                break;
        }
    }
    return 0;
}

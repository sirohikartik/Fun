#include<stdio.h>
#include<stdlib.h>
#include<time.h>
// My own malloc function to understand how it works under the hood!!

#define memory_size 4096

char memory[memory_size]; // this is our ram with a byte sized pages (fixed blocks) -> char per block represents the program allocated within it

char free_mem[memory_size]; // array pointing to free memory pages (simple boolean array that stores whether the idx is free or not (T or F)

void* mymalloc(int size) {
    int window = 0;
    int start_idx = -1;

    for(int j = 0; j < memory_size; j++) {

        if(free_mem[j] == 'T') {

            if(window == 0) start_idx = j;
            window++;

            if(window >= size) {
                for(int k = 0; k < size; k++) {
                    free_mem[start_idx + k] = 'F';
                }
                return (void*)&memory[start_idx];
            }

        } else {
            window = 0;
            start_idx = -1;
        }
    }

    return NULL; 
}
typedef struct {
    char pid;
    int size;
    int stack_size;
    int heap_size;
}Process;

void print_memory() {
    for(int i =0;i<memory_size;i++){
        printf("%c ",memory[i]);
    } 
}

void myfree(char pid) {
    for(int i = 0; i < memory_size; i++) {
        if(memory[i] == pid) {
            memory[i] = '0';
            free_mem[i] = 'T';
        }
    }
}
int main() {
   
    for(int i =0;i<memory_size;i++){
        memory[i] = '0';
        free_mem[i] = 'T';
    }


        // initializing process list
    Process processes[26]; // let's keep 26 procecesses so we can track it in the memory via capital symbols
    for(int i =0;i<26;i++) {
        processes[i].pid = i + 65;
        processes[i].size = i%10 + 20;
        processes[i].stack_size = (int)processes[i].size * 3 / 10;
        processes[i].heap_size = 0;
    }

srand(time(NULL));

for(int i = 0; i < 26; i++) {
    int j = rand() % 26;

    Process temp = processes[i];
    processes[i] = processes[j];
    processes[j] = temp;
}

    // give them to memory - dynamic allocation ( first fit)
    
        // each entry in memory array represents say 5 bytes of data so let's use that i.e. ram is of 20 KB ( 4 * 5 * 1024 ) => 20 KB

    print_memory();
    printf("\n");

    for(int i = 0;i<26;i++) {

        Process P = processes[i];
        
        int window = 0;
        int start_idx = -1;
        int wind = 0;
        for(int j =0;j<memory_size;j++) {
                    if(free_mem[j] == 'T') {
                window++;
                    if(wind==0)
                start_idx  = j;
                wind = 1;
                if(window >= P.size) {
                    for(int k = 0;k<P.size;k++){ 
                            memory[start_idx + k] = P.pid;
                            free_mem[start_idx + k] = 'F';
                    }
                    break;
            }
                    }
                if(free_mem[j] =='F') {
                    window = 0;
                    start_idx = -1;
                    wind = 0;
                }
    }
    } 
   myfree('A');
   myfree('E');
   print_memory(); 


}

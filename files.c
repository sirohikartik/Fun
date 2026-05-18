#include <stdio.h>
#include <dirent.h>
#include<string.h>
#include <stdlib.h> 
typedef struct {
   int capacity ;
   int size ;
   char* arr[100];

} list ;


typedef struct {
   char * parent;
   char * children[];
} Node;


int list_dir(const char* path){
    
    struct dirent *entry;
    DIR* dp;
    dp = opendir(path);
    if(dp==NULL) {
        printf("ERROR : File not accessed.");
        return 1;
    }
    while((entry = readdir(dp))!=NULL){
        puts(entry->d_name);
    }
    closedir(dp);
    return 0;
}

void print_list(list* lst) {
    for(int i =0;i<lst->size;i++){
        printf("%s ", lst->arr[i]);
    }
}

int search(const char* query) {
    DIR* self;
    self = opendir("./");
    
    list lst;
    lst.capacity = 100;
    lst.size = 0;
    if(self==NULL) {
        puts("ERROR : Some error occurred!");
        return 1;
    }
    struct dirent *entry;
    while((entry = readdir(self))!=NULL){
        if(lst.size<lst.capacity) {
        lst.arr[lst.size] =strdup(entry->d_name);
        lst.size++;
        }
        else{
        puts("LIST OVERFLOW");
        return 1;
        }
    }
    print_list(&lst);
    return 0;
}


int main(int args, char* argv[]) {
    search("whatever");
    return 0;
}

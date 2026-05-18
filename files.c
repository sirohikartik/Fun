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
    char* parent;
    char * name;
    list children;   
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
        printf("%s \n", lst->arr[i]);
    }
}

int get_children(char* base, Node* node, char* parent) {
    DIR* self;
    self = opendir(base);
    
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
    if(!parent) node->parent = "-";
    else node->parent = parent;
    node->name = base;
    node->children = lst;
    return 0;
}

Node* create_list(){
    char* base = "./";
    Node* head = (Node*)(malloc(sizeof(Node)));
    
    get_children(base,head,"\0");
    return head;

}


int is_a_dir(char * dir_name){
    DIR* dp;
    dp = opendir(dir_name);
    if(!dp) return 0;
    return 1;
}

int main(int args, char* argv[]) {

    Node* head = create_list();

    print_list(&(head->children));

    return 0;
}

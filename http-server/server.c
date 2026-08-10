#include<stdio.h>
#include <string.h>
#include <sys/_types/_socklen_t.h>
#include<unistd.h>
#include<sys/socket.h>
#include<stdlib.h>
#include <netinet/in.h>
#include <fcntl.h>

void getRoute(char * buffer, char* route ){
    int start = 0;
    while(buffer[start]!='/'){
            start++;
    }
    int j = 0;
    for(int i = start;buffer[i]!=' ' && buffer[i+1]!='H';i++)
    {
        route[j++] = buffer[i];    
    }
    route[j] = '\0';
}

int main(){
    int sockfd = socket(AF_INET, SOCK_STREAM,0);

    if(sockfd <0 ){
        perror("Socket Error");
        exit(1);
    }

    struct sockaddr_in addr;
    struct sockaddr_storage their_addr;

    socklen_t addr_size =  sizeof(their_addr);


    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    int bind_res = bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr));

     if (bind_res < 0) {
        perror("bind");
        close(sockfd);
        exit(1);
    }
    
    printf("Bind result is : %d \n",bind_res);

    int lis_res = listen(sockfd, 10); // that 10 is basically backlog i.e. the max connections allowed


    if(lis_res <0 ){

        perror("Listening Error");

        exit(1);
    }


    int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);

    char buffer[4096];

    while(1){
    ssize_t n = read(new_fd, buffer, sizeof(buffer) - 1);

    if (n > 0) {
        buffer[n] = '\0';

    printf("Browser sent:\n%s\n", buffer);

    char route[10];
    getRoute(buffer, route);

    if (strcmp(route, "/hello") == 0){ 

    int file_fd = open("index.html", O_RDONLY);

    char html[1000000];

    ssize_t html_size = read(file_fd, html, sizeof(html));

    close(file_fd);

    if (html_size < 0) {
    perror("read");
    exit(1);
}

   char header[1024];

    int header_size = snprintf(
    header,
    sizeof(header),
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: %ld\r\n"
    "Connection: close\r\n"
    "\r\n",
    html_size);

    send(new_fd, header, header_size, 0);
    send(new_fd, html, html_size, 0);

    }    
   }}

close(new_fd);
      return 0;
}

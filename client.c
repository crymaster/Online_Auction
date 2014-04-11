#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>
#include<netdb.h>
#include <fcntl.h>
#include <pthread.h>

void menu();
void option1();
void option2();

int sockfd; //Client socket file descriptor

int main(){
    int     retval;
    int     option;
    struct sockaddr_in address;
    //Create socket to connect to server
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd == 0){
        perror("socket() error\n");
    }
    //Server ip address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(8888);
    
    retval = connect(sockfd,(struct sockaddr*)&address,sizeof(address));
    //Connection error
    if(retval == -1){
        perror("connect() error!\n");
        close(sockfd);
        return -1;
    }
    do{
        menu();
        scanf("%d",&option);
        switch(option){
            case 1:
                option1();
                break;
            case 2:
                option2();
                break;
            case 3:
                close(sockfd);
                return 0;
            default:
                printf("Please enter a valid option\n");
        }
    } while(1);
    return 0;
}

void menu(){
    printf("1. Sign In\n");
    printf("2. Sign Up\n");
    printf("3. Exit\n");
    printf("Option: ");
}

void option1(){
}

void option2(){
}

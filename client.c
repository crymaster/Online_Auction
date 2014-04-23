#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<string.h>
#include<netdb.h>
#include <fcntl.h>
#include <pthread.h>

#define CMD_REGISTER 0
#define CMD_SIGNIN   1
#define CMD_JOIN     2
#define CMD_GETINFO  3
#define CMD_BID      4
#define CMD_SIGNOUT  5
#define STDIN        0

void menu();
int menu1_option1();
int menu1_option2();
int menu2_option1();
int menu2_option2();
int menu2_option3();

int sockfd; //socket of server file descriptor

typedef struct{
    char name[40];
    char password[50];
}User;

User user;

int main(){
    int     retval;
    int     option;
    int     menu_type=1;
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
        menu(menu_type);
        scanf("%d",&option);
        getchar();
        switch(option){
            case 1:
                if(menu_type==1){
                    if(menu1_option1())
                        menu_type = 2;
                } else {
                    menu2_option1();
                }
                break;
            case 2:
                if(menu_type==1){
                    if(menu1_option2())
                        menu_type = 2;
                } else {
                    menu2_option2();
                }
                break;
            case 3:
                if(menu_type==1){
                    close(sockfd);
                    return 0;
                } else {
                    if(menu2_option3())
                        menu_type = 1;
                }
                break;
            default:
                printf("Please enter a valid option\n");
        }
    } while(1);
    return 0;
}

void menu(int type){
    if(type==1){
        printf("1. Sign In\n");
        printf("2. Sign Up\n");
        printf("3. Exit\n");
        printf("Option: ");
    } else if(type == 2){
        printf("1. Get Goods info\n");
        printf("2. Join Auction\n");
        printf("3. Log Out\n");
        printf("Option: ");
    }
}

int menu1_option1(){
    int command;        //Variable to receive command from client
    User temp;
    printf("Username:");    scanf("%s",temp.name);
    printf("Password:");    scanf("%s",temp.password);
    command = CMD_SIGNIN;
    write(sockfd,&command,sizeof(int));
    write(sockfd,&temp,sizeof(User));
    read(sockfd,&command,sizeof(int));
    if(command != CMD_SIGNIN){
        printf("Incorrect username or password.\n");
        return 0;
    } else {
        printf("\nHello! %s\n",temp.name);
        strcpy(user.name,temp.name);
        strcpy(user.password,temp.password);
    }
    return 1;
}

int menu1_option2(){
    int command;        //Variable to receive command from client
    User temp;
    char password[50];
    while(1){
        printf("New Username:");    scanf("%s",temp.name);
        if(strcmp(temp.name,"")==0){
            printf("Username must not be empty\n");
        } else {
            break;
        }
    }
    while(1){
        printf("Password:");        scanf("%s",temp.password);
        printf("Password Confirmation:");   scanf("%s",password);
        if(strcmp(temp.password,password)!=0){
            printf("Password and confirmation do not match.\n");
        } else {
            break;
        }
    }
    command = CMD_REGISTER;
    write(sockfd,&command,sizeof(int));
    write(sockfd,&temp,sizeof(User));
    read(sockfd,&command,sizeof(int));
    if(command != CMD_REGISTER){
        printf("Username already existed.\n");
        return 0;
    } else {
        printf("Register successfully!\n");
        strcpy(user.name,temp.name);
        strcpy(user.password,temp.password);
    }
    return 1;
}

int menu2_option1(){
    char line[100];
    int command;
    command = CMD_GETINFO;
    write(sockfd,&command,sizeof(int));
    read(sockfd,line,sizeof(char)*100);
    printf("%s\n",line);
    return 1;
}

int menu2_option2(){
    int command;
    char choice;
    char line[100];
    int  retval;
    int  byte_count;
    fd_set allfds;
    fd_set readfds;
    command = CMD_JOIN;
    printf("You can't quit once you join the auction. Do you want to join?\n");
    printf("Join(1)/Cancel(any key)?:");    scanf("%c",&choice);
    if(choice == '1'){
        write(sockfd,&command,sizeof(int));
        printf("Joining\n");
        read(sockfd,&command,sizeof(int));
        read(sockfd,line,sizeof(line));
        printf("%s",line);
        FD_ZERO(&readfds);
        FD_ZERO(&allfds);
        FD_SET(sockfd,&allfds);
        FD_SET(STDIN,&allfds);
        printf("Bid: ");
        while(1){
            readfds = allfds;
            retval = select(FD_SETSIZE,&readfds,NULL,NULL,NULL);
            if(retval == -1){
                perror("select() error\n");
                return -1;
            }
            if(FD_ISSET(sockfd,&readfds)){
                ioctl(sockfd,FIONREAD,&byte_count);
                if(byte_count==0){
                    printf("Connection lost!\n");
                    exit(1);
                } else {
                    strcpy(line,"");
                    read(sockfd,line,sizeof(line));
                    printf("%s",line);
                    printf("Bid: ");
                }
            } else if(FD_ISSET(STDIN,&readfds)){
                getchar();
                command = CMD_BID;
                printf("1\n");
//                write(sockfd,&command,sizeof(int));
            }
            
        }
    }
    return 1;
}

int menu2_option3(){
    int command;
    command = CMD_SIGNOUT;
    printf("%s has logged out!\n",user.name);
    strcpy(user.name,"");
    strcpy(user.password,"");
    write(sockfd,&command,sizeof(int));
    return 1;
}

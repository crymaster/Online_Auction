#include<sys/socket.h>
#include<stdio.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/time.h>
#include<sys/ioctl.h>
#include<unistd.h>
#include<string.h>
#include<ctype.h>

#define CMD_REGISTER 0
#define CMD_SIGNIN   1
#define CMD_JOIN     2
#define CMD_GETINFO  3
#define CMD_BID      4

void enterGoods();

struct Goods_{
    char name[50];
    int  init_price;
    int  min_incr;
};

typedef struct{
    char name[40];
    char password[50];
}User;

typedef struct Goods_ Goods;

Goods goods;    //Goods to auction

int main(){
    int server_sockfd;  //Server socket descriptor
    int client_sockfd;  //Client socket descriptor
    int i,j;
    int command;        //Variable to receive command from client
    int byte_count;     //Number of bytes in the received queue
    int retval;     //return value
    int countdown = 60; //timeout countdown
    User user;
    fd_set  readfds;    //read file descriptor list
    fd_set  allfds;     ///all file descriptor list
    struct sockaddr_in server_address;  //Server address and port
    struct timeval timeout; //timeout value
    //Create server socket
    server_sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(server_sockfd == 0){
        perror("socket() error!\n");
        return 0;
    }
    //Create server address and port
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); //convert host to network format (big endian byte order)
    server_address.sin_port = htons(8888);
    //Bind address and port to socket
    if(bind(server_sockfd, (struct sockaddr*)&server_address,sizeof(server_address))==-1){
        perror("bind() error!\n");
        close(server_sockfd);
        return -1;
    }
    //Server will listen with maximum connection
    if(listen(server_sockfd,10)==-1){
        perror("listen() error!\n");
        close(server_sockfd);
        return -1;
    }
    //Enter detail of the goods that will be auctioned
    enterGoods();
    do{
        printf("\n*****Press Enter to begin the auction*****\n");
    }
    while(getchar()!='\n');
    printf("60s countdown to wait for connection\n");
    FD_ZERO(&readfds);
    FD_ZERO(&allfds);
    //Set server descriptor to descriptor list
    FD_SET(server_sockfd,&allfds);
    //Set timeout
    timeout.tv_sec = 1;
    timeout.tv_usec = 1000;
    while(1){
        //register all descriptor that might invoke activity
        readfds = allfds;
        //select() block calling process until there is activity on any of file descriptor
        retval = select(FD_SETSIZE,&readfds,NULL,NULL,&timeout);
        //An error occurs
        if(retval == -1){
            printf("select() error!\n");
            close(server_sockfd);
            return -1;
        }
        //Timeout period expired
        if(retval == 0){
            //Countdown
            if(countdown != 1 ){
                countdown--;
                printf("%d\n",countdown);
            }
            else{
                return 0;
            }   
            //Reset timeout value cause it turns 0 after timeout
            timeout.tv_sec = 1;
        }
        
        //When an activity occurs
        for(i=0; i<FD_SETSIZE; i++){
            //Try all possible file descriptor value if it was the one which invoke activity
            if(FD_ISSET(i,&readfds)){
                //If server fd has activity, there is a connection request from new client
                if(i == server_sockfd){
                    //Accept new client file descriptor and set to file descriptor list
                    client_sockfd = accept(server_sockfd, NULL, NULL);
                    FD_SET(client_sockfd,&allfds);
                    printf("Client %d connected\n",client_sockfd);
                } else {
                    //Get number of byte in the received queue for a connection
                    ioctl(i,FIONREAD,&byte_count);
                    //If received queue for this connection is empty then it signals closing connection
                    if(byte_count == 0){
                        close(i);
                        FD_CLR(i,&allfds);
                        printf("Client %d closed\n",i);
                        for (j = 0; j < FD_SETSIZE; j++){
                            //if (client[j] == i) {
                            //    client[j] = -1;
                            break;
                        }
                    } else {
                        //Read command variable from client
                        read(i,&command,sizeof(int));
                        read(i,&user,sizeof(User));
                        if(command == CMD_REGISTER){
                            command = CMD_REGISTER;
                            write(i,&command,sizeof(int));
                        }
                    }	
                }
            }
        }
    }
    close(server_sockfd);
    return 0;
}

void enterGoods(){
    int i;
    printf("Enter detail of the goods in this auction\n");
    printf("Name of the goods: ");      scanf("%s",goods.name);
    printf("Initial price: ");          scanf("%d",&goods.init_price);
    printf("Minimum increment:");       scanf("%d",&goods.min_incr);
    printf("%s  %d  %d\n",goods.name,goods.init_price,goods.min_incr);
    for(i=0; i<FD_SETSIZE; i++){
    //    clients[i] = -1;
    }   
}

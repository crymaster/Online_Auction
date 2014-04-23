#include<sys/socket.h>
#include<stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/time.h>
#include<sys/ioctl.h>
#include<unistd.h>
#include<string.h>
#include<ctype.h>

#define CMD_EXCEPTION -1
#define CMD_REGISTER   0
#define CMD_SIGNIN     1
#define CMD_JOIN       2
#define CMD_GETINFO    3
#define CMD_BID        4
#define CMD_SIGNOUT    5
#define STT_OFFLINE    0
#define STT_ONLINE     1
#define STT_BIDDING    2

struct Goods_{
    char name[50];
    int  init_price;
    int  min_incr;
};

typedef struct{
    char name[40];
    char password[50];
    int  status;        //0:offline, 1:online, 2:bidding
}User;

typedef struct Goods_ Goods;

void enterGoods();
int registerUser(User* user);
User* getUserByName(char *name);
char* getGoodsinfo();
void broadcast(char* str);
int authenticate(User *user);
char* itoa(int a);

Goods goods;    //Goods to auction
User *users;

int main(){
    int server_sockfd;  //Server socket descriptor
    int client_sockfd;  //Client socket descriptor
    int i;
    int command;        //Variable to receive command from client
    int byte_count;     //Number of bytes in the received queue
    int retval;         //return value
    int countdown = 60; //timeout countdown
    int user_online = 0;  //number of user online
    User *user;
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
    users = (User*)malloc(sizeof(User)*FD_SETSIZE);
    for(i = 0; i<FD_SETSIZE; i++){
        strcpy(users[i].name,"");
        strcpy(users[i].password,"");
        users[i].status = STT_OFFLINE;
    }
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
                        //If user is logged in, remove from array
                        if(strcmp(users[i].name,"")!=0){
                            strcpy(users[i].name,"");
                            strcpy(users[i].password,"");
                            users[i].status = STT_OFFLINE;
                            printf("Client: %d  Name:%s has logged out\n",i,users[i].name);                        
                            user_online--;
                        }
                    } else {
                        //Read command variable from client
                        read(i,&command,sizeof(int));
                        //Register New User Process
                        if(command == CMD_REGISTER){
                            user = (User*)malloc(sizeof(User));
                            read(i,user,sizeof(User));
                            if(registerUser(user)!=1){
                                command = CMD_EXCEPTION;
                            } else {
                                strcpy(users[i].name,user->name);
                                strcpy(users[i].password,user->password);
                                users[i].status = STT_ONLINE;
                                printf("Client: %d  Name:%s has logged in\n",i,users[i].name);
                                command = CMD_REGISTER;
                                user_online++;
                            }
                            write(i,&command,sizeof(int));
                        } else if(command == CMD_SIGNIN){
                            user = (User*)malloc(sizeof(User));
                            read(i,user,sizeof(User));
                            if(authenticate(user)!=1){
                                command = CMD_EXCEPTION;
                            } else {
                                strcpy(users[i].name,user->name);
                                strcpy(users[i].password,user->password);
                                users[i].status = STT_ONLINE;
                                command = CMD_SIGNIN;
                                printf("Client: %d  Name:%s has logged in\n",i,users[i].name);                                
                                user_online++;
                            }
                            write(i,&command,sizeof(int));
                        } else if(command == CMD_GETINFO){
                            write(i,getGoodsinfo(),sizeof(char)*100);
                        } else if(command == CMD_SIGNOUT){
                            strcpy(users[i].name,"");
                            strcpy(users[i].password,"");
                            users[i].status = STT_OFFLINE;
                            printf("Client: %d  Name:%s has logged out\n",i,users[i].name);
                            user_online--;
                        } else if(command == CMD_JOIN){
                            users[i].status = STT_BIDDING;
                            command = CMD_JOIN;
                            write(i,&command,sizeof(int));
                            write(i,getGoodsinfo(),sizeof(char)*100);
                            char line[100] = "A client has bid\n";
                            broadcast(line);
                        } else if(command == CMD_BID){
                            char line[100] = "A client has bid\n";
                            broadcast(line);
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

int registerUser(User* user){
    FILE *file;
    if(getUserByName(user->name) != NULL){
        return 0;
    }
    if((file = fopen("users.txt","a"))==NULL)  {
        printf("Error opening file!\n");
        return -1;
    }
    fprintf(file,"%s\t%s\n",user->name,user->password);
    fclose(file);
    return 1;
}

int authenticate(User *user){
    User* temp;
    temp = getUserByName(user->name);
    if(temp==NULL)  return 0;
    if(strcmp(user->password,temp->password)==0)    return 1;
    return 0;
}

void broadcast(char* str){
    int i;
    for(i = 0; i<FD_SETSIZE; i++){
        if(users[i].status == 2){
            write(i,str,sizeof(str));
        }
    }
}

User* getUserByName(char *name){
    FILE *file;
    User* user;
    if((file = fopen("users.txt","r"))==NULL)  {
        printf("Error opening file!\n");
        return NULL;
    }
    user = (User*)malloc(sizeof(User));
    while(fscanf(file,"%s %s",user->name,user->password)!= EOF){
        if(strcmp(user->name,name)==0){
            return user;
        }
    }
    fclose(file);
    return NULL;
}

char* getGoodsinfo(){
    char line[100];
    strcpy(line,"Goods: ");
    strcat(line,goods.name);
    strcat(line,"\nInitial Price: ");
    strcat(line,itoa(goods.init_price));
    strcat(line,"\nStep Size: ");
    strcat(line,itoa(goods.min_incr));
    strcat(line,"\n");
    return  strdup(line);
}

char* itoa(int a){
    char *str;
    char temp;
    int digit;
    int i;
    int n = 0;
    str = (char*)malloc(sizeof(char)*15);
    if(a == 0){
        str[0] = '0';
        str[1] = 0;
        return str;
    }
    while(a>0){
        digit = a%10;
        a /= 10;
        str[n] = digit+'0';
        n++;
    }
    str[n] = 0;
    for(i = 0; i<n/2 ; i++){
        temp = str[i];
        str[i] = str[n-1-i];
        str[n-1-i] = temp;
    }
    return str;
}

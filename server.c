#include<sys/socket.h>
#include<stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/time.h>
#include<time.h>
#include<sys/ioctl.h>
#include<unistd.h>
#include<string.h>
#include<ctype.h>

#define SIG_FAIL      -6
#define SIG_SUCCESS   -5
#define SIG_LOWBID    -4    //Bidding money lower than current price + min increment
#define SIG_NEM       -3    //Not enough money
#define SIG_ALI       -2    //Already logged in
#define SIG_EXCEPTION -1
#define CMD_REGISTER   0
#define CMD_SIGNIN     1
#define CMD_JOIN       2
#define CMD_HISTORY    3
#define CMD_BID        4
#define CMD_SIGNOUT    5
#define CMD_SIGNOUT    5
#define CMD_END        6
#define CMD_WINNER     7
#define CMD_DEPOSIT    8
#define STT_OFFLINE    0
#define STT_ONLINE     1
#define STT_BIDDING    2

typedef struct {
    char name[50];
    int  init_price;
    int  cur_price;
    int  min_incr;
}Goods;

typedef struct{
    char name[40];
    char password[50];
    int  balance;
    int  status;        //0:offline, 1:online, 2:bidding
}User;

typedef struct{
    int client;
    int bid_money;
}LogEntry;

void enterGoods();
int registerUser(User* user);
User* getUserByName(char *name);
char* getGoodsinfo();
char* getHistory(char* user_name);
void broadcast(char* str);
void broadcastEnd(int winner);
int authenticate(User *user);
int isLoggedIn(char* name);
int updateUser(User user);
int writeLog(char *name, char* goods, int price,char* datetime);
char* replace(char* str,char old, char new);
int deposit(User user,char* serial);

Goods goods;    //Goods to auction
User *users;
LogEntry logging[500];

int main(){
    int server_sockfd;  //Server socket descriptor
    int client_sockfd;  //Client socket descriptor
    int i;
    int command;        //Variable to receive command from client
    int byte_count;     //Number of bytes in the received queue
    int log_number = 0;     //Number of log entries
    int retval;         //return value
    int auction_state;     //Default = 0, after first person join = 1, 
                           //during 1st 20s of bid = 2,during 2nd 20s of bid = 3, during 3rd 20s of bid = 4
    int countdown = 60; //timeout countdown
    int user_online = 0;  //number of user online
    int user_bidding =  0; // number of bidding 
    int bid_money;      //money that client bid
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
        users[i].balance = 0;
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
    //Set auction state as begin
    auction_state = 0;
    //Set number of log entry
    log_number = 0;
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
            if(countdown != 0){
                char line[100];
                if(auction_state > 0 && (countdown%10 == 0 || countdown == 5)){
                    sprintf(line,"%d seconds left\n",countdown);
                    broadcast(line);
                }
                countdown--;
                printf("%d\n",countdown);
            }
            else{
                char line[100];
                //Set 2nd countdown
                if(auction_state == 2){
                    countdown = 20;
                    auction_state = 3;
                    sprintf(line,"%d USD 1st time\n",logging[log_number-1].bid_money);
                    broadcast(line);
                //Set 3rd countdown
                } else if(auction_state == 3){
                    countdown = 20;
                    auction_state = 4;
                    sprintf(line,"%d USD 2nd time\n",logging[log_number-1].bid_money);
                    broadcast(line);
                } else if(auction_state == 4){
                    auction_state = 0;
                    sprintf(line,"%s win %s with %d USD\n",users[logging[log_number-1].client].name, goods.name,logging[log_number-1].bid_money);
                    broadcast(line);
                    //Update user balance and log
                    User winner = users[logging[log_number-1].client];
                    winner.balance -= logging[log_number-1].bid_money;
                    updateUser(winner);
                    time_t now;
                    time(&now);
                    writeLog(winner.name,goods.name,goods.cur_price,replace(ctime(&now),' ','.'));
                    //Inform end of this auction
                    broadcastEnd(logging[log_number-1].client);
                    write(logging[log_number-1].client,&winner.balance,sizeof(int));
                } else {
                    return 0;
                }
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
                            printf("Name:%s has logged out\n",users[i].name);       
                            strcpy(users[i].name,"");
                            strcpy(users[i].password,"");
                            if(users[i].status == STT_BIDDING){
                                user_bidding--;
                                printf("Biding: %d users\n",user_bidding);
                            }
                            users[i].status = STT_OFFLINE;
                            user_online--;
                            printf("Online: %d users\n",user_online);
                        }
                    } else {
                        //Read command variable from client
                        read(i,&command,sizeof(int));
                        /****Register New User Process****/
                        if(command == CMD_REGISTER){
                            user = (User*)malloc(sizeof(User));
                            read(i,user,sizeof(User));
                            if(registerUser(user)!=1){
                                command = SIG_EXCEPTION;
                                free(user);
                                write(i,&command,sizeof(int));
                            } else {
                                strcpy(users[i].name,user->name);
                                strcpy(users[i].password,user->password);
                                users[i].balance = user->balance;
                                users[i].status = STT_ONLINE;
                                printf("Name:%s has logged in\n",users[i].name);
                                command = CMD_REGISTER;
                                user_online++;
                                printf("Online: %d users\n",user_online);
                                write(i,&command,sizeof(int));
                                write(i,&users[i].balance,sizeof(int));
                                write(i,getGoodsinfo(),sizeof(char)*100);
                            }
                         /***********Sign in*************/
                        } else if(command == CMD_SIGNIN){   
                            user = (User*)malloc(sizeof(User));
                            read(i,user,sizeof(User));
                            if(authenticate(user)!=1){
                                command = SIG_EXCEPTION;
                                free(user);
                                write(i,&command,sizeof(int));  //send command 
                            } else if(isLoggedIn(user->name)==1){
                                command = SIG_ALI;
                                free(user);
                                write(i,&command,sizeof(int));  //send command
                            } else {    
                                strcpy(users[i].name,user->name);
                                strcpy(users[i].password,user->password);
                                users[i].balance = user->balance;
                                users[i].status = STT_ONLINE;
                                command = CMD_SIGNIN;
                                printf("Name:%s has logged in\n",users[i].name);
                                user_online++;
                                printf("Online: %d users\n",user_online);
                                write(i,&command,sizeof(int));  //send command
                                write(i,&users[i].balance,sizeof(int)); //send balance
                                write(i,getGoodsinfo(),sizeof(char)*100);   //send good info
                            }
                        /***********Get History*********/    
                        } else if(command == CMD_HISTORY){
                            write(i,getHistory(users[i].name),sizeof(char)*300);
                        } else if(command == CMD_SIGNOUT){
                            printf("Name:%s has logged out\n",users[i].name);
                            strcpy(users[i].name,"");
                            strcpy(users[i].password,"");
                            users[i].status = STT_OFFLINE;                        
                            user_online--;
                            printf("Online : %d users\n",user_online);
                        /**********Join auction*******/    
                        } else if(command == CMD_JOIN){
                            //change user status to bidding and update number of bidding users
                            users[i].status = STT_BIDDING;
                            user_bidding++;
                            printf("%s joins the auction\n",users[i].name);
                            printf("Biding: %d users\n",user_bidding);
                            //Reset time after first user join auction
                            if(auction_state == 0 && user_bidding == 1){
                                auction_state = 1;
                                printf("Start 60s auction:\n");
                                countdown = 60;
                            }
                            char line[100];
                            sprintf(line,"%d seconds left\n",countdown);
                            command = CMD_JOIN;
                            write(i,&command,sizeof(int));
                            write(i,&goods.cur_price,sizeof(int));
                            write(i,getGoodsinfo(),sizeof(char)*100);
                            write(i,line,sizeof(char)*100);
                        /*************Bidding************/
                        } else if(command == CMD_BID){
                            read(i,&bid_money,sizeof(int));
                            if(bid_money > users[i].balance){
                                command = SIG_NEM;
                                write(i,&command,sizeof(int));
                            } else if(bid_money>= (goods.cur_price+goods.min_incr)){
                                char line[100];
                                //Set current price, logging and auction state
                                goods.cur_price = bid_money;
                                logging[log_number].client = i;
                                logging[log_number++].bid_money = bid_money;
                                auction_state = 2;
                                sprintf(line,"%s have bid %d USD for %s\n",users[i].name,bid_money,goods.name);
                                printf("%s",line);
                                command = CMD_BID;
                                write(i,&command,sizeof(int));
                                broadcast(line);
                                //Set 1st countdown
                                countdown = 21;
                            } else {
                                command = SIG_LOWBID;
                                write(i,&command,sizeof(int));
                            }
                        //Add money to balance by searching for serial
                        } else if(command == CMD_DEPOSIT){
                            char serial[100];
                            int money;
                            read(i,serial,sizeof(char)*100);
                            money = deposit(users[i],serial);
                            if(money>0){
                                command = SIG_SUCCESS;
                                users[i].balance += money;
                                write(i,&command,sizeof(int));
                                write(i,&money,sizeof(int));
                            } else {
                                command = SIG_FAIL;
                                write(i,&command,sizeof(int));
                            }
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
    printf("Enter detail of the goods in this auction\n");
    printf("Name of the goods: ");      gets(goods.name);
    printf("Initial price: ");          scanf("%d",&goods.init_price);
    goods.cur_price = goods.init_price;
    printf("Minimum increment:");       scanf("%d",&goods.min_incr);
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
    user->balance = 500000;
    fprintf(file,"%s\t%s\t%d\n",user->name,user->password,user->balance);
    fclose(file);
    return 1;
}

int authenticate(User *user){
    User* temp;
    temp = getUserByName(user->name);
    if(temp==NULL)  return 0;
    if(strcmp(user->password,temp->password)==0){
        user->balance = temp->balance;
        return 1;
    }
    return 0;
}

void broadcast(char* str){
    int i;
    for(i = 0; i<FD_SETSIZE; i++){
        if(users[i].status == 2){
            write(i,str,sizeof(char)*100);
        }
    }
}

void broadcastEnd(int winner){
    int i;
    int command = CMD_END;
    char line[100];
    sprintf(line,"%d",command);
    for(i = 0; i<FD_SETSIZE; i++){
        if(users[i].status == 2){
            if(i!=winner){
                write(i,line,sizeof(char)*100);
            }
            else{
                command = CMD_WINNER;
                sprintf(line,"%d",command);
                write(i,line,sizeof(char)*100);
                command = CMD_END;
                sprintf(line,"%d",command);
            }
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
    while(fscanf(file,"%s %s %d",user->name,user->password,&user->balance)!= EOF){
        if(strcmp(user->name,name)==0){
            return user;
        }
    }
    fclose(file);
    return NULL;
}

int updateUser(User user){
    FILE *file;
    User temp[100];
    int n = 0;
    int i;
    if((file = fopen("users.txt","r"))==NULL)  {
        printf("Error opening file!\n");
        return 0;
    }
    while(fscanf(file,"%s %s %d",temp[n].name,temp[n].password,&temp[n].balance)!= EOF){
        if(strcmp(user.name,temp[n].name)==0){
            temp[n].balance = user.balance;
        }
        n++;
    }
    fclose(file);
    if((file = fopen("users.txt","w"))==NULL)  {
        printf("Error opening file!\n");
        return 0;
    }
    for(i = 0; i<n; i++){
        fprintf(file,"%s\t%s\t%d\n",temp[i].name,temp[i].password,temp[i].balance);
    }
    fclose(file);
    return 1;
}

int writeLog(char *name, char* goods, int price,char* datetime){
    FILE *file;
    if((file = fopen("log.txt","a"))==NULL)  {
        printf("Error opening file!\n");
        return 0;
    }
    fprintf(file,"%s\t%s\t%d\t%s",name,goods,price,datetime);
    fclose(file);
    return 1;
}

int deposit(User user,char* serial){
    FILE *file;
    char str[100];
    int flag = 0;
    int money = 0;
    if((file = fopen("serial.txt","r"))==NULL)  {
        printf("Error opening file!\n");
        return 0;
    }
    while(fscanf(file,"%s %d",str,&money)!= EOF){
        if(strcmp(serial,str)==0){
            flag = 1;
            break;
        }
    }
    fclose(file);
    if(!flag){
        return 0;
    }
    user.balance += money;
    updateUser(user);
    return money;
}

int isLoggedIn(char* name){
    int i;
    for(i = 0; i<FD_SETSIZE; i++){
        if(strcmp(name,users[i].name)==0)
            return 1;
    }
    return 0;
}

char* getGoodsinfo(){
    char line[100];
    sprintf(line,"Goods: %s\nInitial Price: %d USD\nCurrent Price: %d USD\nStep Size: %d USD\n",goods.name,goods.init_price,goods.cur_price,goods.min_incr);
    return  strdup(line);
}

char* getHistory(char* user_name){
    char history[300];
    char line[100];
    char name[40];
    char goods_name[50];
    char price[10];
    char* datetime = (char*)malloc(sizeof(char)*30);
    FILE *file;
    if((file = fopen("log.txt","r"))==NULL)  {
        printf("Error opening file!\n");
        return NULL;
    }
    sprintf(history,"Goods                         \tPrice     \tDate&Time\n");
    while(fscanf(file,"%s %s %s %s",name,goods_name,price,datetime)!= EOF){
        if(strcmp(name,user_name)==0){
            datetime = replace(datetime,'.',' ');
            strcat(price," USD");
            sprintf(line,"%-30s\t%-10s\t%s\n",goods_name,price,datetime);
            strcat(history,line);
        }
    }
    fclose(file);
    return strdup(history);
}

char* replace(char* str,char old, char new){
    int n = strlen(str);
    int i;
    char* str1 = strdup(str);
    for(i=0; i<n; i++){
        if(str1[i]==old){
            str1[i] = new;
        }
    }
    return str1;
}

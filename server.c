#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define perro(x) {fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, x, strerror(errno));exit(1);}
#define MAX_CLIENTS	100

static unsigned int cli_count = 0;
static int uid = 10;
const char password[] = "toor";
char ban_list[200];
char names[200][200];
int counter = 0;
int fight = 0;
int online_clients = 0;


/* Client structure */
typedef struct {
	struct sockaddr_in addr;	/* Client remote address */
	int connfd;			/* Connection file descriptor */
	int uid;			/* Client unique identifier */
	char name[32];			/* Client name */
	char pm[32];
	char pmget[32];
	char history[MAX_CLIENTS][1024];
	int root;
} client_t;

client_t *clients[MAX_CLIENTS];

/* Checking if parametr is ID or Nickname */

int int_or_not(char *letter) {
int i = 0;
    while ((letter[i] != '\n') && (letter[i] != '\0')) {
        if ((letter[i] == '0') || (letter[i] == '1') || (letter[i] == '2') || (letter[i] == '3') || (letter[i] == '4') || (letter[i] == '5') || (letter[i] == '6') || (letter[i] == '7') || (letter[i] == '8') || (letter[i] == '9'))
            i++;
        else
            return 0;
    }
    return 1;
}

/* Add client to queue */
void queue_add(client_t *cl){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(!clients[i]){
			clients[i] = cl;
			return;
		}
	}
}

/* Delete client from queue */
void queue_delete(int uid){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				return;
			}
		}
	}
}

/* Send message to all clients but the sender */
void send_message(char *s, int uid){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid != uid){
				if(write(clients[i]->connfd, s, strlen(s))<0){
					perror("write");
					exit(-1);
				}
			}
		}
	}
}

/* Send message to all clients */
void send_message_all(char *s){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(write(clients[i]->connfd, s, strlen(s))<0){
				perror("write");
				exit(-1);
			}
		}
	}
}

/* Send message to sender */
void send_message_self(const char *s, int connfd){
	if(write(connfd, s, strlen(s))<0){
		perror("write");
		exit(-1);
	}
}

/* Send message to client */
void send_message_client(char *s, int uid, int self_id){
	int i;
	int flag = 0;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid == uid){
				if(write(clients[i]->connfd, s, strlen(s))<0){
					perror("write");
					exit(-1);
				}
				flag = 1;
			}
		}
	}
	if (!flag)
		send_message_self("Error. No clients with this ID\n", self_id);
}

/* Send list of active clients */
void send_active_clients(int connfd){
	int i;
	char s[64];
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			sprintf(s, "Client ID: %d | NAME: %s\r\n", clients[i]->uid, clients[i]->name);
			send_message_self(s, connfd);
		}
	}
	sprintf(s, ">>>>>>>>>>>>>>>>>>>>>>>\n");
	send_message_self(s, connfd);
}

/* Strip CRLF */
void strip_newline(char *s){
	while(*s != '\0'){
		if(*s == '\r' || *s == '\n'){
			*s = '\0';
		}
		s++;
	}
}

/* Print ip address */
void print_client_addr(struct sockaddr_in addr){
	printf("%d.%d.%d.%d",
		addr.sin_addr.s_addr & 0xFF,
		(addr.sin_addr.s_addr & 0xFF00)>>8,
		(addr.sin_addr.s_addr & 0xFF0000)>>16,
		(addr.sin_addr.s_addr & 0xFF000000)>>24);
}

/* Handle all communication with the client */
void *handle_client(void *arg){
	char buff_out[8102];
	char buff_in[8102];
	int rlen;
	char last_word[2048];
	cli_count++;
	client_t *cli = (client_t *)arg;
	int admin_try = 0;
	char ban_name[32];

	printf("<<ACCEPT ");
	print_client_addr(cli->addr);
	printf(" REFERENCED BY %d\n", cli->uid);

	sprintf(buff_out, "Hello, there is new user here with ID: %s\r\n> Use \\HELP\n\n", cli->name);
	send_message_all(buff_out);

	/* Receive input from client */
	while((rlen = read(cli->connfd, buff_in, sizeof(buff_in)-1)) > 0){
	        buff_in[rlen] = '\0';
	        buff_out[0] = '\0';
		strip_newline(buff_in);

		/* Ignore empty buffer */
		if(!strlen(buff_in)){
			continue;
		}

		/* Special options */
		if(buff_in[0] == '\\'){
			char *command, *param;
			command = strtok(buff_in," ");
			if(!strcmp(command, "\\QUIT")){
				param = strtok(NULL, " ");
				while(param != NULL){
							strcat(last_word, " ");
							strcat(last_word, param);
							param = strtok(NULL, " ");
				}
				strcat(last_word, "\r\n");
				break;
			}else if(!strcmp(command, "\\PING")){
				send_message_self("PONG\r\n\n", cli->connfd);
			}else if(!strcmp(command, "\\NAME")){
				param = strtok(NULL, " ");
				char * param2 = strtok(NULL, " ");
				if(param){
					if (cli->root==0) {
						if ((param2)==NULL){
							if (strlen(param) < 32) {
								char *old_name = strdup(cli->name);
								if (strstr(ban_list, param) != NULL) {
									sprintf(buff_out, "[BAN][ADMIN] YOU HAVE BEEN BANNED [BAN][ADMIN]\nComment: ");
									strcat(buff_out, "\r\n");
									send_message_self(buff_out, cli->connfd);
								}else{
									if (counter == 0) {
										strcat(names[counter], param);
										counter ++;
										strcpy(cli->name, param);
										sprintf(buff_out, "Client with name/ID %s changed his name to %s\r\n\n", old_name, cli->name);
										free(old_name);
										send_message_all(buff_out);
									}else{
										int name_flag = 0;
										for (int zz = 0; zz <= online_clients; zz++) 
											if (strstr(names[zz],param) != NULL) {
												name_flag = 1;
												break;
											}
										if (!name_flag) {
											strcpy(cli->name, param);
											sprintf(buff_out, "Client with name/ID %s changed his name to %s\r\n\n", old_name, cli->name);
											free(old_name);
											send_message_all(buff_out);
										}else{
											sprintf(buff_out, "This name is already taken. Sorry. \r\n\n");
											send_message_self(buff_out, cli->connfd);
										}
									}
								}
							}else
								send_message_self("Name can't be more than 32 symbols \r\n\n", cli->connfd);
						}else
							send_message_self("Name can't contain more than 1 word \r\n\n", cli->connfd);
					}else{
						for(int j=0;j<online_clients;j++){
							if(!strcmp(param, clients[j]->name)){
								if(param2){
									int name_flag = 0;
									for (int zz = 0; zz <= online_clients; zz++) 
											if (strstr(names[zz],param2) != NULL) {
												name_flag = 1;
												break;
											}
									if (!name_flag) {
										for (int l = 0; l < sizeof(param2); l++)
											clients[j]->name[l]=param2[l];
										sprintf(buff_out, "Client with name %s changed his name to %s\r\n\n", param, param2);
										send_message_all(buff_out);
									}else{
										sprintf(buff_out, "This name is already taken. Sorry. \r\n\n");
										send_message_self(buff_out, cli->connfd);
									}
								}else
									send_message_self("New nickname can't be a NULL \r\n\n", cli->connfd);
								break;
							}
							if (j==online_clients-1)
								send_message_self("No such nickname online. \r\n\n", cli->connfd);
						}


					}
				}else{
					send_message_self("Name can't be a NULL \r\n\n", cli->connfd);
				}
			}else if(!strcmp(command, "\\PRIVATE")){
				param = strtok(NULL, " ");
				if(param){
					if (int_or_not(param)) { // ID
						int uid = atoi(param);
						char str[32];
						sprintf(str, "%d", uid);
						strcat(cli->pm, str);
						strcat(cli->pm, " ");
						param = strtok(NULL, " ");
						if(param){
							time_t rawtime;
  							struct tm * timeinfo;
  							time ( &rawtime );
  							timeinfo = localtime ( &rawtime );
							sprintf(buff_out, "[PM][%s]\n%s", cli->name, asctime(timeinfo));
							int count=0;
							while(param != NULL){
								strcat(buff_out, param);
								strcat(buff_out, " ");
								strcat(cli->history[uid], param);
								strcat(cli->history[uid], " ");
								for (count=0; count<online_clients; count++)
									if (clients[count]->uid == uid) {
										strcat(clients[count]->history[cli->uid], param);
										strcat(clients[count]->history[cli->uid], " ");
										break;
									}
								param = strtok(NULL, " ");
							}
							strcat(cli->history[uid], "- My answer\n");
							strcat(clients[count]->history[cli->uid], "- Companion answer\n");
							strcat(buff_out, "\r\n");
							for (int count=0; count<online_clients; count++)
								if (clients[count]->uid == uid) {
									strcat(clients[count]->pmget, cli->name);
									strcat(clients[count]->pmget, " ");
								}
							send_message_client(buff_out, uid, cli->connfd);
						}else{
							send_message_self("Message can't be a NULL \r\n\n", cli->connfd);
						}
					}else{ // Nickname
						for(int j=0;j<online_clients;j++){
							if(!strcmp(param,clients[j]->name)){
								char str[32];
								sprintf(str, "%s", param);
								strcat(cli->pm, str);
								strcat(cli->pm, " ");
								param = strtok(NULL, " ");
								if(param){
									time_t rawtime;
		  							struct tm * timeinfo;
		  							time ( &rawtime );
		  							timeinfo = localtime ( &rawtime );
									sprintf(buff_out, "[PM][%s]\n%s", cli->name, asctime(timeinfo));
									while(param != NULL){
										strcat(buff_out, param);
										strcat(buff_out, " ");
										strcat(cli->history[clients[j]->uid], param);
										strcat(cli->history[clients[j]->uid], " ");
										strcat(clients[j]->history[cli->uid], param);
										strcat(clients[j]->history[cli->uid], " ");
										param = strtok(NULL, " ");
									}
									strcat(cli->history[clients[j]->uid], "- My answer\n");
									strcat(clients[j]->history[cli->uid], "- Companion answer\n");
									strcat(buff_out, "\r\n");
									strcat(clients[j]->pmget, cli->name);
									strcat(clients[j]->pmget, " ");
									send_message_client(buff_out, clients[j]->uid, cli->connfd);
								}else{
									send_message_self("Message can't be a NULL \r\n\n", cli->connfd);
								}
								break;
							}
							if (j==online_clients-1)
								send_message_self("No such nickname online. \r\n\n", cli->connfd);
						}
					}
				}else{
					send_message_self("Reference can't be a NULL \r\n\n", cli->connfd);
				}
			}else if(!strcmp(command, "\\ACTIVE")){
				sprintf(buff_out, "Number of clients: %d\r\n", cli_count);
				send_message_self(buff_out, cli->connfd);
				send_active_clients(cli->connfd);
			}else if(!strcmp(command, "\\HELP")){
				strcat(buff_out, "\\QUIT     Quit chatroom.\r\n");
				strcat(buff_out, "> \\PING     Server test.\r\n");
				strcat(buff_out, "> \\NAME     <new_name> Change nickname.\r\n");
				strcat(buff_out, "> \\PRIVATE  <ID> <message> Send private message.\r\n");
				strcat(buff_out, "> \\ACTIVE   Show active clients.\r\n");
				strcat(buff_out, "> \\HELP     Show help.\r\n");
				strcat(buff_out, "> \\HISTORY  Show ID's for PM.\n");
				strcat(buff_out, "> \\HISTORY  <active_name> Show private message history.\n");
				strcat(buff_out, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
				strcat(buff_out, "> \\ADMIN    Become an admin.\n");
				strcat(buff_out, "> \\SHUTDOWN Power off server.\n");
				strcat(buff_out, "> \\NAME     <active_name> <new_name> Change nickname by force.\r\n");
				strcat(buff_out, "> \\FIGHT    <ID> Fight for you live.\n");
				strcat(buff_out, "> \\KICK     <ID> <comment> Kick user.\n");
				strcat(buff_out, "> \\BAN      <ID> <comment> Ban user.\n\n");
				send_message_self(buff_out, cli->connfd);
			}else if(!strcmp(command, "\\HISTORY")){
				param = strtok(NULL, " ");
				if(param){
					for(int j=0;j<online_clients;j++){
							if(!strcmp(param,clients[j]->name)){
								strcat(buff_out, cli->history[clients[j]->uid]);
								strcat(buff_out, "\r\n\n");
								send_message_self(buff_out, cli->connfd);
								break;
							}
							if (j==online_clients-1)
								send_message_self("No such nickname online. \r\n\n", cli->connfd);
					}
				}else{
					sprintf(buff_out, "Private messages ID's:\nTo: %s\nFROM: %s\n", cli->pm, cli->pmget);
					send_message_self(buff_out, cli->connfd);
				}
			}else if(!strcmp(command, "\\ADMIN")){
				sprintf(buff_out, "Password please: ");
				send_message_self(buff_out, cli->connfd);
				admin_try = 1;
			}else if(!strcmp(command, "\\BAN")){
				if (cli->root) {
					param = strtok(NULL, " ");
					if(param){
						if (int_or_not(param)){ //ID
							int uid = atoi(param);
							int k = uid;
							int flag_adm = 0;
							for(int j=0;j<online_clients;j++){
								if(clients[j]->uid == k){
									if (clients[j]->root == 1)
										flag_adm = 1;
								}
							}
							if (flag_adm) {
								send_message_self("You can't ban other admin.\r\n\n", cli->connfd);
								sprintf(buff_out, "Name/ID: %s tried to ban you! Fight him!!!\r\n", cli->name);
								send_message_client(buff_out, uid, cli->connfd);
								fight = 1;
							}else {
								for(int j=0;j<online_clients;j++){
									if(clients[j]->uid == k){
										for (int l = 0; l < sizeof(ban_name); l++)
											ban_name[l]=clients[j]->name[l];
									}
								}
								strcat(ban_list, ban_name);
								strcat(ban_list, " ");
								param = strtok(NULL, " ");
								if(param){
									sprintf(buff_out, "[BAN][ADMIN] YOU HAVE BEEN BANNED [BAN][ADMIN]\nComment: ");
									while(param != NULL){
										strcat(buff_out, " ");
										strcat(buff_out, param);
										param = strtok(NULL, " ");
									}
									strcat(buff_out, "\r\n");
									send_message_client(buff_out, uid, cli->connfd);
								}else
									send_message_self("Message can't be a NULL \r\n\n", cli->connfd);
							}
						}else{ // Nickname
							for(int j=0;j<online_clients;j++){
								if(!strcmp(param,clients[j]->name)){
									int flag_adm = 0;
									if (clients[j]->root == 1)
											flag_adm = 1;
									if (flag_adm) {
										send_message_self("You can't ban other admin.\r\n\n", cli->connfd);
										sprintf(buff_out, "Name/ID: %s tried to ban you! Fight him!!!\r\n", cli->name);
										send_message_client(buff_out, clients[j]->uid, cli->connfd);
										fight = 1;
									}else{
										for (int l = 0; l < sizeof(ban_name); l++)
											ban_name[l]=clients[j]->name[l];

									strcat(ban_list, ban_name);
									strcat(ban_list, " ");
									param = strtok(NULL, " ");
									if(param){
										sprintf(buff_out, "[BAN][ADMIN] YOU HAVE BEEN BANNED [BAN][ADMIN]\nComment: ");
										while(param != NULL){
											strcat(buff_out, " ");
											strcat(buff_out, param);
											param = strtok(NULL, " ");
										}
										strcat(buff_out, "\r\n");
										send_message_client(buff_out, clients[j]->uid, cli->connfd);
									}else
										send_message_self("Message can't be a NULL \r\n\n", cli->connfd);
									}
									break;
								}
								if (j==online_clients-1)
									send_message_self("No such nickname online. \r\n\n", cli->connfd);
							}
						}
					}else
						send_message_self("Name can't be a NULL \r\n\n", cli->connfd);
				}else
					send_message_self("You are not admin \r\n\n", cli->connfd);
			}else if(!strcmp(command, "\\KICK")){
				if (cli->root) {
					param = strtok(NULL, " ");
					if(param){
						if (int_or_not(param)){ // ID
							int uid = atoi(param);
							int k = uid;
							int flag_adm = 0;
							for(int j=0;j<online_clients;j++){
								if(clients[j]->uid == k){
									if (clients[j]->root == 1)
										flag_adm = 1;
								}
							}
							if (flag_adm) {
								send_message_self("You can't kick other admin.\r\n\n", cli->connfd);
								sprintf(buff_out, "Name/ID: %s tried to kick you! Be a man, call him a pussy in the chat!\r\n", cli->name);
								send_message_client(buff_out, uid, cli->connfd);
							}else {
								param = strtok(NULL, " ");
								if(param){
									sprintf(buff_out, "[BAN][ADMIN] YOU HAVE BEEN KICKED [BAN][ADMIN]\nComment: ");
									while(param != NULL){
										strcat(buff_out, " ");
										strcat(buff_out, param);
										param = strtok(NULL, " ");
									}
									strcat(buff_out, "\r\n");
									send_message_client(buff_out, uid, cli->connfd);
								}else
									send_message_self("Message can't be a NULL \r\n\n", cli->connfd);
							}
						}else{ // Nickname
							for(int j=0;j<online_clients;j++){
								if(!strcmp(param,clients[j]->name)){
									int flag_adm = 0;
									if (clients[j]->root == 1)
										flag_adm = 1;
									if (flag_adm) {
										send_message_self("You can't kick other admin.\r\n\n", cli->connfd);
										sprintf(buff_out, "Name/ID: %s tried to kick you! Be a man, call him a pussy in the chat!\r\n", cli->name);
										send_message_client(buff_out, clients[j]->uid, cli->connfd);
									}else{
										param = strtok(NULL, " ");
										if(param){
											sprintf(buff_out, "[BAN][ADMIN] YOU HAVE BEEN KICKED [BAN][ADMIN]\nComment: ");
											while(param != NULL){
												strcat(buff_out, " ");
												strcat(buff_out, param);
												param = strtok(NULL, " ");
											}
											strcat(buff_out, "\r\n");
											send_message_client(buff_out, clients[j]->uid, cli->connfd);
										}else
											send_message_self("Message can't be a NULL \r\n\n", cli->connfd);
									}
									break;
								}
								if (j==online_clients-1)
									send_message_self("No such nickname online. \r\n\n", cli->connfd);
							}
						}
					}else
						send_message_self("Name can't be a NULL \r\n\n", cli->connfd);
				}else
					send_message_self("You are not admin \r\n\n", cli->connfd);
			}else if(!strcmp(command, "\\FIGHT")){
				if (fight) {
					param = strtok(NULL, " ");
					if(param){
						int uid = atoi(param);
						int def = rand();
						int attack = rand();
						if (def < attack) {
							send_message_self("You lose.\r\n\n", cli->connfd);
							sprintf(buff_out, "Name/ID: %s tried to fight you! But he lose! Now you can ban him!\r\n", cli->name);
							send_message_client(buff_out, uid, cli->connfd);
							cli->root = 0;
						}else{
							send_message_self("You win!\r\n\n", cli->connfd);
							sprintf(buff_out, "Name/ID: %s fights you! You lose! You can't ban him!\r\n", cli->name);
							send_message_client(buff_out, uid, cli->connfd);
							cli->root = 1;
						}
						fight = 0;
					}else
						send_message_self("ID can't be a NULL \r\n\n", cli->connfd);
				} else
					send_message_self("Locked...\r\n\n", cli->connfd);
			}else if(!strcmp(command, "\\SHUTDOWN")){
				if (cli->root) {
					sprintf(buff_out, "Server is finishing his work, because of %s\r\n\n", cli->name);
					send_message_all(buff_out);
					exit(-1);
				}else{
					send_message_self("You are not admin.\r\n\n", cli->connfd);
				}
			}else{
				send_message_self("Unknown command :c \r\n\n", cli->connfd);
				break;
			}
		}else{
			/* Send message */
			if (!admin_try) {
				time_t rawtime;
  				struct tm * timeinfo;
  				time ( &rawtime );
  				timeinfo = localtime ( &rawtime );
				snprintf(buff_out, sizeof(buff_out), "[%s][TIME]\n%s %s\r\n", cli->name, asctime(timeinfo), buff_in);
				send_message_all(buff_out);
			} else {
				if (!strcmp(buff_in, password)) {
					cli->root = 1;
					admin_try = 0;
					sprintf(buff_out, "Admin granted!\n\n");
					send_message_self(buff_out, cli->connfd);
				}
				else {
					admin_try = 0;
					sprintf(buff_out, "Wrong password!\n\n");
					send_message_self(buff_out, cli->connfd);
				}
			}
		}
	}

	/* Close connection */
	sprintf(buff_out, "User %s is leaving, bye!\n His last words: %s\n", cli->name, last_word);
	send_message_all(buff_out);
	close(cli->connfd);

	/* Delete client from queue and yield thread */
	queue_delete(cli->uid);
	printf("<<LEAVE ");
	print_client_addr(cli->addr);
	printf(" REFERENCED BY %d\n", cli->uid);
	free(cli);
	cli_count--;
	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char *argv[]){
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;

	if(argc != 2) perro("USE ./server [PORT]");

	/* Socket settings */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

	/* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);

	/* Bind */
	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		perror("Socket binding failed");
		return 1;
	}

	/* Listen */
	if(listen(listenfd, 10) < 0){
		perror("Socket listening failed");
		return 1;
	}

	printf("<[SERVER STARTED]>\n");

	/* Accept clients */
	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Check if max clients is reached */
		if((cli_count+1) == MAX_CLIENTS){
			printf("<<MAX CLIENTS REACHED\n");
			printf("<<REJECT ");
			print_client_addr(cli_addr);
			printf("\n");
			close(connfd);
			continue;
		}

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->addr = cli_addr;
		cli->connfd = connfd;
		cli->uid = uid++;
		sprintf(cli->name, "%d", cli->uid);
		online_clients++;

		/* Add client to the queue and fork thread */
		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
	}
}
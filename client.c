#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define MAX_MSG_LENGTH 4096
#define END_STRING "\\QUIT"
#define FIN_STRING "\\SHUTDOWN"
#define MSG_NOSIGNAL SO_NOSIGPIPE

#define perro(x) {fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, x, strerror(errno));exit(1);}



void send_cmd(int sock, int pid) {
	char str[MAX_MSG_LENGTH] = {0};
	char res[MAX_MSG_LENGTH] = {0};
	printf("> ");
	int leave_counter = 0;
	while (fgets(str, MAX_MSG_LENGTH, stdin) == str) {
		if ((strncmp(str, END_STRING, strlen(END_STRING)) == 0) || (leave_counter == 1) || (strncmp(str, FIN_STRING, strlen(FIN_STRING)) == 0))
			if (leave_counter) {
				printf("> Before you leave: Good Luck! Tap ENTER one more time to leave (now for real).");
				leave_counter++;
			}
			else {
				printf("\n> ");
				leave_counter++;
			}
		else
			printf("\n> ");
		if (strlen(str) > 1024)
			printf("Your sentence is too big. Use maximum 2014 symbols!");
		else {
			int j = 0;
   	 		int i;
    			for (i = 0; i<strlen(str) ; i++) {
	        		if ((str[i] == ' ') || (str[i]=='\t')) {
	         			if (j==0) continue;
	          			if (str[i+1] == ' ') continue;
	          			if (str[i+1] == '\t') continue;
	        		}
	        		res[j] = str[i];
	        		j++;
   			}
			i=strlen(res);
    			if ((res[i-2] == ' ') || (res[i-2] == '\t'))
        			res[i-2] = '\0';	
			if(send(sock, res, strlen(str)+1, 0) < 0) perro("send");
		}
	}
	kill(pid, SIGKILL);
	printf("Goodbye.\n");
}

void receive(int sock) {
	char buf[MAX_MSG_LENGTH] = {0};
	int not_kick = 1;
	int filled = 0;	
	while ((filled = recv(sock, buf, MAX_MSG_LENGTH-1, 0))) {
		buf[filled] = '\0';
		if (strncmp(buf, "[BAN]", strlen("[BAN]")) == 0){
			printf("%s", buf);
			not_kick = 0;
			kill(getppid(), SIGKILL);
			break;
		}	
		printf("%s", buf);
		printf("> ");
		fflush(stdout);	
	}
	if (not_kick)
		printf("You have been disconnected from server. Tap ENTER 2 times to leave.\n");
}

int main(int argc, char **argv) {
	if(argc != 3) perro("USE ./client [PORT] 127.0.0.1");

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1) perro("Socket error.");

	struct in_addr server_addr;
	if(!inet_aton(argv[2], &server_addr)) perro("inet_aton");

	struct sockaddr_in connection;
	connection.sin_family = AF_INET;
	memcpy(&connection.sin_addr, &server_addr, sizeof(server_addr));
	connection.sin_port = htons(atoi(argv[1]));
	if (connect(sock, (const struct sockaddr*) &connection, sizeof(connection)) != 0) perro("Connect error.");
	
	int pid;	
	if ((pid = fork())) send_cmd(sock, pid);
	else receive(sock);
	
	return 0;
}

#include "scheduler.h"

int sendcommand(struct command *cmd) {
	int32_t client_sock;
	char buf[MAXDATASIZE];
	struct sockaddr_in server_addr;
	int cmdsize, numbytes;
	int ret = -1;
	
	client_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (client_sock < 0) {
		printf("sendcommand: create socket fail\n");
		return ret;
	}
	
	memset(&server_addr, 0, sizeof(struct sockaddr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SCLISTENERPORT);
	server_addr.sin_addr.s_addr = inet_addr(DEFAULTSERVERIP);
	
	ret = connect(client_sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
	if (ret == -1) {
		printf("sendcommand: connect fail\n");
		return ret;
	}
	
	
	cmdsize = cmd->args_len + 8;
	memcpy(buf, cmd, cmdsize);
	ret = send(client_sock, buf, cmdsize, 0);
	if (ret == -1) {
		printf("sendcommand: send fail\n");
		return ret;
	}
	while (1) {
		numbytes = recv(client_sock, buf, MAXDATASIZE, 0);
		if (numbytes == -1) {
			printf("sendcommand: recv fail\n");
		}
	
		if (numbytes > 0) {
			buf[numbytes] = '\0';
			printf("received: %s\n", buf);
		} else if (numbytes <= 0){
			printf("received < 0, numbytes: %d break\n", numbytes);
			break;
		}
	}

	close(client_sock);
}


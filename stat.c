#ifndef ENQ_H
#define ENQ_H

#include "client.h"


//extern int sendcommand(struct command *cmd);

int main(int argc, char *argv[]) {
	int i;
	struct command *cmd;
	char buf[MAXDATASIZE];
	int cmd_size,index;
	
	cmd_size = 0;
	cmd_size += CMDHEADSIZE;
	for (i = 0; i < argc; i++) {
		cmd_size += strlen(argv[i]);
		cmd_size += 1;
		printf("size: %d, %s\n", cmd_size, argv[i]);
	}
	
	cmd = (struct command *)malloc(cmd_size);
	cmd->type = 3;
	cmd->args_len = cmd_size - CMDHEADSIZE;
	
	index = 0;
	char tmp = ' ';
	for (i = 1; i < argc; i++) {
		memcpy(&cmd->args[0] + index, argv[i], strlen(argv[i]));
		index += strlen(argv[i]);
		memcpy(&cmd->args[0] + index, &tmp, 1);
		index += 1;
	}
	
	sendcommand(cmd);
}

#endif
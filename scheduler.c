
#include "scheduler.h"
#include <netinet/in.h>


struct job* job_enq(struct command *cmd) {
	struct job *j;
	j = malloc(sizeof(struct job));
	j->job_status = JS_INIT;
	j->job_init_priority = 3;
	j->job_cur_priority = 0;
	j->job_waiting = 0;
	memset(j->job_args, 0, sizeof(j->job_args));
	strncpy(j->job_args, cmd->args, cmd->args_len);
	j->job_args_len = cmd->args_len;
	j->job_args[j->job_args_len] = '/0';
	
	LOCK(&sc->sc_lock);
	j->job_id = sc->sc_next_job_id++;
	
	job_loader(j);
	LIST_INSERT_HEAD(&sc->sc_jobs, j, job_entries);
	UNLOCK(&sc->sc_lock);

	DEBUG("job_enq: jobid %d, jobargs %s\n", j->job_id, j->job_args);
	return j;
}

int job_deq(struct command *cmd) {
	struct job *j, *found;
	int32_t job_id;
	job_id = atoi(cmd->args);
	DEBUG("job_deq: cmd->args %s\n",cmd->args);
	found = NULL;
	int ret = 1;
	DEBUG("job_deq: remove job %d\n", job_id);
	LOCK(&sc->sc_lock);
	LIST_FOREACH(j, &sc->sc_jobs, job_entries) {
		if (j->job_id == job_id) {
			LIST_REMOVE(j, job_entries);
			found = j;
			ret = 0;
		}
	}
	UNLOCK(&sc->sc_lock);
	if (found != NULL) {
		free(found);
	}
	return ret;
}

int job_stat(struct command *cmd) {

}

int job_loader(struct job *job) {
	pid_t pid = fork();
	int ret;
	if (0 == pid) {
		char **argv;	
		int i, argc;
		
		for (i = 0; i < job->job_args_len; i++) {
			if (job->job_args[i] == ' ')
				argc++;
		}
		argc++;
		DEBUG("job_loader: argc %d\n", argc);
		
		argv = (char **)malloc(sizeof(char *) * argc);
		
		const char delimiters[] = " ";
		char *token;
		i = 0;
		token = strtok (job->job_args, delimiters);
		argv[i] = token;
		DEBUG("job_loader: token %s\n", token);
		while (token != NULL) {
			token = strtok(NULL, delimiters);
			i++;
			argv[i] = token;
			DEBUG("job_loader: token %s\n", token);
		}
		
		DEBUG("job_loader: child execve %s\n");
		ret = execve(argv[0], argv, NULL);
		if (ret < 0 ) {
			DEBUG("job_loader: job %d load fail\n", job->job_id);
		}
		DEBUG("job_loader: child exit\n");
		free(argv);
		// need or not?
		exit(0);
	} else {
		DEBUG("job_loader: parent kill\n");
		job->job_pid = pid;
		kill(pid, SIGSTOP);
		job->job_status = JS_READY;
	}
}

void job_switch(struct job *current, struct job *next) {
	DEBUG("job_switch: \n");
	
	if (current != NULL && next != NULL) {
		PRINTF("job_switch: from %d to %d\n", current->job_pid, next->job_pid);
	}
	
	if (current != NULL && current->job_pid != 0) {
		kill(current->job_pid, SIGSTOP);
		current->job_status = JS_WAIT;
	}
	if (next != NULL && next->job_pid != 0){
		current = next;
		kill(current->job_pid, SIGCONT);
		current->job_status = JS_RUNNING;		
	}
}

void wait_current_job() {
	int status;
	int ret;
	
	LOCK(&sc->sc_lock);
	if (sc->sc_current == NULL || sc->sc_current->job_pid == 0) {
		goto unlock;
	}
	ret = waitpid(sc->sc_current->job_pid, &status, WNOHANG);
	DEBUG("clean_finishjob: waitpid %d get %d\n", sc->sc_current->job_pid, ret);
	if (ret == sc->sc_current->job_pid) {
		DEBUG("clean_finishjob: current job %d finished\n", ret);
		PRINTF("wait_current_job: job finish  job %d,  pid %d", sc->sc_current->job_id, sc->sc_current->job_pid);
		sc->sc_current->job_status = JS_DONE;
		sc->sc_current = NULL;
	}
unlock:
	UNLOCK(&sc->sc_lock);
}

void clean_done_job() {
	struct job *j, *done_job;
	LOCK(&sc->sc_lock);
	LIST_FOREACH(j, &sc->sc_jobs, job_entries) {
		if (j->job_status == JS_DONE) {
			PRINTF("clean_done_job: clean job %d,  pid %d", j->job_id, j->job_pid);
			LIST_REMOVE(j, job_entries);
			LIST_INSERT_HEAD(&sc->sc_done_jobs, j, job_entries);
		}
	}
	
	while (!LIST_EMPTY(&sc->sc_done_jobs)) {
		j = LIST_FIRST(&sc->sc_done_jobs);
		LIST_REMOVE(j, job_entries);
		free(j);
	}
	UNLOCK(&sc->sc_lock);
}

void do_schedule() {
	struct timeval cur_time, diff_time;
	long last_ms, cur_ms;
	int diff_ms;
	struct job *j, *next = NULL;
	int highest = 3;
	
	LOCK(&sc->sc_lock);
	gettimeofday(&cur_time, 0);
	last_ms = sc->sc_scheduler_last_runtime.tv_sec * 1000 + sc->sc_scheduler_last_runtime.tv_usec / 1000;
	cur_ms = cur_time.tv_sec * 1000 + cur_time.tv_usec / 1000;
	diff_ms = (int) cur_ms - last_ms; 
	sc->sc_scheduler_last_runtime = cur_time;
	
	LIST_FOREACH(j, &sc->sc_jobs, job_entries) {
		if (!(j->job_status == JS_READY || j->job_status == JS_WAIT))
			continue;
		if (sc->sc_current == NULL) {
			sc->sc_current = j;
		}

		j->job_waiting += diff_ms;
		if (j->job_waiting >= 100) {
			if (0 != j->job_cur_priority) {
				j->job_cur_priority--;
				j->job_waiting -= 100;
			}
			if (j->job_cur_priority < highest) {
				highest = j->job_cur_priority;
				next = j;
			}
		}
	}

	if (NULL != next) {
		sc->sc_current->job_cur_priority = sc->sc_current->job_init_priority;
		sc->sc_current->job_waiting = 0;
		job_switch(sc->sc_current, next);
		sc->sc_current = next;
		if (sc->sc_current->job_pid == 0) {
			PRINTF("!!!!!!!!!!!!!!!!!!!!!\n");
			PRINTF("do_schedule: current job_id is null %d", sc->sc_current);
		}
	}
	UNLOCK(&sc->sc_lock);
}

static int socket_listen() {
	PRINTF("socket_listen: call socket_listen\n");
	int ret = -1;
	struct sockaddr_in *listener_addr;
	if (sc->sc_listener_sock != -1) {
		PRINTF("socket_listen: already listening\n");
		return ret;
	}
	sc->sc_listener_sock = socket(AF_INET, SOCK_STREAM, 0);
	
	if (sc->sc_listener_sock == -1) {
		PRINTF("socket_listen: create socket fail\n");
	}
	
	listener_addr = &sc->sc_listener_addr;
	listener_addr->sin_family = AF_INET;
	listener_addr->sin_port = htons(SCLISTENERPORT);
	listener_addr->sin_addr.s_addr = INADDR_ANY;
	
	ret = bind(sc->sc_listener_sock, (struct sockaddr *)listener_addr, sizeof(struct sockaddr));
	if (ret == -1) {
		PRINTF("socket_listen: bind fail\n");
		return ret;
	}
	
	ret = listen(sc->sc_listener_sock, 10);
	if (ret == -1) {
		PRINTF("socket_listen: listen fail\n");
		return ret;
	}
	PRINTF("socket_listen: listening....\n");
	return ret;
}

struct command * recvcommand() {
	char buf[MAXDATASIZE];
	int client_fd, numbytes, sin_size;
	struct sockaddr_in client_addr;
	struct command *cmd;
	if (sc->sc_listener_sock == -1) {
		DEBUG("recvcommand: sc_listener_sock not ready\n");
		return NULL;
	}
	while (1) {
		sin_size = sizeof(struct sockaddr_in);
		if((client_fd = accept(sc->sc_listener_sock, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
			//PRINTF("recvcommand: accept error");
			continue;
		}
		DEBUG("listener: got connection\n");
		memset(buf, 0, sizeof(buf));
		numbytes = recv(client_fd, buf, MAXDATASIZE, 0);
		if(numbytes == -1) {
			PRINTF("recvcommand: recv fail\n");
		}
		if (numbytes) {
			buf[numbytes] = '/0';
			DEBUG("receive: %s\n", buf + CMDHEADSIZE);
		}
		cmd = (struct command *)buf;
		do_req(client_fd, cmd);
	}
}

static void sig_timer(int i) {
	clean_done_job();
	wait_current_job();
	do_schedule();
}

void init_sc() {
	sc = (struct scheduler_context *)malloc(sizeof(struct scheduler_context));
	
	sc->sc_listener_sock = -1;
	LIST_INIT(&sc->sc_jobs);
	LIST_INIT(&sc->sc_done_jobs);
	LIST_INIT(&jobs_head);
	LOCK_INIT(&sc->sc_lock);
	
	struct itimerval timer = { {0, 10000}, {0, 10000}};
	signal(SIGALRM, sig_timer);
	setitimer(ITIMER_REAL, &timer, NULL);
}

static void send_reply(int32_t sock, char *buf, int buf_size) {
	int ret;
	ret = send(sock, buf, buf_size, 0);
	if (ret == -1) {
		DEBUG("send_reply: send fail\n");
		return ret;
	}
}

int do_enq_req(int32_t sock, struct command * req) {
	char buf[MAXDATASIZE];
	struct job *j;
	j = job_enq(req);
	//job_loader(j);
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%d", j->job_id);
	send_reply(sock, buf, strlen(buf));
	shutdown(sock, 2);
	close(sock);
	//DEBUG("close enq_req socket");
}

int do_deq_req(int32_t sock, struct command * req) {
	int ret;
	ret = job_deq(req);
	if (ret == 0) {
		send_reply(sock, "success", 7);
	} else {
		send_reply(sock, "failed", 6);
	}
	shutdown(sock, 2);
	close(sock);
}

int do_stat_req(int32_t sock, struct command * req) {
	struct job *j;
	char buf[MAXDATASIZE];
	DEBUG("do_stat_req: \n");
	
	LOCK(&sc->sc_lock);
	LIST_FOREACH(j, &sc->sc_jobs, job_entries) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "%d %s %d \n",j->job_id, j->job_args, j->job_status);
		send_reply(sock, buf, strlen(buf));
	}
	UNLOCK(&sc->sc_lock);
	shutdown(sock, 2);
	close(sock);
}

int do_req(int32_t sock, struct command * req) {
	switch (req->type) {
	case 1:
		do_enq_req(sock, req);
		break;
	case 2:
		do_deq_req(sock, req);
		break;
	case 3:
		do_stat_req(sock, req);
		break;
	default:
		DEBUG("not support command\n");
	}
}

int main(int argc, char *argv[]) {
	init_sc();
	socket_listen();
	recvcommand();
}
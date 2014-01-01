#ifndef JOBSCHEDULER_H
#define JOBSCHEDULER_H

#include <stdio.h>
#include <sys/queue.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/time.h>

#define SCLISTENERPORT 8123
#define MAXDATASIZE 1024
#define DEFAULTSERVERIP "127.0.0.1"

#define MUTEX_LOCK 1

#ifdef MUTEX_LOCK
#define LOCK_INIT(x)	pthread_mutex_init(x, 0)
#define LOCK(x)			pthread_mutex_lock(x)
#define TRY_LOCK(x)		pthread_mutex_trylock(x)
#define UNLOCK(x)		pthread_mutex_unlock(x)
#define LOCK_DESTROY(x)	pthread_mutex_destroy(x)

#else
#define LOCK_INIT(x)	
#define LOCK(x)			
#define TRY_LOCK(x)		
#define UNLOCK(x)		
#define LOCK_DESTROY(x)	
#endif

#define PRINTF( s, arg... ) printf( s "\n", ##arg)
//#define PRINTF( s, arg... )
//define DEBUG( s, arg... ) printf(s "\n", ##arg)
#define DEBUG( s, arg... ) 

enum job_status_e {
	JS_INIT,
	JS_READY,
	JS_WAIT,
	JS_RUNNING,
	JS_DONE,
};

struct job {
	LIST_ENTRY(job) job_entries;
	int job_id;
	pid_t job_pid;
	//int job_status;
	enum job_status_e job_status;
	int job_init_priority;
	int job_cur_priority;
	int job_waiting;
	char job_args[1024];
	int job_args_len;
	char job_efile[512];
	int job_efile_len;
};



LIST_HEAD(joblist_head, job) jobs_head;
typedef struct joblist_head joblist_head_t;

struct scheduler_context {
	pthread_mutex_t sc_lock;
	// struct job sc_jobs[4096];
	//LIST_HEAD(joblist_head, job_t) sc_jobs;
	joblist_head_t sc_jobs;
	joblist_head_t sc_done_jobs;
	struct job *sc_current;
	int sc_next_job_id;
	
	int32_t sc_listener_sock;
	struct sockaddr_in sc_listener_addr;
	pthread_t *sc_scheduler;
	pthread_t *sc_listener;
	struct timeval sc_scheduler_last_runtime;
};
struct scheduler_context *sc;


//struct job * job_enq(struct command *cmd);

int job_loader(struct job *job);

int job_start(int job_id);

int job_suspend(int job_id);

void job_switch(struct job *current, struct job *next);

void do_schedule();

struct command {
	int type;
	int args_len;
	char args[0];
};

#define CMDHEADSIZE 8

int do_enq_req(int32_t sock, struct command * req);

int do_deq_req(int32_t sock, struct command * req);

int do_stat_req(int32_t sock, struct command * req);

int do_req(int32_t sock, struct command * req);

static void sig_timer(int);


#endif
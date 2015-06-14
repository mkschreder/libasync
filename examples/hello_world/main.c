#include <stdio.h>
#include <stdlib.h>
#include <async/async.h>

#include <unistd.h>

typedef struct child_task {
	struct async_task prepare_string; 
	timestamp_t timeout; 
} child_task_t; 

struct application {
	struct async_process proc1, proc2; 
	child_task_t child_task; 
	timestamp_t timeout1, timeout2; 
}; 

struct application app; 

ASYNC_TASK(const char *, child_task_t, prepare_string, const char *from){
	ASYNC_BEGIN(); 
	AWAIT_DELAY(__self->timeout, 1000000); 
	ASYNC_END(from); 
}

ASYNC_PROCESS_PROC(process_one){
	struct application *app = container_of(__self, struct application, proc1); 
	ASYNC_BEGIN(); 
	while(1){
		printf("\nProcess 1: preparing...\n"); 
		const char *result = AWAIT_TASK(const char *, child_task_t, prepare_string, &app->child_task, "Hello World!"); 
		printf("\nProcess 1: %s\n", result);
		app->timeout1 = timestamp_from_now_us(500000); 
		AWAIT(timestamp_expired(app->timeout1)); 
	}
	ASYNC_END(0); 
}

ASYNC_PROCESS_PROC(process_two){
	struct application *app = container_of(__self, struct application, proc2); 
	ASYNC_BEGIN(); 
	while(1){
		printf("\nProcess 2!\n"); 
		app->timeout2 = timestamp_from_now_us(900000); 
		AWAIT(timestamp_expired(app->timeout2)); 
	}
	ASYNC_END(0); 
}

int main(int argc, char **argv){
	struct async_process _test_proc; 
	ASYNC_PROCESS_INIT(&app.proc1, process_one); 
	ASYNC_QUEUE_WORK(&ASYNC_GLOBAL_QUEUE, &app.proc1);
	ASYNC_PROCESS_INIT(&app.proc2, process_two); 
	ASYNC_QUEUE_WORK(&ASYNC_GLOBAL_QUEUE, &app.proc2); 
	while(ASYNC_RUN_PARALLEL(&ASYNC_GLOBAL_QUEUE)){
		printf("."); fflush(stdout); 
		usleep(100000); 
	}
	return 0; 
}

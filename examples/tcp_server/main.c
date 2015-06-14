

/***************************************************************************/
/*										   */
/* Server program which wait for the client to connect and reads the data  */
/*     using non-blocking socket.						*/
/* The reading of non-blocking sockets is done in a loop until data	 */
/*     arrives to the sockfd.						    */
/*										   */
/* based on Beej's program - look in the simple TCP server for further doc.*/
/*										   */
/*										   */
/***************************************************************************/

#define _BSD_SOURCE
#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/wait.h> 
#include <arpa/inet.h>
#include <strings.h>
#include <fcntl.h> /* Added for the nonblocking socket */
#include <unistd.h>

#include <async/async.h>
#include <async/cbuf.h>

struct proto_message {
	uint8_t id; 
	uint8_t data[20]; 
}; 

typedef struct client {
	struct	sockaddr_in		address; 
	int socket; 
	uint8_t _buffer_data[512]; 
	struct cbuf buffer; 
	char 	line[512]; 
	int outgoing_size, bytes_sent;
	struct async_task recv_line, send_line; 
	struct async_process process; 
	int disconnected, done; 
	struct list_head list; 
} client_t; 

ASYNC_TASK(int, client_t, send_line, const uint8_t *data, int size){
	struct client *self = __self; 
	ASYNC_BEGIN(); 
	self->outgoing_size = size; 
	self->bytes_sent = 0; 
	while(self->bytes_sent < self->outgoing_size){
		int ret = send(self->socket, self->line + self->bytes_sent, self->outgoing_size - self->bytes_sent, 0); 
		if(ret > 0){
			self->bytes_sent += ret; 
		} else if(ret == 0){
			self->disconnected = 1; 
		}
		ASYNC_YIELD(); 
	}
	ASYNC_END(0); 
}

ASYNC_TASK(int, client_t, recv_line){
	struct client *self = __self; 
	ASYNC_BEGIN(); 
	while(1){
		char _tmp[512]; 
		int ret = recv(self->socket, _tmp, sizeof(_tmp), 0);
		if(ret > 0){
			for(int c = 0; c < ret; c++){
				char ch = _tmp[c]; 
				if(ch == '\n'){
					uint16_t length = cbuf_getn(&self->buffer, self->line, sizeof(self->line)); 
					cbuf_clear(&self->buffer);
					ASYNC_TASK_RETURN(length); 
				} else {
					cbuf_put(&self->buffer, ch); 
				}
			}
		} else if(ret == 0){
			self->disconnected = 1; 
			ASYNC_TASK_RETURN(-1); 
		}
		ASYNC_YIELD(); 
	}
	ASYNC_END(0); 
}

ASYNC_PROCESS_PROC(client_process){
	struct client *self = container_of(__self, struct client, process); 
	ASYNC_BEGIN(); 
	while(!self->disconnected){
		// read one command
		if(AWAIT_TASK(int, client_t, recv_line, self) > 0){
			// parse command
			printf("command: %s\n", self->line); 
			// send response
			AWAIT_TASK(int, client_t, send_line, self, self->line, strlen(self->line));
		}
	}
	printf("client disconnected\n"); 
	self->done = 1; 
	ASYNC_END(0); 
}

void client_init(struct client *self, int fd, struct sockaddr_in *addr, int addr_size){
	memset(self, 0, sizeof(struct client)); 
	memcpy(&self->address, addr, sizeof(struct sockaddr_in)); 
	self->socket = fd; 
	INIT_LIST_HEAD(&self->list); 
	cbuf_init(&self->buffer, self->_buffer_data, sizeof(self->_buffer_data)); 
	ASYNC_PROCESS_INIT(&self->process, client_process); 
	ASYNC_QUEUE_WORK(&ASYNC_GLOBAL_QUEUE, &self->process)
}

struct server {
	struct async_process listen_process; 
	int sockfd; 
	struct	sockaddr_in		listen_address;    /* my address information */
	struct	sockaddr_in		client_address; 
	struct list_head clients; 
}; 

ASYNC_PROCESS_PROC(server_listen_proc){
	struct server *self = container_of(__self, struct server, listen_process); 
	int sin_size; 
	int client_fd; 
	
	ASYNC_BEGIN(); 
	while(1){
		if ((client_fd = accept(self->sockfd, (struct sockaddr *)(&self->client_address), &sin_size)) == -1) {
			//perror("accept");
		} else {
			if (getpeername(client_fd, (struct sockaddr*)&self->client_address, &sin_size) == -1) {
				perror("getpeername() failed");
				//return -1;
			}
			printf("Client connected from %s\n", inet_ntoa(self->client_address.sin_addr)); 
			
			fcntl(client_fd, F_SETFL, O_NONBLOCK); 
			
			struct client *cl = malloc(sizeof(struct client)); 
			client_init(cl, client_fd, &self->client_address, sin_size); 
			list_add_tail(&self->clients, &cl->list); 
		}
		// now service all clients
		struct list_head *pos, *n; 
		list_for_each_safe(pos, n, &self->clients){
			struct client *client = container_of(pos, struct client, list); 
			if(client->done){
				list_del_init(&client->list); 
				free(client); 
				printf("Freed client data\n"); 
			}
		}
		ASYNC_YIELD(); 
	}
	ASYNC_END(0); 
}

int8_t server_start(struct server *self, int port){
	INIT_LIST_HEAD(&self->clients); 
	
	if ((self->sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		return -1; 
	}
	self->listen_address.sin_family = AF_INET;	  /* host byte order */
	self->listen_address.sin_port = htons(port);     /* short, network byte order */
	self->listen_address.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */
	bzero(&(self->listen_address.sin_zero), 8);	 /* zero the rest of the struct */

	if (bind(self->sockfd, (struct sockaddr *)&self->listen_address, sizeof(struct sockaddr)) == -1) {
		perror("bind");
		return -1; 
	}
	
	if (listen(self->sockfd, 10) == -1) {
		perror("listen");
		return -1; 
	}
	
	// important to set non blocking state on the socket
	fcntl(self->sockfd, F_SETFL, O_NONBLOCK); 
	
	// start server process
	ASYNC_PROCESS_INIT(&self->listen_process, server_listen_proc); 
	ASYNC_QUEUE_WORK(&ASYNC_GLOBAL_QUEUE, &self->listen_process); 
}

int main(){
	struct server server; 
	server_start(&server, 3456); 
	while(ASYNC_RUN_PARALLEL(&ASYNC_GLOBAL_QUEUE)){
		//printf("."); fflush(stdout); 
		usleep(10000); 
	}
}


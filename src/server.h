#ifndef _SERWER_H
#define _SERWER_H

#include "common.h"

#define HEARTBEAT_BREAK 3

#define SERVERS_QUEUE 1000

typedef struct {
	int clients;
	int client_msgid;
	int server_msgid;
	int client_id[SERVER_CAPACITY];
	char client_name[SERVER_CAPACITY][MAX_NAME_SIZE];
	int servers_queued;
	int servers_to_confirm[SERVERS_QUEUE];
	int servers_answered[SERVERS_QUEUE];
} SERVER_USERS;

int server();

#endif

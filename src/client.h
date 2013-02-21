#ifndef _KLIENT_H
#define _KLIENT_H

#include "common.h"

typedef struct {
	int logged;
	char name[MAX_NAME_SIZE];
	char room[MAX_NAME_SIZE];
	int active_servers;
	int servers[MAX_SERVER_NUM];
	int clients_on_servers[MAX_SERVER_NUM];
	int client_id, server_id, all_servers_id;
} CLIENT_REPO;

int client();

#endif

#ifndef _INCLUDE_COMMON_H
#define _INCLUDE_COMMON_H

#include <time.h>

// shm_key repozytorium
#define ID_REPO 80085

// sem_key semafora dla repozytorium
#define SEM_REPO 80666

// sem_key semafora dla pliku logu
#define SEM_LOG 80777

// Wspólna kolejka dla serwerów, tutaj podpinają się klienci w celu wysłania żądania o listę serwerów
#define SERVER_LIST_MSG_KEY 8168008
#define MAX_SERVER_NUM 17

// max liczba klientów na serwerze
#define SERVER_CAPACITY 20
#define MAX_CLIENTS MAX_SERVER_NUM*SERVER_CAPACITY
#define MAX_NAME_SIZE 20
#define MAX_MSG_SIZE 1024

// maksymalny czas reakcji serwera w sekundach
#define TIMEOUT 5

// Typy komunikatów
typedef enum {
	SERVER_LIST = 1,
	ROOM_LIST,
	ROOM_CLIENT_LIST,
	GLOBAL_CLIENT_LIST,
	LOGIN,
	LOGOUT,
	STATUS,
	HEARTBEAT,
	CHANGE_ROOM,
	PRIVATE,
	PUBLIC
} MSG_TYPE;

typedef struct {
	char name[MAX_NAME_SIZE];
	int server_id;
	char room[MAX_NAME_SIZE];
} CLIENT;

typedef struct {
	char name[MAX_NAME_SIZE];
	int clients;
} ROOM;

typedef struct {
	int client_msgid; // id kolejki komunikatów do komunikacji z klientami
	int server_msgid; // id kolejki komunikatów do komunikacji między serwerami
	int clients; // obciążenie
} SERVER;

/**
 * Struktury komunikatów
 */

typedef struct {
	long type;
	int client_msgid;
} SERVER_LIST_REQUEST;

typedef struct {
	long type;
	int active_servers;
	int servers[MAX_SERVER_NUM];
	int clients_on_servers[MAX_SERVER_NUM];
} SERVER_LIST_RESPONSE;

typedef struct {
	long type;
	int active_rooms;
	ROOM rooms[MAX_CLIENTS];
} ROOM_LIST_RESPONSE;

typedef struct {
	long type;
	int active_clients;
	char names[MAX_CLIENTS][MAX_NAME_SIZE];
} CLIENT_LIST_RESPONSE;

typedef struct {
	long type;
	int client_msgid;
	char client_name[MAX_NAME_SIZE];
} CLIENT_REQUEST;

typedef struct{
	long type;
	int client_msgid;
	char client_name[MAX_NAME_SIZE];
	char room_name[MAX_NAME_SIZE];
} CHANGE_ROOM_REQUEST;

typedef struct {
	long type;
	int status;
} STATUS_RESPONSE;

typedef struct{
	long type;
	int from_id;
	char from_name[MAX_NAME_SIZE];
	char to[MAX_NAME_SIZE];
	time_t time;
	char text[MAX_MSG_SIZE];
} TEXT_MESSAGE;

typedef struct{
	CLIENT clients[MAX_CLIENTS];
	ROOM rooms[MAX_CLIENTS];
	SERVER servers[MAX_SERVER_NUM];
	int active_clients,
		active_rooms,
		active_servers;
} REPO;

#endif

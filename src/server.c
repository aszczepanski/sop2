#include "server.h"
#include "common.h"
#include "main.h"

int semrepo, semlog, semusers;

REPO *repo;
int repoid;

SERVER_USERS *server_users;
int server_data;

static struct sembuf buf;

void P(int semid) {
	buf.sem_num = 0;
	buf.sem_op = -1;
	buf.sem_flg = 0;
	if (semop(semid, &buf, 1) == -1) {
		perror("Blocking semaphore");
		exit(1);
	}
}

void V(int semid) {
	buf.sem_num = 0;
	buf.sem_op = 1;
	buf.sem_flg = 0;
	if (semop(semid, &buf, 1) == -1) {
		perror("Unblocking semaphore");
		exit(1);
	}
}

void write_log(int opt, int server_id, char client_name[MAX_NAME_SIZE]) {
	if (fork() == 0) {
		char serv_id[10];
	
		if (server_id == 0) strcpy(serv_id, "0");
		else {
			int i=0;
			while (server_id) {
				serv_id[i++] = (char)(server_id%10+'0');
				server_id /= 10;
			}

			serv_id[i] = '\0';

			int s = i;
			for (i=0; i<s/2; i++) {
				char tmp = serv_id[i];
				serv_id[i] = serv_id[s-i-1];
				serv_id[s-i-1] = tmp;
			}		
		}

//		printf("%s\n", serv_id);

		P(semlog);

		int fd;
		if ((fd = open("./czat.log", O_CREAT | O_RDWR | O_APPEND, 0666)) == -1) {
			perror("error");
		}
		else {
//			printf("file %d opened\n", fd);

			switch(opt) {
			case 1: 
				write(fd, "ALIVE: <", 8*sizeof(char));
				write(fd, serv_id, strlen(serv_id));
				write(fd, ">", 1*sizeof(char));
			break;
			case 2:
				write(fd, "DOWN: <", 7*sizeof(char));
				write(fd, serv_id, strlen(serv_id));
				write(fd, ">", 1*sizeof(char));
			break;
			case 3:
				write(fd, "KILLED_ZOMBIE: <", 16*sizeof(char));
				write(fd, serv_id, strlen(serv_id));
				write(fd, ">", 1*sizeof(char));
			break;
			case 4:
				write(fd, "LOGGED_IN@<", 11*sizeof(char));
				write(fd, serv_id, strlen(serv_id));
				write(fd, ">: <", 4*sizeof(char));
				write(fd, client_name, strlen(client_name));
				write(fd, ">", 1*sizeof(char));
			break;
			case 5:
				write(fd, "LOGGED_OUT@<", 12*sizeof(char));
				write(fd, serv_id, strlen(serv_id));
				write(fd, ">: <", 4*sizeof(char));
				write(fd, client_name, strlen(client_name));
				write(fd, ">", 1*sizeof(char));
			break;
			case 6:
				write(fd, "DEAD@<", 6*sizeof(char));
				write(fd, serv_id, strlen(serv_id));
				write(fd, ">: <", 4*sizeof(char));
				write(fd, client_name, strlen(client_name));
				write(fd, ">", 1*sizeof(char));
			break;
			}

			close(fd);

//			puts("file closed");
	
			V(semlog);
		}
		exit(0);
	}
}

void register_repo() {

	printf("SERVER registering repo\n");

	if ((semrepo = semget(SEM_REPO, 1, 0666 | IPC_CREAT | IPC_EXCL)) == -1) {
		printf("SERVER repo already exists\n");
		
		semrepo = semget(SEM_REPO, 1, 0666);

//		puts("Blocking");
		P(semrepo);
//		puts("Blocked");
	
		repoid = shmget(ID_REPO, sizeof(REPO), IPC_CREAT | 0666);
		repo = (REPO*)shmat(repoid, NULL, 0);

		semlog = semget(SEM_LOG, 1, 0666);

		P(semusers);

		repo->active_servers++;
		repo->servers[repo->active_servers-1].client_msgid = server_users->client_msgid;
		repo->servers[repo->active_servers-1].server_msgid = server_users->server_msgid;
		repo->servers[repo->active_servers-1].clients = server_users->clients;
	
		printf("SERVER registering client_msgid:%d, server_msgid:%d, clients:%d\n", repo->servers[repo->active_servers-1].client_msgid,
			repo->servers[repo->active_servers-1].server_msgid, repo->servers[repo->active_servers-1].clients);

		int i,j;
		for (i=0; i<repo->active_servers; i++) {
			for (j=0; j<repo->active_servers; j++) {
				if (repo->servers[j].client_msgid > repo->servers[i].client_msgid) {
					SERVER tmp_server;
					tmp_server.client_msgid = repo->servers[i].client_msgid;
					tmp_server.server_msgid = repo->servers[i].server_msgid;
					tmp_server.clients = repo->servers[i].clients;

					repo->servers[i].client_msgid = repo->servers[j].client_msgid;
					repo->servers[i].server_msgid = repo->servers[j].server_msgid;
					repo->servers[i].clients = repo->servers[j].clients;

					repo->servers[j].client_msgid = tmp_server.client_msgid;
					repo->servers[j].server_msgid = tmp_server.server_msgid;
					repo->servers[j].clients = tmp_server.clients;
				}
			}
		}

		write_log(1,server_users->client_msgid,NULL);

		V(semusers);

//		puts("Unblocking");
		V(semrepo);
//		puts("Unblocked");

	}
	else {
		printf("SERVER repo doesn't exists\n");
		
//		puts("SERVER setting sem");
		semctl(semrepo, 0, SETVAL, 0);

		repoid = shmget(ID_REPO, sizeof(REPO), IPC_CREAT | 0666);
		repo = (REPO*)shmat(repoid, NULL, 0);
		
		repo->active_clients = 0;
		repo->active_rooms = 0;
		repo->active_servers = 0;

		semlog = semget(SEM_LOG, 1, 0666 | IPC_CREAT);
		semctl(semlog, 0, SETVAL, 1);

		P(semusers);

		repo->active_servers++;
		repo->servers[repo->active_servers-1].client_msgid = server_users->client_msgid;
		repo->servers[repo->active_servers-1].server_msgid = server_users->server_msgid;
		repo->servers[repo->active_servers-1].clients = server_users->clients;

		write_log(1,server_users->client_msgid,NULL);

		V(semusers);

		printf("SERVER registering client_msgid:%d, server_msgid:%d, clients:%d\n", repo->servers[repo->active_servers-1].client_msgid,
			repo->servers[repo->active_servers-1].server_msgid, repo->servers[repo->active_servers-1].clients);

//		puts("Unblocking sem");
		V(semrepo);
//		puts("Unblocked sem");
	}

}

// returns 1-valid 0-else
int user_validation(char name[MAX_NAME_SIZE], int id) {
	int res = 0, i;

	P(semusers);
	for (i=0; i<server_users->clients; i++) {
		if (id == server_users->client_id[i]) {
			if (strcmp(name, server_users->client_name[i]) == 0) {
				res = 1;
			}
		break;
		}
	}
	V(semusers);

	return res;	
}	

void wait_for_server_list_request() {
	SERVER_LIST_REQUEST server_list_request;
	SERVER_LIST_RESPONSE server_list_response;
	int all_servers_id = msgget(SERVER_LIST_MSG_KEY, 0666 | IPC_CREAT);
	
	int i;

	while(1) {
		if (msgrcv(all_servers_id, &server_list_request, sizeof(SERVER_LIST_REQUEST)-sizeof(long), SERVER_LIST, 0) == -1) {
			perror("error");
		}
		else {
			P(semrepo);

			printf("SERVER list request client id: %d\n", server_list_request.client_msgid);

			server_list_response.type = SERVER_LIST;
			server_list_response.active_servers = repo->active_servers;
			for (i=0; i<MAX_SERVER_NUM; i++) {
				server_list_response.servers[i] = repo->servers[i].client_msgid;
				server_list_response.clients_on_servers[i] = repo->servers[i].clients;
			}

			V(semrepo);

			msgsnd(server_list_request.client_msgid, &server_list_response, sizeof(SERVER_LIST_RESPONSE)-sizeof(long), 0);
		}
	}
}

// returns: server_id-already exists, -1-else
int find_client(char name[MAX_NAME_SIZE]) {
	int i;
//	P(semrepo);
	for (i=0; i<repo->active_clients; i++) {
		if (strcmp(name, repo->clients[i].name) == 0) {
			int res = repo->clients[i].server_id;
			V(semrepo);
			return res;
		}
	}
//	V(semrepo);
	return -1;
}

// returns: 1-success, 0-failed
int add_client(char name[MAX_NAME_SIZE], int client_msgidd) {
	
//	P(semusers);

	if (server_users->clients < SERVER_CAPACITY) {

		P(semrepo);

		server_users->clients++;
		strcpy(server_users->client_name[server_users->clients-1], name); // sort
		server_users->client_id[server_users->clients-1] = client_msgidd;

		int i;
		for (i=0; i<repo->active_servers; i++) {
			if (repo->servers[i].client_msgid == server_users->client_msgid) {
				repo->servers[i].clients++;
				break;
			}
		}

		repo->active_clients++;
		strcpy(repo->clients[repo->active_clients-1].name, name);
		repo->clients[repo->active_clients-1].server_id = server_users->server_msgid;
		strcpy(repo->clients[repo->active_clients-1].room, "");

		int j;
		for (i=0; i<repo->active_clients; i++) {
			for (j=0; j<repo->active_clients; j++) {
				if (strcmp(repo->clients[j].name, repo->clients[i].name) > 0) {
					CLIENT tmp_client;
					strcpy(tmp_client.name, repo->clients[i].name);
					tmp_client.server_id = repo->clients[i].server_id;
					strcpy(tmp_client.room, repo->clients[i].room);

					strcpy(repo->clients[i].name, repo->clients[j].name);
					repo->clients[i].server_id = repo->clients[j].server_id;
					strcpy(repo->clients[i].room, repo->clients[j].room);

					strcpy(repo->clients[j].name, tmp_client.name);
					repo->clients[j].server_id = tmp_client.server_id;
					strcpy(repo->clients[j].room, tmp_client.room);

				}
			}
		}

		if (repo->active_rooms == 0) {
			repo->active_rooms = 1;
			strcpy(repo->rooms[0].name, "");
			repo->rooms[0].clients = 1;
		}
		else {
			int ex = 0;
			for (i=0; i<repo->active_rooms; i++) {
				if (strcmp(repo->rooms[i].name, "") == 0) {
					repo->rooms[i].clients++;
					ex = 1;
					break;
				}
			}
			if (!ex) {
				repo->active_rooms++;
				strcpy(repo->rooms[repo->active_rooms-1].name, "");
				repo->rooms[repo->active_rooms-1].clients = 1;
			}
		}

		V(semrepo);

//		V(semusers);

		return 1;
	}
	else {
//		V(semusers);
		return 0;
	}
}

void signout_user(char name[MAX_NAME_SIZE]) {
//	puts("Blocking");
	P(semrepo);
//	puts("Blocked");

	char room_name[MAX_NAME_SIZE];

	repo->active_clients--;
	int i, found = 0;
	for (i=0; i<repo->active_clients+1; i++) {
		if (found == 1) {
			strcpy(repo->clients[i-1].name, repo->clients[i].name);
			repo->clients[i-1].server_id = repo->clients[i].server_id;
			strcpy(repo->clients[i-1].room, repo->clients[i].room);
		}
		else if (strcmp(name, repo->clients[i].name) == 0) {
			strcpy(room_name, repo->clients[i].room);
			found = 1;
		}
	}

	P(semusers);

	found = 0;
	for (i=0; i<server_users->clients; i++) {
		if (found == 1) {
			server_users->client_id[i-1] = server_users->client_id[i];
			strcpy(server_users->client_name[i-1], server_users->client_name[i]);
		}
		else if (strcmp(server_users->client_name[i], name) == 0) {
			found = 1;
		}
	}

	server_users->clients--;

	for(i=0; i<repo->active_servers; i++) {
		if (repo->servers[i].client_msgid == server_users->client_msgid) {
			repo->servers[i].clients--;
			break;
		}
	}

	V(semusers);

	found = 0;
	for (i=0; i<repo->active_rooms; i++) {
		if (found == 1) {
			strcpy(repo->rooms[i-1].name, repo->rooms[i].name);
			repo->rooms[i-1].clients = repo->rooms[i].clients;
		}
		else if (strcmp(repo->rooms[i].name, room_name) == 0) {
			repo->rooms[i].clients--;
			if (repo->rooms[i].clients == 0) found = 1;
			else break;
		}
	}
	
	if (found == 1) {
		repo->active_rooms--;
	}	


//	puts("Unblocking");
	V(semrepo);
//	puts("Unblocked");
}

void unregister_server(int server_id) {
	P(semrepo);

	int i, clients=0;
	char name[MAX_CLIENTS][MAX_NAME_SIZE];
	for (i=0; i<repo->active_clients; i++) {
		if (repo->clients[i].server_id == server_id) {
			strcpy(name[clients++], repo->clients[i].name);
		}
	}

	V(semrepo);

	for (i=0; i<clients; i++) {
		signout_user(name[i]);
	}

	P(semrepo);
	
	int found = 0;
	for (i=0; i<repo->active_servers; i++) {
		if (found) {
			repo->servers[i-1].client_msgid = repo->servers[i].client_msgid;
			repo->servers[i-1].server_msgid = repo->servers[i].server_msgid;
			repo->servers[i-1].clients = repo->servers[i].clients;
		}
		else if (repo->servers[i].server_msgid == server_id) {
			found = 1;
		}
	}

	if (found == 1) {
		repo->active_servers--;
	}

	int act = repo->active_servers;
	V(semrepo);
	if (act == 0) {
		semctl(semrepo, 0, IPC_RMID, NULL);
	
		int all_servers_id = msgget(SERVER_LIST_MSG_KEY, 0666 | IPC_CREAT);
		msgctl(all_servers_id, IPC_RMID, NULL);
	
		shmdt(repo);
		shmctl(repoid, IPC_RMID, NULL);

		semctl(semlog, 0, IPC_RMID, NULL);
	}
}

void wait_for_login_request() {
	CLIENT_REQUEST client_request;
	
	STATUS_RESPONSE status_response;
	status_response.type = STATUS;
	
	while (1) {

		P(semusers);
		int client_msgid = server_users->client_msgid;
		V(semusers);

		if (msgrcv(client_msgid, &client_request, sizeof(CLIENT_REQUEST)-sizeof(long), LOGIN, 0) == -1) {
			perror("error");
		}
		else {
			int i, printable=1;
			for (i=0; i<MAX_NAME_SIZE-1 && client_request.client_name[i] != '\0'; i++) {
				if (!isprint(client_request.client_name[i])) {
					printable = 0;
					break;
				}
			}

			if (!printable || (i == MAX_NAME_SIZE && client_request.client_name[i] != '\0')) {
				status_response.status = 400;
				msgsnd(client_request.client_msgid, &status_response, sizeof(STATUS_RESPONSE)-sizeof(long), 0);
			}
			else {
				P(semusers);

				printf("SERVER Logging to server: %d\n", server_users->client_msgid);

				printf("SERVER name: %s\n", client_request.client_name);
	
				if (find_client(client_request.client_name) != -1) status_response.status = 409; // name already exists
				else if (add_client(client_request.client_name, client_request.client_msgid) == 0) status_response.status = 503; // can't add client to current server
				else {
					status_response.status = 201; // success
					write_log(4, client_msgid, client_request.client_name);
				}
	
//				printf("SERVER status: %d\n", status_response.status);

				V(semusers);

				msgsnd(client_request.client_msgid, &status_response, sizeof(STATUS_RESPONSE)-sizeof(long), 0);
			}
		}
	}
}

void wait_for_logout_request() {
	CLIENT_REQUEST client_request;
	
	while (1) {

		P(semusers);
		int client_msgid = server_users->client_msgid;
		V(semusers);

		if (msgrcv(client_msgid, &client_request, sizeof(CLIENT_REQUEST)-sizeof(long), LOGOUT, 0) == -1) {
			perror("error");
		}
		else if (user_validation(client_request.client_name, client_request.client_msgid) == 1) {
			signout_user(client_request.client_name);
			write_log(5, client_msgid, client_request.client_name);
		}
	}
}

void wait_for_room_list_request() {
	CLIENT_REQUEST client_request;
	
	ROOM_LIST_RESPONSE room_list_response;
	room_list_response.type = ROOM_LIST;
	
	while (1) {

		P(semusers);
		int client_msgid = server_users->client_msgid;
		V(semusers);

		if (msgrcv(client_msgid, &client_request, sizeof(CLIENT_REQUEST)-sizeof(long), ROOM_LIST, 0) == -1) {
			perror("error");
		}
		else if (user_validation(client_request.client_name, client_request.client_msgid) == 1) {
			P(semrepo);
	
			room_list_response.active_rooms = repo->active_rooms;
			printf("SERVER active rooms: %d\n", room_list_response.active_rooms);
			int i;
			for (i=0; i<repo->active_rooms; i++) {
				strcpy(room_list_response.rooms[i].name, repo->rooms[i].name);
				room_list_response.rooms[i].clients = repo->rooms[i].clients;
				printf("%d name:%s clients:%d\n", i, room_list_response.rooms[i].name, room_list_response.rooms[i].clients);
			}

			V(semrepo);

			if (msgsnd(client_request.client_msgid, &room_list_response, sizeof(ROOM_LIST_RESPONSE)-sizeof(long), 0) != 0) {
				perror("error");
			}
		}
	}
}

void wait_for_room_client_list_request() {
	
	CLIENT_REQUEST client_request;
	
	CLIENT_LIST_RESPONSE client_list_response;
	client_list_response.type = ROOM_CLIENT_LIST;
	
	while (1) {

		P(semusers);
		int client_msgid = server_users->client_msgid;
		V(semusers);

		if (msgrcv(client_msgid, &client_request, sizeof(CLIENT_REQUEST)-sizeof(long), ROOM_CLIENT_LIST, 0) == -1) {
			perror("error");
		}
		else if (user_validation(client_request.client_name, client_request.client_msgid) == 1) {
			puts("SERVER client list request");
	
			P(semrepo);
	
			char client_room[MAX_NAME_SIZE];
			int i;
			for (i=0; i<repo->active_clients; i++) {
				if (strcmp(repo->clients[i].name, client_request.client_name) == 0) {
					strcpy(client_room, repo->clients[i].room);
					break;
				}
			}
	
			int k=0;
			for (i=0; i<repo->active_clients; i++) {
				if (strcmp(repo->clients[i].room, client_room) == 0) {
					strcpy(client_list_response.names[k++], repo->clients[i].name);
				}
			}

			client_list_response.active_clients = k;
	
			printf("SERVER %d users found in room %s\n", k, client_room);

			V(semrepo);

			msgsnd(client_request.client_msgid, &client_list_response, sizeof(CLIENT_LIST_RESPONSE)-sizeof(long), 0);
		}
	}
}

void wait_for_global_client_list_request() {
	
	CLIENT_REQUEST client_request;
	
	CLIENT_LIST_RESPONSE client_list_response;
	client_list_response.type = GLOBAL_CLIENT_LIST;
	
	while (1) {

		P(semusers);
		int client_msgid = server_users->client_msgid;
		V(semusers);

		if (msgrcv(client_msgid, &client_request, sizeof(CLIENT_REQUEST)-sizeof(long), GLOBAL_CLIENT_LIST, 0) == -1) {
			perror("error");
		}
		else if (user_validation(client_request.client_name, client_request.client_msgid) == 1) {
			puts("SERVER client list request");
	
			P(semrepo);
	
			int i;
			for (i=0; i<repo->active_clients; i++) {
				strcpy(client_list_response.names[i], repo->clients[i].name);
			}

			client_list_response.active_clients = repo->active_clients;
	
			printf("SERVER %d users found\n", client_list_response.active_clients);

			V(semrepo);

			msgsnd(client_request.client_msgid, &client_list_response, sizeof(CLIENT_LIST_RESPONSE)-sizeof(long), 0);
		}
	}
}

void wait_for_change_room_request() {

	CHANGE_ROOM_REQUEST change_room_request;
	STATUS_RESPONSE status_response;
	status_response.type = CHANGE_ROOM;	

	while (1) {

		P(semusers);
		int client_msgid = server_users->client_msgid;
		V(semusers);

		if (msgrcv(client_msgid, &change_room_request, sizeof(CHANGE_ROOM_REQUEST)-sizeof(long), CHANGE_ROOM, 0) == -1) {
			perror("error");
		}
		else {
			int i, printable=1;
			for (i=0; i<MAX_NAME_SIZE-1 && change_room_request.room_name[i] != '\0'; i++) {
				if (!isprint(change_room_request.room_name[i])) {
					printable = 0;
					break;
				}
			}

			if (!printable || (i == MAX_NAME_SIZE && change_room_request.room_name[i] != '\0')) {
				status_response.status = 400;
	
				if (msgsnd(change_room_request.client_msgid, &status_response, sizeof(STATUS_RESPONSE)-sizeof(long), 0) != 0) {
					perror("error");
				}

				puts("SERVER wrong room name");
			}
			else {
				if (user_validation(change_room_request.client_name, change_room_request.client_msgid) == 1) {
		
					puts("SERVER room changing");
		
					P(semrepo);
		
					char old_room[MAX_NAME_SIZE];
		
					for (i=0; i<repo->active_clients; i++) {
						if (strcmp(repo->clients[i].name, change_room_request.client_name) == 0) {
							strcpy(old_room, repo->clients[i].room);
							strcpy(repo->clients[i].room, change_room_request.room_name);
							break;
						}
					}
	
					int exists = 0, found = 0;
					for (i=0; i<repo->active_rooms; i++) {
						if (found == 1) {
							strcpy(repo->rooms[i-1].name, repo->rooms[i].name);
							repo->rooms[i-1].clients = repo->rooms[i].clients;
						}
						else if (strcmp(repo->rooms[i].name, old_room) == 0) {
							repo->rooms[i].clients--;
							if (repo->rooms[i].clients == 0) {
								found = 1;
							}
							else {
								break;
							}
						}
					}
		
					if (found == 1) repo->active_rooms--;
			
					for (i=0; i<repo->active_rooms; i++) {
						if (strcmp(repo->rooms[i].name, change_room_request.room_name) == 0) {
							repo->rooms[i].clients++;
							exists = 1;
							break;
						}
					}
		
					if (!exists) {
						repo->active_rooms++;
						strcpy(repo->rooms[repo->active_rooms-1].name, change_room_request.room_name);
						repo->rooms[repo->active_rooms-1].clients = 1;
		
						int j;
						for (i=0; i<repo->active_rooms; i++) {
							for (j=0; j<repo->active_rooms; j++) {
								if (strcmp(repo->rooms[j].name, repo->rooms[i].name) > 0) {
									ROOM tmp_room;
									strcpy(tmp_room.name, repo->rooms[i].name);
									tmp_room.clients = repo->rooms[i].clients;
		
									strcpy(repo->rooms[i].name, repo->rooms[j].name);
									repo->rooms[i].clients = repo->rooms[j].clients;
		
									strcpy(repo->rooms[j].name, tmp_room.name);
									repo->rooms[j].clients = tmp_room.clients;
								}
							}
						}
					}

					V(semrepo);

					status_response.status = 202;

					if (msgsnd(change_room_request.client_msgid, &status_response, sizeof(STATUS_RESPONSE)-sizeof(long), 0) != 0) {
						perror("error");
					}
				}
			}
		}
	}
}

void wait_for_server_response() {
	STATUS_RESPONSE status_response;

	P(semusers);
	server_users->servers_queued = 0;
	int server_msgid = server_users->server_msgid;
	V(semusers);

	while (1) {
		if (msgrcv(server_msgid, &status_response, sizeof(STATUS_RESPONSE)-sizeof(long), STATUS, 0) == -1) {
			perror("error");
		}
		else {
			P(semusers);

			server_users->servers_queued++;
			server_users->servers_to_confirm[server_users->servers_queued-1] = status_response.status;

			V(semusers);
		}
	}
}

void heartbeat_server(int server_id) {
	int i;
	if (fork() == 0) {
		sleep(TIMEOUT);
		
		P(semusers);

		int found = 0;

		for (i=0; i<server_users->servers_queued; i++) {
			if (found == 1) {
				server_users->servers_to_confirm[i-1] = server_users->servers_to_confirm[i];
			}
			else if (server_users->servers_to_confirm[i] == server_id) {
				found = 1;
			}
		}

		if (found == 1) {
			server_users->servers_queued--;
			V(semusers);
		}
		else {
			int server_id = server_users->client_msgid;
			V(semusers);
			unregister_server(server_id);
			write_log(3, server_id, NULL);
		}

		exit(0);
	}
}

void wait_for_public_text_message_client() {
	TEXT_MESSAGE text_message;

	while (1) {

		P(semusers);
		int client_msgid = server_users->client_msgid;
		V(semusers);

		if (msgrcv(client_msgid, &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), PUBLIC, 0) == -1) {
			perror("error");
		}
		else {
			if (user_validation(text_message.from_name, text_message.from_id) == 1) {

				printf("SERVER new public message from %s\n", text_message.from_name);
	
				text_message.from_id = client_msgid;
		
				P(semrepo);
	
				int i;
				for (i=0; i<repo->active_servers; i++) { // TO DO delete zombies
					int cur_server_id = repo->servers[i].server_msgid;
					V(semrepo);
					if (msgsnd(cur_server_id, &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), 0) != 0) {
						perror("error");
						// unregister server !!!
					}

					printf("SERVER message sent to server: %d\n", cur_server_id);

					heartbeat_server(cur_server_id);

					P(semrepo);			
				}
		
				V(semrepo);
			}
		}
	}
}

void wait_for_public_text_message_server() {
	TEXT_MESSAGE text_message;
	
	STATUS_RESPONSE status_response;
	status_response.type = STATUS;

	P(semusers);
	status_response.status = server_users->server_msgid;
	V(semusers);

	char cur_room[MAX_NAME_SIZE];

	while (1) {
		P(semusers);
		int server_msgid = server_users->server_msgid;
		V(semusers);
		if (msgrcv(server_msgid, &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), PUBLIC, 0) == -1) {
			perror("error");
		}
		else {
			printf("SERVER message from server\n");		

			int i,j;
		
			P(semrepo);

			for (i=0; i<repo->active_clients; i++) {
				if (strcmp(repo->clients[i].name, text_message.from_name) == 0) {
					strcpy(cur_room, repo->clients[i].room);
					break;
				}
			}

			P(semusers);

			for (i=0; i<server_users->clients; i++) {
				for (j=0; j<repo->active_clients; j++) {
					if ( (strcmp(repo->clients[j].name, server_users->client_name[i]) == 0)
						&& (strcmp(repo->clients[j].room, cur_room) == 0) ) {
						if (msgsnd(server_users->client_id[i], &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), 0) != 0) {
							perror("error");
						}
						printf("SERVER message sent to %s\n", server_users->client_name[i]);
						break;
					}
				}
			}

			V(semusers);

			V(semrepo);
	
			P(semrepo);

			for (i=0; i<repo->active_servers; i++) {
				if (repo->servers[i].client_msgid == text_message.from_id) {
					if (msgsnd(repo->servers[i].server_msgid, &status_response, sizeof(STATUS_RESPONSE)-sizeof(long), 0) != 0) {
						perror("error");
					}

				}
			}

			V(semrepo);
		}
	}
}

void wait_for_private_text_message_client() {
	TEXT_MESSAGE text_message;

	while (1) {

		P(semusers);
		int client_msgid = server_users->client_msgid;
		V(semusers);
		
		if (msgrcv(client_msgid, &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), PRIVATE, 0) == -1) {
			perror("error");
		}
		else if (user_validation(text_message.from_name, text_message.from_id) == 1) {

			int on_list = 0;
			int i;

			P(semusers);
		
			for (i=0; i<server_users->clients; i++) {
				if(strcmp(server_users->client_name[i], text_message.to) == 0) {
					on_list = 1;
	
					text_message.from_id = 0;				

					if (msgsnd(server_users->client_id[i], &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), 0) == -1) {
						perror("error");
					}
	
					break;
				}
			}
			
			V(semusers);

			if (!on_list) {
			
				int serv_id = -1;

				P(semrepo);
	
				for (i=0; i<repo->active_clients; i++) {
					if (strcmp(repo->clients[i].name, text_message.to) == 0) {
						serv_id = repo->clients[i].server_id;
						break;
					}
				}


				V(semrepo);

				if (serv_id != -1) {

					text_message.from_id = client_msgid;

					if (msgsnd(serv_id, &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), 0) == -1) {
						perror("error");
					}

					heartbeat_server(serv_id);
				}
			}
		}
	}
}

void wait_for_private_text_message_server() {
	TEXT_MESSAGE text_message;
	STATUS_RESPONSE status_response;
	status_response.type = STATUS;

	P(semusers);
	status_response.status = server_users->server_msgid;
	V(semusers);

	int i;

	while (1) {

		P(semusers);
		int server_msgid = server_users->server_msgid;
		V(semusers);
		
		if (msgrcv(server_msgid, &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), PRIVATE, 0) == -1) {
			perror("error");
		}
		else {
			P(semrepo);

			for (i=0; i<repo->active_servers; i++) {
				if (repo->servers[i].client_msgid == text_message.from_id) {
					if (msgsnd(repo->servers[i].server_msgid, &status_response, sizeof(STATUS_RESPONSE)-sizeof(long), 0) != 0) {
						perror("error");
					}
					break;
				}
			}
	
			V(semrepo);
	
			int on_list = 0;
			int i;

			P(semusers);

			for (i=0; i<server_users->clients; i++) {
				if(strcmp(server_users->client_name[i], text_message.to) == 0) {
					on_list = 1;
					text_message.from_id = 0;

					if (msgsnd(server_users->client_id[i], &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), 0) != 0) {
						perror("error");
					}
					break;
				}
			}
			
			V(semusers);
		
			if (!on_list) {			
				int serv_id = -1;

				P(semrepo);

				for (i=0; i<repo->active_clients; i++) {
					if (strcmp(repo->clients[i].name, text_message.to) == 0) {
						serv_id = repo->clients[i].server_id;
						break;
					}
				}


				V(semrepo);

				if (serv_id != -1) {

					P(semusers);
					server_msgid = server_users->server_msgid;
					V(semusers);

					text_message.from_id = server_msgid;

					if (msgsnd(serv_id, &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), 0) != 0) {
						perror("error");
					}

					heartbeat_server(serv_id);
				}
			}
		}
	}
}

void heartbeat_client() {
	STATUS_RESPONSE status_response;
	status_response.type = HEARTBEAT;

	P(semusers);
	status_response.status = server_users->client_msgid;
	V(semusers);

	CLIENT_REQUEST client_request;

	int i=-1, client_id;
	char client_name[MAX_NAME_SIZE];

	while (1) {
		P(semusers);
		if (server_users->clients > 0) {
			i = (i+1)%server_users->clients;
			client_id = server_users->client_id[i];
			strcpy(client_name, server_users->client_name[i]);
		}
		else {
			i=-1;
		}
		V(semusers);

		if (i != -1) {

			printf("SERVER heartbeat %s %d\n", client_name, client_id);

			// ograniczenie czasowe trzeba dodać
			msgsnd(client_id, &status_response, sizeof(STATUS_RESPONSE)-sizeof(long), 0);

			sleep(TIMEOUT);

			if (msgrcv(status_response.status, &client_request, sizeof(CLIENT_REQUEST)-sizeof(long), HEARTBEAT, IPC_NOWAIT) == -1) {
				perror("error");
				puts("SERVER client down\n");
				signout_user(client_name);
				write_log(6, status_response.status, client_name);
			}
			else {
				printf("SERVER %s confirmed\n", client_name);
			}

		}

		sleep(HEARTBEAT_BREAK);
	}
}

int server() {

	puts("You are currently running server program");

	semusers = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);	
	semctl(semusers, 0, SETVAL, 0);

	server_data = shmget(IPC_PRIVATE, sizeof(SERVER_USERS), 0666 | IPC_CREAT);
	server_users = (SERVER_USERS*)shmat(server_data, NULL, 0);
	
	server_users->client_msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
	server_users->server_msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
	server_users->clients = 0;

	V(semusers);
	
	/* server registration */
	register_repo();

	pid_t pids[MAX_FORKS];
	int p=0;

	if ((pids[p++] = fork()) == 0) {
		/* server list request */	
		wait_for_server_list_request();
	}
	else if ((pids[p++] = fork()) == 0) {
		/* login */
		wait_for_login_request();
	}
	else if ((pids[p++] = fork()) == 0) {
		/* logout */
		wait_for_logout_request();
	}
	else if ((pids[p++] = fork()) == 0) {
		/* rooms list */
		wait_for_room_list_request();
	}
	else if ((pids[p++] = fork()) == 0) {
		/* room clients list */
		wait_for_room_client_list_request();
	}
	else if ((pids[p++] = fork()) == 0) {
		/* global clients list */
		wait_for_global_client_list_request();
	}
	else if ((pids[p++] = fork()) == 0) {
		/* heartbeat */
		heartbeat_client();
	}
	else if ((pids[p++] = fork()) == 0) {
		/* change room */
		wait_for_change_room_request();
	}
	else if ((pids[p++] = fork()) == 0) {
		/* public text message - from client */
		wait_for_public_text_message_client();
	}
	else if ((pids[p++] = fork()) == 0) {
		/* public text message - from server */
		wait_for_public_text_message_server();
	}
	else if ((pids[p++] = fork()) == 0) {
		/* private text message - from client */
		wait_for_private_text_message_client();
	}
	else if ((pids[p++] = fork()) == 0) {
		/* private text message - from server */
		wait_for_private_text_message_server();
	}
	else if ((pids[p++] = fork()) == 0) {
		/* server heartbeat */
		wait_for_server_response();
	}
	else {
		char input[MAX_TEXT_SIZE];
		do {
			scanf("%s", input);
		} while (strcmp(input, "exit") != 0);

		int i;
		for (i=0; i<p; i++) {
			kill(pids[i], SIGKILL);
		}

		P(semusers);
		msgctl(server_users->client_msgid, IPC_RMID, NULL);
		msgctl(server_users->server_msgid, IPC_RMID, NULL);
		int server_msgid = server_users->server_msgid;	
		int client_msgid = server_users->client_msgid;
		V(semusers);
	
		write_log(2, client_msgid, NULL);

		unregister_server(server_msgid);
		
		semctl(semusers, 0, IPC_RMID, NULL);
		
		shmdt(server_users);
		shmctl(server_data, IPC_RMID, NULL);
	}

	return 0;
}

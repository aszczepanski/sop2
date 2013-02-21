#include "main.h"
#include "common.h"
#include "client.h"

CLIENT_REPO *client_repo;
int client_repo_data;
int sem_client_repo;

static struct sembuf buf;

void P2(int semid) {
	buf.sem_num = 0;
	buf.sem_op = -1;
	buf.sem_flg = 0;
	if (semop(semid, &buf, 1) == -1) {
		perror("Blocking semaphore");
		exit(1);
	}
}

void V2(int semid) {
        buf.sem_num = 0;
        buf.sem_op = 1;
        buf.sem_flg = 0;
        if (semop(semid, &buf, 1) == -1) {
                perror("Unblocking semaphore");
                exit(1);
        }
}

// returns 0-ok -1-failed
int get_server_list() {
	SERVER_LIST_REQUEST server_list_request;
	server_list_request.type = SERVER_LIST;

	P2(sem_client_repo);
	server_list_request.client_msgid = client_repo->client_id;
	V2(sem_client_repo);
	
	SERVER_LIST_RESPONSE server_list_response;

//	printf("CLIENT ID: %d\n", server_list_request.client_msgid);

	int all_servers_id = msgget(SERVER_LIST_MSG_KEY, 0666 | IPC_CREAT);

	P2(sem_client_repo);	
	client_repo->all_servers_id = all_servers_id;
	V2(sem_client_repo);

	int i;

	int pid, pid2, status;
	if ((pid=fork()) == 0) {
		if (msgsnd(all_servers_id, &server_list_request, sizeof(SERVER_LIST_REQUEST)-sizeof(long), 0) == -1) {
			perror("error");
			exit(0);
		}

		if (msgrcv(server_list_request.client_msgid, &server_list_response, sizeof(SERVER_LIST_RESPONSE)-sizeof(long), SERVER_LIST, 0) == -1) {
			perror("error");
			exit(0);
		}

		P2(sem_client_repo);

		client_repo->active_servers = server_list_response.active_servers;
		printf("CLIENT active servers: %d\n", client_repo->active_servers);

		puts("lp | servers | clients on servers:");
		for (i=0; i<client_repo->active_servers; i++) {
			client_repo->servers[i] = server_list_response.servers[i];
			client_repo->clients_on_servers[i] = server_list_response.clients_on_servers[i];
			printf("%d %d %d\n", i, client_repo->servers[i], client_repo->clients_on_servers[i]);
		}

		V2(sem_client_repo);

		exit(1);
	}
	else if ((pid2=fork()) == 0) {
		sleep(TIMEOUT);
		kill(pid, SIGKILL);
		exit(9);
	}

	waitpid(pid, &status, 0);
	printf("st: %d\n", WEXITSTATUS(status));

	if (WEXITSTATUS(status) == 0) {
		kill(pid, SIGKILL);
		puts("ERROR server is not responding. Try again later.");

		V2(sem_client_repo);

		return -1;
	}
	else {
		kill(pid2, SIGKILL);
		return 0;
	}
}

// returns 0 - logged in, -1 - else
int login() {
	STATUS_RESPONSE status_response;
	CLIENT_REQUEST client_request;

	int res = -1;

	client_request.type = LOGIN;

	P2(sem_client_repo);
	client_request.client_msgid = client_repo->client_id;
	V2(sem_client_repo);
	
	char name[MAX_NAME_SIZE];
	int server_id;

		puts("Enter user name:");
	
		int k=0;
		char c;
		getchar();
		while ((c=getchar()) != EOF && isprint(c) && k < MAX_NAME_SIZE-1) {
			name[k++] = c;
		}

		name[k] = '\0';

		strcpy(client_request.client_name, name);

		if (get_server_list() == 0) {
			puts("Enter server id");
			scanf("%d", &server_id);

			P2(sem_client_repo);
			printf("%d\n", client_repo->active_servers);
			V2(sem_client_repo);

			
			int pid, pid2, status;
			if ((pid=fork()) == 0) {
				if (msgsnd(server_id, &client_request, sizeof(CLIENT_REQUEST)-sizeof(long), 0) == -1) {
					perror("error");
					exit(0);
				}
			
				if (msgrcv(client_request.client_msgid, &status_response, sizeof(STATUS_RESPONSE)-sizeof(long), STATUS, 0) == -1) {
					perror("error");
					exit(0);
				}
			
				printf("status: %d\n", status_response.status);	
				exit(status_response.status);	
			}
			else if ((pid2=fork()) == 0) {
				sleep(TIMEOUT);
				kill(pid, SIGKILL);
				exit(9);
			}	
	
			waitpid(pid, &status, 0);
			if (WEXITSTATUS(status) == 0) {
				kill(pid, SIGKILL);
				puts("ERROR server is not responding. Try again later.");
			}
			else {
				kill(pid2, SIGKILL);
	
				res = WEXITSTATUS(status);
				printf("CLIENT status: %d\n", res);
	
				if (res == 201) {
					P2(sem_client_repo);
					client_repo->server_id = server_id;
					strcpy(client_repo->name, name);
					client_repo->logged = 1;
					V2(sem_client_repo);
					puts("Logged in!");
					return 0;
				}
				else if (res == 409%256) {
					puts("Name already exists!");
				}
				else if (res == 503%256) {
					puts("Server is full, try another one.");
				}
				else if (res == 400%256) {
					puts("Bad user name!");
				}
			
			}
		}
		return -1;
}

int logout() {

	CLIENT_REQUEST client_request;
	
	client_request.type = LOGOUT;

	P2(sem_client_repo);

	client_request.client_msgid = client_repo->client_id;
	strcpy(client_request.client_name, client_repo->name);
	int server_id = client_repo->server_id;
	char name[MAX_NAME_SIZE];
	strcpy(name, client_repo->name);

	V2(sem_client_repo);

	// send request
	int pid, pid2, status;
	if ((pid=fork()) == 0) {
		if (msgsnd(server_id, &client_request, sizeof(CLIENT_REQUEST)-sizeof(long), 0) != 0) {
			perror("error");
			exit(0);
		}
		exit(1);
	}
	else if((pid2=fork()) == 0) {
		sleep(TIMEOUT);
		kill(pid, SIGKILL);
		exit(9);
	}

	waitpid(pid, &status, 0);	
	if (WEXITSTATUS(status) == 0) {
		kill(pid, SIGKILL);
		puts("Server is not responding, logged out");
		return -1;
	}
	else {
		kill(pid2, SIGKILL);
		printf("CLIENT logged out %s\n", name);
		return 0;
	}
}

int get_rooms_list() {

	CLIENT_REQUEST client_request;
	ROOM_LIST_RESPONSE room_list_response;

	client_request.type = ROOM_LIST;

	P2(sem_client_repo);

	client_request.client_msgid = client_repo->client_id;
	strcpy(client_request.client_name, client_repo->name);

	int server_id = client_repo->server_id;

	V2(sem_client_repo);

	int pid, pid2, status;
	if ((pid=fork()) == 0) {
		if (msgsnd(server_id, &client_request, sizeof(CLIENT_REQUEST)-sizeof(long), 0) != 0) {
			perror("error");
			exit(0);
		}
		
		if (msgrcv(client_request.client_msgid, &room_list_response, sizeof(ROOM_LIST_RESPONSE)-sizeof(long), ROOM_LIST, 0) == -1) {
			perror("error");
			exit(0);
		}

		printf("CLIENT active rooms: %d\n", room_list_response.active_rooms);
		int i;
		for (i=0; i<room_list_response.active_rooms; i++) {
			printf("%d name: [%s] clients: %d\n", i, room_list_response.rooms[i].name, room_list_response.rooms[i].clients);
		}

		exit(1);
	}
	else if ((pid2=fork()) == 0) {
		sleep(TIMEOUT);
		kill(pid, SIGKILL);
		exit(9);
	}

	waitpid(pid, &status, 0);
	if (WEXITSTATUS(status) == 0) {
		kill(pid, SIGKILL);
		puts("ERROR server is not responding. Try again later.");
		return -1;
	}
	else {
		kill(pid2, SIGKILL);
		return 0;
	}
}

int get_client_list(int list_type) {
	
	CLIENT_REQUEST client_request;
	CLIENT_LIST_RESPONSE client_list_response;

	client_request.type = list_type;

	P2(sem_client_repo);

	client_request.client_msgid = client_repo->client_id;
	strcpy(client_request.client_name, client_repo->name);

	int server_id = client_repo->server_id;

	V2(sem_client_repo);

	int status, pid, pid2;
	if ((pid=fork()) == 0) {
		if (msgsnd(server_id, &client_request, sizeof(CLIENT_REQUEST)-sizeof(long), 0) != 0) {
			perror("error");
			exit(0);
		}

		puts("CLIENT waiting for client list");

		if (msgrcv(client_request.client_msgid, &client_list_response, sizeof(CLIENT_LIST_RESPONSE)-sizeof(long), list_type, 0) == -1) {
			perror("error");
			exit(0);
		}
	
		printf("CLIENT active clients: %d\n", client_list_response.active_clients);
		int i;
		for (i=0; i<client_list_response.active_clients; i++) {
			printf("%d name: %s\n", i, client_list_response.names[i]);
		}

		exit(1);
	}
	else if ((pid2=fork()) == 0) {
		sleep(TIMEOUT);
		kill(pid, SIGKILL);
		exit(9);
	}

	waitpid(pid, &status, 0);
	if (WEXITSTATUS(status) == 0) {
		kill(pid, SIGKILL);
		puts("ERROR is not responding. Try again later.");
		return -1;
	}
	else {
		kill(pid2, SIGKILL);
		return 0;
	}
}

int change_room() {

	STATUS_RESPONSE status_response;

	CHANGE_ROOM_REQUEST change_room_request;
	change_room_request.type = CHANGE_ROOM;

	P2(sem_client_repo);

	change_room_request.client_msgid = client_repo->client_id;
	strcpy(change_room_request.client_name, client_repo->name);

	int server_id = client_repo->server_id;

	V2(sem_client_repo);

	printf("Enter new room name:\n");

	int k=0;
	char c;
	getchar();
	while ((c=getchar()) != EOF && isprint(c) && k < MAX_NAME_SIZE-1) {
		change_room_request.room_name[k++] = c;
	}

	change_room_request.room_name[k] = '\0';
//	printf("%s\n", change_room_request.room_name);

	int pid, pid2, status;
	if ((pid=fork()) == 0) {

		if (msgsnd(server_id, &change_room_request, sizeof(CHANGE_ROOM_REQUEST)-sizeof(long), 0) != 0) {
			perror("error");
			exit(0);
		}

		if (msgrcv(change_room_request.client_msgid, &status_response, sizeof(STATUS_RESPONSE)-sizeof(long), CHANGE_ROOM, 0) == -1) {
			perror("error");
			exit(0);
		}
	
		printf("CLIENT status:%d\n", status_response.status);

		exit(1);

	}
	else if ((pid2=fork()) == 0) {
		sleep(TIMEOUT);
		kill(pid, SIGKILL);
		exit(9);
	}
	
	waitpid(pid, &status, 0);
	if (WEXITSTATUS(status) == 0) {
		kill(pid, SIGKILL);
		puts("ERROR server is not responding.");
		return -1;
	}
	else {
		kill(pid2, SIGKILL);
		return 0;
	}
}

int send_private_message() {
	
	TEXT_MESSAGE text_message;
	text_message.type = PRIVATE;

	P2(sem_client_repo);

	text_message.from_id = client_repo->client_id;
	strcpy(text_message.from_name, client_repo->name);

	int server_id = client_repo->server_id;

	V2(sem_client_repo);

	printf("Enter recipient:\n");

	int k=0;
	char c;
	getchar();
	while ((c=getchar()) != EOF && isprint(c) && k < MAX_NAME_SIZE-1) {
		text_message.to[k++] = c;
	}

	text_message.to[k] = '\0';

	text_message.time = time(0);

	printf("Enter message:\n");

	k=0;
	while ((c=getchar()) != EOF && isprint(c) && k < MAX_MSG_SIZE-1) {
		text_message.text[k++] = c;
	}

	text_message.text[k] = '\0';

	int pid, pid2, status;
	if ((pid=fork()) == 0) {
		if(msgsnd(server_id, &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), 0) != 0) {
			perror("error");
			exit(0);
		}
		exit(1);
	}
	else if((pid2=fork()) == 0) {
		sleep(TIMEOUT);
		kill(pid, SIGKILL);
		exit(9);
	}

	waitpid(pid, &status, 0);
	if (WEXITSTATUS(status) == 0) {
		kill(pid, SIGKILL);
		puts("ERROR server is not responding. Try again later.");
		return -1;
	}
	else {
		kill(pid2, SIGKILL);
		return 0;
	}
}

int send_public_message() {

	TEXT_MESSAGE text_message;
	text_message.type = PUBLIC;

	P2(sem_client_repo);

	text_message.from_id = client_repo->client_id;
	strcpy(text_message.from_name, client_repo->name);

	int server_id = client_repo->server_id;

	V2(sem_client_repo);

	strcpy(text_message.to, "");

	text_message.time = time(0);

	printf("Enter message\n");

	int k=0;
	char c;
	getchar();
	while ((c=getchar()) != EOF && isprint(c) && k < MAX_MSG_SIZE-1) {
		text_message.text[k++] = c;
	}

	text_message.text[k] = '\0';

	int pid, pid2, status;
	if ((pid=fork()) == 0) {
		if(msgsnd(server_id, &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), 0) == -1) {
			perror("error");
			exit(0);
		}
		exit(1);
	}
	else if ((pid2=fork()) == 0) {
		sleep(TIMEOUT);
		kill(pid, SIGKILL);
		exit(9);
	}

	waitpid(pid, &status, 0);
	if (WEXITSTATUS(status) == 0) {
		kill(pid, SIGKILL);
		puts("ERROR server down");
		return -1;
	}
	else {
		kill(pid2, SIGKILL);
		return 0;
	}

}

void wait_for_incomming_private_messages() {
	TEXT_MESSAGE text_message;

	P2(sem_client_repo);
	int client_id = client_repo->client_id;
	V2(sem_client_repo);

	while (1) {
		msgrcv(client_id, &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), PRIVATE, 0);

		printf("CLIENT Private message from %s to %s at %s", text_message.from_name, text_message.to, ctime(&text_message.time));
		printf("text: %s\n", text_message.text);
	}
}

void wait_for_incomming_public_messages() {
	TEXT_MESSAGE text_message;

	P2(sem_client_repo);
	int client_id = client_repo->client_id;
	V2(sem_client_repo);

	while (1) {
		msgrcv(client_id, &text_message, sizeof(TEXT_MESSAGE)-sizeof(long), PUBLIC, 0);

		printf("CLIENT Public message from %s at %s", text_message.from_name, ctime(&text_message.time));
		printf("text: %s\n", text_message.text);
	}

}

void heartbeat() {
	STATUS_RESPONSE status_response;

	CLIENT_REQUEST client_request;
	client_request.type = HEARTBEAT;

	P2(sem_client_repo);

	client_request.client_msgid = client_repo->client_id;

	V2(sem_client_repo);

	while (1) {
		if (msgrcv(client_request.client_msgid, &status_response, sizeof(STATUS_RESPONSE)-sizeof(long), HEARTBEAT, 0) == -1) {
			perror("error");
		}
		else {
//			printf("CLIENT heartbeat from %d\n", status_response.status);
	
			P2(sem_client_repo);
			int server_id = client_repo->server_id;
			strcpy(client_request.client_name, client_repo->name);
			V2(sem_client_repo);
	
//			printf("%d %d\n", server_id, status_response.status);
	
			if (status_response.status == server_id) {
//				puts("CLIENT server confirmed");
				int pid, pid2, status;
				if ((pid=fork()) == 0) {
					if (msgsnd(status_response.status, &client_request, sizeof(CLIENT_REQUEST)-sizeof(long), 0) == -1) {
						perror("error");
						exit(0);
					}
					exit(1);
				}
				else if ((pid2=fork()) == 0) {
					sleep(TIMEOUT);
					kill(pid, SIGKILL);
					exit(9);
				}

				waitpid(pid, &status, 0);
				if (WEXITSTATUS(status) == 0) {
					kill(pid, SIGKILL);
//					puts("ERROR server is not responding.");
				}
				else {
					kill(pid2, SIGKILL);
//					puts("CLIENT hearbeat response sent");
				}
			}
		}
	}
}

void show_help() {
	puts("");
	puts("Type one of the following words");
	puts("\"help\" - help");
	puts("\"login\" - login");
	puts("\"logout\" - logout");
	puts("\"get_server_list\" - get servers list");
	puts("\"change_room\" - change room");
	puts("\"rooms\" - get rooms list");
	puts("\"room_clients\" - get room clients");
	puts("\"global_clients\" - get all clients list");
	puts("\"private\" - send private message");
	puts("\"public\" - send public message");
	puts("\"exit\" - exit");
	puts("");
}

int client() {
	
	puts("You are currenty running CLIENT program");

	sem_client_repo = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);
	semctl(sem_client_repo, 0, SETVAL, 0);

	client_repo_data = shmget(IPC_PRIVATE, sizeof(CLIENT_REPO), 0600 | IPC_CREAT);
	client_repo = (CLIENT_REPO*)shmat(client_repo_data, NULL, 0);

	client_repo->client_id = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
	client_repo->all_servers_id = msgget(SERVER_LIST_MSG_KEY, 0666 | IPC_CREAT);

	V2(sem_client_repo);

	pid_t pids[MAX_FORKS];
	int p=0;

	if ((pids[p++] = fork()) == 0) {
		//incoming private messages
		wait_for_incomming_private_messages();
	}

	else if ((pids[p++] = fork()) == 0) {
		//incoming public messages
		wait_for_incomming_public_messages();
	}
	else if ((pids[p++] = fork()) == 0) {
		// heartbeat
		heartbeat();
	}		

	else {

		char input[MAX_TEXT_SIZE];

		int end = 0;
		while (!end) {
			printf("What up?\n");
			scanf("%s", input);
			
			if (strcmp(input, "get_server_list") == 0) {
				puts("CLIENT getting server list");
				get_server_list();
			}
			else if (strcmp(input, "help") == 0) {
				puts("CLIENT help");
				show_help();
			}
			else if (strcmp(input, "login") == 0) {
				int inend = 1;
	
				printf("CLIENT logging in\n");
				if (login() == 0) inend = 0;
				
				while (!inend) {
					printf("What up l?\n");
					scanf("%s", input);
	
					if (strcmp(input, "logout") == 0) {
						printf("CLIENT logging out\n");
						logout();
						inend = 1;
					}
					else if (strcmp(input, "rooms") == 0) {
						printf("CLIENT getting rooms list\n");
						if (get_rooms_list() == -1) {
							puts("CLIENT forced log out");
							inend = 1;
						}
					}
					else if (strcmp(input, "room_clients") == 0) {
						printf("CLIENT gettin room clients list\n");
						if (get_client_list(ROOM_CLIENT_LIST) == -1) {
							puts("CLIENT forced log out");
							inend = 1;
						}
					}
					else if (strcmp(input, "global_clients") == 0) {
						printf("CLIENT getting global clients list\n");
						if (get_client_list(GLOBAL_CLIENT_LIST) == -1) {
							puts("CLIENT forced log out");
							inend = 1;
						}
					}	
					else if (strcmp(input, "change_room") == 0) {
						printf("CLIENT changing room\n");
						if (change_room() == -1) {
							puts("CLIENT forced log out");
							inend = 1;
						}
					}
					else if (strcmp(input, "get_server_list") == 0) {
						if (get_server_list() == -1) {
							puts("CLIENT forced log out");
							inend = 1;
						}
					}
					else if (strcmp(input, "private") == 0) {
						printf("CLIENT private message\n");
						if (send_private_message() == -1) {
							puts("CLIENT forced log out");
							inend = 1;
						}
					}
					else if (strcmp(input, "public") == 0) {
						printf("CLIENT public message\n");
						if (send_public_message() == -1) {
							puts("CLIENT forced log out");
							inend = 1;
						}
					}
					else if (strcmp(input, "help") == 0) {
						puts("CLIENT help");
						show_help();
					}

				}
	
			}
			else if (strcmp(input, "exit") == 0) {
				puts("equals exit");
				end = 1;
			}

		}	

		int i;
		for (i=0; i<p; i++) {
			kill(pids[i], SIGKILL);
		}
	
		msgctl(client_repo->client_id, IPC_RMID, NULL);
		shmctl(client_repo_data, IPC_RMID, NULL);
		semctl(sem_client_repo, 0, IPC_RMID, NULL);
	}

	return 0;
}

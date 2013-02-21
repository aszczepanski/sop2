#include "client.h"
#include "server.h"
#include "main.h"

int main(int argc, char *argv[]) {

	puts("Welcome to the Adam's chat! :)");

	int opt;
	opterr = 0;

	while ((opt = getopt(argc, argv, "cs")) != -1){
		switch (opt) {
		case 'c':
			client();
			return 0;
		case 's':
			server();
			return 0;
		case '?':
			puts("bad opt");
			break;
		}
	}

	puts("Try using -c for client program or -s for server program");

	return 0;
}

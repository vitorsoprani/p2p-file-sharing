/*
 * File: client.c
 * Autor: Vitor s Passamani (vitor.spassamani@gmail.com)
 *
 * Implementação do Peer (Cliente)
 */

#include "../include/net_utils.h"
#include "../include/protocol.h"
#include "../include/uthash.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <time.h> /* Necessário para srand() e time() */

#define TRACKER_IP "localhost"
#define TRACKER_PORT "4242"
#define HEARTBEAT_INTERVAL 10

uint16_t my_listen_port;

/* Número mágico exclusivo desta instância do cliente */
int client_magic_number;

/* Estruturas da uthash.h */

struct peer_key {
	struct sockaddr_storage addr;
	int magic_number;
};

struct known_peer {
	struct peer_key key;
	UT_hash_handle hh;
};

struct known_peer *peers_hash = NULL;
pthread_mutex_t peers_hash_mutex = PTHREAD_MUTEX_INITIALIZER;



void print_known_peers(void)
{
	printf("\n[Client Info] Active peers list: \n");

	pthread_mutex_lock(&peers_hash_mutex);
	uint32_t count = HASH_COUNT(peers_hash);

	if (count == 0) {
		printf("\t No active peer on the network.\n");
	} else {
		struct known_peer *cur, *tmp;
		HASH_ITER(hh, peers_hash, cur, tmp) {
			printf(" - ");
			print_sockaddr_storage(&(cur->key.addr));
		}
	}
	pthread_mutex_unlock(&peers_hash_mutex);
}

void add_peer_to_hash(struct sockaddr_storage *new_addr)
{
	struct known_peer *p = NULL;
	struct peer_key search_key;

	memset(&search_key, 0, sizeof(struct peer_key));
	memcpy(&search_key.addr, new_addr, sizeof(struct sockaddr_storage));
	search_key.magic_number = client_magic_number;

	pthread_mutex_lock(&peers_hash_mutex);

	HASH_FIND(hh, peers_hash, &search_key, sizeof(struct peer_key), p);

	if (p == NULL) {
		p = malloc(sizeof(struct known_peer));
		memset(p, 0, sizeof(struct known_peer));
		p->key = search_key;

		HASH_ADD(hh, peers_hash, key, sizeof(struct peer_key), p);
	}

	pthread_mutex_unlock(&peers_hash_mutex);
}


/* * announce_to_tracker
 * get_peers_list: 1 para transferir a lista completa (inicialização).
 * 0 para apenas enviar o status e fechar a ligação (heartbeat).
 */
void announce_to_tracker(int get_peers_list)
{
	struct addrinfo *res = resolve_tcp_address(TRACKER_IP, TRACKER_PORT, 0);
	if (res == NULL) {
		fprintf(stderr, "[Client Err] Couldnt resolve tracker addr.\n");
		return;
	}

	int tracker_fd = connect_to_address(res, NULL);
	freeaddrinfo(res);

	if (tracker_fd < 0) {
		fprintf(stderr, "[Client Err] Couldnt connect to tracker\n");
		return;
	}

	announce_msg_t msg;
	msg.listen_port = my_listen_port;

	if (send_data(tracker_fd, (char *)&msg, sizeof(msg)) < 0) {
		close(tracker_fd);
		return;
	}

	/* Se não precisamos da lista, fechamos o socket imediatamente */
	if (get_peers_list == 0) {
		close(tracker_fd);
		return;
	}

	tracker_resp_header_t header;
	if (receive_data(tracker_fd, (char *)&header, sizeof(header)) <= 0) {
		close(tracker_fd);
		return;
	}

	if (header.peer_count > 0) {
		size_t list_bytes = header.peer_count * sizeof(wire_peer_t);
		wire_peer_t *peer_array = malloc(list_bytes);

		int bytes_recvd = recv(tracker_fd, peer_array, list_bytes, MSG_WAITALL);

		if (bytes_recvd == (int)list_bytes) {
			/*
			 * TODO: shufle no array para que cada peer tenha uma
			 * ordem diferente na lista da uthash
			 */
			for (uint32_t i = 0; i < header.peer_count; i++) {
				add_peer_to_hash(&(peer_array[i].addr));
			}
		} else {
			fprintf(stderr, "[Client Err] Falha ao receber a lista completa de peers.\n");
		}

		free(peer_array);
	}

	close(tracker_fd);
}

void *heartbeat_thread(void *arg)
{
	(void)arg;
	while (1) {
		sleep(HEARTBEAT_INTERVAL);
		printf("[Client Info] sending heartbeat...\n");
		announce_to_tracker(0);
	}
	return NULL;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Uso: %s <porta_p2p_do_cliente>\n", argv[0]);
		return EXIT_FAILURE;
	}

	my_listen_port = (uint16_t)atoi(argv[1]);

	srand(time(NULL));
	client_magic_number = rand();

	printf("[Client Info] Starting client...\n");
	printf("[Client Info] Magic Number: %d\n", client_magic_number);

	printf("[Client Info] Contacting the tracker...\n");
	announce_to_tracker(1);
	print_known_peers();

	pthread_t hb_thread;
	if (pthread_create(&hb_thread, NULL, heartbeat_thread, NULL) != 0) {
		fprintf(stderr, "[Client Err] (main) pthread_create error\n");
		return EXIT_FAILURE;
	}

	pthread_join(hb_thread, NULL);

	return EXIT_SUCCESS;
}

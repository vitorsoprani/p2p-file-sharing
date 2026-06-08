/*
 * File: tracker.c
 * Autor: Vitor s Passamani (vitor.spassamani@gmail.com)
 *
 * Implementação do Tracker Server, responsável por manter e fornecer uma lista
 * com todos os peers participando da rede p2p.
 *
 * O tracker deve receber um heartbeat de cada peer com um período menor que 20
 * segundos, caso contrário será considerado morto e excluído da lista.
 */

#include "../include/net_utils.h"
#include "../include/protocol.h"
#include "../include/uthash.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#define HEARTBEAT_TIMEOUT 20 /* Tempo (s) esperado antes de limpar a lista */
#define PORT "4242" /* Porta na qual o tracker escutará por conexões */
#define BACKLOG 10 /* Tamanho máximo da fila de conexões */
#define TRACKER_MAGIC_NUMBER 0x4242 /* Usado na chave do peer para a hash */

/* Estruturas da uthash */
struct peer_key{
	struct sockaddr_storage addr;
	int magic_number;
};

struct peer_info{
	struct peer_key key;
	int is_alive;
	UT_hash_handle hh; /* Magia negra da uthash.h */
};

struct peer_info *peers_hash = NULL;
pthread_mutex_t peers_hash_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Thread que percorre a hash inteira eliminando os peers mortos, i.e os peers
 * que possuem is_alive == 0.
 * O período entre cada checagem é definido em HEARTBEAT_TIMEOUT.
 */
void *reap_dead_peers(void *arg)
{
	while (1) {
		sleep(HEARTBEAT_TIMEOUT);
		printf("[Tracker Info] Reaping dead peers...\n");

		/* INÍCIO SEÇÃO CRÍTICA */
		pthread_mutex_lock(&peers_hash_mutex);

		struct peer_info *curr_peer, *tmp;
		HASH_ITER(hh, peers_hash, curr_peer, tmp) {
			if (curr_peer->is_alive == 1) {
				curr_peer->is_alive = 0;
			} else {
				printf("[Tracker Info] Removing inactive peer: ");
				HASH_DEL(peers_hash, curr_peer);
				free(curr_peer);
			}
		}

		pthread_mutex_unlock(&peers_hash_mutex);
		/* FIM DA SEÇÃO CRÍTICA */
	}
	return NULL;
}

int main(int argc, char **argv)
{
	int sockfd = -1;
	struct addrinfo *res =  NULL;
	struct addrinfo *bound_info = NULL;
	pthread_t reaper_thread;

	printf("[Tracker Info] Starting tracker...\n");

	res = resolve_tcp_address(NULL, PORT, 1);
	if (res == NULL)
		return EXIT_FAILURE;

	sockfd = bind_to_address(res, &bound_info);
	if (sockfd < 0) {
		freeaddrinfo(res);
		return EXIT_FAILURE;
	}

	printf("[Tracker Info] Tracker up and listenning on: ");
	print_addrinfo(bound_info);
	freeaddrinfo(res);

	start_listening(sockfd, BACKLOG);

	if (pthread_create(&reaper_thread, NULL, reap_dead_peers, NULL) != 0) {
		fprintf(stderr, "[Err] (main) pthread_create error");
		return EXIT_FAILURE;
	}


	while (1) {
		struct sockaddr_storage client_addr;
		int client_sockfd = accept_peer(sockfd, &client_addr);
		if (client_sockfd < 0) {
			continue;
		}
		printf("[Tracker Info] Connection received...\n");

		announce_msg_t msg;
		if (recv(client_sockfd, &msg, sizeof(msg), MSG_WAITALL) < sizeof(msg)) {
			close(client_sockfd);
			continue;
		}

		struct peer_key key;
		memset(&key, 0, sizeof(struct peer_key)); /* OBRIGATÓRIO para o funcionamento correto da uthash */
		key.magic_number = TRACKER_MAGIC_NUMBER;

		memcpy(&key.addr, &client_addr, sizeof(struct sockaddr_storage));
		if (key.addr.ss_family == AF_INET) {
			/* Preciso usar htons pois key.addr será usada para comunicações... */
			((struct sockaddr_in *)&key.addr)->sin_port = htons(msg.listen_port);
		} else {
			((struct sockaddr_in6 *)&key.addr)->sin6_port = htons(msg.listen_port);
		}

		/* INICIO DA SEÇÃO CRÍTICA */
		pthread_mutex_lock(&peers_hash_mutex);

		struct peer_info *p = NULL;
		HASH_FIND(hh, peers_hash, &key, sizeof(struct peer_key), p);

		if (p == NULL) {
			/* Peer ainda não existe */
			p = malloc(sizeof(struct peer_info));
			memset(p, 0, sizeof(struct peer_info));
			p->key = key;
			p->is_alive = 1;
			HASH_ADD(hh, peers_hash, key, sizeof(struct peer_key), p);
			printf("[Tracker] Novo peer adicionado à swarm.\n");
		} else {
			/* Peer ja existe, é um  heartbeat announce */
			p->is_alive = 1;
		}

		/* Prepara a lista de resposta ANTES de liberar o lock */
		/* APENAS o sockaddr_storage são enviados */
		uint32_t count = HASH_COUNT(peers_hash);
		wire_peer_t *peer_array = NULL;

		if (count > 0) {
			peer_array = malloc(count * sizeof(wire_peer_t));
			struct peer_info *cur, *tmp;
			int i = 0;
			HASH_ITER(hh, peers_hash, cur, tmp) {
				memcpy(&peer_array[i].addr, &(cur->key.addr), sizeof(struct sockaddr_storage));
				i++;
			}
		}

		/* FIM DA SEÇÃO CRÍTICA */
		pthread_mutex_unlock(&peers_hash_mutex);

		tracker_resp_header_t header;
		header.peer_count = count;

		if (send(client_sockfd, &header, sizeof(header), MSG_NOSIGNAL) < 0) {
			if (errno == EPIPE) {
				/* cliente ja fechou o pipe, era um heartbeat */
			} else {
				perror("[Tracker Err] (main) send error");
			}
		} else if (count > 0) {
			if (send(client_sockfd, peer_array, count * sizeof(wire_peer_t), MSG_NOSIGNAL) < 0) {
				if (errno == EPIPE) {
					 /* Peer perdeu o interesse ou fechou antecipadamente. Normal. */
				} else {
					perror("[Tracker Err] (main) send error");
				}
			}
		}

		if (peer_array) {
		    free(peer_array);
		}
		close(client_sockfd);
	}

	return EXIT_SUCCESS;
}


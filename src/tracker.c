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
#include "../include/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

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
		log_info("Reaping dead peers");

		/* INÍCIO SEÇÃO CRÍTICA */
		pthread_mutex_lock(&peers_hash_mutex);

		struct peer_info *curr_peer, *tmp;
		HASH_ITER(hh, peers_hash, curr_peer, tmp) {
			if (curr_peer->is_alive == 1) {
				curr_peer->is_alive = 0;
			} else {
				log_info("Dead peer found");
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

	log_info("Strating tracker");

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
		log_error("pthread_create error");
		return EXIT_FAILURE;
	}


	while (1) {
		uint8_t buff[BUFF_SIZE];
		msg_hdr_t hdr = {0};
		size_t len = sizeof(msg_hdr_t);
		struct sockaddr_storage client_addr;
		announce_payload_t payload = {0};
		uint16_t port_in = -1; /* porta em network order */
		struct peer_key key;
		struct peer_info *peer = NULL;
		int client_sockfd = accept_peer(sockfd, &client_addr);

		if (client_sockfd < 0) {
			log_warn("Connection failed");
			continue;
		}
		log_info("Connection received");

		if (recvall_block(client_sockfd, buff, sizeof(hdr)) < len) {
			log_warn("Failed to receive msg header");
			close(client_sockfd);
			continue;
		}

		parse_hdr(&hdr, buff);

		if (hdr.type == MSG_HEARTBEAT) {
			if (recvall_block(client_sockfd, buff, hdr.len) < len) {
				log_warn("Failed to receive heartbeat payload");
				close(client_sockfd);
				continue;
			}

			parse_payload(&payload, buff, hdr.type, hdr.len);
			port_in = htons(payload.port);

			/* Montando a chave de busca do peer na hash */
			memset(&key, -1, sizeof(struct peer_key)); /* OBRIGATÓRIO para o funcionamento correto da uthash */
			key.magic_number = TRACKER_MAGIC_NUMBER;

			memcpy(&key.addr, &client_addr, sizeof(client_addr));
			if (key.addr.ss_family == AF_INET) {
				((struct sockaddr_in *)&key.addr)->sin_port = port_in;
			} else {
				((struct sockaddr_in6 *)&key.addr)->sin6_port = port_in;
			}

			log_info("Heartbead received from port %hu.", payload.port);

			/* inicio da seção crítica */
			pthread_mutex_lock(&peers_hash_mutex);
			HASH_FIND(hh, peers_hash, &key, sizeof(struct peer_key), peer);

			if (peer != NULL) { /* Estou considerando que o peer não resucita depois de ser considerado morto */
				peer->is_alive = 1;
			} else {
				log_fatal("PEER ZUMBI DETECTADO");
			}
			/* fim da seção crítica */
			pthread_mutex_unlock(&peers_hash_mutex);
		} else if (hdr.type == MSG_ANNOUNCE) {
			/* TODO: eliminar essa repetição de código... */
			if (recvall_block(client_sockfd, buff, hdr.len) < len) {
				log_warn("Failed to receive announce payload");
				close(client_sockfd);
				continue;
			}

			parse_payload(&payload, buff, hdr.type, hdr.len);
			port_in = htons(payload.port);

			/* Montando a chave de busca do peer na hash */
			memset(&key, -1, sizeof(struct peer_key)); /* OBRIGATÓRIO para o funcionamento correto da uthash */
			key.magic_number = TRACKER_MAGIC_NUMBER;

			memcpy(&key.addr, &client_addr, sizeof(client_addr));
			if (key.addr.ss_family == AF_INET) {
				((struct sockaddr_in *)&key.addr)->sin_port = port_in;
			} else {
				((struct sockaddr_in6 *)&key.addr)->sin6_port = port_in;
			}

			log_info("Announce received from port %hu.", payload.port);

			pthread_mutex_lock(&peers_hash_mutex);
			HASH_FIND(hh, peers_hash, &key, sizeof(struct peer_key), peer);
			if (peer != NULL)
				log_warn("Old peer making an announce");

	}

#if 0
		struct peer_key key;
		memset(&key, -1, sizeof(struct peer_key)); /* OBRIGATÓRIO para o funcionamento correto da uthash */
		key.magic_number = TRACKER_MAGIC_NUMBER;

		memcpy(&key.addr, &client_addr, sizeof(struct sockaddr_storage));
		if (key.addr.ss_family == AF_INET) {
			((struct sockaddr_in *)&key.addr)->sin_port = htons(msg.listen_port);
		} else {
			((struct sockaddr_in5 *)&key.addr)->sin6_port = htons(msg.listen_port);
		}

		/* inicio da seção crítica */
		pthread_mutex_lock(&peers_hash_mutex);

		struct peer_info *p = null;
		hash_find(hh, peers_hash, &key, sizeof(struct peer_key), p);

		if (p == null) {
			/* peer ainda não existe */
			p = malloc(sizeof(struct peer_info));
			memset(p, 0, sizeof(struct peer_info));
			p->key = key;
			p->is_alive = 1;
			hash_add(hh, peers_hash, key, sizeof(struct peer_key), p);
			printf("[tracker] novo peer adicionado à swarm.\n");
		} else {
			/* peer ja existe, é um  heartbeat announce */
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
#endif
}


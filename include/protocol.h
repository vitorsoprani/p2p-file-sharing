/*
 * File: include/protocol.h
 * Autor: Vitor s Passamani (vitor.spassamani@gmail.com)
 * Estruturas compartilhadas entre Cliente e Tracker para a comunicação via rede.
 */
#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <stdint.h>
#include <sys/socket.h>

#define MSG_HANDSHAKE	1
#define MSG_BITFIELD	2
#define MSG_HAVE	3
#define MSG_REQUEST	4
#define MSG_PIECE	5
#define MSG_ANNOUNCE	6
#define MSG_PEERS	7
#define MSG_HEARTBEAT	8

typedef struct {
	uint8_t type;
	uint32_t len;
} msg_hdr_t;

typedef struct {
	uint16_t port;
} handshake_payload_t;

typedef struct {
	uint8_t *bits;
} bitfield_payload_t;

typedef struct {
	uint32_t index;
} have_payload_t;

typedef struct {
	uint32_t index;
} request_payload_t;

typedef struct {
	uint32_t index;
	uint8_t *piece;
} piece_payload_t;

typedef struct {
	uint16_t port;
} announce_payload_t;

typedef struct {
	struct sockaddr_storage *peers;
} peers_payload_t;


/* TODO: Implementar as funçõe snecessárias para interação peer-tracker e
 * descoberta de novos peers;
 *
 * [] - Peer faz announce para tracker
 * [] - Peer recebe lista do tracker
 * [] - Peer envia heartbeats para o tracker
 *
 * [] - Peer se conecta com os pares recebidos
 * [] - Peer que já estava no loop do epoll aceita novo par
 *
 */

#endif /* _PROTOCOL_H */

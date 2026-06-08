/*
 * File: include/protocol.h
 * Autor: Vitor s Passamani (vitor.spassamani@gmail.com)
 * Estruturas compartilhadas entre Cliente e Tracker para a comunicação via rede.
 */
#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <stdint.h>
#include <sys/socket.h>

/* O que o peer envia para o tracker ao conectar (announce) */
typedef struct {
/*
 * O tracker sabe o IP do peer através do accept(), mas a porta que o accept()
 * retorna é a porta EFÊMERA (aleatória). O peer precisa informar ao tracker
 * em qual porta ele está escutando como servidor para os outros peers.
 */
	uint16_t listen_port;
} announce_msg_t;

/* Cabeçalho de resposta do tracker para o peer */
typedef struct {
	uint32_t peer_count; /* Número de peers na lista */
} tracker_resp_header_t;

/* Estrutura de cada peer enviada na lista */
typedef struct {
	struct sockaddr_storage addr; /* Endereço e porta do peer */
} wire_peer_t;

#endif /* _PROTOCOL_H */

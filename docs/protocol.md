# Especificação do protocolo FitTorrent

## Visão geral
O protocolo FitTorrent opera sobre o protocolo TCP. Por ser um protocolo orientado a *streams*, é usado um modelo de mensagens prefixadas por comprimento, i.e, toda mensgem é enviada da seguinte forma:

```
<MSG_HEADER> <MSG_PAYLOAD>
```

Onde `MSG_HEADER` indica o tipo da mensagem e o comprimento, em bytes, do `MSG_PAYLOAD`. Os clientes conhecem todos os tipos de *payloads* existentes no protocolo, então sabem lidar com as leituras em streams não bloqueantes para cada mensgaem.

### Convenção de ordem de bytes
Para garantir a interoperabilidade entre diferentes arquiteturas de processadores, todos os campos numéricos (inteiros de 16 e 32 bits) devem ser transmitidos em *Network Byte Order* (*Big-Endian*). As implementações devem utilizar funções como `htonl()`, `htons()`, `ntohl()` e `ntohs()` para codificação e decodificação.

## Estrutura Base da Mensagem
Toda mensagem transmitida na rede FitTorrent (seja entre Peers ou entre Peer e Tracker) DEVE iniciar com um cabeçalho padrão de 5 bytes.

```c
typedef struct {
	uint8_t  type;       /* Código de Operação (Opcode) da mensagem */
	uint32_t len;     /* Tamanho do payload em bytes (Network Byte Order) */
} msg_hdr_t;
```

O receptor deve ler exatamente 5 bytes do socket TCP para identificar a mensagem. Se `len > 0`, o receptor deve continuar lendo o fluxo até que exatamente `len` bytes tenham sido consumidos, formando assim o payload.

## Tipos de Mensagem
Os seguintes Opcodes são definidos pelo protocolo:

| **Opcode** | **Constante** | **Categoria** | **Payload **  |
|:----------:|:-------------:|:-------------:|:-------------:|
| 1          |MSG\_HANDSHAKE | P2P           | 2 bytes       |
| 2          |MSG\_BITFIELD  | P2P           | variável      |
| 3          |MSG\_HAVE      | P2P           | 4 bytes       |
| 4          |MSG\_REQUEST   | P2P           | 4 bytes       |
| 5          |MSG\_PIECE     | P2P           | 4 + block_len |
| 6          |MSG\_ANNOUNCE  | Tracker       | 2 bytes       |
| 7          |MSG\_PEERS     | Tracker       | variável      |
| 8          |MSG\_HEARTBEAT | Tracker       | 0 bytes       |

## Protocolo Peer-to-Tracker
A comunicação com o Tracker serve exclusivamente para descoberta de nós. O Tracker não participa da transferência de arquivos.

### MSG\_ANNOUNCE
- **Direção**: Peer -> Tracker
- **Descrição**: Enviada por um Peer assim que entra na rede para registrar sua presença.
- **Payload**: Inteiro sem sinal de 16 bits (`uint16_t`) representando a porta TCP onde o Peer atuará como servidor (escutando por conexões de outros peers). O IP é inferido pelo Tracker.


### MSG\_PEERS
- **Direção**: Tracker -> Peer
- **Descrição**: Resposta imediata do Tracker após um `MSG_ANNOUNCE`. Contém a lista de nós ativos.
- **Payload**: Um array de estruturas binárias representando os pares. O número de pares é calculado por `(header.length / sizeof(struct sockaddr_storage))`

### MSG\_HEARTBEAT
- **Direção**: Peer -> Tracker
- **Descrição**: Sinal de vida periódico enviado pelo Peer para evitar que o Tracker o remova por inatividade.
- **Payload**: Vazio. O Tracker não envia resposta. O socket TCP pode ser fechado pelo Peer imediatamente após o envio para economizar recursos.

## Protocolo Peer-to-Peer
Esta seção define as mensagens trocadas entre dois Peers conectados.

### MSG\_HANDSHAKE
- **Descrição**: Primeira mensagem obrigatória ao estabelecer uma conexão TCP com outro Peer.
- **Payload (2 bytes)**: A porta de escuta P2P (`uint16_t`) do nó remetente.
- **Regra de Fluxo**:
	1. O Nó Iniciador envia `MSG_HANDSHAKE`.
	2. O Nó Receptor aceita a conexão, mapeia o IP (via socket) e a porta (via payload) do Iniciador e responde enviando `MSG_HANDSHAKE`.

### MSG\_BITFIELD
- **Descrição**: Informa o estado atual do arquivo (quais blocos/pieces o nó possui).
- **Payload**: Um array de bytes onde cada bit representa o status de uma peça. (Ex: O bit 0 do byte 0 representa a peça 0. Se o bit for 1, o peer possui a peça).
- **Regras**:
	* O tamanho do payload ditará quantos bytes o bitfield possui. (Ex: um arquivo com 32 blocos terá um payload de 4 bytes).
	* DEVE ser enviada imediatamente após a troca de `MSG_HANDSHAKE`. Se o peer não possui nenhum bloco (é um novo leecher), ele pode enviar um `MSG_BITFIELD` preenchido de zeros.

### MSG\_HAVE
- **Descrição**: Notifica os vizinhos conectados de que o peer acabou de baixar e validar com sucesso uma nova peça.
- **Payload (4 bytes)**: Índice da peça adquirida (`uint32_t`).
- **Ação do Receptor**: O receptor deve atualizar o bitfield associado a este remetente em sua memória local, registrando que agora o vizinho é uma possível fonte para este pedaço.

### MSG\_REQUEST
- **Descrição**: Solicita o envio dos dados reais de um pedaço.
- **Payload**: Índice da peça desejada (`uint32_t`).
- **Regras**:
	* Um Peer SÓ DEVE enviar esta mensagem se, consultando o bitfield em memória, tiver certeza de que o vizinho possui a peça solicitada.
	* O receptor responde de forma assíncrona com `MSG_PIECE`.

### MSG\_PIECE
- **Descrição**: Carrega os dados brutos de um bloco do arquivo. Resposta direta a um `MSG_REQUEST`.
- **Payload**: Um inteiro (`uint32_t`) indicando o índice do pedaço seguido de um vetor de bytes brutos aue compõem o pedaço (`uint8_t *`).

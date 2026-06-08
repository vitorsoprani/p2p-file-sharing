# Compartilhamento de arquivos p2p

## Descrição
Este projeto consiste no desenvolvimento do MVP de uma rede distribuída Peer-to-Peer (P2P) inspirada no protocolo BitTorrent. O principal desafio abordado foi mapear de forma síncrona a entrada, permanência e saída de múltiplos nós ativos na rede (*swarm*), contornando condições de corrida por meio de exclusão mútua (*mutex*) e reduzindo o *overhead* de rede na transmissão de sinais de vida (*heartbeats*) utilizando o comportamento nativo das conexões TCP (tratamento de `EPIPE` e `MSG_NOSIGNAL`).

## Tecnologias Utilizadas
- **Linguagem de programação utilizada:** C (Padrão ISO C99 / POSIX)
- **Bibliotecas/Frameworks utilizados:** Socket API nativa do Linux (`sys/socket.h`, `netdb.h`), POSIX Threads (`pthread.h`) e a biblioteca de macros `uthash.h` (para indexação e busca em tempo constante $O(1)$ dos nós em memória).

## Como Executar

### Requisitos
- Ambiente baseado em Unix/Linux.
- Compilador `gcc`.
- Ferramenta de automação `make`.

### Instruções de Execução
OBS: Nesse estágio do desenvolvimento o programa foi desenvolvido para que todos os processos (tracker e clientes) rodem localmente. Há algumas definições *hard coded*, como o fato dos clientes considerarem que o host do tracker é `localhost`.

1. **Clone o repositório:**

```bash
git clone <URL_DO_REPOSITORIO>
cd <DIRETORIO_DO_PROJETO>
```

2. Instale as dependências
Como o projeto foi construído utilizando apenas as APIs nativas do padrão POSIX C e incluindo a biblioteca de cabeçalho único `uthash.h` localmente, não há necessidade de gerenciadores de pacotes externos. A preparação e compilação dos binários é feita via:

```bash
make clean && make
```

3. Execute o servidor:
Inicie o processo do Tracker central, responsável por orquestrar a descoberta da malha de nós ativos. Por padrão, ele escutará na porta `4242`:

```bash
./bin/server
```

4. Execute o cliente:
Inicie uma ou mais instâncias do Peer cliente. É obrigatório passar a porta que este nó usará para aceitar conexões P2P no futuro como argumento:

```bash
# Terminal 2: Executa o primeiro nó na porta 8000
./bin/client 8000

# Terminal 3: Executa o segundo nó na porta 8001
./bin/client 8001
```

## Como Testar
1. **Validação do Registro Inicial**: Abra o Tracker (`./bin/server`). Em outro terminal, abra o primeiro cliente (`./bin/client 8000`). O cliente se registrará no tracker, baixará a lista contendo apenas ele mesmo e exibirá o log `[Client Info]`.

2. **Validação do Peer Discovery (Descoberta)**: Em um terceiro terminal, abra o segundo cliente (`./bin/client 8001`). O nó irá anunciar sua porta P2P, fará o download da lista do tracker e identificará o nó `8000`.

3. **Validação do Heartbeat**: Deixe os clientes rodando. A cada 10 segundos você observará o log indicando que o cliente está enviando o heartbeat. O cliente envia o anúncio e fecha o socket imediatamente. O tracker identificará o fechamento rápido via erro `EPIPE`, mantendo o nó marcado como vivo sem trafegar dados redundantes de download.

4. **Validação da Atualização Dinâmica (Tolerância a Falhas)**: Encerre o cliente da porta `8001` (`Ctrl+C`). Após 20 segundos sem receber pings (limiar do `HEARTBEAT_TIMEOUT`), o Tracker disparará de forma autônoma a sua thread coletora de lixo, removendo o nó morto da memória e imprimindo `[Tracker Info] Reaping dead peers...`.

## Funcionalidades Implementadas
- **Descoberta de pares na rede (P2P Discovery)**: Mecanismo onde novos nós obtêm endereços de nós vizinhos ativamente no momento em que entram na rede.
- **Atualização dinâmica da lista de nós ativos**: Thread de limpeza assíncrona (reaper thread) rodando no servidor que monitora a inatividade de peers e atualiza a malha em tempo real.
- **Otimização de Sinal de Vida (Heartbeat Otimizado)**: Uso combinado de `MSG_NOSIGNAL` e captura de `EPIPE` para permitir pings de baixo consumo de banda.
- **Chaves de Hash com Proteção e Entropia**: Uso de `memset` nas chaves para evitar lixo de memória (`padding`) do C e injeção de `magic_number` aleatório para garantir que a distribuição dos peers na hash de cada cliente seja diferente.

## Próximos passos
- **Solicitação e envio de arquivos entre pares**: Desenvolvimento final das threads locais de Upload (Servidora) e Download (Cliente) utilizando o protocolo de pacotes prefixados por comprimento (Length-Prefixed Framing; `MSG_BITFIELD`, `MSG_REQUEST`, `MSG_PIECE`).
- **Componente de I/O Concorrente Baseado em Pedaços**: Implementação de chamadas de sistema thread-safe nos clientes para gravação direta em blocos específicos do arquivo.
- **Incremento de Desempenho**: Cache em memória RAM volátil para reter os blocos mais requisitados pelos vizinhos.
- **Incremento de Segurança**: Integração de rotinas de validação criptográfica (hashing SHA) para assinar digitalmente e checar a integridade de cada bloco baixado antes de consolidá-lo no arquivo final.





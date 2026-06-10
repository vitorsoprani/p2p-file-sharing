# Descrição do processo cliente do FitTorrent
O processo é composto por 2 threads, uma thread principal e uma thread responsável por enviar heartbeats para o tracker. O cliclo de via de um processo cliente no FitTorrent é composto por basicamente 2 momentos: a Inicialização e o Runtime.

## Inicialização
Nesse momento o cliente se cadastra no tracker e se conecta com os peers retornados:

1. O cliente lê o arquivo de metainfo, armazena as informações sobre o arquivo e sobre o tracker.
2. O cliente checa em seu sistema de arquivos local para ver se ele já possui alguma parte do arquivo (de alguma tentativa de dowload anterior) ou o arquivo inteiro (ele será um seeder). Caso o arquivo exista, obrigatóriamente existirá também um arquivo no diretório `.torrent` chamado `bitfield`, esse arquivo possui as informações sobre o arquivo préviamente baixado.
3. O cliente envia um `MSG_ANNOUNCE` para o tracker, indicando qual porta ele deixará aberta para escutar por novos peers.
4. O tracker envia um `MSG_PEERS` para o tracker.

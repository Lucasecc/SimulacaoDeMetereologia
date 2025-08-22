===============================================
   SIMULADOR DE ESTAÇÃO METEOROLÓGICA EM C
===============================================

Este projeto é uma simulação robusta de um sistema de processamento de dados climáticos em tempo real, desenvolvido em C. Utilizando programação concorrente com Pthreads, o simulador modela um pipeline completo desde a coleta de dados por sensores até a transmissão para centros meteorológicos, implementando mecanismos de sincronização avançados e funcionalidades de simulação realística.


FUNCIONALIDADES
---------------

O simulador foi construído de forma incremental e agora possui um conjunto completo de funcionalidades profissionais:

- Arquitetura Multithread:
  - Sensores: Threads que coletam dados continuamente.
  - Processadores: Threads que validam os dados coletados.
  - Transmissores: Threads que enviam os dados processados para destinos finais.

- Padrão Produtor-Consumidor:
  - Uso de Buffers Circulares para comunicação segura e eficiente entre os estágios do pipeline.

- Sincronização Avançada:
  - Mutexes (pthread_mutex_t): Para garantir acesso exclusivo e seguro às estruturas de dados compartilhadas.
  - Variáveis de Condição (pthread_cond_t): Para sinalização eficiente entre produtores e consumidores.
  - Barreiras (pthread_barrier_t): Para sincronizar o início e o fim de cada ciclo de simulação entre todas as threads.

- Simulação Realística:
  - Tempos de Operação Variáveis: Uso de usleep() com valores aleatórios para simular o tempo real de cada tarefa.
  - Falha de Transmissão com Lógica de Retry: Os transmissores têm uma chance de 10% de falha e tentam reenviar o mesmo dado.
  - Monitoramento em Tempo Real: Uma thread dedicada monitora e imprime o status dos buffers a cada segundo.

- Implementação Robusta:
  - Tratamento de Erros: Todas as chamadas críticas de Pthreads são verificadas para garantir uma finalização segura em caso de falha.
  - Limpeza de Recursos: Gerenciamento cuidadoso da memória e de todos os primitivos de sincronização para evitar vazamentos.
  - Log Detalhado: Uma função de log centralizada e segura para threads que adiciona timestamp e ID da thread a cada evento.


PRÉ-REQUISITOS
--------------

- Um compilador C (recomenda-se o GCC).
- Um sistema operacional compatível com POSIX Threads (Linux, macOS, ou Subsistema Windows para Linux - WSL).


COMO COMPILAR E EXECUTAR
-------------------------

1. Salvar o Código:
   Certifique-se de que o código-fonte final esteja salvo em um arquivo chamado "simulador_clima.c".

2. Abrir o Terminal:
   Navegue até o diretório onde você salvou o arquivo.

3. Compilar o Programa:
   Execute o seguinte comando. A flag "-pthread" é essencial para linkar a biblioteca de threads.

       gcc -o simulador simulador_clima.c -pthread

4. Executar a Simulação:
   Inicie o programa com o comando:

       ./simulador

5. Configurar a Simulação:
   O programa solicitará que você insira os parâmetros da simulação (número de threads, tamanho dos buffers, etc.) diretamente no terminal.


EXEMPLO DE SAÍDA
----------------

A saída do programa será detalhada, com logs de cada thread, incluindo timestamp e o ID único da thread, além das mensagens do monitor.

[14:50:30] [TID 1401321...] ==================== [CICLO 1 - INICIANDO] ====================
[14:50:30] [TID 1401321...] [COLETA] Sensor 1 coletou um dado. Buffer coleta: 1/5
[14:50:31] [TID 1401329...] [PROCESSAMENTO] Processador 2 processou dados do Sensor 1. Buffer transmissão: 1/5
[14:50:31] [TID 1401330...] [TRANSMISSÃO] Transmissor 1 enviou dados (de Proc. 2) para Centro Sul. Buffer: 0/5
[14:50:31] [TID 1401320...] [MONITOR] Status -> Buffer Coleta: 0/5 | Buffer Transmissão: 0/5
[14:50:32] [TID 1401331...] [FALHA TRANSMISSÃO] Transmissor 2 falhou. Tentando novamente em 1s...


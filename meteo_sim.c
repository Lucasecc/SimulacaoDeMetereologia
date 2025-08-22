#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <stdarg.h>

/*
================================================================================
 I. ESTRUTURAS DE DADOS COMPARTILHADAS
================================================================================
*/

typedef struct {
    int sensor_id;
    int processador_id;
    float temperatura;
    int validado;
} WeatherData;

typedef struct {
    int coletados;
    int processados;
    int transmitidos;
    pthread_mutex_t lock;
} Estatisticas;

typedef struct {
    WeatherData* data;
    int capacidade;
    int count;
    int head;
    int tail;
    pthread_mutex_t lock;
    pthread_cond_t cond_nao_cheio;
    pthread_cond_t cond_nao_vazio;
} CircularBuffer;

typedef struct {
    int id;
    CircularBuffer* buffer_coleta;
    CircularBuffer* buffer_transmissao;
    pthread_barrier_t* barreira;
    int total_ciclos;
} ThreadArgs;

typedef struct {
    CircularBuffer* buffer_coleta;
    CircularBuffer* buffer_transmissao;
} MonitorArgs;

/*
================================================================================
 II. GLOBAIS E FUNÇÃO DE LOG
================================================================================
*/

Estatisticas estatisticas_ciclo = {0, 0, 0};
int ciclo_atual = 0;
volatile bool g_simulacao_ativa = true;
pthread_mutex_t g_log_mutex;

/**
 * @brief Função de log centralizada e thread-safe.
 * Adiciona timestamp e ID da thread a cada mensagem.
 */
void log_event(const char* format, ...) {
    pthread_mutex_lock(&g_log_mutex);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
    printf("[TID %lu] ", (unsigned long)pthread_self());

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    pthread_mutex_unlock(&g_log_mutex);
}

/*
================================================================================
 III. FUNÇÕES DE GERENCIAMENTO DAS ESTRUTURAS
================================================================================
*/

void buffer_init(CircularBuffer* buf, int capacidade) {
    buf->data = (WeatherData*)malloc(capacidade * sizeof(WeatherData));
    buf->capacidade = capacidade;
    buf->count = 0;
    buf->head = 0;
    buf->tail = 0;
    pthread_mutex_init(&buf->lock, NULL);
    pthread_cond_init(&buf->cond_nao_cheio, NULL);
    pthread_cond_init(&buf->cond_nao_vazio, NULL);
}

void buffer_destroy(CircularBuffer* buf) {
    free(buf->data);
    pthread_mutex_destroy(&buf->lock);
    pthread_cond_destroy(&buf->cond_nao_cheio);
    pthread_cond_destroy(&buf->cond_nao_vazio);
}

int buffer_add(CircularBuffer* buf, WeatherData item) {
    pthread_mutex_lock(&buf->lock);
    while (buf->count == buf->capacidade) {
        pthread_cond_wait(&buf->cond_nao_cheio, &buf->lock);
    }
    buf->data[buf->head] = item;
    buf->head = (buf->head + 1) % buf->capacidade;
    buf->count++;
    int current_count = buf->count;
    pthread_cond_signal(&buf->cond_nao_vazio);
    pthread_mutex_unlock(&buf->lock);
    return current_count;
}

WeatherData buffer_remove(CircularBuffer* buf, int* count_after_remove) {
    pthread_mutex_lock(&buf->lock);
    while (buf->count == 0) {
        pthread_cond_wait(&buf->cond_nao_vazio, &buf->lock);
    }
    WeatherData item = buf->data[buf->tail];
    buf->tail = (buf->tail + 1) % buf->capacidade;
    buf->count--;
    if (count_after_remove != NULL) {
        *count_after_remove = buf->count;
    }
    pthread_cond_signal(&buf->cond_nao_cheio);
    pthread_mutex_unlock(&buf->lock);
    return item;
}

/*
================================================================================
 IV. LÓGICA DA SIMULAÇÃO E DAS THREADS
================================================================================
*/

void exibir_estatisticas(int total_ciclos) {
    if (ciclo_atual > 0) {
        pthread_mutex_lock(&estatisticas_ciclo.lock);
        float taxa = (estatisticas_ciclo.processados > 0) 
                     ? ((float)estatisticas_ciclo.transmitidos / estatisticas_ciclo.processados * 100.0f) 
                     : 0.0f;
        log_event("\n[CICLO %d - FINALIZANDO]\n", ciclo_atual);
        log_event("Aguardando sincronização final do ciclo...\n");
        log_event("=== Estatísticas do Ciclo %d ===\n", ciclo_atual);
        log_event("Dados coletados: %d | Processados: %d | Transmitidos: %d\n",
               estatisticas_ciclo.coletados, estatisticas_ciclo.processados, estatisticas_ciclo.transmitidos);
        log_event("Taxa de transmissão (transmitidos/processados): %.1f%%\n\n", taxa);
        estatisticas_ciclo.coletados = 0;
        estatisticas_ciclo.processados = 0;
        estatisticas_ciclo.transmitidos = 0;
        pthread_mutex_unlock(&estatisticas_ciclo.lock);
    }
    ciclo_atual++;
    if (ciclo_atual <= total_ciclos) {
        log_event("==================== [CICLO %d - INICIANDO] ====================\n", ciclo_atual);
    }
}

void* sensor(void* args) {
    ThreadArgs* t_args = (ThreadArgs*)args;
    for (int i = 0; i < t_args->total_ciclos; i++) {
        log_event("Sensor %d aguardando sincronização do ciclo %d...\n", t_args->id, i + 1);
        pthread_barrier_wait(t_args->barreira);

        usleep(100000 + (rand() % 400000));
        WeatherData dado = {.sensor_id = t_args->id, .temperatura = 15.0f + (rand() % 200) / 10.0f};

        int count_depois_de_add = buffer_add(t_args->buffer_coleta, dado);
        
        log_event("[COLETA] Sensor %d coletou um dado. Buffer coleta: %d/%d\n", t_args->id, count_depois_de_add, t_args->buffer_coleta->capacidade);
        
        pthread_mutex_lock(&estatisticas_ciclo.lock);
        estatisticas_ciclo.coletados++;
        pthread_mutex_unlock(&estatisticas_ciclo.lock);

        pthread_barrier_wait(t_args->barreira);
    }
    free(t_args);
    return NULL;
}

void* processador(void* args) {
    ThreadArgs* t_args = (ThreadArgs*)args;
    for (int i = 0; i < t_args->total_ciclos; i++) {
        log_event("Processador %d aguardando sincronização do ciclo %d...\n", t_args->id, i + 1);
        pthread_barrier_wait(t_args->barreira);

        WeatherData dado_coletado = buffer_remove(t_args->buffer_coleta, NULL);

        usleep(200000 + (rand() % 400000));
        dado_coletado.processador_id = t_args->id;
        dado_coletado.validado = 1;

        log_event("[BACKPRESSURE] Processador %d verificando espaço no buffer de transmissão...\n", t_args->id);
        int count_transm_depois_de_add = buffer_add(t_args->buffer_transmissao, dado_coletado);

        log_event("[PROCESSAMENTO] Processador %d processou dados do Sensor %d. Buffer transmissão: %d/%d\n", 
               t_args->id, dado_coletado.sensor_id, count_transm_depois_de_add, t_args->buffer_transmissao->capacidade);
        
        pthread_mutex_lock(&estatisticas_ciclo.lock);
        estatisticas_ciclo.processados++;
        pthread_mutex_unlock(&estatisticas_ciclo.lock);
        
        pthread_barrier_wait(t_args->barreira);
    }
    free(t_args);
    return NULL;
}

void* transmissor(void* args) {
    ThreadArgs* t_args = (ThreadArgs*)args;
    const char* centros[] = {"Centro Sul", "Centro Norte", "Centro Leste", "Centro Oeste"};
    for (int i = 0; i < t_args->total_ciclos; i++) {
        log_event("Transmissor %d aguardando sincronização do ciclo %d...\n", t_args->id, i + 1);
        int serial_thread = pthread_barrier_wait(t_args->barreira);
        
        if (serial_thread == PTHREAD_BARRIER_SERIAL_THREAD) {
            exibir_estatisticas(t_args->total_ciclos);
        }

        int count_depois_de_remover;
        WeatherData dado_a_enviar = buffer_remove(t_args->buffer_transmissao, &count_depois_de_remover);

        bool enviado_com_sucesso = false;
        while (!enviado_com_sucesso) {
            if ((rand() % 100) < 10) { 
                log_event("[FALHA TRANSMISSÃO] Transmissor %d falhou. Tentando novamente em 1s...\n", t_args->id);
                sleep(1);
            } else {
                log_event("[TRANSMISSÃO] Transmissor %d enviou dados (de Proc. %d) para %s. Buffer: %d/%d\n", 
                       t_args->id, dado_a_enviar.processador_id, centros[rand() % 4], count_depois_de_remover, t_args->buffer_transmissao->capacidade);

                pthread_mutex_lock(&estatisticas_ciclo.lock);
                estatisticas_ciclo.transmitidos++;
                pthread_mutex_unlock(&estatisticas_ciclo.lock);
                
                enviado_com_sucesso = true;
            }
        }

        usleep(300000 + (rand() % 400000));
        pthread_barrier_wait(t_args->barreira);
    }
    free(t_args);
    return NULL;
}

void* monitor(void* args) {
    MonitorArgs* m_args = (MonitorArgs*)args;
    log_event("[MONITOR] Thread de monitoramento iniciada.\n");

    while (g_simulacao_ativa) {
        sleep(1);

        pthread_mutex_lock(&m_args->buffer_coleta->lock);
        int count_coleta = m_args->buffer_coleta->count;
        int cap_coleta = m_args->buffer_coleta->capacidade;
        pthread_mutex_unlock(&m_args->buffer_coleta->lock);

        pthread_mutex_lock(&m_args->buffer_transmissao->lock);
        int count_transm = m_args->buffer_transmissao->count;
        int cap_transm = m_args->buffer_transmissao->capacidade;
        pthread_mutex_unlock(&m_args->buffer_transmissao->lock);
        
        if (g_simulacao_ativa) {
           log_event("[MONITOR] Status -> Buffer Coleta: %d/%d | Buffer Transmissão: %d/%d\n",
                  count_coleta, cap_coleta, count_transm, cap_transm);
        }
    }

    log_event("[MONITOR] Thread de monitoramento finalizada.\n");
    free(m_args);
    return NULL;
}


/*
================================================================================
 V. FUNÇÃO PRINCIPAL
================================================================================
*/
int main() {
    int num_sensores, num_processadores, num_transmissores, tam_buf_coleta, tam_buf_transm, total_ciclos;

    printf("--- Configuração do Simulador da Estação Meteorológica (Versão Robusta) ---\n");
    printf("Digite o número de sensores: "); if (scanf("%d", &num_sensores) != 1) { fprintf(stderr, "ERRO: Entrada inválida.\n"); return EXIT_FAILURE; }
    printf("Digite o número de processadores: "); if (scanf("%d", &num_processadores) != 1) { fprintf(stderr, "ERRO: Entrada inválida.\n"); return EXIT_FAILURE; }
    printf("Digite o número de transmissores: "); if (scanf("%d", &num_transmissores) != 1) { fprintf(stderr, "ERRO: Entrada inválida.\n"); return EXIT_FAILURE; }
    printf("Digite o tamanho do buffer de COLETA: "); if (scanf("%d", &tam_buf_coleta) != 1) { fprintf(stderr, "ERRO: Entrada inválida.\n"); return EXIT_FAILURE; }
    printf("Digite o tamanho do buffer de TRANSMISSÃO: "); if (scanf("%d", &tam_buf_transm) != 1) { fprintf(stderr, "ERRO: Entrada inválida.\n"); return EXIT_FAILURE; }
    printf("Digite a duração da simulação (número de ciclos): "); if (scanf("%d", &total_ciclos) != 1) { fprintf(stderr, "ERRO: Entrada inválida.\n"); return EXIT_FAILURE; }
    
    srand(time(NULL));

    if (pthread_mutex_init(&g_log_mutex, NULL) != 0) {
        fprintf(stderr, "ERRO CRÍTICO: Falha ao inicializar o mutex de log.\n");
        return EXIT_FAILURE;
    }

    log_event("\n=== Iniciando Simulação da Estação Meteorológica ===\n");
    log_event("Configuração: %d sensores, %d processadores, %d transmissores\n", num_sensores, num_processadores, num_transmissores);
    log_event("Buffer coleta: %d slots | Buffer transmissão: %d slots\n", tam_buf_coleta, tam_buf_transm);
    log_event("Ciclos: %d\n\n", total_ciclos);

    CircularBuffer buffer_coleta, buffer_transmissao;
    buffer_init(&buffer_coleta, tam_buf_coleta);
    buffer_init(&buffer_transmissao, tam_buf_transm);

    pthread_barrier_t barreira;
    int total_threads_simulacao = num_sensores + num_processadores + num_transmissores;
    if (pthread_barrier_init(&barreira, NULL, total_threads_simulacao) != 0) {
        fprintf(stderr, "ERRO CRÍTICO: Falha ao inicializar a barreira.\n"); return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&estatisticas_ciclo.lock, NULL) != 0) {
        fprintf(stderr, "ERRO CRÍTICO: Falha ao inicializar o mutex de estatísticas.\n"); return EXIT_FAILURE;
    }
    
    pthread_t* threads = (pthread_t*)malloc(total_threads_simulacao * sizeof(pthread_t));
    if (threads == NULL) {
        fprintf(stderr, "ERRO CRÍTICO: Falha ao alocar memória para as threads.\n"); return EXIT_FAILURE;
    }
    int thread_idx = 0;

    pthread_t monitor_thread_id;
    MonitorArgs* m_args = (MonitorArgs*)malloc(sizeof(MonitorArgs));
    m_args->buffer_coleta = &buffer_coleta;
    m_args->buffer_transmissao = &buffer_transmissao;
    if (pthread_create(&monitor_thread_id, NULL, monitor, m_args) != 0) {
        fprintf(stderr, "ERRO CRÍTICO: Falha ao criar a thread de monitoramento.\n"); return EXIT_FAILURE;
    }

    for (int i = 0; i < num_sensores; i++) {
        ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
        *args = (ThreadArgs){.id=i+1, .buffer_coleta=&buffer_coleta, .barreira=&barreira, .total_ciclos=total_ciclos};
        if (pthread_create(&threads[thread_idx++], NULL, sensor, args) != 0) {
            fprintf(stderr, "ERRO CRÍTICO: Falha ao criar a thread Sensor %d.\n", i+1); return EXIT_FAILURE;
        }
    }
    for (int i = 0; i < num_processadores; i++) {
        ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
        *args = (ThreadArgs){.id=i+1, .buffer_coleta=&buffer_coleta, .buffer_transmissao=&buffer_transmissao, .barreira=&barreira, .total_ciclos=total_ciclos};
        if (pthread_create(&threads[thread_idx++], NULL, processador, args) != 0) {
            fprintf(stderr, "ERRO CRÍTICO: Falha ao criar a thread Processador %d.\n", i+1); return EXIT_FAILURE;
        }
    }
    for (int i = 0; i < num_transmissores; i++) {
        ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
        *args = (ThreadArgs){.id=i+1, .buffer_transmissao=&buffer_transmissao, .barreira=&barreira, .total_ciclos=total_ciclos};
        if (pthread_create(&threads[thread_idx++], NULL, transmissor, args) != 0) {
            fprintf(stderr, "ERRO CRÍTICO: Falha ao criar a thread Transmissor %d.\n", i+1); return EXIT_FAILURE;
        }
    }
    
    for (int i = 0; i < total_threads_simulacao; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "AVISO: Falha ao aguardar a thread de simulação %d.\n", i);
        }
    }
    exibir_estatisticas(total_ciclos);

    log_event("[MAIN] Simulação principal concluída. Solicitando finalização do monitor...\n");
    g_simulacao_ativa = false;
    if (pthread_join(monitor_thread_id, NULL) != 0) {
        fprintf(stderr, "AVISO: Falha ao aguardar a thread de monitoramento.\n");
    }

    log_event("[MAIN] Limpando todos os recursos...\n");
    free(threads);
    buffer_destroy(&buffer_coleta);
    buffer_destroy(&buffer_transmissao);
    pthread_barrier_destroy(&barreira);
    pthread_mutex_destroy(&estatisticas_ciclo.lock);
    pthread_mutex_destroy(&g_log_mutex);

    printf("\n=== Simulação Finalizada ===\n");
    return EXIT_SUCCESS;
}
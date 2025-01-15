#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

// Variáveis compartilhadas que controlam o estado do sistema
int pista_livre = 1;               // Indica se a pista está livre (1) ou ocupada (0)
int helicopteros_aguardando = 0;   // Contador de helicópteros aguardando pela pista
int avioes_aguardando = 0;         // Contador de aviões aguardando pela pista
int emergencia_ativa = 0;          // Flag que indica se há uma emergência
int condicoes_meteorologicas = 1;  // Indica as condições meteorológicas (1: boas, 0: ruins)

// Mecanismos de sincronização para controlar o acesso às variáveis compartilhadas
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_aviao = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_helicoptero = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_emergencia = PTHREAD_COND_INITIALIZER;

// Estrutura para passar argumentos para as threads de aviões e helicópteros
typedef struct {
    int emergencia; // Indica se é uma emergência (0: não, 1: sim)
    int posicao;    // Identificador da aeronave (posição)
} ThreadArgs;

// Função para simular o comportamento dos helicópteros
void* helicoptero(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    int emergencia = args->emergencia;
    int posicao = args->posicao;

    pthread_mutex_lock(&mutex); // Bloqueia o mutex para acessar variáveis compartilhadas

    if (emergencia) { // Se for emergência, a aeronave tem prioridade
        printf("Helicóptero [%d] em emergência! Solicitando pista imediatamente.\n", posicao);
        emergencia_ativa = 1;

        // Espera até que a pista esteja livre e as condições meteorológicas sejam boas
        while (!pista_livre || !condicoes_meteorologicas) {
            pthread_cond_wait(&cond_emergencia, &mutex);
        }
        pista_livre = 0; // Ocupa a pista
        emergencia_ativa = 0;
    } else {
        helicopteros_aguardando++; // Incrementa contador de helicópteros aguardando
        printf("Helicóptero [%d] aguardando pista.\n", posicao);

        // Espera até que a pista esteja livre, não haja emergência e as condições sejam boas
        while (!pista_livre || emergencia_ativa || !condicoes_meteorologicas) {
            pthread_cond_wait(&cond_helicoptero, &mutex);
        }
        helicopteros_aguardando--; // Decrementa o contador após obter a pista
        pista_livre = 0; // Ocupa a pista
    }
    pthread_mutex_unlock(&mutex); // Libera o mutex após modificar as variáveis

    printf("Helicóptero [%d] está usando a pista.\n", posicao);
    sleep(2); // Simula o tempo que o helicóptero usa a pista

    pthread_mutex_lock(&mutex); // Bloqueia o mutex para liberar a pista
    pista_livre = 1; // Libera a pista
    // Notifica as threads que aguardam a pista
    if (emergencia_ativa) {
        pthread_cond_signal(&cond_emergencia);
    } else if (helicopteros_aguardando > 0) {
        pthread_cond_signal(&cond_helicoptero);
    } else {
        pthread_cond_signal(&cond_aviao);
    }
    pthread_mutex_unlock(&mutex);

    return NULL;
}

// Função para simular o comportamento dos aviões
void* aviao(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    int emergencia = args->emergencia;
    int posicao = args->posicao;

    pthread_mutex_lock(&mutex); // Bloqueia o mutex para acessar variáveis compartilhadas

    if (emergencia) { // Se for emergência, a aeronave tem prioridade
        printf("Avião [%d] em emergência! Solicitando pista imediatamente.\n", posicao);
        emergencia_ativa = 1;

        // Espera até que a pista esteja livre e as condições meteorológicas sejam boas
        while (!pista_livre || helicopteros_aguardando > 0 || !condicoes_meteorologicas) {
            pthread_cond_wait(&cond_emergencia, &mutex);
        }
        pista_livre = 0; // Ocupa a pista
        emergencia_ativa = 0;
    } else {
        avioes_aguardando++; // Incrementa contador de aviões aguardando
        printf("Avião [%d] aguardando pista.\n", posicao);

        // Espera até que a pista esteja livre, não haja emergência, nem helicópteros aguardando e as condições sejam boas
        while (!pista_livre || emergencia_ativa || helicopteros_aguardando > 0 || !condicoes_meteorologicas) {
            pthread_cond_wait(&cond_aviao, &mutex);
        }
        avioes_aguardando--; // Decrementa o contador após obter a pista
        pista_livre = 0; // Ocupa a pista
    }
    pthread_mutex_unlock(&mutex); // Libera o mutex após modificar as variáveis

    printf("Avião [%d] está usando a pista.\n", posicao);
    sleep(2); // Simula o tempo que o avião usa a pista

    pthread_mutex_lock(&mutex); // Bloqueia o mutex para liberar a pista
    pista_livre = 1; // Libera a pista
    // Notifica as threads que aguardam a pista
    if (emergencia_ativa) {
        pthread_cond_signal(&cond_emergencia);
    } else if (helicopteros_aguardando > 0) {
        pthread_cond_signal(&cond_helicoptero);
    } else {
        pthread_cond_signal(&cond_aviao);
    }
    pthread_mutex_unlock(&mutex);

    return NULL;
}

// Função para simular as condições meteorológicas
void* clima(void* arg) {
    while (1) {
        sleep(rand() % 5 + 3); // Simula mudanças climáticas a cada 3-7 segundos
        pthread_mutex_lock(&mutex); // Bloqueia o mutex para modificar as condições
        condicoes_meteorologicas = rand() % 2; // 0: ruins, 1: boas
        if (condicoes_meteorologicas) {
            printf("Condições meteorológicas melhoraram! Pista liberada.\n");
            // Notifica as threads que aguardam para usar a pista
            pthread_cond_broadcast(&cond_aviao);
            pthread_cond_broadcast(&cond_helicoptero);
        } else {
            printf("Condições meteorológicas ruins! Pista fechada.\n");
        }
        pthread_mutex_unlock(&mutex); // Libera o mutex após modificar as condições
    }
    return NULL;
}

// Função para monitorar o status do sistema
void* monitor(void* arg) {
    while (1) {
        sleep(5); // Atualiza o status a cada 5 segundos
        pthread_mutex_lock(&mutex); // Bloqueia o mutex para acessar variáveis compartilhadas
        printf("\n=== STATUS DO SISTEMA ===\n");
        printf("Pista: %s\n", pista_livre ? "Livre" : "Ocupada");
        printf("Helicópteros aguardando: %d\n", helicopteros_aguardando);
        printf("Aviões aguardando: %d\n", avioes_aguardando);
        printf("Emergência ativa: %s\n", emergencia_ativa ? "Sim" : "Não");
        printf("Condições meteorológicas: %s\n", condicoes_meteorologicas ? "Boas" : "Ruins");
        printf("========================\n\n");

        // Encerra o sistema se não houver mais aeronaves aguardando
        if (avioes_aguardando == 0 && helicopteros_aguardando == 0) {
            printf("Não há mais aeronaves aguardando. Encerrando o sistema.\n");
            pthread_mutex_unlock(&mutex);
            exit(0);
        }
        pthread_mutex_unlock(&mutex); // Libera o mutex após verificar o status
    }
    return NULL;
}

// Função principal
int main() {
    pthread_t threads[12]; // Array de threads para helicópteros e aviões
    ThreadArgs args[16];   // Array para passar argumentos para as threads
    pthread_t thread_clima, thread_monitor; // Threads para clima e monitoramento

    srand(time(NULL)); // Inicializa o gerador de números aleatórios

    // Criar threads para helicópteros
    for (int i = 0; i < 8; i++) {
        args[i].emergencia = i % 2; // Alterna entre emergência e não emergência
        args[i].posicao = i;
        pthread_create(&threads[i], NULL, helicoptero, &args[i]);
        usleep(100000); // Aguarda um curto intervalo antes de criar a próxima thread
    }

    // Criar threads para aviões
    for (int i = 8; i < 16; i++) {
        args[i].emergencia = (i + 1) % 2;
        args[i].posicao = i;
        pthread_create(&threads[i], NULL, aviao, &args[i]);
        usleep(100000);
    }

    // Criar threads para clima e monitoramento
    pthread_create(&thread_clima, NULL, clima, NULL);
    pthread_create(&thread_monitor, NULL, monitor, NULL);

    // Esperar todas as threads de aeronaves terminarem
    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
    }

    // Cancelar threads de clima e monitoramento após o término
    pthread_cancel(thread_clima);
    pthread_cancel(thread_monitor);

    return 0;
}

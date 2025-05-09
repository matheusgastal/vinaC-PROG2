#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#define MAX_NOME 1024

#include <unistd.h>
#include <time.h>

struct membro {
    char nome[MAX_NOME];
    int uid;
    long int tam_original;
    long int tam_disco;
    time_t data_mod;
    long int ord;
    long loc;
};


struct diretorio {
    struct membro *membros;
    int total;
};

void insere_sem_compressao(char *archive, char **arquivos, int n);
void lista_informacoes(char *archive);
void move_membro(char *archive, char *nome_mover, char *nome_target);
void extrai_arquivos(char *archive, char **arquivos, int num);
void remove_arquivos(char *archive, char **arquivos, int n);
void insere_compactado(char *archive, char **arquivos, int num);
#endif




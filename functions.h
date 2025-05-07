#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#define MAX_NOME 8

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

void insere_sem_compressao(char *archive, char **arquivos, int n);
void lista_informacoes(char *archive);
void extrai_arquivos(char *archive, char **arquivos, int n);
void remove_arquivos(char *archive, char **arquivos, int n);

#endif
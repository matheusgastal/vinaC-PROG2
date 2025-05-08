#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include "functions.h"
#include "lz.h"
#include <unistd.h>
#define MAX_MEMBROS 5

long int get_tamanho(FILE *f){
    fseek(f,0,SEEK_END);
    long int tam = ftell(f);
    fseek(f,0,SEEK_SET);
    return tam;
}

void transfere_info(struct membro mb, struct membro diretorio[], int index, long int tam_membro, long int loc, char **arquivos){
            strcpy(mb.nome, arquivos[index]);
            mb.data_mod = time(NULL);
            mb.ord = index;
            mb.uid = getuid();
            mb.tam_original = tam_membro;
            mb.tam_disco = tam_membro;
            mb.loc = loc;
    
            diretorio[index] = mb;
}


void mover_arquivo(FILE *archive, long origem, long destino, long tamanho) {
    if (tamanho <= 0 || origem == destino) return;

    char *buffer = malloc(tamanho);
    if (!buffer) {
        perror("Erro de alocação");
        return;
    }

    fseek(archive, origem, SEEK_SET);
    fread(buffer, 1, tamanho, archive);

    fseek(archive, destino, SEEK_SET);
    fwrite(buffer, 1, tamanho, archive);

    free(buffer);
}


void insere_sem_compressao(char *archive, char **arquivos, int num) {
    struct membro diretorio[MAX_MEMBROS]; 
    int total_membros = 0;

    FILE *fp_archive = fopen(archive, "rb+");
    if (!fp_archive) {
        fp_archive = fopen(archive, "wb+");
        if (!fp_archive) {
            perror("Erro ao criar archive");
            return;
        }
    }

    long offset = 0;
    long int tam_archiver = get_tamanho(fp_archive);

    if (tam_archiver > 0) {
        // Lê diretório existente
        fseek(fp_archive, -sizeof(struct membro) * MAX_MEMBROS, SEEK_END);
        while (fread(&diretorio[total_membros], sizeof(struct membro), 1, fp_archive) == 1 &&
               total_membros < MAX_MEMBROS) {
            long fim = diretorio[total_membros].loc + diretorio[total_membros].tam_disco;
            if (fim > offset) offset = fim;
            total_membros++;
        }
    }

    for (int i = 0; i < num && total_membros < MAX_MEMBROS; i++) {
        FILE *fp_membro = fopen(arquivos[i], "rb");
        if (!fp_membro) {
            perror("Erro ao abrir membro");
            continue;
        }

        long tam_novo = get_tamanho(fp_membro);
        char *buffer = malloc(tam_novo);
        if (!buffer) {
            perror("Erro de alocação");
            fclose(fp_membro);
            continue;
        }

        fread(buffer, 1, tam_novo, fp_membro);
        fclose(fp_membro);

        int idx_existente = -1;
        for (int j = 0; j < total_membros; j++) {
            if (strcmp(arquivos[i], diretorio[j].nome) == 0) {
                idx_existente = j;
                break;
            }
        }

        if (idx_existente != -1) {
            long tam_antigo = diretorio[idx_existente].tam_disco;
            long diff = tam_novo - tam_antigo;
            long local = diretorio[idx_existente].loc;

            if (diff == 0) {
                fseek(fp_archive, local, SEEK_SET);
                fwrite(buffer, 1, tam_novo, fp_archive);
            } else {
                if (diff > 0) {
                    for (int k = total_membros - 1; k > idx_existente; k--) {
                        mover_arquivo(fp_archive, diretorio[k].loc, diretorio[k].loc + diff, diretorio[k].tam_disco);
                        diretorio[k].loc += diff;
                    }
                } else {
                    for (int k = idx_existente + 1; k < total_membros; k++) {
                        mover_arquivo(fp_archive, diretorio[k].loc, diretorio[k].loc + diff, diretorio[k].tam_disco);
                        diretorio[k].loc += diff;
                    }
                    ftruncate(fileno(fp_archive), tam_archiver + diff);
                }

                fseek(fp_archive, local, SEEK_SET);
                fwrite(buffer, 1, tam_novo, fp_archive);
            }

            diretorio[idx_existente].tam_disco = tam_novo;
            diretorio[idx_existente].tam_original = tam_novo;
            diretorio[idx_existente].data_mod = time(NULL);
            diretorio[idx_existente].uid = getuid();

        } else {
            fseek(fp_archive, offset, SEEK_SET);
            fwrite(buffer, 1, tam_novo, fp_archive);

            struct membro m;
            transfere_info(m, diretorio, total_membros, tam_novo, offset, arquivos);
            offset += tam_novo;
            total_membros++;
        }

        free(buffer);
    }

    // Escreve diretório atualizado no final do arquivo
    fseek(fp_archive, offset, SEEK_SET);
    fwrite(diretorio, sizeof(struct membro), MAX_MEMBROS, fp_archive);

    fclose(fp_archive);
}

void lista_informacoes(char *archiver) {
    FILE *archive = fopen(archiver, "rb+");
    if (archive == NULL) {
        perror("Erro ao abrir archiver.\n");
        return;
    }

    long int tam_arquivo = get_tamanho(archive);
    if ((unsigned long int)tam_arquivo < sizeof(int)) {
        printf("Archiver vazio!\n");
        fclose(archive);
        return;
    }

    // Lê quantidade de arquivos
    int qntd_arquivos;
    fseek(archive, -sizeof(int), SEEK_END);
    fread(&qntd_arquivos, sizeof(int), 1, archive);

    long int tam_dir = sizeof(struct membro) * qntd_arquivos + sizeof(int);
    long int inicio_dir = tam_arquivo - tam_dir;
    fseek(archive, inicio_dir, SEEK_SET);

    struct membro *membros = malloc(sizeof(struct membro) * qntd_arquivos);
    if (!membros) {
        perror("Erro ao alocar memória para membros");
        fclose(archive);
        return;
    }

    fread(membros, sizeof(struct membro), qntd_arquivos, archive);

    if (qntd_arquivos > 0) {
        printf("ARQUIVOS NO ARCHIVE \"%s\":\n", archiver);
        printf("-----------------------------------------------------\n");
        for (int i = 0; i < qntd_arquivos; i++) {
            printf("Nome: %s\n", membros[i].nome);
            printf("UID: %d\n", membros[i].uid);
            printf("Tamanho Original: %ld bytes\n", membros[i].tam_original);
            printf("Tamanho em Disco: %ld bytes\n", membros[i].tam_disco);
            printf("Data de Modificação: %s", ctime(&membros[i].data_mod));
            printf("Offset no Archive: %ld\n", membros[i].loc);
            printf("-----------------------------------------------------\n");
        }
    }

    free(membros);
    fclose(archive);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "functions.h"
#include "lz.h"


// Funções auxiliares

long int get_tamanho(FILE *f) {
    fseek(f, 0, SEEK_END);
    long int tam = ftell(f);
    fseek(f, 0, SEEK_SET);
    return tam;
}

void transfere_info(struct membro *mb, char *nome_arquivo, int index, long int tam_membro, long int loc) {
    strcpy(mb->nome, nome_arquivo);
    mb->data_mod = time(NULL);
    mb->ord = index;
    mb->uid = getuid();
    mb->tam_original = tam_membro;
    mb->tam_disco = tam_membro;
    mb->loc = loc;
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
    struct diretorio dir = {NULL, 0};

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

    // Carrega diretório se existir
    if (tam_archiver > sizeof(int)) {
        int membros_anteriores;
        fseek(fp_archive, -sizeof(int), SEEK_END);
        fread(&membros_anteriores, sizeof(int), 1, fp_archive);

        long int tam_dir = membros_anteriores * sizeof(struct membro) + sizeof(int);
        fseek(fp_archive, tam_archiver - tam_dir, SEEK_SET);

        dir.membros = malloc(membros_anteriores * sizeof(struct membro));
        if (!dir.membros) {
            perror("Erro ao alocar diretório");
            fclose(fp_archive);
            return;
        }

        fread(dir.membros, sizeof(struct membro), membros_anteriores, fp_archive);
        dir.total = membros_anteriores;

        for (int i = 0; i < dir.total; i++) {
            long fim = dir.membros[i].loc + dir.membros[i].tam_disco;
            if (fim > offset) offset = fim;
        }
    }

    // Realoca para adicionar novos membros
    struct membro *tmp = realloc(dir.membros, (dir.total + num) * sizeof(struct membro));
    if (!tmp) {
        perror("Erro ao realocar diretório");
        fclose(fp_archive);
        free(dir.membros);
        return;
    }
    dir.membros = tmp;

    for (int i = 0; i < num; i++) {
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
        for (int j = 0; j < dir.total; j++) {
            if (strcmp(arquivos[i], dir.membros[j].nome) == 0) {
                idx_existente = j;
                break;
            }
        }

        if (idx_existente != -1) {
            long tam_antigo = dir.membros[idx_existente].tam_disco;
            long diff = tam_novo - tam_antigo;
            long local = dir.membros[idx_existente].loc;

            if (diff == 0) {
                fseek(fp_archive, local, SEEK_SET);
                fwrite(buffer, 1, tam_novo, fp_archive);
            } else {
                if (diff > 0) {
                    for (int k = dir.total - 1; k > idx_existente; k--) {
                        mover_arquivo(fp_archive, dir.membros[k].loc, dir.membros[k].loc + diff, dir.membros[k].tam_disco);
                        dir.membros[k].loc += diff;
                    }
                } else {
                    for (int k = idx_existente + 1; k < dir.total; k++) {
                        mover_arquivo(fp_archive, dir.membros[k].loc, dir.membros[k].loc + diff, dir.membros[k].tam_disco);
                        dir.membros[k].loc += diff;
                    }
                    ftruncate(fileno(fp_archive), tam_archiver + diff);
                }

                fseek(fp_archive, local, SEEK_SET);
                fwrite(buffer, 1, tam_novo, fp_archive);
            }

            dir.membros[idx_existente].tam_disco = tam_novo;
            dir.membros[idx_existente].tam_original = tam_novo;
            dir.membros[idx_existente].data_mod = time(NULL);
            dir.membros[idx_existente].uid = getuid();

        } else {
            fseek(fp_archive, offset, SEEK_SET);
            fwrite(buffer, 1, tam_novo, fp_archive);

            transfere_info(&dir.membros[dir.total], arquivos[i], dir.total, tam_novo, offset);
            offset += tam_novo;
            dir.total++;
        }

        free(buffer);
    }

    // Escreve diretório atualizado
    fseek(fp_archive, offset, SEEK_SET);
    fwrite(dir.membros, sizeof(struct membro), dir.total, fp_archive);
    fwrite(&dir.total, sizeof(int), 1, fp_archive);

    free(dir.membros);
    fclose(fp_archive);
}

void lista_informacoes(char *archiver) {
    FILE *archive = fopen(archiver, "rb");
    if (archive == NULL) {
        perror("Erro ao abrir archive");
        return;
    }

    long int tam_arquivo = get_tamanho(archive);
    if (tam_arquivo < sizeof(int)) {
        printf("Archive vazio!\n");
        fclose(archive);
        return;
    }

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h> 
#include "functions.h"
#include "lz.h"
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
    char *buffer = malloc(tamanho);
    if(!buffer) {
        perror("Erro de alocação");
        return;
    }

    fseek(archive, origem, SEEK_SET);
    fread(buffer, tamanho, 1, archive);

    fseek(archive, destino, SEEK_SET);
    fwrite(buffer, 1, tamanho, archive);

    free(buffer);
}

void insere_sem_compressao(char *archive, char **arquivos, int num) {
    struct membro diretorio[MAX_MEMBROS]; 
    int total_membros = 0;
    
    FILE *fp_archive = fopen(archive, "rb+");
    if (fp_archive == NULL) {
        fp_archive = fopen(archive, "wb+");
        if (fp_archive == NULL) {
            perror("Erro ao criar o archive");
            return;
        }
    }

    long int tam_archiver = get_tamanho(fp_archive);
    long int offset = 0;

    if (tam_archiver == 0) {
        offset = 0;
        total_membros = 0;
    } else {
        // CASO 2: arquivo já tem membros -> lemos o diretório existente
        fseek(fp_archive, -sizeof(struct membro) * MAX_MEMBROS, SEEK_END); // tentar ler do final
        while (fread(&diretorio[total_membros], sizeof(struct membro), 1, fp_archive) == 1) {
            long fim = diretorio[total_membros].loc + diretorio[total_membros].tam_disco;
            if (fim > offset) 
            offset = fim;
            total_membros++;
        }
    }

    // Inserção de novos arquivos
    for (int i = 0; i < num; i++) {
        FILE *fp_membro_i = fopen(arquivos[i], "rb");
        if (fp_membro_i == NULL) {
            perror("Erro ao abrir membro");
            continue;
        }

        long int tam_membro = get_tamanho(fp_membro_i);
        char *buffer = malloc(tam_membro);
        if (buffer == NULL) {
            perror("Erro no malloc");
            fclose(fp_membro_i);
            continue;
        }

        fread(buffer, tam_membro, 1 , fp_membro_i);
        fclose(fp_membro_i);

        // Escreve no archive
        fseek(fp_archive, offset, SEEK_SET);
        fwrite(buffer, tam_membro, 1, fp_archive);
        free(buffer);

        // Preenche metadados
        struct membro m;
        transfere_info(m, diretorio, total_membros, tam_membro, offset, arquivos);

        offset += tam_membro;
        total_membros++;
    }

    // Escreve o diretório atualizado no final
    fseek(fp_archive, offset, SEEK_SET);
    fwrite(diretorio, sizeof(struct membro), total_membros, fp_archive);

    fclose(fp_archive);


    for (int i = 0; i < num && total_membros < MAX_MEMBROS; i++) {
        int membro_existente = -1;
        for (int j = 0; j < total_membros; j++) {
            if (strcmp(arquivos[i], diretorio[j].nome) == 0) {
                membro_existente = j;
                break;
            }
        }

        FILE *fp_membro = fopen(arquivos[i], "rb");
        if (fp_membro == NULL) {
            perror("Erro ao abrir arquivo");
            continue;
        }

        long int tam_novo = get_tamanho(fp_membro);
        char *buffer = malloc(tam_novo);
        if (buffer == NULL) {
            perror("Erro de alocação");
            fclose(fp_membro);
            continue;
        }

        fread(buffer, tam_novo, 1, fp_membro);
        fclose(fp_membro);

        if (membro_existente != -1) {
            long int tam_atual = diretorio[membro_existente].tam_disco;
            long int diff = tam_novo - tam_atual;

            if (diff != 0) {
                fseek(fp_archive, -sizeof(struct membro) * MAX_MEMBROS, SEEK_END);
                long pos_trunc = ftell(fp_archive);
                
                if (diff > 0) {
                    for (int k = total_membros - 1; k > membro_existente; k--) {
                        mover_arquivo(fp_archive, diretorio[k].loc, diretorio[k].loc + diff, diretorio[k].tam_disco);
                        diretorio[k].loc += diff;
                    }
                } else {
                    for (int k = membro_existente + 1; k < total_membros; k++) {
                        mover_arquivo(fp_archive, diretorio[k].loc, diretorio[k].loc + diff, diretorio[k].tam_disco);
                        diretorio[k].loc += diff;
                    }
                    ftruncate(fileno(fp_archive), pos_trunc + diff);
                }
            }

            fseek(fp_archive, diretorio[membro_existente].loc, SEEK_SET);
            fwrite(buffer, tam_novo, 1, fp_archive);
            
            diretorio[membro_existente].tam_disco = tam_novo;
            diretorio[membro_existente].tam_original = tam_novo;
            diretorio[membro_existente].data_mod = time(NULL);
            diretorio[membro_existente].uid = getuid();
        } else {
            fseek(fp_archive, offset, SEEK_SET);
            fwrite(buffer, tam_novo, 1, fp_archive);
            
            transfere_info(diretorio[total_membros], arquivos[i], total_membros, tam_novo, offset, arquivos);
            offset += tam_novo;
            total_membros++;
        }

        free(buffer);
    }

    fseek(fp_archive, offset, SEEK_SET);
    fwrite(diretorio, sizeof(struct membro), MAX_MEMBROS, fp_archive);
    fclose(fp_archive);
}

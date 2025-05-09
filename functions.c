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

void imprime_info(struct membro mb){
        printf("Nome: %s\n", mb.nome);
        printf("UID: %d\n", mb.uid);
        printf("Tamanho Original: %ld bytes\n", mb.tam_original);
        printf("Tamanho em Disco: %ld bytes\n", mb.tam_disco);
        printf("Data de Modificação: %s", ctime(&mb.data_mod));
        printf("Offset no Archive: %ld\n", mb.loc);
        printf("-----------------------------------------------------\n");
}

long int get_tamanho(FILE *f) {
    fseek(f, 0, SEEK_END);
    long int tam = ftell(f);
    fseek(f,0,SEEK_SET);
    return tam;
}


int conta_membros_no_archive(const char *archive) {
    FILE *fp = fopen(archive, "rb");
    if (!fp) {
        perror("Erro ao abrir archive");
        return -1;  
    }

    fseek(fp, 0, SEEK_END);
    long tam = ftell(fp);
    if (tam < sizeof(int)) {
        fclose(fp);
        return 0; 
    }

    fseek(fp, -sizeof(int), SEEK_END);
    int total_membros;
    if (fread(&total_membros, sizeof(int), 1, fp) != 1) {
        perror("Erro ao ler quantidade de membros");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return total_membros;
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

    int membros_anteriores = conta_membros_no_archive(archive);
    if (membros_anteriores < 0) {
        fclose(fp_archive);
        return;
    }

    if (membros_anteriores > 0) {
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

    int qntd_arquivos = conta_membros_no_archive(archiver);
    if (qntd_arquivos <= 0) {
        if (qntd_arquivos == 0)
            printf("Archive vazio!\n");
        fclose(archive);
        return;
    }

    long int tam_arquivo = get_tamanho(archive);
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

    printf("ARQUIVOS NO ARCHIVE \"%s\":\n", archiver);
    printf("-----------------------------------------------------\n");
    for (int i = 0; i < qntd_arquivos; i++) {
        imprime_info(membros[i]);
    }

    free(membros);
    fclose(archive);
}


void move_membro(char *archive, char *nome_mover, char *nome_target) {
    FILE *fp = fopen(archive, "rb+");
    if (!fp) {
        perror("Erro ao abrir archive");
        return;
    }

    int total = conta_membros_no_archive(archive);
    if (total <= 0) {
        printf("Archive vazio ou erro ao ler número de membros.\n");
        fclose(fp);
        return;
    }

    long tam_total = get_tamanho(fp);
    long tam_dir = sizeof(struct membro) * total + sizeof(int);
    long inicio_dir = tam_total - tam_dir;

    struct membro *membros = malloc(sizeof(struct membro) * total);
    if (!membros) {
        perror("Erro de alocação");
        fclose(fp);
        return;
    }

    fseek(fp, inicio_dir, SEEK_SET);
    fread(membros, sizeof(struct membro), total, fp);

    // Localiza os índices
    int idx_mover = -1, idx_target = -1;
    for (int i = 0; i < total; i++) {
        if (strcmp(membros[i].nome, nome_mover) == 0) 
            idx_mover = i;
        if (nome_target && strcmp(membros[i].nome, nome_target) == 0) 
            idx_target = i;
    }

    if (idx_mover == -1) {
        printf("Membro a mover não encontrado.\n");
        free(membros);
        fclose(fp);
        return;
    }

    if (nome_target && idx_target == -1) {
        printf("Membro target não encontrado.\n");
        free(membros);
        fclose(fp);
        return;
    }

    // Determina nova ordem
    struct membro mover = membros[idx_mover];
    int nova_pos = (nome_target == NULL) ? 0 : idx_target + 1;

    // Remove da posição atual e insere na nova
    for (int i = idx_mover; i < total - 1; i++){
        membros[i] = membros[i + 1];
    } 
    for (int i = total - 1; i > nova_pos; i--) {
        membros[i] = membros[i - 1];
    }
    membros[nova_pos] = mover;

    // Atualiza campo .ord
    for (int i = 0; i < total; i++) {
        membros[i].ord = i;
    }
    
    // Reescreve a área de dados na nova ordem
    char *buffer = malloc(mover.tam_disco);
    if (!buffer) {
        perror("Erro ao alocar buffer temporário");
        free(membros);
        fclose(fp);
        return;
    }

    FILE *temp = tmpfile();
    long offset = 0;
    for (int i = 0; i < total; i++) {
        fseek(fp, membros[i].loc, SEEK_SET);
        fread(buffer, 1, membros[i].tam_disco, fp);

        fwrite(buffer, 1, membros[i].tam_disco, temp);
        membros[i].loc = offset;
        offset += membros[i].tam_disco;
    }

    // Trunca o arquivo original
    ftruncate(fileno(fp), 0);
    fseek(fp, 0, SEEK_SET);
    fseek(temp, 0, SEEK_SET);

    // Copia dados reorganizados de volta para o archive
    fseek(temp, 0, SEEK_SET);
    while ((offset = fread(buffer, 1, mover.tam_disco, temp)) > 0) {
        fwrite(buffer, 1, offset, fp);
    }

    // Reescreve o diretório atualizado
    fwrite(membros, sizeof(struct membro), total, fp);
    fwrite(&total, sizeof(int), 1, fp);

    free(buffer);
    free(membros);
    fclose(temp);
    fclose(fp);
}


void insere_compactado(char *archive, char **arquivos, int num) {
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
    int membros_anteriores = conta_membros_no_archive(archive);
    if (membros_anteriores < 0) {
        fclose(fp_archive);
        return;
    }

    if (membros_anteriores > 0) {
        long int tam_dir = membros_anteriores * sizeof(struct membro) + sizeof(int);
        fseek(fp_archive, tam_archiver - tam_dir, SEEK_SET);
        dir.membros = malloc(membros_anteriores * sizeof(struct membro));
        fread(dir.membros, sizeof(struct membro), membros_anteriores, fp_archive);
        dir.total = membros_anteriores;

        for (int i = 0; i < dir.total; i++) {
            long fim = dir.membros[i].loc + dir.membros[i].tam_disco;
            if (fim > offset) offset = fim;
        }
    }

    dir.membros = realloc(dir.membros, (dir.total + num) * sizeof(struct membro));

    for (int i = 0; i < num; i++) {
        FILE *fp = fopen(arquivos[i], "rb");
        if (!fp) {
            perror("Erro ao abrir membro");
            continue;
        }

        long tam_orig = get_tamanho(fp);
        char *buffer_orig = malloc(tam_orig);
        fread(buffer_orig, 1, tam_orig, fp);
        fclose(fp);

        long tam_comp = tam_orig * 2; // buffer maior por segurança
        char *buffer_comp = malloc(tam_comp);

        tam_comp = lz_compress((unsigned char *)buffer_orig, tam_orig, (unsigned char *)buffer_comp, tam_comp);

        char *final_buffer;
        long final_size;
        int compressed = 1;

        if (tam_comp >= tam_orig || tam_comp == -1) {
            final_buffer = buffer_orig;
            final_size = tam_orig;
            compressed = 0;
        } else {
            final_buffer = buffer_comp;
            final_size = tam_comp;
        }

        int idx_existente = -1;
        for (int j = 0; j < dir.total; j++) {
            if (strcmp(arquivos[i], dir.membros[j].nome) == 0) {
                idx_existente = j;
                break;
            }
        }

        if (idx_existente != -1) {
            dir.membros[idx_existente].tam_disco = final_size;
            dir.membros[idx_existente].tam_original = tam_orig;
            dir.membros[idx_existente].data_mod = time(NULL);
            dir.membros[idx_existente].uid = getuid();
            fseek(fp_archive, dir.membros[idx_existente].loc, SEEK_SET);
            fwrite(final_buffer, 1, final_size, fp_archive);
        } else {
            fseek(fp_archive, offset, SEEK_SET);
            fwrite(final_buffer, 1, final_size, fp_archive);

            transfere_info(&dir.membros[dir.total], arquivos[i], dir.total, tam_orig, offset);
            dir.membros[dir.total].tam_disco = final_size;
            offset += final_size;
            dir.total++;
        }

        free(buffer_orig);
        free(buffer_comp);
    }

    fseek(fp_archive, offset, SEEK_SET);
    fwrite(dir.membros, sizeof(struct membro), dir.total, fp_archive);
    fwrite(&dir.total, sizeof(int), 1, fp_archive);

    free(dir.membros);
    fclose(fp_archive);
}


void remove_arquivos(char *archive, char **nomes, int num_remover) {
    FILE *fp = fopen(archive, "rb+");
    if (!fp) {
        perror("Erro ao abrir archive");
        return;
    }

    int total = conta_membros_no_archive(archive);
    if (total <= 0) {
        fclose(fp);
        return;
    }

    long tam_total = get_tamanho(fp);
    long tam_dir = sizeof(struct membro) * total + sizeof(int);
    long inicio_dir = tam_total - tam_dir;

    struct membro *dir = malloc(sizeof(struct membro) * total);
    fseek(fp, inicio_dir, SEEK_SET);
    fread(dir, sizeof(struct membro), total, fp);

    struct membro *novos = malloc(sizeof(struct membro) * total);
    int novos_total = 0;

    for (int i = 0; i < total; i++) {
        int remover = 0;
        for (int j = 0; j < num_remover; j++) {
            if (strcmp(nomes[j], dir[i].nome) == 0) {
                remover = 1;
                break;
            }
        }

        if (!remover) {
            novos[novos_total++] = dir[i];
        }
    }

    FILE *tmp = tmpfile();
    char *buffer = malloc(1024 * 1024); // até 1MB por membro
    long offset = 0;

    for (int i = 0; i < novos_total; i++) {
        fseek(fp, novos[i].loc, SEEK_SET);
        fread(buffer, 1, novos[i].tam_disco, fp);

        fwrite(buffer, 1, novos[i].tam_disco, tmp);
        novos[i].loc = offset;
        novos[i].ord = i;
        offset += novos[i].tam_disco;
    }

    ftruncate(fileno(fp), 0);
    fseek(fp, 0, SEEK_SET);
    fseek(tmp, 0, SEEK_SET);

    size_t lidos;
    while ((lidos = fread(buffer, 1, 1024 * 1024, tmp)) > 0) {
        fwrite(buffer, 1, lidos, fp);
    }

    fwrite(novos, sizeof(struct membro), novos_total, fp);
    fwrite(&novos_total, sizeof(int), 1, fp);

    free(buffer);
    free(novos);
    free(dir);
    fclose(tmp);
    fclose(fp);
}


void extrai_arquivos(char *archive, char **arquivos, int num) {
    FILE *fp = fopen(archive, "rb");
    if (!fp) {
        perror("Erro ao abrir archive");
        return;
    }

    int total = conta_membros_no_archive(archive);
    if (total <= 0) {
        fclose(fp);
        return;
    }

    long tam_total = get_tamanho(fp);
    long tam_dir = sizeof(struct membro) * total + sizeof(int);
    long inicio_dir = tam_total - tam_dir;

    struct membro *dir = malloc(sizeof(struct membro) * total);
    fseek(fp, inicio_dir, SEEK_SET);
    fread(dir, sizeof(struct membro), total, fp);

    for (int i = 0; i < total; i++) {
        int extrair = (num == 0);
        for (int j = 0; j < num && !extrair; j++) {
            if (strcmp(arquivos[j], dir[i].nome) == 0) {
                extrair = 1;
                break;
            }
        }

        if (!extrair) continue;

        fseek(fp, dir[i].loc, SEEK_SET);
        char *buffer = malloc(dir[i].tam_disco);
        fread(buffer, 1, dir[i].tam_disco, fp);

        char *saida = malloc(dir[i].tam_original);
        long tam_saida;

        if (dir[i].tam_disco == dir[i].tam_original) {
            memcpy(saida, buffer, dir[i].tam_original);
            tam_saida = dir[i].tam_original;
        } else {
            tam_saida = lz_decompress((unsigned char *)buffer, dir[i].tam_disco, (unsigned char *)saida, dir[i].tam_original);
        }

        FILE *out = fopen(dir[i].nome, "wb");
        fwrite(saida, 1, tam_saida, out);
        fclose(out);

        free(buffer);
        free(saida);
    }

    free(dir);
    fclose(fp);
}

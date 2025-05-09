#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "functions.h"

void print_usage() {
    fprintf(stderr, "Uso: vinac <opcao> <archive> [membro1 membro2 ...]\n");
    fprintf(stderr, "Opções disponíveis:\n");
    fprintf(stderr, "  -ip         Inserir membros sem compressão\n");
    fprintf(stderr, "  -ic         Inserir membros com compressão\n");
    fprintf(stderr, "  -m          Mover membro para depois de outro\n");
    fprintf(stderr, "  -x          Extrair membros\n");
    fprintf(stderr, "  -r          Remover membros\n");
    fprintf(stderr, "  -c          Listar conteúdo do archive\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage();
        return -1;
    }

    int qtd_arquivos = argc - 3;
    char *arquivos[qtd_arquivos];

    for (int i = 0; i < qtd_arquivos; i++) {
        arquivos[i] = argv[i + 3];
    }

    int opcao = -1;

    if (strcmp(argv[1], "-ip") == 0)
        opcao = 1;
    else if (strcmp(argv[1], "-ic") == 0)
        opcao = 2;
    else if (strcmp(argv[1], "-m") == 0)
        opcao = 3;
    else if (strcmp(argv[1], "-x") == 0)
        opcao = 4;
    else if (strcmp(argv[1], "-r") == 0)
        opcao = 5;
    else if (strcmp(argv[1], "-c") == 0)
        opcao = 6;

    if (opcao == -1) {
        fprintf(stderr, "Opção inválida: %s\n", argv[1]);
        print_usage();
        return -1;
    }

    switch (opcao) {
        case 1:
            printf("Executando inserção sem compressão...\n");
            insere_sem_compressao(argv[2], arquivos, qtd_arquivos);
            break;

        case 2:
            printf("Executando inserção com compressão...\n");
            insere_compactado(argv[2], arquivos, qtd_arquivos);
            break;

        case 3:
            printf("Executando movimentação de membro...\n");
            if (argc == 4) {
                move_membro(argv[2], argv[3], NULL);
            } else if (argc == 5) {
                move_membro(argv[2], argv[3], argv[4]);
            } else {
                fprintf(stderr, "Uso incorreto da opção -m.\n");
                print_usage();
                return -1;
            }
            break;

        case 4:
            printf("Executando extração de membros...\n");
            extrai_arquivos(argv[2], arquivos, qtd_arquivos);
            break;

        case 5:
            printf("Executando remoção de membros...\n");
            remove_arquivos(argv[2], arquivos, qtd_arquivos);
            break;

        case 6:
            printf("Executando listagem do conteúdo...\n");
            lista_informacoes(argv[2]);
            break;
    }

    return 0;
}

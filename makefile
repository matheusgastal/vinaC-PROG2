# Definição das variáveis
FLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -Wall

# Regra padrão para gerar o executável
all: vina

vina: main.o functions.o lz.o
	gcc $(FLAGS) main.o functions.o lz.o -o vina

main.o: main.c functions.h
	gcc $(FLAGS) -c main.c

functions.o: functions.c functions.h lz.h
	gcc $(FLAGS) -c functions.c

lz.o: lz.c lz.h
	gcc $(FLAGS) -c lz.c

clean:
	rm -f main.o functions.o lz.o vina

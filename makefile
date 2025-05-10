# Definição das variáveis
CC = gcc
FLAGS = -std=c99 -Wall
OBJ = main.o functions.o lz.o
TARGET = vina

# Regra padrão para gerar o executável
all: $(TARGET)

# Regra para gerar o executável
$(TARGET): $(OBJ)
	$(CC) $(FLAGS) $(OBJ) -o $(TARGET)

# Regra para compilar main.c em main.o
main.o: main.c functions.h
	$(CC) $(FLAGS) -c main.c

# Regra para compilar functions.c em functions.o
functions.o: functions.c functions.h lz.h
	$(CC) $(FLAGS) -c functions.c

# Regra para compilar lz.c em lz.o
lz.o: lz.c lz.h
	$(CC) $(FLAGS) -c lz.c

# Limpeza dos arquivos objetos e do executável
clean:
	rm -f $(OBJ) $(TARGET)

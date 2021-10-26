# Repertoires du projet

BIN_DIR = bin
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
OBJPROF_DIR = obj-prof

# Options de compilation/édition des liens

CC = clang
LD = clang
INC = -I$(INC_DIR)

# debug :
# CFLAGS += $(INC) -Wall -std=c99 -O0 -g -Wextra
# LDFLAGS = -lm -lpthread

# opti build :
CFLAGS += $(INC) -Wall -std=c99 -Wextra -O2
LDFLAGS = -lm -lpthread

# gprof :
#CFLAGS += $(INC) -Wall -std=c99 -pg -Wextra
#LDFLAGS = -lm -lpthread -pg

# Liste des fichiers objet

OBJPROF_FILES =

OBJ_FILES = $(OBJ_DIR)/jpeg2ppm.o    	$(OBJ_DIR)/extract_bloc.o   $(OBJ_DIR)/iqzz.o 		  $(OBJ_DIR)/export_ppm.o\
			$(OBJ_DIR)/extract_image.o	$(OBJ_DIR)/upsampling.o  	$(OBJ_DIR)/jpeg_reader.o  $(OBJ_DIR)/bitstream.o\
			$(OBJ_DIR)/huffman.o		$(OBJ_DIR)/loeffler.o	  	$(OBJ_DIR)/process.o

# cible par défaut

TARGET = $(BIN_DIR)/jpeg2ppm

all: $(TARGET)

$(TARGET): $(OBJPROF_FILES) $(OBJ_FILES)
	$(LD) $(LDFLAGS) $(OBJPROF_FILES) $(OBJ_FILES) -o $(TARGET)

$(OBJ_DIR)/jpeg2ppm.o: $(SRC_DIR)/jpeg2ppm.c $(INC_DIR)/jpeg_reader.h $(INC_DIR)/bitstream.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/jpeg2ppm.c -o $(OBJ_DIR)/jpeg2ppm.o

$(OBJ_DIR)/jpeg_reader.o: $(SRC_DIR)/jpeg_reader.c $(INC_DIR)/bitstream.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/jpeg_reader.c -o $(OBJ_DIR)/jpeg_reader.o

$(OBJ_DIR)/bitstream.o: $(SRC_DIR)/bitstream.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/bitstream.c -o $(OBJ_DIR)/bitstream.o

$(OBJ_DIR)/huffman.o: $(SRC_DIR)/huffman.c $(INC_DIR)/bitstream.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/huffman.c -o $(OBJ_DIR)/huffman.o

$(OBJ_DIR)/extract_bloc.o: $(SRC_DIR)/extract_bloc.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/extract_bloc.c -o $(OBJ_DIR)/extract_bloc.o

$(OBJ_DIR)/iqzz.o: $(SRC_DIR)/iqzz.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/iqzz.c -o $(OBJ_DIR)/iqzz.o

$(OBJ_DIR)/process.o: $(SRC_DIR)/process.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/process.c -o $(OBJ_DIR)/process.o

$(OBJ_DIR)/loeffler.o: $(SRC_DIR)/loeffler.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/loeffler.c -o $(OBJ_DIR)/loeffler.o

$(OBJ_DIR)/export_ppm.o: $(SRC_DIR)/export_ppm.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/export_ppm.c -o $(OBJ_DIR)/export_ppm.o

$(OBJ_DIR)/extract_image.o: $(SRC_DIR)/extract_image.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/extract_image.c -o $(OBJ_DIR)/extract_image.o

$(OBJ_DIR)/upsampling.o: $(SRC_DIR)/upsampling.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/upsampling.c -o $(OBJ_DIR)/upsampling.o


.PHONY: clean

clean:
	rm -f $(TARGET) $(OBJ_FILES)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE (256 * 1024) // 256 KB

typedef struct Block {
    char data[BLOCK_SIZE];  // Bloque de datos de 256 KB
    struct Block *next;     // Puntero al siguiente bloque
    int block_num;          // Número de bloque
} Block;

// Función para crear un nuevo bloque
Block* create_block(int block_num) {
    Block *new_block = (Block *)malloc(sizeof(Block));
    if (!new_block) {
        printf("Error al asignar memoria para un nuevo bloque\n");
        exit(1);
    }
    new_block->block_num = block_num;
    new_block->next = NULL;
    return new_block;
}

// Función para escribir bloques a un archivo
void write_blocks_to_file(Block *head, const char *output_file) {
    FILE *file = fopen(output_file, "wb");
    if (!file) {
        printf("Error al abrir el archivo de salida\n");
        exit(1);
    }
    
    Block *current = head;
    while (current) {
        fwrite(current->data, sizeof(char), BLOCK_SIZE, file);
        current = current->next;
    }

    fclose(file);
    printf("Archivo empaquetado guardado en %s\n", output_file);
}

// Función para empaquetar archivos
void create_archive(char *output_file, char **input_files, int num_files) {
    Block *head = NULL, *current = NULL;
    int block_num = 0;

    for (int i = 0; i < num_files; i++) {
        FILE *file = fopen(input_files[i], "rb");
        if (!file) {
            printf("Error al abrir el archivo %s\n", input_files[i]);
            continue;
        }

        while (!feof(file)) {
            Block *new_block = create_block(block_num++);
            size_t bytes_read = fread(new_block->data, sizeof(char), BLOCK_SIZE, file);

            if (!head) {
                head = new_block;
                current = head;
            } else {
                current->next = new_block;
                current = new_block;
            }

            // Si leímos menos que el tamaño del bloque, significa que es el último bloque
            if (bytes_read < BLOCK_SIZE) {
                break;
            }
        }

        fclose(file);
    }

    // Escribir todos los bloques al archivo de salida
    write_blocks_to_file(head, output_file);

    // Liberar memoria de los bloques
    while (head) {
        Block *temp = head;
        head = head->next;
        free(temp);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s -c <archivoSalida> <archivo1> <archivo2> ...\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-c") == 0) {
        char *output_file = argv[2];
        create_archive(output_file, &argv[3], argc - 3);
    }

    return 0;
}

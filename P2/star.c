/* Kevin Jiménez Molinares
   2021475925
   SO
   Proyecto 2
*/ 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BLOCK_SIZE 262144 // 256K
#define MAX_FILES 250
#define MAX_FILENAME 256

/*
 Representa una entrada de archivo en el encabezado del archivo empaquetado
 - filename: Nombre del archivo (hasta 255 caracteres más terminador nulo)
 - size: Tamaño del archivo en bytes
 - first_block: Índice del primer bloque donde se almacenan los datos del archivo en el archivo empaquetado
 */
typedef struct {
    char filename[MAX_FILENAME];
    size_t size;
    int first_block;
} FileEntry;

/*
 Representa un bloque de datos en el archivo empaquetado
 - next_block: Índice del siguiente bloque de datos (o -1 si es el último)
 - data: Contenido de los datos del archivo (tamaño fijo de BLOCK_SIZE)
 */
typedef struct Block {
    int next_block;
    char data[BLOCK_SIZE];
} Block;

/*
 Representa el encabezado del archivo empaquetado
 - files: Array de entradas de archivos (máximo MAX_FILES)
 - file_count: Número actual de archivos en el archivo empaquetado
 - first_free_block: Índice del primer bloque libre en el archivo empaquetado
 */
typedef struct {
    FileEntry files[MAX_FILES];
    int file_count;
    int first_free_block;
} Header;

int create_archive(const char* archive_name, char** files, int file_count);
int extract_archive(const char* archive_name);
int list_contents(const char* archive_name);
int delete_file_from_archive(const char* archive_name, const char* filename);
int update_file_in_archive(const char* archive_name, const char* filename);
int defragment_archive(const char* archive_name);
int add_file_to_archive(int fd, const char* filename, Header* header);
void extend_archive(int fd, Header* header);

int verbose = 0;


void print_verbose(const char* message) {
    if (verbose > 0) {
        printf("%s\n", message);
    }
}


void print_extra_verbose(const char* message) {
    if (verbose > 1) {
        printf("%s\n", message);
    }
}

/*
Agrega bloques adicionales al archivo empaquetado para evitar frecuentes expansiones de archivos
 */
void extend_archive(int fd, Header* header) {
    Block new_block = { .next_block = -1 };
    lseek(fd, 0, SEEK_END);

    for (int i = 0; i < 10; i++) {
        write(fd, &new_block, sizeof(Block));
        int new_block_index = (lseek(fd, 0, SEEK_CUR) - sizeof(Header)) / sizeof(Block);
        new_block.next_block = header->first_free_block;
        header->first_free_block = new_block_index;
    }

    print_verbose("Archivo extendido dinámicamente para agregar más bloques.");
}

/*
 Agrega un archivo al archivo empaquetado.
 */
int add_file_to_archive(int fd, const char* filename, Header* header) {
    struct stat st;
    if (stat(filename, &st) == -1) {
        perror("Error al obtener información del archivo");
        return -1;
    }

    int file_fd = open(filename, O_RDONLY);
    if (file_fd == -1) {
        perror("Error al abrir archivo");
        return -1;
    }

    FileEntry* entry = &header->files[header->file_count++];
    strncpy(entry->filename, filename, MAX_FILENAME - 1);
    entry->size = st.st_size;

    int current_block;
    if (header->first_free_block != -1) {
        current_block = header->first_free_block;
        lseek(fd, current_block * sizeof(Block) + sizeof(Header), SEEK_SET);
        read(fd, &header->first_free_block, sizeof(int));
    } else {
        extend_archive(fd, header);
        current_block = header->first_free_block;
        lseek(fd, current_block * sizeof(Block) + sizeof(Header), SEEK_SET);
        read(fd, &header->first_free_block, sizeof(int));
    }

    entry->first_block = current_block;

    size_t bytes_read;
    Block block = {0};

    while ((bytes_read = read(file_fd, block.data, BLOCK_SIZE)) > 0) {
        if (header->first_free_block != -1) {
            block.next_block = header->first_free_block;
            lseek(fd, header->first_free_block * sizeof(Block) + sizeof(Header), SEEK_SET);
            read(fd, &header->first_free_block, sizeof(int));
        } else {
            extend_archive(fd, header);
            block.next_block = header->first_free_block;
            lseek(fd, header->first_free_block * sizeof(Block) + sizeof(Header), SEEK_SET);
            read(fd, &header->first_free_block, sizeof(int));
        }
        lseek(fd, current_block * sizeof(Block) + sizeof(Header), SEEK_SET);
        write(fd, &block, sizeof(Block));
        current_block = block.next_block;

        char message[100];
        snprintf(message, sizeof(message), "Bloque agregado en posición %d", current_block);
        print_extra_verbose(message);
    }

    close(file_fd);
    print_verbose("Archivo agregado");
    return 0;
}

/*
 Crea un archivo empaquetado y agrega múltiples archivos al mismo.
 */
int create_archive(const char* archive_name, char** files, int file_count) {
    int fd = open(archive_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error al crear archivo");
        return -1;
    }

    Header header = {0};
    header.first_free_block = -1;
    write(fd, &header, sizeof(Header));

    for (int i = 0; i < file_count; i++) {
        print_verbose("Agregando archivo...");
        add_file_to_archive(fd, files[i], &header);
    }

    lseek(fd, 0, SEEK_SET);
    write(fd, &header, sizeof(Header));

    close(fd);
    print_verbose("Archivo empacado creado");
    return 0;
}

/*
  Extrae todos los archivos de un archivo empaquetado.
 */
int extract_archive(const char* archive_name) {
    int fd = open(archive_name, O_RDONLY);
    if (fd == -1) {
        perror("Error al abrir archivo");
        return -1;
    }

    Header header;
    if (read(fd, &header, sizeof(Header)) != sizeof(Header)) {
        perror("Error al leer el encabezado");
        close(fd);
        return -1;
    }

    for (int i = 0; i < header.file_count; i++) {
        FileEntry* entry = &header.files[i];
        int out_fd = open(entry->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd == -1) {
            perror("Error al crear archivo de salida");
            continue;
        }

        int current_block = entry->first_block;
        size_t remaining = entry->size;
        Block block;

        while (remaining > 0 && current_block != -1) {
            lseek(fd, current_block * sizeof(Block) + sizeof(Header), SEEK_SET);
            if (read(fd, &block, sizeof(Block)) != sizeof(Block)) {
                perror("Error al leer bloque durante la extracción");
                close(out_fd);
                close(fd);
                return -1;
            }
            size_t to_write = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;
            if (write(out_fd, block.data, to_write) != to_write) {
                perror("Error al escribir datos al archivo de salida");
                close(out_fd);
                close(fd);
                return -1;
            }
            remaining -= to_write;
            current_block = block.next_block;
        }
        close(out_fd);
        print_verbose("Archivo extraído con éxito");
    }

    close(fd);
    return 0;
}


/*
  Lista todos los archivos dentro de un archivo empaquetado.
 */
int list_contents(const char* archive_name) {
    int fd = open(archive_name, O_RDONLY);
    if (fd == -1) {
        perror("Error al abrir archivo");
        return -1;
    }

    Header header;
    read(fd, &header, sizeof(Header));

    printf("Contenido del archivo %s:\n", archive_name);
    for (int i = 0; i < header.file_count; i++) {
        FileEntry* entry = &header.files[i];
        printf("%s (%zu bytes)\n", entry->filename, entry->size);
        print_extra_verbose("Archivo listado");
    }

    close(fd);
    return 0;
}

/*
  Elimina un archivo específico del archivo empaquetado y actualiza los bloques libres.
 */
int delete_file_from_archive(const char* archive_name, const char* filename) {
    int fd = open(archive_name, O_RDWR);
    if (fd == -1) {
        perror("Error al abrir archivo");
        return -1;
    }

    Header header;
    if (read(fd, &header, sizeof(Header)) != sizeof(Header)) {
        perror("Error al leer el encabezado");
        close(fd);
        return -1;
    }

    int file_index = -1;
    for (int i = 0; i < header.file_count; i++) {
        if (strcmp(header.files[i].filename, filename) == 0) {
            file_index = i;
            break;
        }
    }

    if (file_index == -1) {
        fprintf(stderr, "Archivo no encontrado: %s\n", filename);
        close(fd);
        return -1;
    }

    int current_block = header.files[file_index].first_block;
    while (current_block != -1) {
        Block block;
        lseek(fd, current_block * sizeof(Block) + sizeof(Header), SEEK_SET);
        
        if (read(fd, &block, sizeof(Block)) != sizeof(Block)) {
            perror("Error al leer el bloque durante la eliminación");
            close(fd);
            return -1;
        }

        int next_block = block.next_block;

        if (next_block == current_block) {
            break;
        }

        block.next_block = header.first_free_block;
        header.first_free_block = current_block;

        lseek(fd, current_block * sizeof(Block) + sizeof(Header), SEEK_SET);
        if (write(fd, &block, sizeof(Block)) != sizeof(Block)) {
            perror("Error al escribir el bloque actualizado durante la eliminación");
            close(fd);
            return -1;
        }

        current_block = next_block;
    }

    for (int i = file_index; i < header.file_count - 1; i++) {
        header.files[i] = header.files[i + 1];
    }
    header.file_count--;

    lseek(fd, 0, SEEK_SET);
    if (write(fd, &header, sizeof(Header)) != sizeof(Header)) {
        perror("Error al escribir el encabezado actualizado");
        close(fd);
        return -1;
    }

    close(fd);
    print_verbose("Archivo eliminado correctamente del archivo empaquetado.");
    return 0;
}

/*
  Actualiza un archivo en el archivo empaquetado eliminándolo y luego volviéndolo a agregar.
 */
int update_file_in_archive(const char* archive_name, const char* filename) {
    print_verbose("Iniciando actualización de archivo...");

    delete_file_from_archive(archive_name, filename);

    int fd = open(archive_name, O_RDWR);
    if (fd == -1) {
        perror("Error al abrir archivo para actualización");
        return -1;
    }

    Header header;
    if (read(fd, &header, sizeof(Header)) != sizeof(Header)) {
        perror("Error al leer el encabezado");
        close(fd);
        return -1;
    }

    print_verbose("Agregando el archivo actualizado...");
    int result = add_file_to_archive(fd, filename, &header);

    lseek(fd, 0, SEEK_SET);
    write(fd, &header, sizeof(Header));

    close(fd);
    print_verbose("Archivo actualizado con éxito");
    return result;
}

/*
 Desfragmenta el archivo empaquetado, moviendo bloques para liberar espacio contiguo.
 */
int defragment_archive(const char* archive_name) {
    int fd = open(archive_name, O_RDWR);
    if (fd == -1) {
        perror("Error al abrir archivo");
        return -1;
    }

    Header header;
    if (read(fd, &header, sizeof(Header)) != sizeof(Header)) {
        perror("Error al leer el encabezado");
        close(fd);
        return -1;
    }

    int new_block_index = 0;
    Block block;
    int current_block;
    size_t remaining;

    for (int i = 0; i < header.file_count; i++) {
        FileEntry* entry = &header.files[i];
        current_block = entry->first_block;
        remaining = entry->size;

        entry->first_block = new_block_index;

        while (remaining > 0) {
            lseek(fd, current_block * sizeof(Block) + sizeof(Header), SEEK_SET);
            if (read(fd, &block, sizeof(Block)) != sizeof(Block)) {
                perror("Error al leer el bloque durante la desfragmentación");
                close(fd);
                return -1;
            }

            size_t to_write = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;
            block.next_block = (remaining <= BLOCK_SIZE) ? -1 : new_block_index + 1;

            lseek(fd, new_block_index * sizeof(Block) + sizeof(Header), SEEK_SET);
            if (write(fd, &block, sizeof(Block)) != sizeof(Block)) {
                perror("Error al escribir bloque durante la desfragmentación");
                close(fd);
                return -1;
            }

            remaining -= to_write;
            current_block = block.next_block;
            new_block_index++;
        }
    }

    header.first_free_block = new_block_index;

    lseek(fd, 0, SEEK_SET);
    if (write(fd, &header, sizeof(Header)) != sizeof(Header)) {
        perror("Error al escribir el encabezado actualizado");
        close(fd);
        return -1;
    }

    if (ftruncate(fd, new_block_index * sizeof(Block) + sizeof(Header)) == -1) {
        perror("Error al truncar el archivo después de la desfragmentación");
        close(fd);
        return -1;
    }

    close(fd);
    print_verbose("Archivo desfragmentado correctamente.");
    return 0;
}


int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <opción> <archivo_star> [archivos...]\n", argv[0]);
        return 1;
    }

    char* option = argv[1];
    char* archive_name = argv[2];

    if (strstr(option, "v") != NULL) {
        verbose = 1;
        if (strstr(option, "vv") != NULL) {
            verbose = 2;
        }
        option = strtok(option, "v"); 
    }

    switch(option[1]) {
        case 'c':
            return create_archive(archive_name, &argv[3], argc - 3);
        case 'x':
            return extract_archive(archive_name);
        case 't':
            return list_contents(archive_name);
        case 'u':
            if (argc == 4) {
                return update_file_in_archive(archive_name, argv[3]);
            }
            break;
        case 'r':
            if (argc == 4) {
                int fd = open(archive_name, O_RDWR);
                if (fd == -1) {
                    perror("Error al abrir archivo");
                    return 1;
                }

                Header header;
                if (read(fd, &header, sizeof(Header)) != sizeof(Header)) {
                    perror("Error al leer el encabezado");
                    close(fd);
                    return 1;
                }

                int result = add_file_to_archive(fd, argv[3], &header);

                lseek(fd, 0, SEEK_SET);
                write(fd, &header, sizeof(Header));

                close(fd);
                return result;
            } else {
                fprintf(stderr, "Uso: %s -r <archivo_star> <archivo_a_agregar>\n", argv[0]);
            }
            break;
        case 'p':
            return defragment_archive(archive_name);
        case '-':
            if (strcmp(option, "--delete") == 0 && argc == 4) {
                return delete_file_from_archive(archive_name, argv[3]);
            }
            break;
        default:
            fprintf(stderr, "Opción no reconocida o argumentos insuficientes: %s\n", option);
            break;
    }

    return 1;
}

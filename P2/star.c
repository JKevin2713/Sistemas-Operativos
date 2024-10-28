#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BLOCK_SIZE 262144 // 256K
#define MAX_FILES 250
#define MAX_FILENAME 256

typedef struct {
    char filename[MAX_FILENAME];
    size_t size;
    int first_block;
} FileEntry;

typedef struct {
    int next_block;
    char data[BLOCK_SIZE];
} Block;

typedef struct {
    FileEntry files[MAX_FILES];
    int file_count;
    int first_free_block;
} Header;


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
    entry->first_block = header->first_free_block;

    int current_block = header->first_free_block;
    size_t bytes_read;
    Block block = {0};

    while ((bytes_read = read(file_fd, block.data, BLOCK_SIZE)) > 0) {
        block.next_block = current_block + 1;
        lseek(fd, current_block * sizeof(Block) + sizeof(Header), SEEK_SET);
        write(fd, &block, sizeof(Block));
        current_block++;
    }

    header->first_free_block = current_block;

    close(file_fd);
    printf("Archivo agregado: %s\n", filename);
    return 0;
}

int create_archive(const char* archive_name, char** files, int file_count) {
    int fd = open(archive_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error al crear archivo");
        return -1;
    }

    Header header = {0};
    write(fd, &header, sizeof(Header));

    for (int i = 0; i < file_count; i++) {
        add_file_to_archive(fd, files[i], &header);
    }

    lseek(fd, 0, SEEK_SET);
    write(fd, &header, sizeof(Header));

    close(fd);
    printf("Archivo empacado creado: %s\n", archive_name);
    return 0;
}



int extract_archive(const char* archive_name) {
    int fd = open(archive_name, O_RDONLY);
    if (fd == -1) {
        perror("Error al abrir archivo");
        return -1;
    }

    Header header;
    read(fd, &header, sizeof(Header));

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

        while (remaining > 0) {
            lseek(fd, current_block * sizeof(Block) + sizeof(Header), SEEK_SET);
            read(fd, &block, sizeof(Block));
            size_t to_write = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;
            write(out_fd, block.data, to_write);
            remaining -= to_write;
            current_block = block.next_block;
        }

        close(out_fd);
        printf("Archivo extraído: %s\n", entry->filename);
    }

    close(fd);
    return 0;
}

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
    }

    close(fd);
    return 0;
}

int delete_file_from_archive(const char* archive_name, const char* filename) {
    int fd = open(archive_name, O_RDWR);
    if (fd == -1) {
        perror("Error al abrir archivo");
        return -1;
    }

    Header header;
    read(fd, &header, sizeof(Header));

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

    // Marcar bloques como libres (simplificado)
    header.files[file_index].filename[0] = '\0';

    // Actualizar el encabezado
    lseek(fd, 0, SEEK_SET);
    write(fd, &header, sizeof(Header));

    close(fd);
    printf("Archivo eliminado: %s\n", filename);
    return 0;
}

int update_file_in_archive(const char* archive_name, const char* filename) {
    int fd = open(archive_name, O_RDWR);
    if (fd == -1) {
        perror("Error al abrir archivo");
        return -1;
    }

    Header header;
    read(fd, &header, sizeof(Header));

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

    // Eliminar el archivo existente
    delete_file_from_archive(archive_name, filename);

    // Agregar el archivo actualizado
    add_file_to_archive(fd, filename, &header);

    close(fd);
    printf("Archivo actualizado: %s\n", filename);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <opción> <archivo_star> [archivos...]\n", argv[0]);
        return 1;
    }
    
    char* option = argv[1];
    char* archive_name = argv[2];

    switch(option[1]) {  // Asumimos que la opción siempre comienza con '-'
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
        case '-':
            if (strcmp(option, "--delete") == 0 && argc == 4) {
                return delete_file_from_archive(archive_name, argv[3]);
            }
            break;
        default:
            break;
    }

    fprintf(stderr, "Opción no reconocida o argumentos insuficientes: %s\n", option);
    return 1;
}

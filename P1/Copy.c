#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_THREADS 10

// estrutura que se utiliza para pasar parametros a los hilos
typedef struct {
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];
} thread_args;

// Función para copiar archivos
int copy_file(const char *source_path, const char *dest_path) {
    int source_fd, dest_fd;
    ssize_t num_read;
    char buffer[BUFFER_SIZE];

    source_fd = open(source_path, O_RDONLY);
    if (source_fd == -1) {
        perror("Error al abrir archivo fuente");
        return -1;
    }

    dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd == -1) {
        perror("Error al crear archivo destino");
        close(source_fd);
        return -1;
    }

    while ((num_read = read(source_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(dest_fd, buffer, num_read) != num_read) {
            perror("Error al escribir en archivo destino");
            close(source_fd);
            close(dest_fd);
            return -1;
        }
    }

    if (num_read == -1) {
        perror("Error al leer archivo fuente");
    }

    close(source_fd);
    close(dest_fd);
    return 0;
}

// funcion que se usa para ejecutar los hilos que copian archivos
void* thread_copy(void* args) {
    thread_args* paths = (thread_args*) args;
    copy_file(paths->source_path, paths->dest_path);
    free(args); 
    pthread_exit(NULL);
}

// funcion de copia recursiva que usa hilos
int copy_directory(const char *source_dir, const char *dest_dir) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];
    pthread_t threads[MAX_THREADS];
    int thread_count = 0;

    dir = opendir(source_dir);
    if (dir == NULL) {
        perror("Error al abrir directorio fuente");
        return -1;
    }

    if (mkdir(dest_dir, 0755) == -1 && errno != EEXIST) {
        perror("Error al crear directorio destino");
        closedir(dir);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(source_path, sizeof(source_path), "%s/%s", source_dir, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, entry->d_name);

        if (stat(source_path, &statbuf) == -1) {
            perror("Error al obtener información del archivo");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            //Copiar directorios recursivamente en el hilo principal
            if (copy_directory(source_path, dest_path) == -1) {
                closedir(dir);
                return -1;
            }
        } else {
            //Crear un hilo para cada archivo
            if (thread_count >= MAX_THREADS) {
                for (int i = 0; i < thread_count; i++) {
                    pthread_join(threads[i], NULL);
                }
                thread_count = 0;
            }

            thread_args* args = malloc(sizeof(thread_args));
            strcpy(args->source_path, source_path);
            strcpy(args->dest_path, dest_path);

            pthread_create(&threads[thread_count], NULL, thread_copy, (void*) args);
            thread_count++;
        }
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    closedir(dir);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <directorio_origen> <directorio_destino>\n", argv[0]);
        return 1;
    }

    const char *source_dir = argv[1];
    const char *dest_dir = argv[2];

    if (copy_directory(source_dir, dest_dir) == -1) {
        fprintf(stderr, "Error al copiar el directorio\n");
        return 1;
    }

    printf("Copia completada exitosamente.\n");
    return 0;
}

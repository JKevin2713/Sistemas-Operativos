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
#define MAX_QUEUE 100

typedef struct {
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];
} file_task;

file_task file_queue[MAX_QUEUE];
int queue_size = 0;
int queue_start = 0;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
int files_remaining = 0;
char completion_messages[MAX_THREADS][100]; 
pthread_mutex_t message_mutex = PTHREAD_MUTEX_INITIALIZER;

int copy_file(const char *source_path, const char *dest_path, int thread_id) {
    int source_fd, dest_fd;
    ssize_t num_read;
    char buffer[BUFFER_SIZE];

    printf("Hilo %d: Iniciando copia de %s a %s\n", thread_id, source_path, dest_path);

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

    pthread_mutex_lock(&message_mutex);
    snprintf(completion_messages[thread_id], sizeof(completion_messages[thread_id]), "Hilo %d: Copia de %s completada", thread_id, source_path);
    pthread_mutex_unlock(&message_mutex);

    printf("Hilo %d: Copia de %s completada\n", thread_id, source_path);

    return 0;
}


void* thread_worker(void* arg) {
    int thread_id = *(int*)arg; 

    while (1) {
        pthread_mutex_lock(&queue_mutex);


        while (queue_size == 0 && files_remaining > 0) {
            printf("Hilo %d: Esperando tareas...\n", thread_id);
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }
        if (files_remaining == 0) {
            pthread_mutex_unlock(&queue_mutex);
            printf("Hilo %d: No quedan más archivos por copiar. Terminando.\n", thread_id);
            break;
        }
        file_task task = file_queue[queue_start];
        queue_start = (queue_start + 1) % MAX_QUEUE;
        queue_size--;
        pthread_mutex_unlock(&queue_mutex);
        copy_file(task.source_path, task.dest_path, thread_id);
        pthread_mutex_lock(&queue_mutex);
        files_remaining--;
        pthread_mutex_unlock(&queue_mutex);
    }
    pthread_exit(NULL);
}


void add_task_to_queue(const char* source_path, const char* dest_path) {
    pthread_mutex_lock(&queue_mutex);

    file_task task;
    strcpy(task.source_path, source_path);
    strcpy(task.dest_path, dest_path);

    file_queue[(queue_start + queue_size) % MAX_QUEUE] = task;
    queue_size++;

    printf("Archivo agregado a la cola: %s -> %s\n", source_path, dest_path);

    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
}


int process_directory(const char *source_dir, const char *dest_dir) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];

    printf("Procesando directorio: %s -> %s\n", source_dir, dest_dir);

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
            if (process_directory(source_path, dest_path) == -1) {
                closedir(dir);
                return -1;
            }
        } else {
            pthread_mutex_lock(&queue_mutex);
            files_remaining++;
            pthread_mutex_unlock(&queue_mutex);
            add_task_to_queue(source_path, dest_path);
        }
    }
    closedir(dir);
    printf("Finalizando procesamiento del directorio: %s\n", source_dir);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <directorio_origen> <directorio_destino>\n", argv[0]);
        return 1;
    }

    const char *source_dir = argv[1];
    const char *dest_dir = argv[2];

    pthread_t threads[MAX_THREADS];
    int thread_ids[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, thread_worker, &thread_ids[i]);
        printf("Hilo %d creado.\n", i);
    }
    if (process_directory(source_dir, dest_dir) == -1) {
        fprintf(stderr, "Error al procesar el directorio\n");
        return 1;
    }

    pthread_mutex_lock(&queue_mutex);
    pthread_cond_broadcast(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(threads[i], NULL);
        printf("Hilo %d terminado.\n", i);
    }
    pthread_mutex_lock(&message_mutex);
    for (int i = 0; i < MAX_THREADS; i++) {
        printf("%s\n", completion_messages[i]);
    }
    pthread_mutex_unlock(&message_mutex);

    printf("Copia completada exitosamente.\n");
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define MAX_THREADS 10


typedef struct {
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];
} file_task;

typedef struct {
    file_task tasks[BUFFER_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} task_queue;

task_queue queue;
pthread_t thread_pool[MAX_THREADS];
int active_threads = 0;
pthread_mutex_t active_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t active_cond = PTHREAD_COND_INITIALIZER;

void init_queue(task_queue *q) {
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}


void enqueue(task_queue *q, file_task task) {
    pthread_mutex_lock(&q->mutex);
    q->tasks[q->rear] = task;
    q->rear = (q->rear + 1) % BUFFER_SIZE;
    q->count++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}


file_task dequeue(task_queue *q) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0) {
        pthread_cond_wait(&q->cond, &q->mutex);
    }
    file_task task = q->tasks[q->front];
    q->front = (q->front + 1) % BUFFER_SIZE;
    q->count--;
    pthread_mutex_unlock(&q->mutex);
    return task;
}

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


void* thread_copy(void* args) {
    while (1) {
        file_task task = dequeue(&queue);
        if (strcmp(task.source_path, "TERMINATE") == 0) {
            break;
        }

        struct timespec start_time, end_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time); 

        copy_file(task.source_path, task.dest_path);

        clock_gettime(CLOCK_MONOTONIC, &end_time); 

        
        double duration = (end_time.tv_sec - start_time.tv_sec) +
                          (end_time.tv_nsec - start_time.tv_nsec) / 1e9;


        printf("Copiando: %s a %s tomó %.6f segundos.\n", task.source_path, task.dest_path, duration);
    }

    pthread_mutex_lock(&active_mutex);
    active_threads--;
    if (active_threads == 0) {
        pthread_cond_signal(&active_cond);
    }
    pthread_mutex_unlock(&active_mutex);

    pthread_exit(NULL);
}


int copy_directory(const char *source_dir, const char *dest_dir) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];

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
            if (copy_directory(source_path, dest_path) == -1) {
                closedir(dir);
                return -1;
            }
        } else {

            file_task task;
            strcpy(task.source_path, source_path);
            strcpy(task.dest_path, dest_path);
            enqueue(&queue, task);
        }
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

    init_queue(&queue);

    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&thread_pool[i], NULL, thread_copy, NULL);
    }

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time); 

    if (copy_directory(source_dir, dest_dir) == -1) {
        fprintf(stderr, "Error al copiar el directorio\n");
        return 1;
    }

    for (int i = 0; i < MAX_THREADS; i++) {
        file_task terminate_task;
        strcpy(terminate_task.source_path, "TERMINATE");
        enqueue(&queue, terminate_task);
    }

    pthread_mutex_lock(&active_mutex);
    while (active_threads > 0) {
        pthread_cond_wait(&active_cond, &active_mutex);
    }
    pthread_mutex_unlock(&active_mutex);

    clock_gettime(CLOCK_MONOTONIC, &end_time); 


    double total_duration = (end_time.tv_sec - start_time.tv_sec) +
                            (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    printf("Copia completada exitosamente en %.6f segundos.\n", total_duration);


    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(thread_pool[i], NULL);
    }

    return 0;
}

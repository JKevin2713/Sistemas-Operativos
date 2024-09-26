#include <stdio.h> 
#include <stdlib.h> 
#include <dirent.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <unistd.h> 
#include <string.h> 
#include <errno.h> 

#define BUFFER_SIZE 1024 // Definición del tamaño del buffer que va a almacenar los archivos que se copian

/**
    Funcion que copia los archivos de un directorio a otro
    el programa esta basado en los archivos de practica del curso
**/

int copy_file(const char *source_path, const char *dest_path) {

    //Descriptores y numero de bytes
    int source_fd, dest_fd; 
    ssize_t num_read; 
    char buffer[BUFFER_SIZE]; 

   //Abre el archivo en modo lectura
    source_fd = open(source_path, O_RDONLY); 
    if (source_fd == -1) { 
        perror("Error al abrir archivo fuente"); 
        return -1; 
    }

    //Crea o abre el archivo de destino
    dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644); 
    if (dest_fd == -1) {
        perror("Error al crear archivo destino"); 
        close(source_fd); 
        return -1; 
    }

    //Copia el contenido
    while ((num_read = read(source_fd, buffer, BUFFER_SIZE)) > 0) { 
        if (write(dest_fd, buffer, num_read) != num_read) { 
            perror("Error al escribir en archivo destino"); 
            close(source_fd); 
            close(dest_fd); 
            return -1;
        }
    }
    // Por si ocurre un error al leer el archivo en la primera condición
    if (num_read == -1) { 
        perror("Error al leer archivo fuente"); 
    }


    close(source_fd);
    close(dest_fd); 

    return 0; 
}

/**
Funcion pensada para copiar el directorio de manera recursiva
**/

int copy_directory(const char *source_dir, const char *dest_dir) {

    /**
        definicion de puntero, estructuras de almacenamiento y rutas
    **/
    DIR *dir; 
    struct dirent *entry; 
    struct stat statbuf; 
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX]; 

    // Abre el directorio de origen
    dir = opendir(source_dir); 
    if (dir == NULL) { 
        perror("Error al abrir directorio fuente"); 
        return -1; 
    }

    // Crear el directorio destino si no existe
    if (mkdir(dest_dir, 0755) == -1 && errno != EEXIST) { 
        perror("Error al crear directorio destino"); 
        closedir(dir); 
        return -1; 
    }

    // Recorre los archivos del directorio de origen
    while ((entry = readdir(dir)) != NULL) { 
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) { // Ignora los archivos para evitar enciclarse o errores "." y ".."
            continue; 
        }

        //snprintf ayuda a que no se desborde el buffer
        snprintf(source_path, sizeof(source_path), "%s/%s", source_dir, entry->d_name); 
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, entry->d_name); 

        // Comprobar si es un archivo o un directorio
        if (stat(source_path, &statbuf) == -1) { 
            perror("Error al obtener información del archivo"); 
            continue; 
        }

        //directorio
        if (S_ISDIR(statbuf.st_mode)) { 
            
            //recursion para copiar el directorio
            if (copy_directory(source_path, dest_path) == -1) { 
                closedir(dir); 
                return -1; 
            }
        //archivo
        } else { 
            
            if (copy_file(source_path, dest_path) == -1) {  //queda pendiente implementar la funcion que copia archivos
                closedir(dir); 
                return -1; 
            }
        }
    }

    closedir(dir); // Cerrar directorio fuente
    return 0; 
}

int main(int argc, char *argv[]) {
    if (argc != 3) { 
        fprintf(stderr, "Uso: %s <directorio_origen> <directorio_destino>\n", argv[0]); 
        return 1; 
    }

    const char *source_dir = argv[1]; // directorio fuente
    const char *dest_dir = argv[2]; //directorio de destino

    //copia de directorio
    if (copy_directory(source_dir, dest_dir) == -1) {
        fprintf(stderr, "Error al copiar el directorio\n");
        return 1;
    }

    printf("Copia completada exitosamente.\n");
    return 0;
}

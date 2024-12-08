#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "image-lib.h"
#include <ctype.h>

#define OUTPUT_DIR "./old_photo_PAR_A/"

typedef struct {
    char **files;
    int start;
    int end;
    char *texture_file;
    double elapsed_time;
} ThreadData;

// Função auxiliar para extrair o número do nome do arquivo
int extract_number(const char *str) {
    while (*str && !isdigit(*str)) str++; 
    return isdigit(*str) ? atoi(str) : 0;
}

// Função de comparação por nome
int compare_by_name(const void *a, const void *b) {
    const char *file1 = *(const char **)a;
    const char *file2 = *(const char **)b;

    int num1 = extract_number(file1);
    int num2 = extract_number(file2);

    if (num1 != num2) {
        return num1 - num2;
    }

    return strcmp(file1, file2);
}

// Função de comparação por tamanho
int compare_by_size(const void *a, const void *b) {
    const char *file1 = *(const char **)a;
    const char *file2 = *(const char **)b;

    struct stat stat1, stat2;
    if (stat(file1, &stat1) != 0 || stat(file2, &stat2) != 0) {
        fprintf(stderr, "Erro ao obter tamanho do arquivo\n");
        return 0;
    }

    return (stat1.st_size - stat2.st_size);
}

// Validação da ordenação por nome
int is_sorted_by_name(char **files, int count) {
    for (int i = 1; i < count; i++) {
        if (strcmp(files[i - 1], files[i]) > 0) {
            return 0; // Não está ordenado
        }
    }
    return 1; // Está ordenado
}

// Validação da ordenação por tamanho
int is_sorted_by_size(char **files, int count) {
    struct stat stat1, stat2;
    for (int i = 1; i < count; i++) {
        if (stat(files[i - 1], &stat1) != 0 || stat(files[i], &stat2) != 0) {
            fprintf(stderr, "Erro ao obter informações de tamanho para validação\n");
            return 0;
        }
        if (stat1.st_size > stat2.st_size) {
            return 0; // Não está ordenado
        }
    }
    return 1; // Está ordenado
}

// Função para verificar se uma imagem já foi processada
int is_image_processed(const char *file_name) {
    char out_file[256];
    sprintf(out_file, "%s%s", OUTPUT_DIR, strrchr(file_name, '/') + 1);
    return access(out_file, F_OK) == 0; 
}

void *process_images(void *arg) {
    ThreadData *data = (ThreadData *)arg;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    gdImagePtr in_img, out_smoothed_img, out_contrast_img, out_textured_img, out_sepia_img;
    gdImagePtr in_texture_img = read_png_file(data->texture_file);
    if (!in_texture_img) {
        fprintf(stderr, "Erro ao carregar textura %s\n", data->texture_file);
        pthread_exit(NULL);
    }

    for (int i = data->start; i < data->end; i++) {
        char *file_name = data->files[i];

        if (is_image_processed(file_name)) {
            printf("Imagem já processada: %s\n", file_name);
            continue;
        }

        in_img = read_jpeg_file(file_name);
        if (!in_img) {
            fprintf(stderr, "Erro ao carregar imagem %s\n", file_name);
            continue;
        }

        out_contrast_img = contrast_image(in_img);
        gdImageDestroy(in_img);

        out_smoothed_img = smooth_image(out_contrast_img);
        gdImageDestroy(out_contrast_img);

        out_textured_img = texture_image(out_smoothed_img, in_texture_img);
        gdImageDestroy(out_smoothed_img);

        out_sepia_img = sepia_image(out_textured_img);
        gdImageDestroy(out_textured_img);

        if (!out_sepia_img) {
            fprintf(stderr, "Erro: Imagem final está nula para %s\n", file_name);
            continue;
        }

        char out_file[256];
        sprintf(out_file, "%s%s", OUTPUT_DIR, strrchr(file_name, '/') + 1);

        if (!write_jpeg_file(out_sepia_img, out_file)) {
            fprintf(stderr, "Erro ao salvar imagem %s\n", out_file);
        }

        gdImageDestroy(out_sepia_img);
    }

    gdImageDestroy(in_texture_img);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    data->elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                         (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    printf("Thread [%ld] finalizou em %.3f segundos.\n", pthread_self(), data->elapsed_time);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    struct timespec start_time_total, end_time_total;
    clock_gettime(CLOCK_MONOTONIC, &start_time_total);

    if (argc < 4) {
        fprintf(stderr, "Uso: %s <dir> <n_threads> <-name|-size>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *input_dir = argv[1];
    int n_threads = atoi(argv[2]);
    char *sort_option = argv[3];
    struct dirent *entry;
    DIR *dir = opendir(input_dir);

    if (!dir) {
        fprintf(stderr, "Erro ao abrir diretório\n");
        exit(EXIT_FAILURE);
    }

    if (mkdir(OUTPUT_DIR, 0755) && errno != EEXIST) {
        fprintf(stderr, "Erro ao criar diretório de saída\n");
        exit(EXIT_FAILURE);
    }

    char **file_list = malloc(1000 * sizeof(char *));
    int file_count = 0;

    while ((entry = readdir(dir))) {
        if (strstr(entry->d_name, ".jpeg")) {
            char *file_path = malloc(strlen(input_dir) + strlen(entry->d_name) + 2);
            sprintf(file_path, "%s/%s", input_dir, entry->d_name);
            file_list[file_count++] = file_path;
        }
    }
    closedir(dir);

    if (strcmp(sort_option, "-name") == 0) {
        qsort(file_list, file_count, sizeof(char *), compare_by_name);
        if (!is_sorted_by_name(file_list, file_count)) {
            fprintf(stderr, "Erro: Ordenação por nome falhou\n");
            exit(EXIT_FAILURE);
        }
    } else if (strcmp(sort_option, "-size") == 0) {
        qsort(file_list, file_count, sizeof(char *), compare_by_size);
        if (!is_sorted_by_size(file_list, file_count)) {
            fprintf(stderr, "Erro: Ordenação por tamanho falhou\n");
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Opção de ordenação inválida. Use -name ou -size.\n");
        exit(EXIT_FAILURE);
    }

    printf("Ordenação validada com sucesso.\n");

    if (n_threads > file_count) {
        n_threads = file_count;
    }

    pthread_t threads[n_threads];
    ThreadData thread_data[n_threads];
    int images_per_thread = file_count / n_threads;
    int remaining_images = file_count % n_threads;

    for (int i = 0; i < n_threads; i++) {
        thread_data[i].files = file_list;
        thread_data[i].start = i * images_per_thread;
        thread_data[i].end = (i + 1) * images_per_thread;
        if (i == n_threads - 1) thread_data[i].end += remaining_images;
        thread_data[i].texture_file = "./paper-texture.png";
        thread_data[i].elapsed_time = 0.0;

        pthread_create(&threads[i], NULL, process_images, &thread_data[i]);
    }

    for (int i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    FILE *log_file;
    char log_file_name[256];
    sprintf(log_file_name, "timing_%d-%s.txt", n_threads, sort_option + 1);
    log_file = fopen(log_file_name, "w");
    if (!log_file) {
        fprintf(stderr, "Erro ao criar arquivo de log\n");
        exit(EXIT_FAILURE);
    }

    double total_thread_time = 0.0;
    for (int i = 0; i < n_threads; i++) {
        fprintf(log_file, "Thread %d: %.3f segundos\n", i + 1, thread_data[i].elapsed_time);
        total_thread_time += thread_data[i].elapsed_time;
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time_total);
    double total_time = (end_time_total.tv_sec - start_time_total.tv_sec) +
                        (end_time_total.tv_nsec - start_time_total.tv_nsec) / 1e9;

    fprintf(log_file, "Tempo total do programa: %.3f segundos\n", total_time);
    fclose(log_file);

    for (int i = 0; i < file_count; i++) {
        free(file_list[i]);
    }
    free(file_list);

    printf("Tempo total de execução: %.3f segundos.\n", total_time);
    printf("Processamento completo!\n");
    return 0;
}

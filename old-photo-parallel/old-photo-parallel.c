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

#define OUTPUT_DIR "./old_photo_PAR_A/"

typedef struct {
    char **files;
    int start;
    int end;
    char *texture_file;
} ThreadData;

// Função que processa um subconjunto de imagens
#include <time.h>

void *process_images(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time); // Tempo inicial da thread

    /* input images */
    gdImagePtr in_img;
    /* output images */
    gdImagePtr out_smoothed_img;
    gdImagePtr out_contrast_img;
    gdImagePtr out_textured_img;
    gdImagePtr out_sepia_img;

    // Carregar a textura uma vez
    gdImagePtr in_texture_img = read_png_file(data->texture_file);
    if (!in_texture_img) {
        fprintf(stderr, "Erro ao carregar textura %s\n", data->texture_file);
        pthread_exit(NULL);
    }

    for (int i = data->start; i < data->end; i++) {
        char *file_name = data->files[i];
        printf("Thread processando: %s\n", file_name);

        in_img = read_jpeg_file(file_name);
        if (!in_img) {
            fprintf(stderr, "Erro ao carregar imagem %s\n", file_name);
            continue;
        }

        // Aplicar transformações sequenciais
        out_contrast_img = contrast_image(in_img);
        gdImageDestroy(in_img);

        out_smoothed_img = smooth_image(out_contrast_img);
        gdImageDestroy(out_contrast_img);

        out_textured_img = texture_image(out_smoothed_img, in_texture_img);
        gdImageDestroy(out_smoothed_img);

        out_sepia_img = sepia_image(out_textured_img);
        gdImageDestroy(out_textured_img);

        // Salvar imagem processada
        char out_file[256];
        sprintf(out_file, "%s%s", OUTPUT_DIR, file_name);
        if (!write_jpeg_file(out_sepia_img, out_file)) {
            fprintf(stderr, "Erro ao salvar imagem %s\n", out_file);
        }
        gdImageDestroy(out_sepia_img);
    }

    gdImageDestroy(in_texture_img); // Liberar textura

    clock_gettime(CLOCK_MONOTONIC, &end_time); // Tempo final da thread

    // Calcular tempo decorrido
    double elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                          (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    printf("Thread [%ld] finalizou em %.3f segundos.\n", pthread_self(), elapsed_time);

    pthread_exit(NULL);
}

// Função principal
int main(int argc, char *argv[]) {
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
        perror("Erro ao abrir diretório");
        exit(EXIT_FAILURE);
    }

    // Criar diretório de saída
    if (mkdir(OUTPUT_DIR, 0755) && errno != EEXIST) {
        perror("Erro ao criar diretório de saída");
        exit(EXIT_FAILURE);
    }

    // Carregar nomes dos arquivos JPEG
    char **file_list = malloc(1000 * sizeof(char *));
    int file_count = 0;

    while ((entry = readdir(dir))) {
        if (strstr(entry->d_name, ".jpeg")) {
            file_list[file_count] = malloc(strlen(entry->d_name) + 1);
            strcpy(file_list[file_count], entry->d_name);
            file_count++;
        }
    }
    closedir(dir);

    // Ordenar arquivos se necessário
    if (strcmp(sort_option, "-name") == 0) {
        qsort(file_list, file_count, sizeof(char *), (int (*)(const void *, const void *))strcmp);
    }

    // Adicionar ordenação por tamanho se necessário 

    // MUDAR|!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if (strcmp(sort_option, "-size") == 0) {
        qsort(file_list, file_count, sizeof(char *), (int (*)(const void *, const void *))strcmp);
    }


    // Ordenar por tamanho? para igualar mais ou menos o tamanho das imagens e ter tamanhos iguais nas diferentes threads


    // Dividir trabalho entre threads
    if(n_threads > file_count){
        printf("Número de threads ajustado ao número de imagens total.\n");
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
        if (i == n_threads - 1) thread_data[i].end += remaining_images; // Ultima thread pega o resto

        thread_data[i].texture_file = "./paper-texture.png";

        pthread_create(&threads[i], NULL, process_images, &thread_data[i]);
    }

    // Aguardar threads terminarem
    for (int i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Liberar memória
    for (int i = 0; i < file_count; i++) {
        free(file_list[i]);
    }
    free(file_list);

    printf("Processamento completo!\n");
    return 0;
}

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
#include <ctype.h>
#include "image-lib.h"

#define OUTPUT_DIR "old_photo_PAR_B/"
#define BATCH_SIZE 5

typedef struct {
    char **files;
    int total_files;
    int current_index;
    int processed_count;
    double total_processing_time;
    pthread_mutex_t mutex;
} TaskQueue;

typedef struct {
    TaskQueue *task_queue;
    char *texture_file;
    double elapsed_time;
} ThreadData;

// ============================
//      HELPER FUNCTIONS
// ============================

int extract_number(const char *str) { /* as in original */ }
int compare_by_name(const void *a, const void *b) { /* as in original */ }
int compare_by_size(const void *a, const void *b) { /* as in original */ }

/**
 * Checks if an image has already been processed by verifying the existence
 * of the output file.
 */
int is_image_processed(const char *file_name) {
    char out_file[256];
    sprintf(out_file, "%s%s", OUTPUT_DIR, strrchr(file_name, '/') + 1);
    return access(out_file, F_OK) == 0;
}

// ============================
//     THREAD FUNCTIONS
// ============================

/**
 * Fetches a batch of images to process.
 * Skips already processed images.
 * Returns the number of images fetched.
 */
int fetch_batch(TaskQueue *queue, char **batch) {
    pthread_mutex_lock(&queue->mutex);
    int batch_count = 0;

    while (batch_count < BATCH_SIZE && queue->current_index < queue->total_files) {
        char *file_name = queue->files[queue->current_index];
        queue->current_index++;

        // Skip already processed images
        if (!is_image_processed(file_name)) {
            batch[batch_count++] = file_name;
        }
    }

    pthread_mutex_unlock(&queue->mutex);
    return batch_count;
}

/**
 * Processes images in batches.
 */
void *process_images(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    char *batch[BATCH_SIZE];

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    gdImagePtr texture_img = read_png_file(data->texture_file);
    if (!texture_img) {
        fprintf(stderr, "Error loading texture %s\n", data->texture_file);
        pthread_exit(NULL);
    }

    while (1) {
        int batch_count = fetch_batch(data->task_queue, batch);

        if (batch_count == 0) break;

        for (int i = 0; i < batch_count; i++) {
            char *file_name = batch[i];

            gdImagePtr img = read_jpeg_file(file_name);
            if (!img) {
                fprintf(stderr, "Error loading image %s\n", file_name);
                continue;
            }

            gdImagePtr contrast_img = contrast_image(img);
            gdImageDestroy(img);

            if (!contrast_img) continue;
            gdImagePtr smooth_img = smooth_image(contrast_img);
            gdImageDestroy(contrast_img);

            if (!smooth_img) continue;
            gdImagePtr textured_img = texture_image(smooth_img, texture_img);
            gdImageDestroy(smooth_img);

            if (!textured_img) continue;
            gdImagePtr sepia_img = sepia_image(textured_img);
            gdImageDestroy(textured_img);

            if (!sepia_img) {
                fprintf(stderr, "Final image null for %s\n", file_name);
                continue;
            }

            char out_file[256];
            sprintf(out_file, "%s%s", OUTPUT_DIR, strrchr(file_name, '/') + 1);
            if (!write_jpeg_file(sepia_img, out_file)) {
                fprintf(stderr, "Error saving image %s\n", out_file);
            }
            gdImageDestroy(sepia_img);

            // Update processed count and time
            pthread_mutex_lock(&data->task_queue->mutex);
            data->task_queue->processed_count++;
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            double image_time = (end_time.tv_sec - start_time.tv_sec) +
                                (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
            data->task_queue->total_processing_time += image_time;
            pthread_mutex_unlock(&data->task_queue->mutex);
        }
    }

    gdImageDestroy(texture_img);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    data->elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                         (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    pthread_exit(NULL);
}

/**
 * Monitors statistics when 'S' is pressed.
 */
void *monitor_statistics(void *arg) {
    TaskQueue *queue = (TaskQueue *)arg;

    while (1) {
        printf("Press 'S' to show statistics or 'Q' to quit monitoring:\n");
        char input = getchar();
        if (input == 'S' || input == 's') {
            pthread_mutex_lock(&queue->mutex);
            int remaining = queue->total_files - queue->processed_count;
            double avg_time = queue->processed_count > 0
                                  ? queue->total_processing_time / queue->processed_count
                                  : 0.0;
            pthread_mutex_unlock(&queue->mutex);

            printf("Images processed: %d\n", queue->processed_count);
            printf("Images remaining: %d\n", remaining);
            printf("Average processing time: %.3f seconds\n", avg_time);
        } else if (input == 'Q' || input == 'q') {
            break;
        }
    }
    pthread_exit(NULL);
}

// ============================
//           MAIN
// ============================

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <dir> <n_threads> <-name|-size>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *input_dir = argv[1];
    int n_threads = atoi(argv[2]);
    char *sort_option = argv[3];
    DIR *dir = opendir(input_dir);

    if (!dir) {
        fprintf(stderr, "Error opening directory %s\n", input_dir);
        exit(EXIT_FAILURE);
    }

    if (mkdir(OUTPUT_DIR, 0755) && errno != EEXIST) {
        fprintf(stderr, "Error creating output directory %s\n", OUTPUT_DIR);
        closedir(dir);
        exit(EXIT_FAILURE);
    }

    char **file_list = malloc(1000 * sizeof(char *));
    int file_count = 0;
    struct dirent *entry;

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
    } else if (strcmp(sort_option, "-size") == 0) {
        qsort(file_list, file_count, sizeof(char *), compare_by_size);
    } else {
        fprintf(stderr, "Invalid sorting option. Use -name or -size.\n");
        free(file_list);
        exit(EXIT_FAILURE);
    }

    TaskQueue task_queue = {file_list, file_count, 0, 0, 0.0, PTHREAD_MUTEX_INITIALIZER};

    pthread_t threads[n_threads];
    ThreadData thread_data[n_threads];

    for (int i = 0; i < n_threads; i++) {
        thread_data[i] = (ThreadData){&task_queue, "./paper-texture.png", 0.0};
        if (pthread_create(&threads[i], NULL, process_images, &thread_data[i]) != 0) {
            fprintf(stderr, "Error creating thread %d\n", i + 1);
            exit(EXIT_FAILURE);
        }
    }

    // Start statistics monitoring thread
    pthread_t stats_thread;
    if (pthread_create(&stats_thread, NULL, monitor_statistics, &task_queue) != 0) {
        fprintf(stderr, "Error creating statistics thread\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_cancel(stats_thread);  // Stop monitoring when processing is complete
    pthread_join(stats_thread, NULL);

    for (int i = 0; i < file_count; i++) free(file_list[i]);
    free(file_list);
    return 0;
}

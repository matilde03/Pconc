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

#define MAX_PATH_LENGTH 512

// ============================
//        DATA STRUCTURES
// ============================

/**
 * TaskQueue: A structure representing a queue of tasks (images) to be processed.
 * @files: An array of strings containing the file paths of the images to be processed.
 * @total_files: The total number of image files in the queue.
 * @current_index: The current index in the array of files, indicating the next image to process.
 * @processed_count: The total number of images that have been successfully processed.
 * @failed_count: The total number of images that failed to process.
 * @total_processing_time: The cumulative time spent processing all images, in seconds.
 * @mutex: A mutex to synchronize access to the task queue across multiple threads.
 */
typedef struct {
    char **files;                // Paths to image files
    int total_files;             // Total number of files
    int current_index;           // Next file index to process
    int processed_count;         // Count of successfully processed images
    int failed_count;            // Count of images that failed to process
    double total_processing_time; // Cumulative processing time (in seconds)
    pthread_mutex_t mutex;       // Mutex for thread synchronization
} TaskQueue;

/**
 * ThreadData: A structure representing the data passed to each thread.
 * @task_queue: A pointer to the TaskQueue structure shared by all threads.
 * @texture_file: A string containing the path to the texture file used for processing images.
 * @output_dir: A string containing the path to the directory where processed images will be saved.
 * @elapsed_time: The total time taken by this specific thread to process its assigned images, in seconds.
 */
typedef struct {
    TaskQueue *task_queue;       // Pointer to shared TaskQueue
    char *texture_file;          // Path to texture file
    char *output_dir;            // Directory to save processed images
    double elapsed_time;         // Total processing time for this thread
} ThreadData;

volatile int monitoring = 1;  // Flag to control the monitoring thread

// ============================
//      HELPER FUNCTIONS
// ============================

/**
 * extract_number: Extracts the first integer number found in a string.
 * Useful for sorting files by names containing numeric sequences.
 * @str: Input string.
 * @return: Extracted integer or 0 if no number is found.
 */
int extract_number(const char *str) {
    while (*str && !isdigit(*str)) str++;
    return isdigit(*str) ? atoi(str) : 0;
}

/**
 * compare_by_name: Comparator for sorting file names alphabetically.
 * Sorts based on numeric sequences if present.
 */
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

/**
 * compare_by_size: Comparator for sorting files by size in ascending order.
 */
int compare_by_size(const void *a, const void *b) {
    const char *file1 = *(const char **)a;
    const char *file2 = *(const char **)b;

    struct stat stat1, stat2;
    if (stat(file1, &stat1) != 0 || stat(file2, &stat2) != 0) {
        fprintf(stderr, "Error retrieving file size.\n");
        return 0;
    }
    return (stat1.st_size - stat2.st_size);
}

/**
 * is_image_processed: Verification to check if the processed image has been produced before.
 */       
int is_image_processed(const char *file_name, const char *output_dir) {
    char out_file[MAX_PATH_LENGTH];
    sprintf(out_file, "%s/%s", output_dir, strrchr(file_name, '/') + 1);
    return access(out_file, F_OK) == 0;
}

/**
 * fetch_next_image: Retrieves the next unprocessed image file from the task queue.
 */       
char *fetch_next_image(TaskQueue *queue, const char *output_dir) {
    pthread_mutex_lock(&queue->mutex);
    char *next_image = NULL;

    while (queue->current_index < queue->total_files) {
        char *file_name = queue->files[queue->current_index++];
        if (!is_image_processed(file_name, output_dir)) {
            next_image = file_name;
            break;
        }
    }

    pthread_mutex_unlock(&queue->mutex);
    return next_image;
}

/**
 * process_images: Step by step processing of unprocessed image files.
 */  
void *process_images(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    gdImagePtr texture_img = read_png_file(data->texture_file);
    if (!texture_img) {
        fprintf(stderr, "Error loading texture %s\n", data->texture_file);
        pthread_exit(NULL);
    }

    char *file_name;
    while ((file_name = fetch_next_image(data->task_queue, data->output_dir)) != NULL) {
        gdImagePtr img = read_jpeg_file(file_name);
        if (!img) {
            fprintf(stderr, "Error loading image %s\n", file_name);
            pthread_mutex_lock(&data->task_queue->mutex);
            data->task_queue->failed_count++;
            pthread_mutex_unlock(&data->task_queue->mutex);
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
            pthread_mutex_lock(&data->task_queue->mutex);
            data->task_queue->failed_count++;
            pthread_mutex_unlock(&data->task_queue->mutex);
            continue;
        }

        char out_file[MAX_PATH_LENGTH];
        sprintf(out_file, "%s/%s", data->output_dir, strrchr(file_name, '/') + 1);
        if (!write_jpeg_file(sepia_img, out_file)) {
            fprintf(stderr, "Error saving image %s\n", out_file);
            pthread_mutex_lock(&data->task_queue->mutex);
            data->task_queue->failed_count++;
            pthread_mutex_unlock(&data->task_queue->mutex);
        }
        gdImageDestroy(sepia_img);

        pthread_mutex_lock(&data->task_queue->mutex);
        data->task_queue->processed_count++;
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        data->task_queue->total_processing_time += (end_time.tv_sec - start_time.tv_sec) +
                                                    (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
        pthread_mutex_unlock(&data->task_queue->mutex);
    }

    gdImageDestroy(texture_img);
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    data->elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                         (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    pthread_exit(NULL);
}

/**
 * monitor_statistics: Waits for user input to show statistics data, stops waiting when all files are processed.
 */  
void *monitor_statistics(void *arg) {
    TaskQueue *queue = (TaskQueue *)arg;

    printf("Press 'S' to show statistics or 'Q' to quit monitoring:\n");

    while (1) {
        pthread_mutex_lock(&queue->mutex);

        int remaining = 0;
        for (int i = queue->current_index; i < queue->total_files; i++) {
            if (!is_image_processed(queue->files[i], "./old_photo_PAR_B")) {
                remaining++;
            }
        }

        double avg_time = queue->processed_count > 0
                              ? queue->total_processing_time / queue->processed_count
                              : 0.0;

        pthread_mutex_unlock(&queue->mutex);

        if (remaining <= 0) {
            printf("\nAll images have been processed. Exiting monitoring.\n");
            break;
        }

        // Verifica a entrada do usuário com timeout
        struct timeval timeout = {1, 0}; // Timeout de 1 segundo
        fd_set set;
        FD_ZERO(&set);
        FD_SET(STDIN_FILENO, &set);

        int result = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);

        if (result > 0) {
            char input = getchar();
            if (input == 'S' || input == 's') {
                pthread_mutex_lock(&queue->mutex);

                printf("\n--- Statistics ---\n");
                printf("Images processed: %d\n", queue->processed_count);
                printf("Images failed: %d\n", queue->failed_count);
                printf("Images remaining: %d\n", remaining);
                printf("Average processing time: %.3f seconds\n", avg_time);
                printf("------------------\n");

                printf("\nPress 'S' to show statistics or 'Q' to quit monitoring:\n");

                pthread_mutex_unlock(&queue->mutex);
            } else if (input == 'Q' || input == 'q') {
                printf("\nExiting monitoring by user request.\n");
                break;
            }
        }
    }

    pthread_exit(NULL);
}

/**
 * save_timing_report: Creates and prints to a file all relevant time values.
 */  
void save_timing_report(char *output_dir, const char *sort_option, int n_threads, double total_time, ThreadData *data, int thread_count) {
    char report_name[MAX_PATH_LENGTH];
    sprintf(report_name, "%s/timing_B_%d%s.txt", output_dir, n_threads, strcmp(sort_option, "-name") == 0 ? "-name" : "-size");
    FILE *report = fopen(report_name, "w");
    if (!report) {
        fprintf(stderr, "Error creating report file %s\n", report_name);
        return;
    }

    fprintf(report, "Total execution time: %.3f seconds\n", total_time);  
    for (int i = 0; i < thread_count; i++) {
        fprintf(report, "Thread %d time: %.3f seconds\n", i + 1, data[i].elapsed_time);
    }
    fprintf(report, "Non-parallel execution time: %.3f seconds\n", total_time - data[0].elapsed_time);
    fclose(report);
}

/**
 * free_file_list: Frees the memory allocated for the file list.
 */
void free_file_list(char **file_list, int file_count) {
    for (int i = 0; i < file_count; i++) {
        free(file_list[i]);
    }
    free(file_list);
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <dir> <n_threads> <-name|-size>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *input_dir = argv[1];
    int n_threads = atoi(argv[2]);
    if (n_threads <= 0) {
        fprintf(stderr, "Error: Number of threads must be positive\n");
        exit(EXIT_FAILURE);
    }

    char *sort_option = argv[3];
    if (strcmp(sort_option, "-name") != 0 && strcmp(sort_option, "-size") != 0) {
        fprintf(stderr, "Error: Sorting option must be -name or -size\n");
        exit(EXIT_FAILURE);
    }

    char output_dir[MAX_PATH_LENGTH];
    sprintf(output_dir, "%s/old_photo_PAR_B", input_dir);

    DIR *dir = opendir(input_dir);
    if (!dir) {
        fprintf(stderr, "Error opening directory %s\n", input_dir);
        exit(EXIT_FAILURE);
    }

    if (mkdir(output_dir, 0755) && errno != EEXIST) {
        fprintf(stderr, "Error creating output directory %s\n", output_dir);
        closedir(dir);
        exit(EXIT_FAILURE);
    }

    size_t file_list_capacity = 10;
    char **file_list = malloc(file_list_capacity * sizeof(char *));
    if (!file_list) {
        fprintf(stderr, "Memory allocation error for file list\n");
        closedir(dir);
        exit(EXIT_FAILURE);
    }
    int file_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir))) {
        if (strstr(entry->d_name, ".jpeg")) {
            if (file_count == file_list_capacity) {
                file_list_capacity *= 2;
                char **temp = realloc(file_list, file_list_capacity * sizeof(char *));
                if (!temp) {
                    fprintf(stderr, "Memory reallocation error for file list\n");
                    free_file_list(file_list, file_count);
                    closedir(dir);
                    exit(EXIT_FAILURE);
                }
                file_list = temp;
            }
            char *file_path = malloc(strlen(input_dir) + strlen(entry->d_name) + 2);
            if (!file_path) {
                fprintf(stderr, "Memory allocation error for file path\n");
                free_file_list(file_list, file_count);
                closedir(dir);
                exit(EXIT_FAILURE);
            }
            sprintf(file_path, "%s/%s", input_dir, entry->d_name);
            file_list[file_count++] = file_path;
        }
    }
    closedir(dir);

    if (strcmp(sort_option, "-name") == 0) {
        qsort(file_list, file_count, sizeof(char *), compare_by_name);
    } else if (strcmp(sort_option, "-size") == 0) {
        qsort(file_list, file_count, sizeof(char *), compare_by_size);
    }

    TaskQueue task_queue = {file_list, file_count, 0, 0, 0, 0.0, PTHREAD_MUTEX_INITIALIZER};
    pthread_t threads[n_threads];
    ThreadData thread_data[n_threads];

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (int i = 0; i < n_threads; i++) {
        thread_data[i] = (ThreadData){&task_queue, "./paper-texture.png", output_dir, 0.0};
        if (pthread_create(&threads[i], NULL, process_images, &thread_data[i]) != 0) {
            fprintf(stderr, "Error creating thread %d\n", i + 1);
            for (int j = 0; j < i; j++) pthread_join(threads[j], NULL);
            free_file_list(file_list, file_count);
            pthread_mutex_destroy(&task_queue.mutex);
            exit(EXIT_FAILURE);
        }
    }

    pthread_t stats_thread;
    if (pthread_create(&stats_thread, NULL, monitor_statistics, &task_queue) != 0) {
        fprintf(stderr, "Error creating statistics thread\n");
        for (int i = 0; i < n_threads; i++) pthread_join(threads[i], NULL);
        free_file_list(file_list, file_count);
        pthread_mutex_destroy(&task_queue.mutex);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    monitoring = 0;
    pthread_join(stats_thread, NULL);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double total_time = (end_time.tv_sec - start_time.tv_sec) +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    save_timing_report(input_dir, sort_option, n_threads, total_time, thread_data, n_threads);

    pthread_mutex_destroy(&task_queue.mutex);
    free_file_list(file_list, file_count);
    return 0;
}

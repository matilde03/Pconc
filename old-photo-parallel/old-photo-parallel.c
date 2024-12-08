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

// ============================
//        DATA STRUCTURES
// ============================

/**
 * ThreadData: Stores data for each thread's image processing tasks.
 * @files: List of image file paths.
 * @start: Start index in the file list for the thread.
 * @end: End index in the file list for the thread.
 * @texture_file: Path to the texture file used in processing.
 * @elapsed_time: Time taken by the thread to process its tasks.
 */
typedef struct {
    char **files;
    int start;
    int end;
    char *texture_file;
    double elapsed_time;
} ThreadData;

// ============================
//        HELPER FUNCTIONS
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
 * is_image_processed: Checks if an image has already been processed.
 * @file_name: Path to the image file.
 * @return: 1 if processed, 0 otherwise.
 */
int is_image_processed(const char *file_name) {
    char out_file[256];
    sprintf(out_file, "%s%s", OUTPUT_DIR, strrchr(file_name, '/') + 1);
    return access(out_file, F_OK) == 0;
}

// ============================
//      THREAD FUNCTIONS
// ============================

/**
 * process_images: Processes a subset of images assigned to a thread.
 * Applies image effects and saves the processed images.
 * @arg: Pointer to ThreadData structure.
 */
void *process_images(void *arg) {
    ThreadData *data = (ThreadData *)arg;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    gdImagePtr in_texture_img = read_png_file(data->texture_file);
    if (!in_texture_img) {
        fprintf(stderr, "Error loading texture %s\n", data->texture_file);
        pthread_exit(NULL);
    }

    for (int i = data->start; i < data->end; i++) {
        char *file_name = data->files[i];

        if (is_image_processed(file_name)) {
            continue;
        }

        gdImagePtr in_img = read_jpeg_file(file_name);
        if (!in_img) {
            fprintf(stderr, "Error loading image %s\n", file_name);
            continue;
        }

        // Apply transformations: contrast -> smooth -> texture -> sepia
        gdImagePtr out_contrast_img = contrast_image(in_img);
        gdImageDestroy(in_img);

        if (!out_contrast_img) continue;
        gdImagePtr out_smoothed_img = smooth_image(out_contrast_img);
        gdImageDestroy(out_contrast_img);

        if (!out_smoothed_img) continue;
        gdImagePtr out_textured_img = texture_image(out_smoothed_img, in_texture_img);
        gdImageDestroy(out_smoothed_img);

        if (!out_textured_img) continue;
        gdImagePtr out_sepia_img = sepia_image(out_textured_img);
        gdImageDestroy(out_textured_img);

        if (!out_sepia_img) {
            fprintf(stderr, "Final image null for %s\n", file_name);
            continue;
        }

        // Save processed image
        char out_file[256];
        sprintf(out_file, "%s%s", OUTPUT_DIR, strrchr(file_name, '/') + 1);
        if (!write_jpeg_file(out_sepia_img, out_file)) {
            fprintf(stderr, "Error saving image %s\n", out_file);
        }
        gdImageDestroy(out_sepia_img);
    }

    gdImageDestroy(in_texture_img);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    data->elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                         (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    pthread_exit(NULL);
}

// ============================
//     RESOURCE MANAGEMENT
// ============================

/**
 * free_resources: Frees allocated memory and closes open directories.
 * @file_list: List of file paths to be freed.
 * @file_count: Number of files in the list.
 * @dir: Directory pointer to be closed (if not NULL).
 */
void free_resources(char **file_list, int file_count, DIR *dir) {
    if (dir) closedir(dir);
    if (file_list) {
        for (int i = 0; i < file_count; i++) {
            free(file_list[i]);
        }
        free(file_list);
    }
}

// ============================
//           MAIN
// ============================

/**
 * main: Entry point for the program. Reads input, spawns threads,
 * and processes images using parallelism.
 */
// ============================
//           MAIN
// ============================

/**
 * main: Entry point for the program. Reads input, spawns threads,
 * and processes images using parallelism or sequentially.
 */
int main(int argc, char *argv[]) {
    struct timespec start_time_total, start_time_seq, end_time_total, end_time_seq;
    clock_gettime(CLOCK_MONOTONIC, &start_time_total);
    clock_gettime(CLOCK_MONOTONIC, &start_time_seq);

    // Validate input arguments
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
    if (!file_list) {
        fprintf(stderr, "Memory allocation failed for file_list\n");
        closedir(dir);
        exit(EXIT_FAILURE);
    }

    // Read directory and filter .jpeg files
    int file_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strstr(entry->d_name, ".jpeg")) {
            char *file_path = malloc(strlen(input_dir) + strlen(entry->d_name) + 2);
            if (!file_path) {
                fprintf(stderr, "Memory allocation failed for file_path\n");
                free_resources(file_list, file_count, dir);
                exit(EXIT_FAILURE);
            }
            sprintf(file_path, "%s/%s", input_dir, entry->d_name);
            file_list[file_count++] = file_path;
        }
    }
    closedir(dir);

    if (file_count == 0) {
        fprintf(stderr, "No .jpeg files found in directory %s\n", input_dir);
        free_resources(file_list, file_count, NULL);
        exit(EXIT_FAILURE);
    }

    // Sort files based on user input
    if (strcmp(sort_option, "-name") == 0) {
        qsort(file_list, file_count, sizeof(char *), compare_by_name);
    } else if (strcmp(sort_option, "-size") == 0) {
        qsort(file_list, file_count, sizeof(char *), compare_by_size);
    } else {
        fprintf(stderr, "Invalid sorting option. Use -name or -size.\n");
        free_resources(file_list, file_count, NULL);
        exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time_seq);

    // ==========================
    // PROCESSAMENTO PARALELO
    // ==========================
    if (n_threads > file_count) n_threads = file_count;

    pthread_t threads[n_threads];
    ThreadData thread_data[n_threads];
    int images_per_thread = file_count / n_threads;
    int remaining_images = file_count % n_threads;

    // Create threads
    for (int i = 0; i < n_threads; i++) {
        thread_data[i].files = file_list;
        thread_data[i].start = i * images_per_thread;
        thread_data[i].end = (i + 1) * images_per_thread;
        if (i == n_threads - 1) thread_data[i].end += remaining_images;
        thread_data[i].texture_file = "./paper-texture.png";
        thread_data[i].elapsed_time = 0.0;

        if (pthread_create(&threads[i], NULL, process_images, &thread_data[i]) != 0) {
            fprintf(stderr, "Error creating thread %d\n", i + 1);
            for (int j = 0; j < i; j++) {
                pthread_cancel(threads[j]);
            }
            free_resources(file_list, file_count, NULL);
            exit(EXIT_FAILURE);
        }
    }

    // Wait for threads to complete
    for (int i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Write log
    FILE *log_file;
    char log_file_name[256];
    sprintf(log_file_name, "%s/timing_%d-%s.txt", OUTPUT_DIR, n_threads, sort_option + 1);
    log_file = fopen(log_file_name, "w");
    if (!log_file) {
        fprintf(stderr, "Error creating log file\n");
        free_resources(file_list, file_count, NULL);
        exit(EXIT_FAILURE);
    }

    // Write parallel execution times for each thread (as before)
    for (int i = 0; i < n_threads; i++) {
        fprintf(log_file, "Thread %d: %.3f seconds\n", i + 1, thread_data[i].elapsed_time);
    }

    // Log the total time (sequential and parallel)
    clock_gettime(CLOCK_MONOTONIC, &end_time_total);
    double sequential_time = (end_time_seq.tv_sec - start_time_seq.tv_sec) +
                        (end_time_seq.tv_nsec - start_time_seq.tv_nsec) / 1e9;

    double total_time = (end_time_total.tv_sec - start_time_total.tv_sec) +
                        (end_time_total.tv_nsec - start_time_total.tv_nsec) / 1e9;

    fprintf(log_file, "Total sequential execution time: %.3f seconds\n", sequential_time);
    fprintf(log_file, "Total program execution time: %.3f seconds\n", total_time);
    fclose(log_file);

    free_resources(file_list, file_count, NULL);

    return 0;
}

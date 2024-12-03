/******************************************************************************
 * Programacao Concorrente
 * MEEC 21/22
 *
 * Projecto - Parte1
 *                           serial-complexo.c
 * 
 * Compilacao: gcc serial-complexo -o serial-complex -lgd
 *           
 *****************************************************************************/

#include <gd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <time.h>
#include "image-lib.h"

/* the directories wher output files will be placed */
#define OLD_IMAGE_DIR "./Old-image-dir/"

/******************************************************************************
 * main()
 *
 * Arguments: (none)
 * Returns: 0 in case of sucess, positive number in case of failure
 * Side-Effects: creates thumbnail, resized copy and watermarked copies
 *               of images
 *
 * Description: implementation of the complex serial version 
 *              This application only works for a fixed pre-defined set of files
 *
 *****************************************************************************/
int main(){
    struct timespec start_time_total, end_time_total;
    struct timespec start_time_seq, end_time_seq;
    struct timespec start_time_par, end_time_par;



	clock_gettime(CLOCK_MONOTONIC, &start_time_total);
	clock_gettime(CLOCK_MONOTONIC, &start_time_seq);

	/* array containg the names of files to be processed	 */
	char * files [] =  {"img-IST-0.jpeg", "img-IST-1.jpeg", "img-IST-2.jpeg", "img-IST-3.jpeg", "img-IST-4.jpeg", "img-IST-5.jpeg", "img-IST-6.jpeg", "img-IST-7.jpeg", "img-IST-8.jpeg", "img-IST-9.jpeg"};
	/* length of the files array (number of files to be processed	 */
	int nn_files = 10;


	/* file name of the image created and to be saved on disk	 */
	char out_file_name[100];

	/* input images */
	gdImagePtr in_img;
	/* output images */
	gdImagePtr out_smoothed_img;
	gdImagePtr out_contrast_img;
	gdImagePtr out_textured_img;
	gdImagePtr out_sepia_img;

	/* creation of output directories */
	if (create_directory(OLD_IMAGE_DIR) == 0){
		fprintf(stderr, "Impossible to create %s directory\n", OLD_IMAGE_DIR);
		exit(-1);
	}

	gdImagePtr in_texture_img =  read_png_file("./paper-texture.png");

	clock_gettime(CLOCK_MONOTONIC, &end_time_seq);
	clock_gettime(CLOCK_MONOTONIC, &start_time_par);

	/* Iteration over all the files to resize images
	 */
	for (int i = 0; i < nn_files; i++){	

		printf("image %s\n", files[i]);
		/* load of the input file */
	    in_img = read_jpeg_file(files[i]);
		if (in_img == NULL){
			fprintf(stderr, "Impossible to read %s image\n", files[i]); 
			continue;
		}

		out_contrast_img = contrast_image(in_img);
		out_smoothed_img = smooth_image(out_contrast_img);
		out_textured_img = texture_image(out_smoothed_img , in_texture_img);
		out_sepia_img = sepia_image(out_textured_img); 

		/* save resized */ 
		sprintf(out_file_name, "%s%s", OLD_IMAGE_DIR, files[i]);
		if(write_jpeg_file(out_sepia_img, out_file_name) == 0){
			fprintf(stderr, "Impossible to write %s image\n", out_file_name);
		}
		gdImageDestroy(out_smoothed_img);
		gdImageDestroy(out_sepia_img);
		gdImageDestroy(out_contrast_img);
		gdImageDestroy(in_img);
	}

	clock_gettime(CLOCK_MONOTONIC, &end_time_par);
	clock_gettime(CLOCK_MONOTONIC, &end_time_total);


struct timespec par_time = diff_timespec(&end_time_par, &start_time_par);
struct timespec seq_time = diff_timespec(&end_time_seq, &start_time_seq);
struct timespec total_time = diff_timespec(&end_time_total, &start_time_total);
    printf("\tseq \t %10jd.%09ld\n", seq_time.tv_sec, seq_time.tv_nsec);
    printf("\tpar \t %10jd.%09ld\n", par_time.tv_sec, par_time.tv_nsec);
    printf("total \t %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);


	exit(0);
}


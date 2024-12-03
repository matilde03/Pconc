/******************************************************************************
 * Programacao Concorrente
 * MEEC 22/23
 *
 * Projecto - Parte A
 *                           apply_smooth.c
 * 
 * Compilacao: gcc apply_smooth.c -o apply_smooth -lgd
 *           
 *****************************************************************************/

#include <gd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include "image-lib.h"

/* the directories wher output files will be placed */
#define SMOOTH_DIR "./Smooth-dir/"

/******************************************************************************
 * main()
 *
 * Arguments: (none)
 * Returns: 0 in case of success, positive number in case of failure
 * Side-Effects: creates a copy of original images , but smoothed
 *
 * Description: Example of application of image transformation
 *              This application only works for a fixed pre-defined set of files
 *
 *****************************************************************************/
int main(){

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

	/* creation of output directories */
	if (create_directory(SMOOTH_DIR) == 0){
		fprintf(stderr, "Impossible to create %s directory\n", SMOOTH_DIR);
		exit(-1);
	}

	/* Iteration over all the files to smooth them
	 */
	for (int i = 0; i < nn_files; i++){	

		printf("smooth %s\n", files[i]);
		/* load of the input file */
	    in_img = read_jpeg_file(files[i]);
		if (in_img == NULL){
			fprintf(stderr, "Impossible to read %s image\n", files[i]);
			continue;
		}
		/* smooths  each image */
		out_smoothed_img = smooth_image(in_img);
  		if (out_smoothed_img == NULL) {
            fprintf(stderr, "Impossible to smooth %s image\n", files[i]);
        }else{
			/* save smoothed image */
			sprintf(out_file_name, "%s%s", SMOOTH_DIR, files[i]);
			if(write_jpeg_file(out_smoothed_img, out_file_name) == 0){
	            fprintf(stderr, "Impossible to write %s image\n", out_file_name);
			}
			gdImageDestroy(out_smoothed_img);
		}
		gdImageDestroy(in_img);
	}

	exit(0);
}


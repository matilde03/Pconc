/******************************************************************************
 * Programacao Concorrente
 * MEEC 22/23
 *
 * Projecto - Parte A
 *                           apply_contrast.c
 * 
 * Compilacao: gcc apply_contrast.c -o apply_contrast -lgd
 *           
 *****************************************************************************/

#include <gd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include "image-lib.h"

/* the directories where output files will be placed */
#define CONTRAST_DIR "./Contrast-dir/"

/******************************************************************************
 * main()
 *
 * Arguments: (none)
 * Returns: 0 in case of success, positive number in case of failure
 * Side-Effects: creates a copy of images with higher contrast
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
	gdImagePtr out_high_contrats_img;

	/* creation of output directories */
	if (create_directory(CONTRAST_DIR) == 0){
		fprintf(stderr, "Impossible to create %s directory\n", CONTRAST_DIR);
		exit(-1);
	}

	/* Iteration over all the files to increase contrats
	 */
	for (int i = 0; i < nn_files; i++){	

		printf("contrast %s\n", files[i]);
		/* load of the input file */
	    in_img = read_jpeg_file(files[i]);
		if (in_img == NULL){
			fprintf(stderr, "Impossible to read %s image\n", files[i]);
			continue;
		}
		/* increase contrast of each image */
		out_high_contrats_img = contrast_image(in_img); 
  		if (out_high_contrats_img == NULL) {
            fprintf(stderr, "Impossible to increase contrast to %s image\n", files[i]);
        }else{
			/* save high contrast image */
			sprintf(out_file_name, "%s%s", CONTRAST_DIR, files[i]);
			if(write_jpeg_file(out_high_contrats_img, out_file_name) == 0){
	            fprintf(stderr, "Impossible to write %s image\n", out_file_name);
			}
			gdImageDestroy(out_high_contrats_img);
		}
		gdImageDestroy(in_img);
	}

	exit(0);
}


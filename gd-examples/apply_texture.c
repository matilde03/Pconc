/******************************************************************************
 * Programacao Concorrente
 * MEEC 22/23
 *
 * Projecto - Parte A
 *                           apply_texture.c
 * 
 * Compilacao: gcc apply_texture.c -o apply_texture -lgd
 *           
 *****************************************************************************/

#include "gd.h"
#include "image-lib.h"

/* the directories wher output files will be placed */
#define TEXTURE_DIR "./Texture-dir/"

/******************************************************************************
 * main()
 *
 * Arguments: (none)
 * Returns: 0 in case of success, positive number in case of failure
 * Side-Effects: creates a copy of images with an texture applied
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
	gdImagePtr in_img,  in_texture_img;
	/* output images */
	gdImagePtr out_texture_img;

	/* creation of output directories */
	if (create_directory(TEXTURE_DIR) == 0){
		fprintf(stderr, "Impossible to create %s directory\n", TEXTURE_DIR);
		exit(-1);
	}

    in_texture_img = read_png_file("paper-texture.png");
	if(in_texture_img == NULL){
		fprintf(stderr, "Impossible to read %s image\n", "paper-texture.png");
		exit(-1);
	}


	/* Iteration over all the files to apply texture
	 */
	for (int i = 0; i < nn_files; i++){	

	    printf("texture  %s\n", files[i]);
		/* load of the input file */
	    in_img = read_jpeg_file(files[i]);
		if (in_img == NULL){
			fprintf(stderr, "Impossible to read %s image\n", files[i]);
			continue;
		}

		/* apply texture */
		out_texture_img = texture_image(in_img, in_texture_img);
  		if (out_texture_img == NULL) {
            fprintf(stderr, "Impossible to creat thumbnail of %s image\n", files[i]);
        }else{
			/* save rextured image */
			sprintf(out_file_name, "%s%s", TEXTURE_DIR, files[i]);
			if(write_jpeg_file(out_texture_img, out_file_name) == 0){
	            fprintf(stderr, "Impossible to write %s image\n", out_file_name);
			}
			gdImageDestroy(out_texture_img);
		}
		gdImageDestroy(in_img);

	}

	gdImageDestroy(in_texture_img);
	exit(0);
}


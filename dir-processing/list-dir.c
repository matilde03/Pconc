#include <dirent.h> 
#include <stdio.h> 
#include <sys/stat.h>

int main(void) {
  DIR *d;
  struct dirent *f;
  d = opendir(".");
  if (d) {
    while ((f = readdir(d)) != NULL) {
        printf("%s ", f->d_name);
        struct stat st;
        stat(f->d_name, &st);
        printf("%ld\n", st.st_size);
    }
    closedir(d);
  }
  return(0);
}
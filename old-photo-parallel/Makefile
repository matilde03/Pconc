all: old-photo-parallel

old-photo-parallel: old-photo-parallel.c image-lib.c image-lib.h
	gcc old-photo-parallel.c image-lib.c image-lib.h -g -o old-photo-parallel -lgd

clean:
	rm old-photo-parallel

clean_all: clean
	rm -fr ./*-dir

run_all: all
	./old-photo-parallel

all: old-photo-serial

old-photo-serial: old-photo-serial.c image-lib.c image-lib.h
	gcc old-photo-serial.c image-lib.c image-lib.h -g -o old-photo-serial -lgd

clean:
	rm old-photo-serial

clean_all: clean
	rm -fr ./*-dir

run_all: all
	./old-photo-serial

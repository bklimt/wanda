
all: bin/wanda

obj/wanda.o: wanda.cc
	cc -c -o obj/wanda.o `pkg-config --cflags MagickWand` wanda.cc

bin/wanda: obj/wanda.o
	cc -o bin/wanda `pkg-config --libs MagickWand` -lstdc++ obj/wanda.o -lgflags -lglog

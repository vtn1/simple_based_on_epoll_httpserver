objects=main.o
myhttp:$(objects)
	gcc -o myhttp $(objects) -lpthread
main.o:main.c
	gcc -c main.c
clean:
	rm myhttp main.o

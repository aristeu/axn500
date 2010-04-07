VERSION := "0.1"

polar: polar.c
	gcc -Wall -o polar -DVERSION=\"$(VERSION)\" polar.c

clean:
	rm -f *.o polar


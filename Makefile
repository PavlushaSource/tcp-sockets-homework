make:
	gcc sockets.c -o sockets && ./sockets

clean:
	rm -f sockets

run:
	./sockets

.PHONY: main clean run
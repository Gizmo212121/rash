rash: main.c
	gcc -g -Wall -Wextra -pedantic -Werror main.c -o rash
clean:
	rm rash
run:
	./rash
run-debug:
	valgrind --leak-check=full --track-origins=yes ./rash

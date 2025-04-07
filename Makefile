rash: main2.c
	gcc -g -Wall -Wextra -pedantic -Werror main2.c -o rash
clean:
	rm rash
run:
	valgrind --leak-check=full --track-origins=yes ./rash

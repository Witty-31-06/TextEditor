main: main.c
	$(CC) main.c -o main -Wall -Wextra -pedantic -Werror -fsanitize=address -fdiagnostics-color -std=c99

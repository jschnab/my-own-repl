#include <stdio.h>
#include <stdlib.h>

// if we are compiling on Windows
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

// fake readline function
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strl(cpy)-1] = '\0';
    return cpy;
}

// fake add_history function
void add_history(char* unused) {}

// if we are on MacOS or Linux
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

int main(int argc, char* argv[]) {
    
    puts("REPL version 0.0.1");
    puts("Press Ctrl+c to exit\n");

    while(1){
        char* input = readline("repl> ");

        add_history(input);

        printf("No you're a %s\n", input);

        free(input);
    }

    return 0;
}

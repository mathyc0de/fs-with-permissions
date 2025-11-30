// cmd.c
#include "core_utils.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_INPUT 256



int main() {
    int current_inode = 0; // inode raiz
    char input[MAX_INPUT];
    if (start_fs() != 0 || try_login() != 0) return -1;


    printf("MiniFS Terminal. Digite 'exit' para sair.\n");

    while (1) {
        printf("%s@[%s]> ", username, inode_table[current_inode].name); 
        if (!fgets(input, MAX_INPUT, stdin)) break;

        // Remove \n final
        input[strcspn(input, "\n")] = 0;

        // Sair
        if (strcmp(input, "exit") == 0) break;

        // Parse do comando
        char *cmd = strtok(input, " ");
        char *arg1 = strtok(NULL, " ");
        char *arg2 = strtok(NULL, " ");
        char *arg3 = strtok(NULL, "");

        if (!cmd) continue;
        int handled = 0;
        

        for (int i = 0; i < command_count; i++) {
            if (strcmp(cmd, commands[i].name) == 0) {
                commands[i].fn(
                    &current_inode,
                    arg1, arg2, arg3,
                    authenticated_uid
                );
                handled = 1;
                break;
            }
        }

        if (!handled) {
            printf("Comando não reconhecido: %s\n", cmd);
        }
    }

    printf("Saindo...\n");
    unmount_fs();
    return 0;
}


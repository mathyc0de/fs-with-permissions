// cmd.c
#include "core_utils.h"
#include "fs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_INPUT 256

int start_fs() {
    if (access(DISK_NAME, F_OK) == 0) {
    // Disco existe → montar
        if (mount_fs() != 0) {
            fprintf(stderr, "Erro ao montar o filesystem!\n");
            return -1;
        }
    } 
    else {
        // Disco não existe → criar
        if (init_fs() != 0) {
            fprintf(stderr, "Erro ao inicializar o filesystem!\n");
            return -1;
        }
        // Criar o root caso seja a primeira execução do disco
        create_root();
        if (create_user() != 0) {
            remove(DISK_NAME);
            fprintf(stderr, "Erro ao inicializar o filesystem!\n");
            return -1;
        }
    }
}

int try_login() {
    char username[MAX_NAMESIZE], password[MAX_PASSWORD_SIZE];
    while (!user) {
        // Input do Usuário
        printf("Usuário: ");
        fgets(username, MAX_NAMESIZE, stdin);
        printf("\nSenha: ");
        fgets(password, MAX_PASSWORD_SIZE, stdin);
        printf("\n");

        login(username, password);

    }
}



int main() {
    int current_inode = 0; // inode raiz
    char input[MAX_INPUT];
    if (start_fs() != 0 || try_login() != 0) return -1;


    



    printf("MiniFS Terminal. Digite 'exit' para sair.\n");

    while (1) {
        printf("%s@[%s]> ", user, inode_table[current_inode].name); 
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
        //char *arg4 = strtok(NULL, " ");

        if (!cmd) continue;
        

        if (strcmp(cmd, "cd") == 0 && arg1) {
            cmd_cd(&current_inode, arg1);
        }
        else if (strcmp(cmd, "mkdir") == 0 && arg1) {
            cmd_mkdir(current_inode, arg1, user);
        }
        else if (strcmp(cmd, "touch") == 0 && arg1) {
            cmd_touch(current_inode, arg1, user);
        }
        else if (strcmp(cmd, "rm") == 0 && arg1) {
            cmd_rm(current_inode, arg1, user);
        }
        else if (strcmp(cmd, "clear") == 0) {
            system("clear");
        }
        else if (strcmp(cmd, "rmdir") == 0 && arg1) {
            cmd_rmdir(current_inode, arg1, user);
        }
        else if (strcmp(cmd, "echo") == 0 && arg1) {
            char *content = arg1;
            char *redir = arg2; // > ou >>
            char *filename = arg3;

            if (redir && filename && content) {
                if (strcmp(redir, ">") == 0) {
                    cmd_echo_arrow(current_inode, filename, content, user);
                }
                else if (strcmp(redir, ">>") == 0) {
                    cmd_echo_arrow_arrow(current_inode, filename, content, user);
                }
            } else {
                printf("Falha: echo conteudo >|>> arquivo\n");
            }
            
        }
        else if (strcmp(cmd, "cat") == 0 && arg1) {
            char* content;
            if (cmd_cat(current_inode, arg1, user, content) != -1) {
                printf("%s\n", content);
            };
            free(content);
        }
        else if (strcmp(cmd, "ls") == 0) {
            if (arg1 && strcmp(arg1, "-l") == 0) {
                cmd_ls(current_inode, arg2? arg2 : ".", user, 1);
            }
            else {
                cmd_ls(current_inode, arg1? arg1 : ".", user, 0);
            }
        }
        else if (strcmp(cmd, "cp") == 0 && arg1 && arg2) {
            cmd_cp(current_inode, ".", arg1, ".", arg2, user);
        }
        else if (strcmp(cmd, "mv") == 0 && arg1 && arg2) {
            cmd_mv(current_inode, ".", arg1, ".", arg2, user);
        }
        else if (strcmp(cmd, "ln") == 0 && arg1 && strcmp(arg1, "-s") == 0 && arg2 && arg3) {
            cmd_ln_s(current_inode, arg2, arg3, user);
        }
        else if (strcmp(cmd, "su") == 0 && arg1){
            strncpy(user, arg1, 10);
        }
        else if (strcmp(cmd, "unlink") == 0 && arg1){
            cmd_unlink(current_inode, arg1, user);
        }
        else if (strcmp(cmd, "df") == 0){
            cmd_df();
        }
        else {
            printf("Comando não reconhecido\n");
        }
    }

    printf("Saindo...\n");
    unmount_fs();
    return 0;
}


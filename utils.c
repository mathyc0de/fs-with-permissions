#include "utils.h"
#include "fs.h"
#include "core_utils.h"
#include <unistd.h>
#include <sys/random.h>
#include <stdio.h>
#include <crypt.h>

char username[32];
Command commands[] = {
    {"cd",      cmd_cd},
    {"mkdir",   cmd_mkdir},
    {"touch",   cmd_touch},
    {"rm",      cmd_rm},
    {"clear",   cmd_clear},
    {"rmdir",   cmd_rmdir},
    {"echo",    cmd_echo},
    {"cat",     cmd_cat},
    {"ls",      cmd_ls},
    {"cp",      cmd_cp},
    {"mv",      cmd_mv},
    {"ln",      cmd_ln},
    {"sudo",    cmd_sudo},
    {"unlink",  cmd_unlink},
    {"df",      cmd_df},
    {"chmod",   cmd_chmod},
    {"chown",   cmd_chown},
    {"create-user", cmd_create_user}
};

const int command_count = sizeof(commands) / sizeof(commands[0]);


const char table[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";


int gen_salt(char *out, size_t length) {
    // salt aleatório gerado para evitar que a senha seja decodificada com brute force por tentativa.
    unsigned char buf[length];
    getrandom(buf, length, 0);

    for (size_t i = 0; i < length; i++)
        out[i] = table[buf[i] % 64];

    out[length] = '\0';

    return 0;
}

int encrypt_password(char password[MAX_PASSWORD_SIZE], char out_buffer[MAX_HASH_SIZE]) {
    // gera um hash da senha
    char salt_body[16];
    gen_salt(salt_body, sizeof(salt_body));

    char salt[32];
    snprintf(salt, sizeof(salt), "$6$%s$", salt_body);
    const char* hash = crypt(password, salt);
    strncpy(out_buffer, hash, MAX_HASH_SIZE - 1);
    out_buffer[MAX_HASH_SIZE - 1] = '\0';
    
    if (!out_buffer) {
        perror("crypt");
        return 1;
    }

    return 0;
}


int start_fs() {
    // Monta o disco
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
    return 0;
}

int try_login() {
    printf("------------ Login -----------\n\n");
    char password[MAX_PASSWORD_SIZE];
    while (authenticated_uid == -1) {
        // Input do Usuário
        printf("Usuário: ");
        fgets(username, MAX_NAMESIZE, stdin);
        username[strcspn(username, "\n")] = '\0'; // remove o '\n' da string
        int uid = assert_user_exists(username); // verifica se o usuário existe antes de prosseguir
        if (uid == -1) {
            printf("\nUsuário não encontrado!\n");
            continue;
        }
        printf("Senha: ");
        fgets(password, MAX_PASSWORD_SIZE, stdin);
        password[strcspn(password, "\n")] = '\0';
        login(username, password, uid);

    }
    return 0;
}
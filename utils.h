#ifndef UTILS_H
#define UTILS_H
#include "core_utils.h"

typedef void (*command_handler)(
    int *current_inode,
    const char *a1,
    const char *a2,
    const char *a3,
    int authenticated_uid
);

typedef struct {
    const char *name;
    command_handler fn;
} Command;


int encrypt_password(char password[MAX_PASSWORD_SIZE], char out_buffer[MAX_HASH_SIZE]);
int start_fs();
int try_login();

extern Command commands[];
extern char username[32];
extern const int command_count;

#endif
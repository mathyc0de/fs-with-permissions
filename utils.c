#include "utils.h"
#include <unistd.h>
#include <sys/random.h>
#include <stdio.h>

const char table[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

int gen_salt(char *out, size_t length) {
    unsigned char buf[length];
    getrandom(buf, length, 0);

    for (size_t i = 0; i < length; i++)
        out[i] = table[buf[i] % 64];

    out[length] = '\0';

    return 0;
}

int encrypt_password(char* password, char* out_buffer) {
    // FALTA FAZER
    printf("Falta implementar isso aqui\n");
    return 0;
}


#ifndef CORE_UTILS_H
#define CORE_UTILS_H
#include "fs.h"

#define ROOT_UID 0
#define MAX_NAMESIZE 32
#define MAX_PASSWORD_SIZE 32
#define MAX_HASH_SIZE 128
#define MAX_UID_SIZE 3
#define MAX_PASSWD_ENTRY MAX_NAMESIZE + MAX_UID_SIZE + 3 // NOME(32):x:UID(3)> = 32 + 6
#define MAX_SHADOW_ENTRY 256


// typedef struct
// {
//     uint32_t uid;
//     char name[MAX_NAMESIZE];
// } user_t;



// int cmd_cd(int *current_inode, const char *path);
// int cmd_cat(int current_inode, const char *path, int user_id, char* buffer);
// int cmd_mkdir(int current_inode, const char *fullpath, int user_id);
// int cmd_touch(int current_inode, const char *fullpath, int user_id);
// int cmd_echo(int current_inode, const char *fullpath, const char *content, int user_id);
// // int cmd_echo_arrow_arrow(int current_inode, const char *fullpath, const char *content, int user_id);
// int cmd_clear();
// int cmd_ln();
// int cmd_su();
// int cmd_cp(int current_inode, const char *src_path, const char *src_name,
//            const char *dst_path, const char *dst_name, int user_id);
// int cmd_mv(int current_inode, const char *src_path, const char *src_name,
//            const char *dst_path, const char *dst_name, int user_id);
// int cmd_ln_s(int current_inode, const char *target_path, const char *link_path, int user_id);
// int cmd_ls(int current_inode, const char *path, int user_id, int info_args);
// int cmd_rm(int current_inode, const char *filepath, int user_id);
// int cmd_rmdir(int current_inode, const char *filepath, int user_id);
// int cmd_unlink(int current_inode, const char *filepath, int user_id);
// int cmd_df(void);

void cmd_cd(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid);
void cmd_mkdir(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid);
void cmd_touch(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid);
void cmd_rm(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid);
void cmd_clear(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid);
void cmd_rmdir(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid);
void cmd_echo(int *current_inode, const char *content, const char *redir, const char *filename, int uid);
void cmd_cat(int *current_inode, const char *file, const char *arg2, const char *arg3, int uid);
void cmd_ls(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid);
void cmd_cp(int *current_inode, const char *src, const char *dst, const char *arg3, int uid);
void cmd_mv(int *current_inode, const char *src, const char *dst, const char *arg3, int uid);
void cmd_ln(int *current_inode, const char *opt, const char *src, const char *dst, int uid);
void cmd_su(int *current_inode, const char *user, const char *arg2, const char *arg3, int uid);
void cmd_unlink(int *current_inode, const char *f, const char *arg2, const char *arg3, int uid);
void cmd_df(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid);



int create_root();
int create_user();
int login(const char* username, const char* password, int uid);
int assert_user_exists(char* username);


extern int authenticated_uid;

#endif
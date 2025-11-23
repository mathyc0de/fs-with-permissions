#ifndef CORE_UTILS_H
#define CORE_UTILS_H

int cmd_cd(int *current_inode, const char *path);
int cmd_cat(int current_inode, const char *path, int user_id);
int cmd_mkdir(int current_inode, const char *fullpath, int user_id);
int cmd_touch(int current_inode, const char *fullpath, int user_id);
int cmd_echo_arrow(int current_inode, const char *fullpath, const char *content, int user_id);
int cmd_echo_arrow_arrow(int current_inode, const char *fullpath, const char *content, int user_id);

int cmd_cp(int current_inode, const char *src_path, const char *src_name,
           const char *dst_path, const char *dst_name, int user_id);
int cmd_mv(int current_inode, const char *src_path, const char *src_name,
           const char *dst_path, const char *dst_name, int user_id);
int cmd_ln_s(int current_inode, const char *target_path, const char *link_path, int user_id);
int cmd_ls(int current_inode, const char *path, int user_id, int info_args);
int cmd_rm(int current_inode, const char *filepath, int user_id);
int cmd_rmdir(int current_inode, const char *filepath, int user_id);
int cmd_unlink(int current_inode, const char *filepath, int user_id);
int cmd_df(void);

#endif
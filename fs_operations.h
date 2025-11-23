#ifndef FS_OPERATIONS_H
#define FS_OPERATIONS_H
#include "fs.h"

/* Diretórios */
int dirFindEntry(int dir_inode, const char *name, inode_type_t type, int *out_inode);
int dirAddEntry(int dir_inode, const char *name, inode_type_t type, int inode_index);
int dirRemoveEntry(int dir_inode, const char *name, inode_type_t type);

/* Permissões */
int hasPermission(const inode_t *inode, int user_id, permission_t perm);

/* Manipulação de conteúdos */
int createDirectory(int parent_inode, const char *name, int user_id, int* output_inode);
int deleteDirectory(int parent_inode, const char *name, int user_id);
int createFile(int parent_inode, const char *name, int user_id);
int deleteFile(int parent_inode, const char *name, int user_id);
int addContentToInode(int inode_number, const char *data, size_t data_size, int user_id);
int readContentFromInode(int inode_number, char *buffer, size_t buffer_size, size_t *out_bytes, int user_id);

int resolvePath(const char *path, int current_inode, int *inode_out);
int createDirectoriesRecursively(const char *path, int current_inode, int user_id);
static void splitPath(const char *full_path, char *dir_path, char *base_name);

int createSymlink(int parent_inode, int target_index, const char *link_name, int user_id);

#endif
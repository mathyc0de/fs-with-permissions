#ifndef FS_H
#define FS_H

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define DISK_NAME "disk.dat"
#define FS_MAGIC 0xF5F5F5F5
#define DISK_SIZE_MB 64
#define MAX_INODES 128
#define BLOCK_SIZE 512
#define BLOCKS_PER_INODE 12
#define MAX_BLOCKS ((DISK_SIZE_MB * 1024 * 1024) / BLOCK_SIZE)
#define MAX_NAMESIZE 32

#define ROOT_INODE 0
#define ROOT_UID 0

typedef struct {
    uint32_t magic; // identificador do FS
    uint32_t block_bitmap_bytes;
    uint32_t inode_bitmap_bytes;
    uint32_t inode_table_bytes;
    uint32_t meta_blocks;
    uint32_t data_blocks;
    uint32_t off_block_bitmap;
    uint32_t off_inode_bitmap;
    uint32_t off_inode_table;
    uint32_t off_data_region;
} fs_header_t;

typedef enum {
    FILE_REGULAR,
    FILE_DIRECTORY,
    FILE_SYMLINK,
    FILE_ANY
} inode_type_t;

typedef enum {
    PERM_NONE  = 0,        // 000 000
    PERM_EXEC  = 1 << 0,   // 001
    PERM_WRITE = 1 << 1,   // 010
    PERM_READ  = 1 << 2,   // 100

    PERM_RX    = PERM_READ | PERM_EXEC,    
    PERM_RWX   = PERM_READ | PERM_WRITE | PERM_EXEC,   // 111

    PERM_ALL   = (PERM_RWX << 3) | PERM_RWX
} permission_t;

typedef struct {
    inode_type_t type;
    char name[MAX_NAMESIZE];
    uint32_t creator_uid;
    uint32_t owner_uid;
    uint32_t size;
    time_t creation_date;       
    time_t modification_date;   
    permission_t permissions;
    uint32_t blocks[BLOCKS_PER_INODE];
    uint32_t next_inode;
    uint32_t link_target_index;     
} inode_t;

typedef struct {
    char name[MAX_NAMESIZE];
    uint32_t inode_index;
} dir_entry_t;

typedef struct {
    char name[MAX_NAMESIZE];
    inode_type_t type;
    char creator[MAX_NAMESIZE];
    char owner[MAX_NAMESIZE];
    uint32_t size;
    time_t creation_date;       
    time_t modification_date;   
    uint16_t permissions;
    uint32_t inode_index;
} fs_entry_t;

typedef struct {
    fs_entry_t *entries;
    int count;
} fs_dir_list_t;


/* Funções principais */
int init_fs(void);
int mount_fs(void);
int sync_fs(void);
void sync_inode(int inode_num);
int unmount_fs(void);

/* Utilitarios */
const char *format_time(time_t t, char *buf, size_t buflen);
int show_inode_info(int inode_index);

/* Alocação */
int allocateBlock(void);
void freeBlock(int block_index);
int allocateInode(void);
void freeInode(int inode_index);

/* Leitura e escrita nos blocos */
int readBlock(uint32_t block_index, void *buffer);
int writeBlock(uint32_t block_index, const void *buffer);


/* Variáveis globais */
extern unsigned char *block_bitmap;
extern unsigned char *inode_bitmap;
extern inode_t *inode_table;
extern FILE *disk;

/* Variáveis computadas (para testes) */
extern size_t computed_block_bitmap_bytes;
extern size_t computed_inode_bitmap_bytes;
extern size_t computed_inode_table_bytes;
extern uint32_t computed_meta_blocks;
extern uint32_t computed_data_blocks;

extern off_t off_data_region;


#endif /* FS_H */

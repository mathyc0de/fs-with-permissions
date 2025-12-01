#include "fs.h"
#include "fs_operations.h"
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

/* ---- Variáveis globais ---- */
unsigned char *block_bitmap = NULL;
unsigned char *inode_bitmap = NULL;
inode_t *inode_table = NULL;
FILE *disk = NULL;

/* Layout do FS */
off_t off_block_bitmap = 0;
off_t off_inode_bitmap = 0;
off_t off_inode_table = 0;
off_t off_data_region = 0;

size_t computed_block_bitmap_bytes = 0;
size_t computed_inode_bitmap_bytes = 0;
size_t computed_inode_table_bytes = 0;
uint32_t computed_meta_blocks = 0;
uint32_t computed_data_blocks = 0;

/* ---- Calcula layout do FS ---- */
static void compute_layout(void) {
    size_t inode_bmap_bytes = (MAX_INODES + 7) / 8;
    size_t inode_tbl_bytes = MAX_INODES * sizeof(inode_t);

    /* Primeiro assumimos todos os blocos de dados disponíveis */
    size_t data_blocks = MAX_BLOCKS;

    /* Calcula bytes do bitmap de blocos */
    size_t bmap_bytes = (data_blocks + 7) / 8;

    /* Computa tamanhos */
    computed_block_bitmap_bytes = bmap_bytes;
    computed_inode_bitmap_bytes = inode_bmap_bytes;
    computed_inode_table_bytes = inode_tbl_bytes;

    /* Número de blocos ocupados pela meta-região */
    computed_meta_blocks = (computed_block_bitmap_bytes +
                            computed_inode_bitmap_bytes +
                            computed_inode_table_bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;

    /* Blocos de dados efetivos */
    computed_data_blocks = MAX_BLOCKS - computed_meta_blocks;

    /* Offsets */
    off_block_bitmap = sizeof(fs_header_t);
    off_inode_bitmap = off_block_bitmap + computed_block_bitmap_bytes;
    off_inode_table = off_inode_bitmap + computed_inode_bitmap_bytes;
    off_data_region = off_inode_table + computed_inode_table_bytes;
    off_data_region = ((off_data_region + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
}

/* ---- Inicializa um novo filesystem ---- */
int init_fs(void) {
    if (access(DISK_NAME, F_OK) == 0) {
        printf("[INFO] Disco existente detectado. Montando FS...\n");
        return mount_fs();
    }

    printf("[INFO] Inicializando novo filesystem...\n");
    disk = fopen(DISK_NAME, "wb+");
    if (!disk) { perror("Erro ao criar disco"); return -1; }

    ftruncate(fileno(disk), DISK_SIZE_MB * 1024 * 1024);
    compute_layout();

    block_bitmap = calloc(1, computed_block_bitmap_bytes);
    inode_bitmap = calloc(1, computed_inode_bitmap_bytes);
    inode_table = calloc(MAX_INODES, sizeof(inode_t));
    if (!block_bitmap || !inode_bitmap || !inode_table) {
        perror("Erro ao alocar memória para FS");
        fclose(disk);
        return -1;
    }

    /* Cria diretório raiz */
    int root_inode = allocateInode();
    inode_table[root_inode].type = FILE_DIRECTORY;
    inode_table[root_inode].size = 0;
    inode_table[root_inode].creation_date = time(NULL);
    inode_table[root_inode].modification_date = time(NULL);
    inode_table[root_inode].permissions = PERM_ALL;
    strcpy(inode_table[root_inode].name, "/");
    inode_table[root_inode].owner_uid = 0;
    inode_table[root_inode].creator_uid = 0;
    dirAddEntry(ROOT_INODE, ".", FILE_DIRECTORY, ROOT_INODE);
    dirAddEntry(ROOT_INODE, "..", FILE_DIRECTORY, ROOT_INODE);
    sync_inode(root_inode);

    /* Escreve header no disco */
    fs_header_t header = {0};
    header.magic = FS_MAGIC;
    header.block_bitmap_bytes = computed_block_bitmap_bytes;
    header.inode_bitmap_bytes = computed_inode_bitmap_bytes;
    header.inode_table_bytes = computed_inode_table_bytes;
    header.meta_blocks = computed_meta_blocks;
    header.data_blocks = computed_data_blocks;
    header.off_block_bitmap = off_block_bitmap;
    header.off_inode_bitmap = off_inode_bitmap;
    header.off_inode_table = off_inode_table;
    header.off_data_region = off_data_region;

    fseek(disk, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, disk);
    fflush(disk);

    /* Escreve bitmaps e tabela de inodes */
    // bitmap de blocos
    fseek(disk, off_block_bitmap, SEEK_SET);
    fwrite(block_bitmap, 1, computed_block_bitmap_bytes, disk);

    // bitmap de inodes
    fseek(disk, off_inode_bitmap, SEEK_SET);
    fwrite(inode_bitmap, 1, computed_inode_bitmap_bytes, disk);

    // tabela de inodes
    fseek(disk, off_inode_table, SEEK_SET);
    fwrite(inode_table, 1, computed_inode_table_bytes, disk);

    printf("\n[INFO] Filesystem criado com sucesso.\n\n");

    printf("[INFO] Disposição do disco:\n");
    printf("[INFO]   |--Espaço para cabecalho: %ldB\n", sizeof(fs_header_t));
    printf("[INFO]   |--Espaço para bitmap de blocos: %ldB\n", computed_block_bitmap_bytes);
    printf("[INFO]   |--Espaço para bitmap de inodes: %ldB\n", computed_inode_bitmap_bytes);
    printf("[INFO]   |--Espaço para tabela de inodes: %ldB\n", computed_inode_table_bytes);
    printf("         |\n");
    printf("[INFO]   |--Espaço disponivel: %dB\n", computed_data_blocks * BLOCK_SIZE);
    printf("[INFO]   |--Equivalente a: %d blocos\n\n", computed_data_blocks);
    return 0;
}

/* ---- Monta filesystem existente ---- */
int mount_fs(void) {
    printf("[INFO] Montando filesystem existente...\n");
    disk = fopen(DISK_NAME, "rb+");
    if (!disk) { perror("Erro ao abrir disco"); return -1; }

    fs_header_t header;
    fseek(disk, 0, SEEK_SET);
    if (fread(&header, sizeof(header), 1, disk) != 1) {
        fprintf(stderr, "Erro ao ler header do FS.\n");
        fclose(disk);
        return -1;
    }

    if (header.magic != FS_MAGIC) {
        fprintf(stderr, "Disco inválido ou corrompido.\n");
        fclose(disk);
        return -1;
    }

    /* Restaura variáveis globais */
    computed_block_bitmap_bytes = header.block_bitmap_bytes;
    computed_inode_bitmap_bytes = header.inode_bitmap_bytes;
    computed_inode_table_bytes = header.inode_table_bytes;
    computed_meta_blocks = header.meta_blocks;
    computed_data_blocks = header.data_blocks;
    off_block_bitmap = header.off_block_bitmap;
    off_inode_bitmap = header.off_inode_bitmap;
    off_inode_table = header.off_inode_table;
    off_data_region = header.off_data_region;

    /* Aloca memória */
    block_bitmap = malloc(computed_block_bitmap_bytes);
    inode_bitmap = malloc(computed_inode_bitmap_bytes);
    inode_table = malloc(computed_inode_table_bytes);
    if (!block_bitmap || !inode_bitmap || !inode_table) {
        perror("Erro ao alocar memória para FS");
        fclose(disk);
        return -1;
    }

    /* Lê conteúdo do disco */
    // bitmap de blocos
    fseek(disk, off_block_bitmap, SEEK_SET);
    fread(block_bitmap, 1, computed_block_bitmap_bytes, disk);

    // bitmap de inodes
    fseek(disk, off_inode_bitmap, SEEK_SET);
    fread(inode_bitmap, 1, computed_inode_bitmap_bytes, disk);

    // tabela de inodes
    fseek(disk, off_inode_table, SEEK_SET);
    fread(inode_table, 1, computed_inode_table_bytes, disk);


    printf("[INFO] Filesystem montado com sucesso!\n\n");

    printf("[INFO] Disposição do disco:\n");
    printf("[INFO]   |--Espaço para cabecalho: %ldB\n", sizeof(fs_header_t));
    printf("[INFO]   |--Espaço para bitmap de blocos: %ldB\n", computed_block_bitmap_bytes);
    printf("[INFO]   |--Espaço para bitmap de inodes: %ldB\n", computed_inode_bitmap_bytes);
    printf("[INFO]   |--Espaço para tabela de inodes: %ldB\n", computed_inode_table_bytes);
    printf("         |\n");
    printf("[INFO]   |--Espaço disponivel: %dB\n", computed_data_blocks * BLOCK_SIZE);
    printf("[INFO]   |--Equivalente a: %d blocos\n\n", computed_data_blocks);
    return 0;
}

/* ---- Sincroniza FS inteiro ---- */
int sync_fs(void) {
    if (!disk || !block_bitmap || !inode_bitmap || !inode_table) return -1;
    fseek(disk, off_block_bitmap, SEEK_SET);
    fwrite(block_bitmap, 1, computed_block_bitmap_bytes, disk);

    fseek(disk, off_inode_bitmap, SEEK_SET);
    fwrite(inode_bitmap, 1, computed_inode_bitmap_bytes, disk);

    fseek(disk, off_inode_table, SEEK_SET);
    fwrite(inode_table, 1, computed_inode_table_bytes, disk);

    fflush(disk);
    fsync(fileno(disk));

    return 0;
}

/* ---- Persiste um inode específico no disco ---- */
void sync_inode(int inode_num) {
    if (!disk || !inode_table) return;
    fseek(disk, off_inode_table + inode_num * sizeof(inode_t), SEEK_SET);
    fwrite(&inode_table[inode_num], sizeof(inode_t), 1, disk);
    fflush(disk);
}


/* ---- Desmonta FS ---- */
int unmount_fs(void) {
    sync_fs();
    free(block_bitmap); block_bitmap = NULL;
    free(inode_bitmap); inode_bitmap = NULL;
    free(inode_table); inode_table = NULL;
    if (disk) { fclose(disk); disk = NULL; }
    return 0;
}

/* ----- Utilitarios --------*/
/* Formata timestamp para prints */
const char *format_time(time_t t, char *buf, size_t buflen) {
    if (!buf || buflen == 0) return NULL;
    struct tm tm;
    if (localtime_r(&t, &tm) == NULL) {
        buf[0] = '\0';
        return buf;
    }
    strftime(buf, buflen, "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

/* Função de debug para visualizar informações de inodes */
int show_inode_info(int inode_index) {
    if (!inode_table) return -1;
    if (inode_index < 0 || inode_index >= MAX_INODES) return -1;

    inode_t *ino = &inode_table[inode_index];
    char ctime_buf[64] = {0}, mtime_buf[64] = {0};
    format_time(ino->creation_date, ctime_buf, sizeof(ctime_buf));
    format_time(ino->modification_date, mtime_buf, sizeof(mtime_buf));

    const char *type_str = "unknown";
    if (ino->type == FILE_REGULAR) type_str = "regular file";
    else if (ino->type == FILE_DIRECTORY) type_str = "directory";
    else if (ino->type == FILE_SYMLINK) type_str = "symlink";

    // Monta string de permissões rwxrwxrwx
    char perm_str[10] = "---------";
    int pos = 0;
    for (int who = 3; who >= 0; who -= 3) {
        perm_str[pos++] = (ino->permissions & (PERM_READ << who)) ? 'r' : '-';
        perm_str[pos++] = (ino->permissions & (PERM_WRITE << who)) ? 'w' : '-';
        perm_str[pos++] = (ino->permissions & (PERM_EXEC << who)) ? 'x' : '-';
    }
    perm_str[6] = '\0';

    printf("Inode %d:\n", inode_index);
    printf("  name: %s\n", ino->name);
    printf("  type: %s\n", type_str);
    printf("  creator UID: %u\n", ino->creator_uid);
    printf("  owner UID: %u\n", ino->owner_uid);
    printf("  size: %u bytes\n", ino->size);
    printf("  permissions: %s (0%o)\n", perm_str, (unsigned)ino->permissions);
    printf("  created: %s\n", ctime_buf);
    printf("  modified: %s\n", mtime_buf);
    if (ino->type == FILE_SYMLINK) {
        printf("  symlink -> inode %u\n", ino->link_target_index);
    }
    printf("  blocks:");
    for (int i = 0; i < BLOCKS_PER_INODE; ++i) {
        if (ino->blocks[i] != 0)
            printf(" %u", ino->blocks[i]);
    }
    if (ino->next_inode != 0) printf("  (next inode: %u)", ino->next_inode);
    printf("\n");

    return 0;
}



/* ---- alocação ---- */
/* Aloca novo bloco */
int allocateBlock(void) {
    for (uint32_t i = 0; i < computed_data_blocks; i++){
        uint32_t byte = i / 8;
        uint8_t bit = i % 8;

        if ((block_bitmap[byte] & (1 << bit)) == 0) {
            block_bitmap[byte] |= (1 << bit);
            return i;
        }
    }
    return -1;
}

/* Libera bloco existente */
void freeBlock(int block_index) {
    if (block_index >= 0 && block_index < (int)computed_data_blocks) {
        uint32_t byte = block_index / 8;
        uint8_t bit = block_index % 8;
        if ((block_bitmap[byte] & (1 << bit)) == 0) return;
        block_bitmap[byte] &= ~(1 << bit);
    }
}

/* Aoca novo inode */
int allocateInode(void) {
    for (uint32_t i = 0; i < MAX_INODES; i++) {
        uint32_t byte = i / 8;
        uint8_t bit = i % 8;

        if ((inode_bitmap[byte] & (1 << bit)) == 0) {
            inode_bitmap[byte] |= (1 << bit);
            memset(&inode_table[i], 0, sizeof(inode_t));
            inode_table[i].next_inode = 0;
            return i;
        }
    }
    return -1;
}

/* Libera inode existent */
void freeInode(int inode_index) {
    if (inode_index < 0 || inode_index >= MAX_INODES)
        return;

    inode_t *inode = &inode_table[inode_index];

    for (int i = 0; i < BLOCKS_PER_INODE; i++) {
        int block = inode->blocks[i];
        if (block > 0)
            freeBlock(block);
    }

    if (inode->next_inode)
        freeInode(inode->next_inode);

    uint32_t byte = inode_index / 8;
    uint8_t bit = inode_index % 8;
    inode_bitmap[byte] &= ~(1 << bit);

    memset(inode, 0, sizeof(inode_t));
}

/* ---- leitura e escrita ---- */
/* Le bloco */
int readBlock(uint32_t block_index, void *buffer){
    if (!disk || block_index >= computed_data_blocks) return -1;
    off_t offset = off_data_region + (off_t)block_index * BLOCK_SIZE;
    fseek(disk, offset, SEEK_SET);
    size_t read_bytes = fread(buffer, 1, BLOCK_SIZE, disk);
    return (read_bytes == BLOCK_SIZE) ? 0 : -1;
}

/* Escreve bloco */
int writeBlock(uint32_t block_index, const void *buffer){
    if (!disk || block_index >= computed_data_blocks) return -1;
    off_t offset = off_data_region + (off_t)block_index * BLOCK_SIZE;
    fseek(disk, offset, SEEK_SET);
    size_t written_bytes = fwrite(buffer, 1, BLOCK_SIZE, disk);
    fflush(disk);
    fsync(fileno(disk));
    return (written_bytes == BLOCK_SIZE) ? 0 : -1;
}
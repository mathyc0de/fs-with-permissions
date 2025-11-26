#include "core_utils.h"
#include "utils.h"
#include "fs_operations.h"
#include <stdlib.h>

/* ---- Comandos de FS ---- */

    // Criação de pastas base
//     create_user();

//     create_root();

//     // Criação do usuário
//     char* password;

//     printf("Digite o nome do usuário a ser criado: ");
//     fgets(user, MAX_NAMESIZE, stdin);
//     printf("\nCrie uma senha: ");
//     fgets(password, MAX_PASSWORD_SIZE, stdin);
//     create_user(user, password);

//     int create_foundation_folder() {

    
// }

user_t *user = NULL;

int get_next_uid() {
    char* buffer;
    cmd_cat(ROOT_INODE, "etc/passwd", ROOT_UID, buffer);

    char *last_line_break = strrchr(buffer, '\n');
    char *last_line = last_line_break ? last_line_break + 1 : buffer;
    char *last_colon = strrchr(last_line, ':');
    free(buffer);

    return atoi(last_colon) + 1;
}

int create_root() {
    cmd_echo_arrow(ROOT_INODE, "etc/passwd", "root:x:0", ROOT_UID);
    cmd_echo_arrow(ROOT_INODE, "etc/shadow", "root:!", ROOT_UID);
    return 0;
}

int create_user() {
    char username[MAX_NAMESIZE], password[MAX_PASSWORD_SIZE], passwd_entry[MAX_PASSWD_ENTRY], shadow_entry[MAX_SHADOW_ENTRY], encrypted_password[MAX_PASSWORD_SIZE];
    int passwd_inode, shadow_inode;

    // Input do Usuário
    printf("Digite o nome do usuário a ser criado: ");
    fgets(username, MAX_NAMESIZE, stdin);
    printf("\nCrie uma senha: ");
    fgets(password, MAX_PASSWORD_SIZE, stdin);

    // Obtendo os I-Nodes do passwd e shadow
    resolvePath("etc/passwd", ROOT_INODE, &passwd_inode);
    resolvePath("etc/shadow", ROOT_INODE, &shadow_inode);

    int new_uid = get_next_uid();

    // Lógica do passwd
    snprintf(passwd_entry, sizeof(passwd_entry), "%s:x:%d", username, new_uid);
    addContentToInode(passwd_inode, passwd_entry, strlen(passwd_entry), ROOT_UID);

    // Lógica do shadow
    encrypt_password(password, encrypted_password);
    snprintf(shadow_entry, sizeof(shadow_entry), "%s:%s", username, encrypted_password);
    addContentToInode(shadow_inode, shadow_entry, sizeof(shadow_entry), ROOT_UID);
    return 0;
}

int login(char* username, char* password) {
    printf("Falta implementar!\n");
    if (1) {
        // user->name
    }
}

// cd (muda diretorio)
int cmd_cd(int *current_inode, const char *path) {
    if (!current_inode || !path) return -1;

    int target_inode;
    if (resolvePath(path, *current_inode, &target_inode) != 0) return -1;

    inode_t *inode = &inode_table[target_inode];
    if (inode->type != FILE_DIRECTORY) return -1;

    *current_inode = target_inode;
    return 0;
}

// mkdir (cria diretorio) com criação recursiva
int cmd_mkdir(int current_inode, const char *full_path, int user_id) {
    if (!full_path) return -1;

    char dir_path[256], name[256];
    splitPath(full_path, dir_path, name);

    int parent_inode;
    if (resolvePath(dir_path, current_inode, &parent_inode) != 0) {
        if (createDirectoriesRecursively(dir_path, current_inode, user_id) != 0) return -1;
        if (resolvePath(dir_path, current_inode, &parent_inode) != 0) return -1;
    }

    return createDirectory(parent_inode, name, user_id, NULL);
}

// touch (cria arquivo) com criação recursiva
int cmd_touch(int current_inode, const char *full_path, int user_id) {
    if (!full_path) return -1;

    char dir_path[256], name[256];
    splitPath(full_path, dir_path, name);

    int parent_inode;
    if (resolvePath(dir_path, current_inode, &parent_inode) != 0) {
        if (createDirectoriesRecursively(dir_path, current_inode, user_id) != 0) return -1;
        if (resolvePath(dir_path, current_inode, &parent_inode) != 0) return -1;
    }

    return createFile(parent_inode, name, user_id);
}

// echo > (sobrescreve conteúdo) com criação recursiva
int cmd_echo_arrow(int current_inode, const char *full_path, const char *content, int user_id) {
    if (!full_path || !content) return -1;

    char dir_path[256], name[256];
    splitPath(full_path, dir_path, name);

    int parent_inode;
    if (resolvePath(dir_path, current_inode, &parent_inode) != 0) {
        if (createDirectoriesRecursively(dir_path, current_inode, user_id) != 0) return -1;
        if (resolvePath(dir_path, current_inode, &parent_inode) != 0) return -1;
    }

    int inode_index;
    if (dirFindEntry(parent_inode, name, FILE_REGULAR, &inode_index) != 0) {
        if (createFile(parent_inode, name, user_id) != 0) return -1;
        if (dirFindEntry(parent_inode, name, FILE_REGULAR, &inode_index) != 0) return -1;
    }

    inode_t *inode = &inode_table[inode_index];
    inode->size = 0;  
    if (inode->next_inode) freeInode(inode->next_inode);
    inode->next_inode = 0;
    for (int i = 0; i < BLOCKS_PER_INODE; i++) inode->blocks[i] = 0;

    return addContentToInode(inode_index, content, strlen(content), user_id);
}

// echo >> (anexa conteúdo) com criação recursiva
int cmd_echo_arrow_arrow(int current_inode, const char *full_path, const char *content, int user_id) {
    if (!full_path || !content) return -1;


    char dir_path[256], name[256];
    splitPath(full_path, dir_path, name);

    int parent_inode;
    if (resolvePath(dir_path, current_inode, &parent_inode) != 0) {
        if (createDirectoriesRecursively(dir_path, current_inode, user_id) != 0) return -1;
        if (resolvePath(dir_path, current_inode, &parent_inode) != 0) return -1;
    }

    int inode_index;
    if (dirFindEntry(parent_inode, name, FILE_REGULAR, &inode_index) != 0) {
        if (createFile(parent_inode, name, user_id) != 0) return -1;
        if (dirFindEntry(parent_inode, name, FILE_REGULAR, &inode_index) != 0) return -1;
    }

    return addContentToInode(inode_index, content, strlen(content), user_id);
}


// cat (le conteudo de arquivo)
int cmd_cat(int current_inode, const char *path, int user_id, char* buffer) {
    if (!path || !user_id) return -1;
    // resolve o inode do arquivo
    int target_inode;
    if (resolvePath(path, current_inode, &target_inode) != 0)
        return -1;

    // procura o inode pelo indice
    inode_t *inode = &inode_table[target_inode];
    if (!inode || inode->type != FILE_REGULAR) {
        fprintf(stderr, "Erro: %s não é um arquivo regular.\n", path);
        return -1;
    }

    // assegura que há permissão para leitura
    if (!hasPermission(inode, user_id, PERM_READ)) {
        fprintf(stderr, "Erro: permissão negada para %s.\n", path);
        return -1;
    }

    size_t filesize = inode->size;
    if (filesize == 0) return 0; // arquivo vazio

    buffer = malloc(filesize + 1);
    if (!buffer) return -1;

    // Lê arquivo
    size_t bytes_read = 0;
    if (readContentFromInode(target_inode, buffer, filesize + 1, &bytes_read, user_id) != 0) {
        free(buffer);
        return -1;
    }

    buffer[bytes_read] = '\0';
    // printf("%s\n", buffer);

    // free(buffer);
    return 0;
}

// cp 9copia arquivo) com criaçãp recursiva
int cmd_cp(int current_inode, const char *src_path, const char *src_name,
           const char *dst_path, const char *dst_name, int user_id) {
    if (!src_name || !dst_name) return -1;

    int src_parent_inode = current_inode;
    int dst_parent_inode = current_inode;
    const char *src_base = src_name;
    const char *dst_base = dst_name;

    // Se src_name contem '/', separe dir e base
    char tmpbuf[256];
    const char *slash = strrchr(src_name, '/');
    if (slash) {
        size_t dirlen = slash - src_name;
        if (dirlen >= sizeof(tmpbuf)) return -1;
        strncpy(tmpbuf, src_name, dirlen);
        tmpbuf[dirlen] = '\0';
        src_base = slash + 1;
        if (resolvePath(tmpbuf, current_inode, &src_parent_inode) != 0) return -1;
    } else {
        // se foi passado um src_path != "." resolva-o
        if (src_path && src_path[0] != '\0' && strcmp(src_path, ".") != 0) {
            if (resolvePath(src_path, current_inode, &src_parent_inode) != 0) return -1;
        }
    }

    // Processa dst: se dst_name contem '/', separe dir e base
    slash = strrchr(dst_name, '/');
    if (slash) {
        size_t dirlen = slash - dst_name;
        if (dirlen >= sizeof(tmpbuf)) return -1;
        strncpy(tmpbuf, dst_name, dirlen);
        tmpbuf[dirlen] = '\0';
        dst_base = slash + 1;

        // tenta resolver o diretório; se não existir, tenta criar recursivamente
        if (resolvePath(tmpbuf, current_inode, &dst_parent_inode) != 0) {
            if (createDirectoriesRecursively(tmpbuf, current_inode, user_id) != 0) return -1;
            if (resolvePath(tmpbuf, current_inode, &dst_parent_inode) != 0) return -1;
        }
    } else {
        // se foi passado um dst_path != "." resolva-o (p.ex.: cp arq dir/arq2 onde dst_path veio como "dir")
        if (dst_path && dst_path[0] != '\0' && strcmp(dst_path, ".") != 0) {
            if (resolvePath(dst_path, current_inode, &dst_parent_inode) != 0) {
                // tentar criar o caminho de destino
                if (createDirectoriesRecursively(dst_path, current_inode, user_id) != 0) return -1;
                if (resolvePath(dst_path, current_inode, &dst_parent_inode) != 0) return -1;
            }
        }
    }

    // Agora temos src_parent_inode + src_base e dst_parent_inode + dst_base
    int src_file_inode;
    if (dirFindEntry(src_parent_inode, src_base, FILE_REGULAR, &src_file_inode) != 0) {
        // tenta também aceitar symlink/any (se quiser copiar links) → mas por enquanto requer arquivo regular
        return -1;
    }

    inode_t *src_inode = &inode_table[src_file_inode];
    if (!src_inode) return -1;

    // Lê o conteúdo diretamente por inode
    char *buffer = malloc(src_inode->size + 1);
    if (!buffer) return -1;
    size_t bytes_read = 0;
    if (readContentFromInode(src_file_inode, buffer, src_inode->size + 1, &bytes_read, user_id) != 0) {
        free(buffer);
        return -1;
    }

    // Cria arquivo destino se necessário
    int dst_file_inode;
    if (dirFindEntry(dst_parent_inode, dst_base, FILE_REGULAR, &dst_file_inode) != 0) {
        if (createFile(dst_parent_inode, dst_base, user_id) != 0) {
            free(buffer);
            return -1;
        }
        if (dirFindEntry(dst_parent_inode, dst_base, FILE_REGULAR, &dst_file_inode) != 0) {
            free(buffer);
            return -1;
        }
    } else {
        // Se o arquivo já existe, precisamos sobrescrever: zera inode antes de escrever
        inode_t *dst_inode = &inode_table[dst_file_inode];
        dst_inode->size = 0;
        dst_inode->next_inode = 0;
        for (int i = 0; i < BLOCKS_PER_INODE; ++i) dst_inode->blocks[i] = 0;
    }

    // Escreve no inode destino usando addContentToInode
    int res = addContentToInode(dst_file_inode, buffer, bytes_read, user_id);
    free(buffer);
    return res;
}



// mv (move)
int cmd_mv(int current_inode, const char *src_path, const char *src_name,
           const char *dst_path, const char *dst_name, int user_id) {
    // Copia o arquivo
    if (cmd_cp(current_inode, src_path, src_name, dst_path, dst_name, user_id) != 0) return -1;

    // Apaga o arquivo de origem
    int src_parent_inode;
    if (resolvePath(src_path, current_inode, &src_parent_inode) != 0) return -1;

    return deleteFile(src_parent_inode, src_name, user_id);
}


// ln -s (cria link simbolico)
int cmd_ln_s(int current_inode,
             const char *target_path,
             const char *link_path,
             int user_id) {
    if (!target_path || !link_path)
        return -1;

    int target_index;
    if (resolvePath(target_path, current_inode, &target_index) != 0) return -1;
    


    char link_dir[256];
    char link_name[256];
    splitPath(link_path, link_dir, link_name);

    int link_dir_index;

    // --- 2. Garante que o diretório do link exista ---
    // Tenta resolver o link_path normalmente
    if (resolvePath(link_dir, current_inode, &link_dir_index) != 0) {
        // Se falhar, cria diretórios recursivamente
        if (createDirectoriesRecursively(link_dir, current_inode, user_id) != 0)
            return -1;

        // Tenta resolver novamente agora que o caminho existe
        if (resolvePath(link_dir, current_inode, &link_dir_index) != 0)
            return -1;
    }

    // --- 3. Cria o link simbólico ---
    createSymlink(link_dir_index, target_index, link_name, user_id);
}


// ls (lista elementos)
int cmd_ls(int current_inode, const char *path, int user_id, int info_arg) {
    // checa se o caminho existe
    int target_inode = current_inode;
    if (path && strlen(path) > 0) {
        if (resolvePath(path, current_inode, &target_inode) != 0) {
            printf("ls: caminho não encontrado: %s\n", path);
            return -1;
        }
    }
    
    inode_t *dir_inode = &inode_table[target_inode];

    // itera sobre cada next dentro do inode
    do {
        // itera sobre cada bloco dentro do inode do diretório
        for (int block_idx = 0; block_idx < BLOCKS_PER_INODE; block_idx++) {
            if (dir_inode->blocks[block_idx] == 0) continue;
            
            dir_entry_t *entries = malloc(BLOCK_SIZE);
            if (readBlock(dir_inode->blocks[block_idx], entries) != 0) {
                free(entries);
                return -1;
            }
            
            int entries_per_block = BLOCK_SIZE / sizeof(dir_entry_t);
            for (int entry_idx = 0; entry_idx < entries_per_block; entry_idx++) {
                if (entries[entry_idx].inode_index == 0)
                    continue;

                inode_t *entry_inode = &inode_table[entries[entry_idx].inode_index];

                // Determina tipo de arquivo
                char type = '-';
                if (entry_inode->type == FILE_DIRECTORY) type = 'd';
                else if (entry_inode->type == FILE_REGULAR) type = 'f';
                else if (entry_inode->type == FILE_SYMLINK) type = 'l';
                

                // Caso utilize o argumento para emular o ls - l
                if (info_arg) {
                    // Formata permissões (rwxrwxrwx)
                    char perm_str[10] = "---------";
                    for (int who = 6; who >= 0; who -= 3) {
                        perm_str[8-who-2] = (entry_inode->permissions & (PERM_READ << who)) ? 'r' : '-';
                        perm_str[8-who-1] = (entry_inode->permissions & (PERM_WRITE << who)) ? 'w' : '-';
                        perm_str[8-who] = (entry_inode->permissions & (PERM_EXEC << who)) ? 'x' : '-';
                    }

                    // Formata datas
                    char ctime_buf[32], mtime_buf[32];
                    format_time(entry_inode->creation_date, ctime_buf, sizeof(ctime_buf));
                    format_time(entry_inode->modification_date, mtime_buf, sizeof(mtime_buf));

                    printf("%c %s %d %d %8lu %s %s", 
                        type,
                        perm_str,
                        entry_inode->owner_uid,
                        entry_inode->creator_uid,
                        (unsigned long)entry_inode->size,
                        mtime_buf, 
                        entry_inode->name
                    );

                    // Se for link simbólico, mostra o alvo
                    if (entry_inode->type == FILE_SYMLINK) {
                        printf(" -> %s", inode_table[entry_inode->link_target_index].name);
                    }
                    printf("\n");
                }
                else {
                    printf("-%c     %s\n", type, entry_inode->name);
                }
            }

            free(entries);
        }
        dir_inode = &inode_table[dir_inode->next_inode];
    } while (dir_inode->next_inode != 0);
    return 0;
}

// remove elementos (usada tanto por rmdir quanto por rm)
int cmd_remove(int current_inode, const char *filepath, int user_id, int remove_dir) {
    if (!filepath) return -1;

    char parent_path[1024];
    char name[MAX_NAMESIZE];

    // Encontra a última barra para separar caminho/nome
    splitPath(filepath, parent_path, name);
    
    // resolve o inode do diretorio pai
    int parent_inode;
    if (resolvePath(parent_path, current_inode, &parent_inode) != 0) {
        if (remove_dir)
            printf("rmdir: diretório não encontrado: %s\n", parent_path);
        else
            printf("Arquivo não encontrado\n");
        return -1;
    }

    // Procura o arquivo com o nome dentro do diretório pai
    int target_inode;
    if (dirFindEntry(parent_inode, name, FILE_ANY, &target_inode) != 0) {
        if (remove_dir)
            printf("rmdir: não existe o diretório: %s\n", filepath);
        else
            printf("Arquivo não encontrado\n");
        return -1;
    }

    inode_t *target = &inode_table[target_inode];

    // Verifica conforme o tipo de rm (e.g. rm ou rmdir)
    if (remove_dir) {
        if (target->type != FILE_DIRECTORY) {
            printf("rmdir: não é um diretório: %s\n", filepath);
            return -1;
        }
        if (deleteDirectory(parent_inode, name, user_id) != 0) {
            printf("rmdir: não foi possível remover '%s'\n", filepath);
            return -1;
        }
        return 0;
    } else {
        if (target->type == FILE_DIRECTORY) {
            printf("rm: não é possível remover '%s': é um diretório\n", filepath);
            return -1;
        }
        if (deleteFile(parent_inode, name, user_id) != 0) {
            printf("Erro ao remover arquivo: %s\n", filepath);
            return -1;
        }
        return 0;
    }
}

// rm (remove arquivo)
int cmd_rm(int current_inode, const char *filepath, int user_id) {
    return cmd_remove(current_inode, filepath, user_id, 0);
}

// rmdir (remove diretorio)
int cmd_rmdir(int current_inode, const char *filepath, int user_id) {
    printf("passou");
    return cmd_remove(current_inode, filepath, user_id, 1);
}

int cmd_unlink(int current_inode, const char *filepath, int user_id){
    if (!filepath) return -1;

    char parent_path[1024];
    char name[MAX_NAMESIZE];

    // Encontra a última barra para separar caminho/nome
    splitPath(filepath, parent_path, name);
    
    // resolve o inode do diretorio pai
    int parent_inode;
    if (resolvePath(parent_path, current_inode, &parent_inode) != 0) {
        printf("Link não encontrado\n");
        return -1;
    }

    // Procura o arquivo com o nome dentro do diretório pai
    int target_inode;
    if (dirFindEntry(parent_inode, name, FILE_ANY, &target_inode) != 0) {
        printf("Link não encontrado\n");
        return -1;
    }

    inode_t *target = &inode_table[target_inode];

    // Verifica se é um link simbolico
    if (target->type != FILE_SYMLINK) {
        printf("Alvo não é um link: %s\n", filepath);
        return -1;
        }

    if (deleteSymlink(parent_inode, target_inode, user_id) != 0) {
            printf("Não foi possível remover '%s'\n", filepath);
            return -1;
        }
    return 0;
}

int cmd_df(){
    int free_blocks = 0;

    for (uint32_t i = 0; i < computed_data_blocks; i++){
        uint32_t byte = i / 8;
        uint8_t bit = i % 8;

        if ((block_bitmap[byte] & (1 << bit)) == 0) {
            free_blocks ++;
        }
    }

    int used_blocks = computed_data_blocks - free_blocks;
    int use_percentage = (used_blocks * 100 + computed_data_blocks -1) / computed_data_blocks;

    printf("Filesystem     N-blocks     Used Available Use%% Mounted on\n");
    printf("%-14s %-12d %-6d %-5d %3d%%   /~\n",
           DISK_NAME, computed_data_blocks, used_blocks, free_blocks, use_percentage);

    return 0;
}
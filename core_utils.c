#include "core_utils.h"
#include "utils.h"
#include "fs_operations.h"
#include <stdlib.h>
#include <crypt.h>
#define UNREFERENCED(x) (void)(x)


/* ---- Comandos de FS ---- */

int authenticated_uid = -1;
const char* passwd_path = "etc/passwd";
const char* shadow_path = "etc/shadow";

int parse_octal_permissions(const char *str, uint8_t *out)
{
    if (!str || !out)
        return -1;

    // Valida se são apenas dois dígitos
    if (strlen(str) != 2)
        return -1;

    // Verifica se ambos são dígitos octais
    if (str[0] < '0' || str[0] > '7') return -1;
    if (str[1] < '0' || str[1] > '7') return -1;

    int owner  = str[0] - '0'; // faz o mesmo que a função 'atoi()', porém nesse caso é feita uma subtração da distância entre o ASCI de um número para o ASCI 0 (e.g '7' - '0' => 55 - 48 = 7)
    int others = str[1] - '0';

    *out = (owner << 3) | others;

    return 0;
}


// cd (muda diretorio)
int _cd(int *current_inode, const char *path, int user_id) {
    if (!current_inode || !path) return -1;

    int target_inode;
    if (resolvePath(path, *current_inode, &target_inode) != 0) return -1;

    inode_t *inode = &inode_table[target_inode];
    if (inode->type != FILE_DIRECTORY) return -1;

    if (!hasPermission(inode, authenticated_uid, PERM_EXEC) && user_id != ROOT_UID) {
        printf("cd: Acesso negado, requer permissão X.\n");
        return -1;
    };
    *current_inode = target_inode;
    return 0;
}

// _mkdir (cria diretorio) com criação recursiva
int _mkdir(int current_inode, const char *full_path, int user_id) {
    if (!full_path) return -1;

    char dir_path[256], name[256];
    splitPath(full_path, dir_path, name);

    int parent_inode;
    if (resolvePath(dir_path, current_inode, &parent_inode) != 0) {
        if (createDirectoriesRecursively(dir_path, current_inode, user_id) != 0) return -1;
        if (resolvePath(dir_path, current_inode, &parent_inode) != 0) return -1;
    }

    inode_t *parent = &inode_table[parent_inode];

    if (!hasPermission(parent, user_id, PERM_WRITE | PERM_EXEC) && user_id != ROOT_UID) {
        printf("mkdir: acesso negado — requer W e X no diretório pai.\n");
        return -1;
    }

    return createDirectory(parent_inode, name, user_id, NULL);
}

// _touch (cria arquivo) com criação recursiva
int _touch(int current_inode, const char *full_path, int user_id) {
    if (!full_path) return -1;

    char dir_path[256], name[256];
    splitPath(full_path, dir_path, name);

    int parent_inode;
    if (resolvePath(dir_path, current_inode, &parent_inode) != 0) {
        if (createDirectoriesRecursively(dir_path, current_inode, user_id) != 0) return -1;
        if (resolvePath(dir_path, current_inode, &parent_inode) != 0) return -1;
    }

    inode_t *parent = &inode_table[parent_inode];

    if (!hasPermission(parent, user_id, PERM_WRITE | PERM_EXEC) && user_id != ROOT_UID) {
        printf("touch: acesso negado — requer W e X no diretório pai.\n");
        return -1;
    }

    return createFile(parent_inode, name, user_id);
}

// echo > (sobrescreve conteúdo) com criação recursiva
int _echo_arrow(int current_inode, const char *full_path, const char *content, int user_id) {
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
    if (!hasPermission(inode, user_id, PERM_WRITE) && user_id != ROOT_UID) {
        printf("echo: acesso negado — requer permissão W.\n");
        return -1;
    }

    inode->size = 0;  
    if (inode->next_inode) freeInode(inode->next_inode);
    inode->next_inode = 0;
    for (int i = 0; i < BLOCKS_PER_INODE; i++) inode->blocks[i] = 0;
    return addContentToInode(inode_index, content, strlen(content), user_id);
}

// echo >> (anexa conteúdo) com criação recursiva
int _echo_arrow_arrow(int current_inode, const char *full_path, const char *content, int user_id) {
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
        if (!hasPermission(&inode_table[inode_index], user_id, PERM_WRITE) && user_id != ROOT_UID) {
            printf("echo: Acesso negado, requer permissão W.\n");
            return -1;
        }
        if (createFile(parent_inode, name, user_id) != 0) return -1;
        if (dirFindEntry(parent_inode, name, FILE_REGULAR, &inode_index) != 0) return -1;
    }


    return addContentToInode(inode_index, content, strlen(content), user_id);
}


// _cat (le conteudo de arquivo)
int _cat(int current_inode, const char *path, int user_id, char** buffer) {
    if (!path) return -1;
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
    if (!hasPermission(inode, user_id, PERM_READ) && user_id != ROOT_UID) {
        printf("Acesso negado, requer permissão R\n");
        return -1;
    }

    size_t filesize = inode->size;
    if (filesize == 0) { // arquivo vazio
        *buffer = malloc(1);
        (*buffer)[0] = '\0';
        return 0;
    }

    *buffer = malloc(filesize + 1);
    if (!*buffer) return -1;

    // Lê arquivo
    size_t bytes_read = 0;
    if (readContentFromInode(target_inode, *buffer, filesize + 1, &bytes_read, user_id) != 0) {
        free(*buffer);
        *buffer = NULL;
        return -1;
    }

    (*buffer)[bytes_read] = '\0';

    return 0;
}

// _cp 9copia arquivo) com criaçãp recursiva
int _cp(int current_inode, const char *src_path, const char *src_name,
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
        // se foi passado um dst_path != "." resolva-o (p.ex.: _cp arq dir/arq2 onde dst_path veio como "dir")
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


    if (!hasPermission(src_inode, user_id, PERM_READ) && user_id != ROOT_UID) {
        printf("cp: Acesso negado, requer permissão de leitura no arquivo fonte.\n");
        return -1;
    }

    inode_t *dst_parent = &inode_table[dst_parent_inode];
    if (!hasPermission(dst_parent, user_id, PERM_WRITE | PERM_EXEC) && user_id != ROOT_UID) {
        printf("cp: Acesso negado, requer permissão de escrita e execução no diretório destino.\n");
        return -1;
    }
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



// _mv (move)
int _mv(int current_inode, const char *src_path, const char *src_name,
           const char *dst_path, const char *dst_name, int user_id) {
    // Copia o arquivo
    if (_cp(current_inode, src_path, src_name, dst_path, dst_name, user_id) != 0) return -1;

    // Apaga o arquivo de origem
    int src_parent_inode;
    if (resolvePath(src_path, current_inode, &src_parent_inode) != 0) return -1;

    inode_t *src_parent = &inode_table[src_parent_inode];
    if (!hasPermission(src_parent, user_id, PERM_WRITE | PERM_EXEC) && user_id != ROOT_UID) {
        printf("mv: Acesso negado, requer permissão no diretório de origem.\n");
        return -1;
    }

    return deleteFile(src_parent_inode, src_name, user_id);
}


// ln -s (cria link simbolico)
int _ln_s(int current_inode,
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
    inode_t *dir_inode = &inode_table[link_dir_index];
    if (!hasPermission(dir_inode, user_id, PERM_WRITE | PERM_EXEC) && user_id != ROOT_UID) {
        printf("ln: Acesso negado, requer permissões W e X no diretório.\n");
        return -1;
    }


    // --- 3. Cria o link simbólico ---
    createSymlink(link_dir_index, target_index, link_name, user_id);
    return 0;
}


// _ls (lista elementos)
int _ls(int current_inode, const char *path, int user_id, int info_arg) {
    // checa se o caminho existe
    UNREFERENCED(user_id);
    int target_inode = current_inode;
    if (path && strlen(path) > 0) {
        if (resolvePath(path, current_inode, &target_inode) != 0) {
            printf("ls: caminho não encontrado: %s\n", path);
            return -1;
        }
    }
    
    inode_t *dir_inode = &inode_table[target_inode];

    if (!hasPermission(dir_inode, user_id, PERM_EXEC) && user_id != ROOT_UID) {
        printf("ls: Acesso negado, requer permissão X no diretório.\n");
        return -1;
    }

    if (!hasPermission(dir_inode, user_id, PERM_READ) && user_id != ROOT_UID) {
        printf("ls: Acesso negado, requer permissão R no diretório.\n");
        return -1;
    }

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
                

                // Caso utilize o argumento para emular o _ls - l
                if (info_arg) {
                    // Formata permissões (rwxrwxrwx)
                    char perm_str[10] = "---------";
                    for (int who = 3; who >= 0; who -= 3) {
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

// remove elementos (usada tanto por _rmdir quanto por rm)
int _rm(int current_inode, const char *filepath, int user_id, int remove_dir) {
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


    inode_t *parent = &inode_table[parent_inode];
    if (!hasPermission(parent, user_id, PERM_WRITE | PERM_EXEC) && user_id != ROOT_UID) {
        printf("rm: Acesso negado, requer permissões W e X no diretório pai.\n");
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

    // Verifica conforme o tipo de rm (e.g. rm ou _rmdir)
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
int rm(int current_inode, const char *filepath, int user_id) {
    return _rm(current_inode, filepath, user_id, 0);
}

// _rmdir (remove diretorio)
int _rmdir(int current_inode, const char *filepath, int user_id) {
    return _rm(current_inode, filepath, user_id, 1);
}

int _unlink(int current_inode, const char *filepath, int user_id){
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

    inode_t *parent = &inode_table[parent_inode];
    if (!hasPermission(parent, user_id, PERM_WRITE | PERM_EXEC) && user_id != ROOT_UID) {
        printf("unlink: Acesso negado, requer permissões W e X no diretório pai.\n");
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

int _df(){
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

int _chmod(int current_inode, const char *path, const char* permission_str, int user_id) {
// read - peso 4
// write - peso 2
// execute - peso 1
// exemplo - 77 = rwx rwx
    int target_inode;
    if (resolvePath(path, current_inode, &target_inode) != 0) {
        printf("chmod: esse arquivo não existe\n");
        return -1;
    }
        

    inode_t *inode = &inode_table[target_inode];

    // Somente dono pode alterar permissões
    if (inode->owner_uid != user_id & user_id != ROOT_UID) {
        printf("chmod: Acesso negado, apenas o dono pode usar chmod.\n");
        return -1;
    }

    uint8_t new_perm;
    if (parse_octal_permissions(permission_str, &new_perm) != 0) {
        printf("chmod: formato inválido (use octal, ex: 75, 64, 60)\n");
        return -1;
    }

    inode->permissions = new_perm;
    sync_fs();
    
    return 0;
}



int assert_user_exists(const char* username) {
    char* content;
    _cat(ROOT_INODE, passwd_path, ROOT_UID, &content); // Lê o arquivo passwd para encontrar o usuário e seu UID

    char* line_ptr = strtok(content, "\n"); // Itera sobre cada linha do passwd
    while (line_ptr != NULL) {

        char *colon1_ptr = strchr(line_ptr, ':'); // Procura o ':' que divide as linhas do passwd que contém <nome>:x:<UID>. Nesse caso será utilizado como marcador para o final da string.

        int name_length = colon1_ptr - line_ptr; // calcula o número de carácteres do inicio da linha até o colon ':'
        char username_found[name_length + 1]; 
        strncpy(username_found, line_ptr, name_length); // copia somente o nome do usuario
        username_found[name_length] = '\0';


        if (strcmp(username_found, username) == 0) { // usuário encontrado, procura o UID agora

            char *colon2_ptr = strchr(colon1_ptr + 1, ':'); // <nome>:x:<UID> (ponteiro posicionado no último colon ':' para encotrar UID)

            char *uid_str = colon2_ptr + 1; // CUIDADO, PRECISO VERIFICAR SE ESSE PONTEIRO PODE INCLUIR O '\n' DO FINAL DA STRING

            int uid = atoi(uid_str); // converte a string para inteiro
            free(content);

            return uid;
        }

        line_ptr = strtok(NULL, "\n"); // pula de linha
    }

    free(content);
    return -1;
}


int _chown(int current_inode, const char *path, const char* new_owner, int user_id) {
    if (user_id != ROOT_UID) {printf("chown: Acesso negado, cocê precisa ser root para utilizar esse comando. Utilize o comando 'sudo'\n"); return -1; }
    int new_owner_uid = assert_user_exists(new_owner);
    if (new_owner_uid == -1) {printf("chown: O usuário %s não existe\n", new_owner); return -1;}

    int target_inode;
    if (resolvePath(path, current_inode, &target_inode) != 0) {
        printf("chown: esse arquivo não existe\n");
        return -1;
    }

    inode_t *inode = &inode_table[target_inode];
    inode->owner_uid = new_owner_uid;
    sync_fs();
    return 0;
}

// Utils

int get_next_uid() {
    char *content = NULL;

    // Lê o arquivo passwd
    _cat(ROOT_INODE, passwd_path, ROOT_UID, &content);
    if (!content) return -1;

    char buffer[2048];
    strncpy(buffer, content, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    free(content);

    // tratamento caso o passwd termine com '\n'
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
        buffer[len - 1] = '\0';

    // encontra a última linha
    char *last_line_break = strrchr(buffer, '\n');
    char *last_line = last_line_break ? last_line_break + 1 : buffer;

    // encontra o último 'colon' o do UID
    char *last_colon = strrchr(last_line, ':');
    if (!last_colon) return -1;

    int uid = atoi(last_colon + 1);
    return uid + 1;
}

int create_root() {
    _echo_arrow(ROOT_INODE, passwd_path, "root:x:0\n", ROOT_UID);
    _echo_arrow(ROOT_INODE, shadow_path, "root:!\n", ROOT_UID);
    _mkdir(ROOT_INODE, "home/", ROOT_UID);
    _chmod(ROOT_INODE, "home/", "77", ROOT_UID);
    return 0;
}

int create_user() {
    char username[MAX_NAMESIZE], password[MAX_PASSWORD_SIZE], passwd_entry[MAX_PASSWD_ENTRY], shadow_entry[MAX_SHADOW_ENTRY], encrypted_password[MAX_HASH_SIZE], user_home[MAX_NAMESIZE + 7];
    int passwd_inode, shadow_inode;


    // Input do Usuário
    printf("------------ Criação de usuário -----------\n\n");
    printf("Digite o nome do usuário a ser criado: ");
    fgets(username, MAX_NAMESIZE, stdin);
    printf("Defina a senha: ");
    fgets(password, MAX_PASSWORD_SIZE, stdin);

    // Remove o '\n'
    username[strcspn(username, "\n")] = '\0';
    password[strcspn(password, "\n")] = '\0';

    // Obtendo os I-Nodes do passwd e shadow
    resolvePath(passwd_path, ROOT_INODE, &passwd_inode);
    resolvePath(shadow_path, ROOT_INODE, &shadow_inode);

    // obtém o próximo UID disponível
    int new_uid = get_next_uid();
    
    // Lógica do passwd
    snprintf(passwd_entry, sizeof(passwd_entry), "%s:x:%d\n", username, new_uid);
    addContentToInode(passwd_inode, passwd_entry, strlen(passwd_entry), ROOT_UID);

    
    // Lógica do shadow
    encrypt_password(password, encrypted_password);
    snprintf(shadow_entry, sizeof(shadow_entry), "%s:%s\n", username, encrypted_password);
    addContentToInode(shadow_inode, shadow_entry, strlen(shadow_entry), ROOT_UID);

    // Cria a home do usuário
    snprintf(user_home, sizeof(user_home), "home/%s/", username);
    _mkdir(ROOT_INODE, user_home, new_uid);
    printf("Usuário criado!\n\n");
    return 0;
}


int login(const char* username, const char* password, int uid) {
    char* content;
    char password_found[MAX_HASH_SIZE];

    // Buffers precisam ser zerados antes do uso
    password_found[0] = '\0';
    _cat(ROOT_INODE, shadow_path, ROOT_UID, &content);
    // Itera linhas
    char* line = strtok(content, "\n");
    while (line != NULL)
    {
        // Manipulação da string para encontrar o usuário no shadow <nome_usuario>:<hash da senha>
        char* colon1_ptr = strchr(line, ':');  // posiciona um ponteiro no colon para obter o tamanho do nome

        int user_length = colon1_ptr - line;

        char username_found[user_length + 1];
        strncpy(username_found, line, user_length);
        username_found[user_length] = '\0';

        // Compara usuário
        if (strcmp(username_found, username) == 0) {
            // usuário encontrado!
            // manipulação da string para obter o hash da senha
            size_t end_line = strcspn(colon1_ptr + 1, "\n");
            strncpy(password_found, colon1_ptr + 1, end_line);
            password_found[end_line] = '\0';

            if (strcmp(password_found, crypt(password, password_found)) == 0) { // caso o hash da senha informada coincida com o hash da senha armazenada, autentica o usuário
                authenticated_uid = uid;
            } else {
                authenticated_uid = -1;
            }
            break;
        }

        line = strtok(NULL, "\n");
    }

    printf("%s\n", authenticated_uid != -1 ? "Usuário autenticado!\n" : "Login inválido!");

    free(content);
    return authenticated_uid;
}


// CMD IMPLEMENTATION


void cmd_cd(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid) {
    if (!arg1) { printf("Uso: cd <dir>\n"); return; }
    UNREFERENCED(arg1); UNREFERENCED(arg2); UNREFERENCED(arg3);
    _cd(current_inode, arg1, uid);
}


void cmd_mkdir(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid) {
    if (!arg1) { printf("Uso: mkdir <nome>\n"); return; }
    UNREFERENCED(arg2); UNREFERENCED(arg3);
    _mkdir(*current_inode, arg1, uid);
}

void cmd_touch(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid) {
    if (!arg1) { printf("Uso: touch <arquivo>\n"); return; }
    UNREFERENCED(arg2); UNREFERENCED(arg3);
    _touch(*current_inode, arg1, uid);
}

void cmd_rm(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid) {
    if (!arg1) { printf("Uso: rm <arquivo>\n"); return; }
    UNREFERENCED(arg2); UNREFERENCED(arg3);
    rm(*current_inode, arg1, uid);
}

void cmd_clear(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid) {
    UNREFERENCED(current_inode); UNREFERENCED(arg1); UNREFERENCED(arg2); UNREFERENCED(arg3); UNREFERENCED(uid);
    system("clear");
}

void cmd_rmdir(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid) {
    if (!arg1) { printf("Uso: rmdir <dir>\n"); return; }
    UNREFERENCED(arg2); UNREFERENCED(arg3);
    _rmdir(*current_inode, arg1, uid);
}

void cmd_echo(int *current_inode, const char *content, const char *redir, const char *filename, int uid) {
    if (!content || !redir || !filename) {
        printf("Uso: echo <conteudo> >|>> <arquivo>\n");
        return;
    }

    if (strcmp(redir, ">") == 0)
        _echo_arrow(*current_inode, filename, content, uid);

    else if (strcmp(redir, ">>") == 0)
        _echo_arrow_arrow(*current_inode, filename, content, uid);

    else
        printf("Operador inválido: %s (use > ou >>)\n", redir);
}

void cmd_cat(int *current_inode, const char *file, const char *arg2, const char *arg3, int uid) {
    if (!file) { printf("Uso: cat <arquivo>\n"); return; }
    UNREFERENCED(arg2); UNREFERENCED(arg3);
    char *content = NULL;

    if (_cat(*current_inode, file, uid, &content) != -1)
        printf("%s\n", content);

    free(content);
}


void cmd_ls(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid) {
    UNREFERENCED(arg3);
    if (arg1 && strcmp(arg1, "-l") == 0)
        _ls(*current_inode, arg2 ? arg2 : ".", uid, 1);
    else
        _ls(*current_inode, arg1 ? arg1 : ".", uid, 0);
}


void cmd_cp(int *current_inode, const char *src, const char *dst, const char *arg3, int uid) {
    if (!src || !dst) { printf("Uso: cp <src> <dst>\n"); return; }
    UNREFERENCED(arg3);
    _cp(*current_inode, ".", src, ".", dst, uid);
}


void cmd_mv(int *current_inode, const char *src, const char *dst, const char *arg3, int uid) {
    if (!src || !dst) { printf("Uso: mv <src> <dst>\n"); return; }
    UNREFERENCED(arg3);
    _mv(*current_inode, ".", src, ".", dst, uid);
}


void cmd_ln(int *current_inode, const char *opt, const char *src, const char *dst, int uid) {
    if (!opt || !src || !dst || strcmp(opt, "-s") != 0) {
        printf("Uso: ln -s <src> <dst>\n");
        return;
    }
    _ln_s(*current_inode, src, dst, uid);
}

void cmd_sudo(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid) {
    if (!arg1) { printf("Uso: sudo <comando> <args>\n"); return; }
    int handled = 0, tries = 3;
    char password[MAX_PASSWORD_SIZE];
    while (tries)
    {
        printf("Senha: ");
        fgets(password, MAX_PASSWORD_SIZE, stdin);
        password[strcspn(password, "\n")] = '\0';
        if (login(username, password, uid) != -1) {break;}
        tries--;
    }
    // restaura o uid anterior caso o usuário gaste todas as tentativas.
    if (authenticated_uid == -1) {
        authenticated_uid = uid; 
        return;
    } 
    

    for (int i = 0; i < command_count; i++) {
        if (strcmp(arg1, commands[i].name) == 0) {
            commands[i].fn(
                current_inode,
                arg2, arg3, "",
                ROOT_UID
            );
            handled = 1;
            break;
        }
    }

    if (!handled) {
        printf("sudo: Comando não reconhecido: %s\n", arg1);
    }
}


void cmd_unlink(int *current_inode, const char *f, const char *arg2, const char *arg3, int uid) {
    if (!f) { printf("Uso: unlink <arquivo>\n"); return; }
    UNREFERENCED(arg2); UNREFERENCED(arg3);
    _unlink(*current_inode, f, uid);
}


void cmd_df(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid) {
    UNREFERENCED(current_inode); UNREFERENCED(arg1); UNREFERENCED(arg2); UNREFERENCED(arg3); UNREFERENCED(uid);
    _df();
}

void cmd_chmod(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid) {
    if (!arg1 || !arg2 || !atoi(arg2)) { printf("Uso: chmod <caminho do arquivo> <código da permissão (2 digítos owner|others)>\n"); return; }
    UNREFERENCED(arg3);
    _chmod(*current_inode, arg1, arg2, uid);
}


void cmd_chown(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid) {
    if (!arg1 || !arg2) { printf("Uso: chown <caminho do arquivo> <nome do novo dono>\n"); return; }
    UNREFERENCED(arg3);
    _chown(*current_inode, arg1, arg2, uid);
}


void cmd_create_user(int *current_inode, const char *arg1, const char *arg2, const char *arg3, int uid) {
    UNREFERENCED(current_inode); UNREFERENCED(arg1); UNREFERENCED(arg2); UNREFERENCED(arg3); UNREFERENCED(uid);
    create_user();
}


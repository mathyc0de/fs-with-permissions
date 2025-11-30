#include "fs.h"
#define UNREFERENCED(x) (void)(x)

/* ---- diretórios ---- */
/* Tenta encontrar elemento em um diretório */
int dirFindEntry(int dir_inode, const char *name, inode_type_t type, int *out_inode) {
    if (dir_inode < 0 || dir_inode >= MAX_INODES || !name || !out_inode) 
        return -1;
    

    if (strlen(name) >= sizeof(((dir_entry_t*)0)->name)) {
        // nome muito grande para o campo do dir_entry_t
        return -1;
    }

    int current_inode = dir_inode;

    while (current_inode >= 0) {
        inode_t *dir = &inode_table[current_inode];
        if (dir->type != FILE_DIRECTORY) return -1;        

        for (int i = 0; i < BLOCKS_PER_INODE; i++) {
            if (dir->blocks[i] == 0) continue;

            // buffer alocado dinamicamente para não sobrecarregar a pilha
            dir_entry_t *buffer = malloc(BLOCK_SIZE);
            if (!buffer) return -1;

            if (readBlock(dir->blocks[i], buffer) != 0) {
                free(buffer);
                return -1;
            }

            int entries = BLOCK_SIZE / sizeof(dir_entry_t);
            for (int j = 0; j < entries; j++) {
                if (strcmp(buffer[j].name, name) == 0 &&
                    (inode_table[buffer[j].inode_index].type == type || type == FILE_SYMLINK || type == FILE_ANY)) {
                        
                    *out_inode = buffer[j].inode_index;
                    free(buffer);
                    return 0;
                }
            }
            free(buffer);
        }
        if (dir->next_inode == 0)
            break;

        current_inode = dir->next_inode;
    }

    return -1;
}

/* Adiciona elemento a um diretorio */
int dirAddEntry(int dir_inode, const char *name, inode_type_t type, int inode_index) {
    if (dir_inode < 0 || dir_inode >= MAX_INODES || !name)
        return -1;

    // evita duplicados
    int found;
    if (dirFindEntry(dir_inode, name, type, &found) == 0)
        return -1;

    int current_inode = dir_inode;

    while (current_inode >= 0) {
        inode_t *dir = &inode_table[current_inode];
        if (dir->type != FILE_DIRECTORY)
            return -1;

        // aloca buffers no heap
        dir_entry_t *buffer = malloc(BLOCK_SIZE);
        dir_entry_t *empty = malloc(BLOCK_SIZE);
        if (!buffer || !empty) {
            free(buffer);
            free(empty);
            return -1;
        }
        memset(empty, 0, BLOCK_SIZE);

        // tenta colocar em todos os blocos existentes
        for (int i = 2; i < BLOCKS_PER_INODE; i++) {
            if (dir->blocks[i] == 0) {
                // se bloco não existe, aloca
                int new_block = allocateBlock();
                if (new_block < 0) {
                    free(buffer);
                    free(empty);
                    return -1;
                }
                dir->blocks[i] = new_block;
                if (writeBlock(new_block, empty) != 0) {
                    free(buffer);
                    free(empty);
                    return -1;
                }
            }

            if (readBlock(dir->blocks[i], buffer) != 0) {
                free(buffer);
                free(empty);
                return -1;
            }

            for (int j = 0; j < BLOCK_SIZE / sizeof(dir_entry_t); j++) {
                if (buffer[j].inode_index == 0) {
                    strncpy(buffer[j].name, name, sizeof(buffer[j].name) - 1);
                    buffer[j].name[sizeof(buffer[j].name) - 1] = '\0';
                    buffer[j].inode_index = inode_index;

                    if (writeBlock(dir->blocks[i], buffer) != 0) {
                        free(buffer);
                        free(empty);
                        return -1;
                    }

                    dir->size += sizeof(dir_entry_t);
                    dir->modification_date = time(NULL);

                    free(buffer);
                    free(empty);
                    return 0;
                }
            }
        }

        // todos os blocos do inode cheio → cria next_inode
        if (dir->next_inode == 0) {
            int next = allocateInode();
            if (next < 0) {
                free(buffer);
                free(empty);
                return -1;
            }

            inode_t *next_inode = &inode_table[next];
            memset(next_inode, 0, sizeof(inode_t));
            next_inode->type = FILE_DIRECTORY;

            int new_block = allocateBlock();
            if (new_block < 0) {
                free(buffer);
                free(empty);
                return -1;
            }

            next_inode->blocks[0] = new_block;
            if (writeBlock(new_block, empty) != 0) {
                free(buffer);
                free(empty);
                return -1;
            }

            dir->next_inode = next;
        }

        free(buffer);
        free(empty);
        current_inode = dir->next_inode;
    }

    return -1;
}

/* Remove elemento de um diretorio */
int dirRemoveEntry(int dir_inode, const char *name, inode_type_t type) {
    if (dir_inode < 0 || dir_inode >= MAX_INODES || !name)
        return -1;

    UNREFERENCED(type);
    int current_inode = dir_inode;

    while (current_inode >= 0) {
        inode_t *dir = &inode_table[current_inode];
        if (dir->type != FILE_DIRECTORY)
            return -1;

        // buffer dinâmico
        dir_entry_t *buffer = malloc(BLOCK_SIZE);
        if (!buffer)
            return -1;

        for (int i = 0; i < BLOCKS_PER_INODE; i++) {
            uint32_t block_index = dir->blocks[i];
            if (block_index == 0)
                continue;

            if (readBlock(block_index, buffer) != 0) {
                free(buffer);
                return -1;
            }

            for (int j = 0; j < BLOCK_SIZE / sizeof(dir_entry_t); j++) {
                if (buffer[j].inode_index != 0 && strcmp(buffer[j].name, name) == 0) {
                    int target_inode = buffer[j].inode_index;

                    // limpa entrada
                    buffer[j].inode_index = 0;
                    buffer[j].name[0] = '\0';

                    if (writeBlock(block_index, buffer) != 0) {
                        free(buffer);
                        return -1;
                    }

                    // limpa dados do inode alvo
                    inode_t *target = &inode_table[target_inode];
                    for (int k = 0; k < BLOCKS_PER_INODE; k++) {
                        if (target->blocks[k] != 0) {
                            freeBlock(target->blocks[k]);
                            target->blocks[k] = 0;
                        }
                    }
                    freeInode(target_inode);

                    inode_table[dir_inode].size -= sizeof(dir_entry_t);
                    inode_table[dir_inode].modification_date = time(NULL);

                    free(buffer);
                    return 0;
                }
            }
        }

        free(buffer);
        current_inode = dir->next_inode;
    }

    return -1;
}

/* Verifica permissoes */
int hasPermission(const inode_t *inode, int user_id, permission_t perm) {
    if (inode->owner_uid == user_id) {
        return ((inode->permissions >> 6) & PERM_RWX) & perm;
    } else {
        return (inode->permissions & PERM_RWX) & perm;
    }
}

/* Cria diretorio */
int createDirectory(int parent_inode, const char *name, int user_id, int* output_inode){
    if (parent_inode < 0 || parent_inode >= MAX_INODES || !name) return -1;
    int dummy_output;
    if (dirFindEntry(parent_inode, name, FILE_DIRECTORY, &dummy_output) == 0) return -1;

    inode_t *parent= &inode_table[parent_inode];
    if (parent_inode != ROOT_INODE){
    if (!hasPermission(parent, user_id, PERM_WRITE)) return -1;
    }

    int new_inode_index = allocateInode();
    if (new_inode_index < 0) return -1;

    inode_t *new_inode = &inode_table[new_inode_index];

    time_t now = time(NULL);

    new_inode->type = FILE_DIRECTORY;
    strncpy(new_inode->name, name, MAX_NAMESIZE-1);
    new_inode->name[MAX_NAMESIZE-1] = '\0';
    new_inode->creation_date = now;
    new_inode->modification_date = now;
    new_inode->size = 0;
    new_inode->creator_uid = user_id;
    new_inode->owner_uid = user_id;
    new_inode->permissions = PERM_RWX << 6 | PERM_RX << 3 | PERM_RX;
    new_inode->link_target_index = -1;

    int block = allocateBlock();
    if (block < 0) return -1;
    new_inode->blocks[0] = block;

    dir_entry_t entries[BLOCK_SIZE / sizeof(dir_entry_t)] = {0};

    strncpy(entries[0].name, ".", sizeof(entries[0].name));
    entries[0].inode_index = new_inode_index;
    strncpy(entries[1].name, "..", sizeof(entries[1].name));
    entries[1].inode_index = parent_inode;

    if (writeBlock(block, entries) != 0) return -1;
    
    if (dirAddEntry(parent_inode, name, FILE_DIRECTORY, new_inode_index) != 0) return -1;
    sync_fs();
    return 0;
}

/* Cria diretorios recursivamente s*/
int createDirectoriesRecursively(const char *path, int current_inode, int user_id) {
    if (!path) return -1;
    if (path[0] == '\0') return -1;

    // Se path == "." nada a fazer
    if (strcmp(path, ".") == 0) return 0;

    // Vamos caminhar token por token, tentando resolver cada nível e criando quando não existir.
    int cur = current_inode;

    // Se começa com '~', trate como absoluto
    const char *p = path;
    if (p[0] == '~') {
        cur = ROOT_INODE;
        p++;
        if (*p == '/') p++;
    }

    char token[256];
    while (*p) {
        int i = 0;
        // extrai token até '/' ou fim
        while (*p && *p != '/' && i < (int)sizeof(token)-1) token[i++] = *p++;
        token[i] = '\0';
        if (*p == '/') p++; // pula '/'

        if (token[0] == '\0' || strcmp(token, ".") == 0) continue;
        if (strcmp(token, "..") == 0) {
            int parent_inode;
            if (dirFindEntry(cur, "..", FILE_DIRECTORY, &parent_inode) != 0) return -1;
            cur = parent_inode;
            continue;
        }

        int next_inode;
        // tentamos achar token no diretório atual (aceitamos FILE_DIRECTORY ou FILE_SYMLINK -> seguido)
        if (dirFindEntry(cur, token, FILE_DIRECTORY, &next_inode) != 0) {
            // não existe -> criar diretório aqui
            if (createDirectory(cur, token, user_id, NULL) != 0) {
                return -1;
            }
            // recuperar inode do diretório criado
            if (dirFindEntry(cur, token, FILE_DIRECTORY, &next_inode) != 0) return -1;
        }

        // se for symlink, resolva link_target_index (resolvePath já faz isso, mas como estamos passo a passo:)
        int depth = 0;
        while (inode_table[next_inode].type == FILE_SYMLINK) {
            next_inode = inode_table[next_inode].link_target_index;
            if (++depth > 16) return -1;
        }

        cur = next_inode;
    }

    return 0;
}

/* Deleta diretorio existente */
int deleteDirectory(int parent_inode, const char *name, int user_id){
    if (parent_inode < 0 || parent_inode >= MAX_INODES || !name) return -1;

    int target_inode;
    if (dirFindEntry(parent_inode, name, FILE_DIRECTORY, &target_inode) != 0) return -1;

    inode_t *target = &inode_table[target_inode];
    if (!hasPermission(target, user_id, PERM_WRITE)) return -1;
    if (target->type != FILE_DIRECTORY) return -1;

    for (int i = 0; i < BLOCKS_PER_INODE; i++) {
        if (target->blocks[i] == 0) continue;

        char *raw = malloc(BLOCK_SIZE);
        if (!raw) return -1;

        if (readBlock(target->blocks[i], raw) != 0) {
            free(raw);
            return -1;
        }
        dir_entry_t *entries = (dir_entry_t *) raw;
        size_t num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
        if (!entries) return -1;

        for (size_t j = 0; j < num_entries; j++) {
            if (entries[j].inode_index != 0 &&
                strcmp(entries[j].name, ".") != 0 &&
                strcmp(entries[j].name, "..") != 0) {
                    free(raw);
                    return -1; // diretorio nao vazio
            }
        }
        free(raw);
    }

    if (dirRemoveEntry(parent_inode, name, FILE_DIRECTORY) != 0) return -1;
    freeInode(target_inode);
    sync_fs();
    return 0;

}

/* Cria arquivo */
int createFile(int parent_inode, const char *name, int user_id){
    if (parent_inode < 0 || parent_inode >= MAX_INODES || !name) return -1;
    int dummy_output;
    if (dirFindEntry(parent_inode, name, FILE_REGULAR, &dummy_output) == 0) return -1;

    inode_t *parent= &inode_table[parent_inode];
    if (parent_inode != ROOT_INODE){
    if (!hasPermission(parent, user_id, PERM_WRITE)) return -1;
    }

    int new_inode_index = allocateInode();
    if (new_inode_index < 0) return -1;
    inode_t *new_inode = &inode_table[new_inode_index];

    time_t now = time(NULL);

    new_inode->type = FILE_REGULAR;
    strncpy(new_inode->name, name, MAX_NAMESIZE-1);
    new_inode->name[MAX_NAMESIZE-1] = '\0';
    new_inode->creation_date = now;
    new_inode->modification_date = now;
    new_inode->size = 0;
    new_inode->creator_uid = user_id;
    new_inode->owner_uid = user_id;
    new_inode->permissions = PERM_RWX << 6 | PERM_RX << 3 | PERM_RX;
    new_inode->link_target_index = -1;

    if (dirAddEntry(parent_inode, name, FILE_REGULAR, new_inode_index) != 0) return -1;
    sync_fs();
    return 0;
}

/* Deleta arquivo */
int deleteFile(int parent_inode, const char *name, int user_id){
    if (parent_inode < 0 || parent_inode >= MAX_INODES || !name) return -1;
    int target_inode;
    if (dirFindEntry(parent_inode, name, FILE_REGULAR, &target_inode) == -1) return -1;

    inode_t *target = &inode_table[target_inode];
    if (!hasPermission(target, user_id, PERM_WRITE)) return -1;

    if (target->type != FILE_REGULAR && target->type != FILE_SYMLINK) return -1;

    for (int i = 0; i < BLOCKS_PER_INODE; i++) {
        if (target->blocks[i] != 0)
            freeBlock(target->blocks[i]);
    }

    if (dirRemoveEntry(parent_inode, name, target->type) == -1) return -1;
    freeInode(target_inode);
    sync_fs();
    return 0;
}

/* Adiciona conteudo a um inode */
int addContentToInode(int inode_index, const char *data, size_t data_size, int user_id) {
    if (!data) return -1;
    if (inode_index < 0 || inode_index >= MAX_INODES) return -1;

    inode_t *inode = &inode_table[inode_index];
    // Permissão de escrita
    if (!hasPermission(inode, user_id, PERM_WRITE)) return -1;

    size_t written = 0;

    // Vai até o último inode encadeado
    inode_t *current = inode;
    int current_idx = inode_index;
    while (current->next_inode != 0) {
        current_idx = current->next_inode;
        current = &inode_table[current_idx];
    }

    // Determina onde está o último bloco parcialmente preenchido (se existir)
    int last_block_slot = -1;
    for (int i = BLOCKS_PER_INODE - 1; i >= 0; --i) {
        if (current->blocks[i] != 0) {
            last_block_slot = i;
            break;
        }
    }

    size_t file_offset = inode->size;
    size_t inner_offset = file_offset % BLOCK_SIZE;

    // Se não há nenhum bloco no inode atual, ou último bloco está cheio -> precisamos criar novo bloco
    if (last_block_slot == -1 || (inner_offset == 0 && inode->size != 0)) {
        last_block_slot = -1; // forçar alocação abaixo
        inner_offset = 0;
    }

    // --- Preencha bloco parcialmente usado (se houver) ---
    if (last_block_slot != -1 && inner_offset > 0) {
        uint32_t block_num = current->blocks[last_block_slot];
        char block_buffer[BLOCK_SIZE];

        if (readBlock(block_num, block_buffer) != 0) return -1;

        size_t can_write = BLOCK_SIZE - inner_offset;
        size_t to_write = (data_size - written < can_write) ? (data_size - written) : can_write;

        memcpy(block_buffer + inner_offset, data + written, to_write);

        if (writeBlock(block_num, block_buffer) != 0) return -1;

        written += to_write;
        file_offset += to_write;
        inner_offset = file_offset % BLOCK_SIZE;
    }

    // --- Agora escreva blocos completos / novos --- 
    while (written < data_size) {
        // encontra slot de bloco livre no inode atual
        int slot = -1;
        for (int i = 0; i < BLOCKS_PER_INODE; ++i) {
            if (current->blocks[i] == 0) { slot = i; break; }
        }

        // se inode atual cheio, alocar novo inode e usar seu slot 0
        if (slot == -1) {
            int new_inode_idx = allocateInode();
            if (new_inode_idx < 0) return -1;
            current->next_inode = new_inode_idx;
            current = &inode_table[new_inode_idx];
            current_idx = new_inode_idx;
            // garantir tipo do inode encadeado (arquivo regular)
            current->type = FILE_REGULAR;
            slot = 0;
        }

        // aloca bloco para esse slot
        if (current->blocks[slot] == 0) {
            int new_block = allocateBlock();
            if (new_block < 0) return -1;
            current->blocks[slot] = new_block;
        }

        // escrever até encher o bloco (ou o que sobrar)
        size_t to_write = (data_size - written >= BLOCK_SIZE) ? BLOCK_SIZE : (data_size - written);
        char block_buffer[BLOCK_SIZE] = {0};
        // se estiver escrevendo menos que um bloco completo, copiamos só os bytes a escrever
        memcpy(block_buffer, data + written, to_write);

        if (writeBlock(current->blocks[slot], block_buffer) != 0) return -1;

        written += to_write;
        file_offset += to_write;
    }

    // atualiza metadados do inode raiz (tamanho e timestamp)
    inode->size = file_offset;
    inode->modification_date = time(NULL);

    // persiste mudanças
    return sync_fs();
}

/* Le conteudo de um inode */
int readContentFromInode(int inode_number, char *buffer, size_t buffer_size, size_t *out_bytes, int user_id) {
    if (!buffer || !out_bytes) return -1;

    int target_inode = inode_number;
    int depth = 0;

    // Segue links simbólicos, com limite de 16
    while (inode_table[target_inode].type == FILE_SYMLINK) {
        target_inode = inode_table[target_inode].link_target_index;
        if (++depth > 16) return -1; // evita loop infinito
    }

    inode_t *inode = &inode_table[target_inode];
    if (!inode || !hasPermission(inode, user_id, PERM_READ)) return -1;

    size_t total_size = inode->size;
    if (buffer_size < total_size + 1) return -1; // espaço para '\0'

    size_t offset = 0;
    inode_t *current = inode;

    while (current) {
        for (int i = 0; i < BLOCKS_PER_INODE; i++) {
            if (current->blocks[i] == 0) continue;

            char block_buffer[BLOCK_SIZE];
            if (readBlock(current->blocks[i], block_buffer) != 0) return -1;

            size_t to_copy = BLOCK_SIZE;
            if (offset + to_copy > total_size) to_copy = total_size - offset;

            memcpy(buffer + offset, block_buffer, to_copy);
            offset += to_copy;

            if (offset >= total_size) break; // já leu todo o arquivo
        }

        if (offset >= total_size) break;

        if (current->next_inode != 0) {
            current = &inode_table[current->next_inode];
        } else {
            current = NULL;
        }
    }

    buffer[offset] = '\0';
    *out_bytes = offset;
    return 0;
}

/* Cria link simbolico */
int createSymlink(int parent_inode, int target_index, const char *link_name, int user_id) {
    // 1. Verifica se link_name já existe
    int dummy_output;
    if (!dirFindEntry(parent_inode, link_name, FILE_SYMLINK, &dummy_output)) return -1; // erro, já existe
    

    inode_t *parent = &inode_table[parent_inode];
    if (!hasPermission(parent, user_id, PERM_WRITE)) return -1;

    // 2. Aloca um novo i-node
    int inode_index = allocateInode();
    inode_t *inode = &inode_table[inode_index];
    if (!inode) return -1;

    // 3. Preenche campos
    strncpy(inode->name, link_name, MAX_NAMESIZE-1);
    inode->name[MAX_NAMESIZE-1] = '\0';
    inode->type = FILE_SYMLINK;
    inode->size = 0;
    inode->link_target_index = target_index;
    inode->creation_date = time(NULL);
    inode->modification_date = inode->creation_date;
    inode->creator_uid = user_id;
    inode->owner_uid = user_id;
    inode->permissions = inode_table[target_index].permissions;

    if (dirAddEntry(parent_inode, link_name, FILE_SYMLINK, inode_index) != 0){
        freeInode(inode_index);
        return -1;
    }

    sync_inode(inode_index);
    return 0;
}

int deleteSymlink(int parent_inode, int target_inode_idx, int user_id) {
    if (parent_inode < 0 || parent_inode >= MAX_INODES || !target_inode_idx) return -1;

    inode_t *target = &inode_table[target_inode_idx];
    if (!hasPermission(target, user_id, PERM_WRITE)) return -1;

    if (target->type != FILE_SYMLINK) return -1;

    if (dirRemoveEntry(parent_inode, target->name, target->type) == -1) return -1;
    freeInode(target_inode_idx);
    sync_fs();
    return 0;
}

/* Encontra inode a partir de um path */
int resolvePath(const char *path, int current_inode, int *inode_out) {
    if (!path || !inode_out) return -1;

    int current = current_inode;

    // Caminho absoluto
    if (path[0] == '~') {
        current = ROOT_INODE;
        path++; // pula o '~'
        if (*path == '/') path++; // pula barra inicial
    }
    
    char token[256];
    const char *p = path;
    while (*p) {
        int i = 0;
        while (*p && *p != '/') token[i++] = *p++;
        token[i] = '\0';
        if (*p == '/') p++; // pula barra

        if (strcmp(token, ".") == 0 || token[0] == '\0') continue;

        if (strcmp(token, "..") == 0) {
            int parent_inode;
            if (dirFindEntry(current, "..", FILE_DIRECTORY, &parent_inode) != 0) return -1;
            current = parent_inode;
            continue;
        }
        
        int next_inode;
        const char *next_slash = strchr(p, '/');
        int type = next_slash ? FILE_DIRECTORY : FILE_ANY;

        if (dirFindEntry(current, token, type, &next_inode) != 0) return -1;

        int depth = 0;
        while (inode_table[next_inode].type == FILE_SYMLINK) {
            next_inode = inode_table[next_inode].link_target_index;
            if (++depth > 16) return -1; // evita loops
        }

        current = next_inode;
    }

    *inode_out = current;
    return 0;
}

/* Separa path entre caminho do pai e o nome do arquivo */
void splitPath(const char *full_path, char *dir_path, char *base_name) {
    const char *last_slash = strrchr(full_path, '/');
    if (last_slash) {
        size_t dir_len = last_slash - full_path;
        strncpy(dir_path, full_path, dir_len);
        dir_path[dir_len] = '\0';
        strcpy(base_name, last_slash + 1);
    } else {
        strcpy(dir_path, "."); // diretório atual
        strcpy(base_name, full_path);
    }
}
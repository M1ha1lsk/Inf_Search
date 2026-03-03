#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#endif

struct DocNode {
    size_t doc_id;
    DocNode* next;
};

struct IndexEntry {
    char* term;
    DocNode* docs;
    IndexEntry* next;
};

struct DocURL {
    char* url;
};

const size_t HASH_TABLE_SIZE = 1000003;
IndexEntry* hash_table[HASH_TABLE_SIZE] = {nullptr};

DocURL* doc_urls = nullptr;
size_t max_doc_id = 0;

// Глобальный массив всех ID документов для поддержки NOT
size_t* all_doc_ids = nullptr;
size_t all_docs_count = 0;

unsigned long hash(const char* str) {
    unsigned long h = 5381;
    int c;
    while ((c = *str++)) h = ((h << 5) + h) + c;
    return h % HASH_TABLE_SIZE;
}

void add_to_index(const char* term, size_t doc_id) {
    unsigned long idx = hash(term);
    IndexEntry* entry = hash_table[idx];

    while (entry) {
        if (strcmp(entry->term, term) == 0) {
            DocNode* n = entry->docs;
            while (n) {
                if (n->doc_id == doc_id) return;
                n = n->next;
            }
            DocNode* new_n = new DocNode;
            new_n->doc_id = doc_id;
            new_n->next = entry->docs;
            entry->docs = new_n;
            return;
        }
        entry = entry->next;
    }

    entry = new IndexEntry;
    entry->term = new char[strlen(term) + 1];
    strcpy(entry->term, term);
    entry->docs = new DocNode;
    entry->docs->doc_id = doc_id;
    entry->docs->next = nullptr;
    entry->next = hash_table[idx];
    hash_table[idx] = entry;
}

bool parse_line(const char* line, char* term_out, size_t term_size) {
    const char* sep = strchr(line, ' ');
    if (!sep) return false;

    size_t len = sep - line;
    if (len >= term_size) len = term_size - 1;
    strncpy(term_out, line, len);
    term_out[len] = '\0';

    sep++;
    const char* p = sep;
    while (*p) {
        char* end;
        size_t id = strtoull(p, &end, 10);
        if (end == p) break;
        add_to_index(term_out, id);
        if (*end == '\0') break;
        p = end + 1;
    }
    return true;
}

DocNode* get_docs(const char* term) {
    unsigned long idx = hash(term);
    IndexEntry* e = hash_table[idx];
    while (e) {
        if (strcmp(e->term, term) == 0) return e->docs;
        e = e->next;
    }
    return nullptr;
}

int cmp_id(const void* a, const void* b) {
    size_t x = *(size_t*)a, y = *(size_t*)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

size_t* list_to_array(DocNode* head, size_t* n) {
    size_t cnt = 0;
    for (DocNode* p = head; p; p = p->next) cnt++;
    if (!cnt) { *n = 0; return nullptr; }
    size_t* arr = new size_t[cnt];
    size_t i = 0;
    for (DocNode* p = head; p; p = p->next) arr[i++] = p->doc_id;
    qsort(arr, cnt, sizeof(size_t), cmp_id);
    *n = cnt;
    return arr;
}

size_t* intersect(size_t* a, size_t na, size_t* b, size_t nb, size_t* out_n) {
    size_t* r = new size_t[(na < nb) ? na : nb];
    size_t i = 0, j = 0, k = 0;
    while (i < na && j < nb) {
        if (a[i] == b[j]) {
            r[k++] = a[i];
            i++;
            j++;
        } else if (a[i] < b[j]) {
            i++;
        } else {
            j++;
        }
    }
    *out_n = k;
    return r;
}

size_t* union_(size_t* a, size_t na, size_t* b, size_t nb, size_t* out_n) {
    size_t* r = new size_t[na + nb];
    size_t i = 0, j = 0, k = 0;
    while (i < na && j < nb) {
        if (a[i] == b[j]) {
            r[k++] = a[i];
            i++;
            j++;
        } else if (a[i] < b[j]) {
            r[k++] = a[i];
            i++;
        } else {
            r[k++] = b[j];
            j++;
        }
    }
    while (i < na) r[k++] = a[i++];
    while (j < nb) r[k++] = b[j++];
    *out_n = k;
    return r;
}

void load_urls(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Предупреждение: не удалось открыть файл URL\n");
        return;
    }

    char line[4096];
    size_t url_count = 0;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (!*line) continue;

        // Парсим id
        const char* id_pos = strstr(line, "\"id\":");
        if (!id_pos) continue;
        
        id_pos += 5; // пропускаем "id":
        while (*id_pos == ' ') id_pos++;
        
        char* end;
        size_t id = strtoull(id_pos, &end, 10);
        if (end == id_pos) continue;

        // Парсим url
        const char* url_pos = strstr(line, "\"url\":");
        if (!url_pos) continue;
        
        url_pos += 6; // пропускаем "url":
        while (*url_pos == ' ') url_pos++;
        
        if (*url_pos != '"') continue;
        url_pos++; // пропускаем открывающую кавычку
        
        const char* url_end = strchr(url_pos, '"');
        if (!url_end) continue;

        size_t url_len = url_end - url_pos;

        // Расширяем массив если нужно
        if (id > max_doc_id) {
            size_t new_sz = id + 1;
            DocURL* new_arr = new DocURL[new_sz];
            
            // Инициализируем новые элементы
            for (size_t i = 0; i < new_sz; i++) {
                new_arr[i].url = nullptr;
            }
            
            // Копируем старые данные
            if (doc_urls) {
                for (size_t i = 0; i <= max_doc_id; i++) {
                    new_arr[i].url = doc_urls[i].url;
                }
                delete[] doc_urls;
            }
            
            doc_urls = new_arr;
            max_doc_id = new_sz - 1;
        }

        // Исправляем URL: добавляем 'h' в начало если нужно
        if (url_len >= 3 && strncmp(url_pos, "ttp", 3) == 0) {
            // Создаем новый URL с 'h' в начале
            char* temp = new char[url_len + 2];
            temp[0] = 'h';
            strncpy(temp + 1, url_pos, url_len);
            temp[url_len + 1] = '\0';
            
            doc_urls[id].url = temp;
        } else {
            // Сохраняем как есть
            doc_urls[id].url = new char[url_len + 1];
            strncpy(doc_urls[id].url, url_pos, url_len);
            doc_urls[id].url[url_len] = 0;
        }
        
        url_count++;
    }
    fclose(f);
    
    fprintf(stderr, "Загружено URL: %zu\n", url_count);
}

// Функция для сбора всех ID документов
void collect_all_doc_ids() {
    if (!doc_urls || max_doc_id == 0) return;
    
    // Сначала подсчитаем сколько документов реально есть
    all_docs_count = 0;
    for (size_t i = 0; i <= max_doc_id; i++) {
        if (doc_urls[i].url) {
            all_docs_count++;
        }
    }
    
    if (all_docs_count == 0) return;
    
    // Выделяем память
    all_doc_ids = new size_t[all_docs_count];
    
    // Заполняем массив
    size_t idx = 0;
    for (size_t i = 0; i <= max_doc_id; i++) {
        if (doc_urls[i].url) {
            all_doc_ids[idx++] = i;
        }
    }
    
    fprintf(stderr, "📊 Собрано ID всех документов: %zu\n", all_docs_count);
}

// Функция для вычисления NOT (все документы минус документы с термом)
size_t* not_operation(size_t* a, size_t na, size_t* out_n) {
    if (!all_doc_ids || all_docs_count == 0) {
        *out_n = 0;
        return nullptr;
    }
    
    // Если терм не найден, возвращаем все документы
    if (!a || na == 0) {
        size_t* res = new size_t[all_docs_count];
        memcpy(res, all_doc_ids, all_docs_count * sizeof(size_t));
        *out_n = all_docs_count;
        return res;
    }
    
    // Иначе возвращаем все документы, которых нет в a
    size_t* res = new size_t[all_docs_count];
    size_t i = 0, j = 0, k = 0;
    
    while (i < all_docs_count && j < na) {
        if (all_doc_ids[i] < a[j]) {
            res[k++] = all_doc_ids[i++];
        } else if (all_doc_ids[i] == a[j]) {
            i++;
            j++;
        } else {
            j++;
        }
    }
    
    // Добавляем оставшиеся документы
    while (i < all_docs_count) {
        res[k++] = all_doc_ids[i++];
    }
    
    *out_n = k;
    return res;
}

// Парсер выражений
enum Op { AND, OR, NOT, TERM };

struct Expr {
    Op op;
    Expr* left;
    Expr* right;
    char* term;
    
    Expr() : op(TERM), left(nullptr), right(nullptr), term(nullptr) {}
    ~Expr() {
        if (left) delete left;
        if (right) delete right;
        if (term) delete[] term;
    }
};

char* trim(char* s) {
    while (*s && isspace(*s)) s++;
    char* e = s + strlen(s) - 1;
    while (e >= s && isspace(*e)) *e-- = 0;
    return s;
}

char* parse_expr(char* s, Expr*& out);
char* parse_or(char* s, Expr*& out);
char* parse_and(char* s, Expr*& out);
char* parse_not(char* s, Expr*& out);
char* parse_term(char* s, Expr*& out);

char* parse_term(char* s, Expr*& out) {
    s = trim(s);
    if (*s == '(') {
        s++;
        Expr* e = nullptr;
        s = parse_expr(s, e);
        s = trim(s);
        if (*s == ')') s++;
        out = e;
        return s;
    }
    
    char* start = s;
    while (*s && !isspace(*s) && *s != '&' && *s != '|' && *s != '!' && *s != '(' && *s != ')') {
        s++;
    }
    
    size_t len = s - start;
    if (len > 0) {
        out = new Expr;
        out->op = TERM;
        out->term = new char[len + 1];
        strncpy(out->term, start, len);
        out->term[len] = 0;
    } else {
        out = nullptr;
    }
    return s;
}

char* parse_not(char* s, Expr*& out) {
    s = trim(s);
    if (*s == '!') {
        s++;
        Expr* e = nullptr;
        s = parse_not(s, e);
        if (e) {
            out = new Expr;
            out->op = NOT;
            out->left = e;
        }
        return s;
    }
    return parse_term(s, out);
}

char* parse_and(char* s, Expr*& out) {
    s = parse_not(s, out);
    while (1) {
        s = trim(s);
        if (*s == '&' && *(s + 1) == '&') {
            s += 2;
            Expr* right = nullptr;
            s = parse_not(s, right);
            if (right) {
                Expr* node = new Expr;
                node->op = AND;
                node->left = out;
                node->right = right;
                out = node;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    return s;
}

char* parse_or(char* s, Expr*& out) {
    s = parse_and(s, out);
    while (1) {
        s = trim(s);
        if (*s == '|' && *(s + 1) == '|') {
            s += 2;
            Expr* right = nullptr;
            s = parse_and(s, right);
            if (right) {
                Expr* node = new Expr;
                node->op = OR;
                node->left = out;
                node->right = right;
                out = node;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    return s;
}

char* parse_expr(char* s, Expr*& out) {
    return parse_or(s, out);
}

// Вычисление выражения с поддержкой NOT
size_t* eval_expr(Expr* e, size_t* n) {
    if (!e) {
        *n = 0;
        return nullptr;
    }

    switch (e->op) {
        case TERM: {
            DocNode* docs = get_docs(e->term);
            return list_to_array(docs, n);
        }
        
        case NOT: {
            size_t na = 0;
            size_t* a = eval_expr(e->left, &na);
            size_t* res = not_operation(a, na, n);
            delete[] a;
            return res;
        }
        
        case AND: {
            size_t na = 0, nb = 0;
            size_t* a = eval_expr(e->left, &na);
            size_t* b = eval_expr(e->right, &nb);
            
            if (!a || !b || na == 0 || nb == 0) {
                delete[] a;
                delete[] b;
                *n = 0;
                return nullptr;
            }
            
            size_t* res = intersect(a, na, b, nb, n);
            delete[] a;
            delete[] b;
            return res;
        }
        
        case OR: {
            size_t na = 0, nb = 0;
            size_t* a = eval_expr(e->left, &na);
            size_t* b = eval_expr(e->right, &nb);
            
            if (!a && !b) {
                *n = 0;
                return nullptr;
            }
            if (!a) {
                *n = nb;
                return b;
            }
            if (!b) {
                *n = na;
                return a;
            }
            
            size_t* res = union_(a, na, b, nb, n);
            delete[] a;
            delete[] b;
            return res;
        }
    }
    
    *n = 0;
    return nullptr;
}

// Структура для хранения результатов поиска
struct SearchResults {
    size_t* doc_ids;
    size_t total_count;
    size_t current_offset;
    
    SearchResults() : doc_ids(nullptr), total_count(0), current_offset(0) {}
    
    void free() {
        delete[] doc_ids;
        doc_ids = nullptr;
        total_count = 0;
        current_offset = 0;
    }
};

// Глобальная переменная для хранения последних результатов
SearchResults last_results;

// Функция для вывода страницы результатов
void print_results_page(size_t offset, size_t page_size = 50) {
    if (!last_results.doc_ids || last_results.total_count == 0) {
        printf("Нет результатов для отображения\n");
        return;
    }
    
    size_t end = offset + page_size;
    if (end > last_results.total_count) {
        end = last_results.total_count;
    }
    
    printf("\n=== Результаты %zu - %zu из %zu ===\n", 
           offset + 1, end, last_results.total_count);
    
    for (size_t i = offset; i < end; i++) {
        size_t id = last_results.doc_ids[i];
        if (id <= max_doc_id && doc_urls && doc_urls[id].url) {
            printf("%zu. %s\n", i + 1, doc_urls[id].url);
        } else {
            printf("%zu. (URL для ID %zu не найден)\n", i + 1, id);
        }
    }
    
    // Показываем навигацию
    printf("\n");
    if (offset > 0) {
        printf("[предыдущие 50] ");
    }
    if (end < last_results.total_count) {
        printf("[следующие 50] ");
    }
    printf("\n> ");
    fflush(stdout);
}

// Обработка запроса с поддержкой пагинации
void process_query_with_pagination(const char* query_str) {
    if (!query_str || strlen(query_str) == 0) return;
    
    // Очищаем предыдущие результаты
    last_results.free();
    
    // Копируем строку для парсинга
    char query[4096];
    strncpy(query, query_str, sizeof(query) - 1);
    query[sizeof(query) - 1] = '\0';
    
    // Парсим запрос
    Expr* root = nullptr;
    char* q = query;
    q = parse_expr(q, root);
    
    if (!root) {
        printf("Ошибка парсинга: %s\n", query_str);
        return;
    }

    // Вычисляем результат
    size_t count = 0;
    size_t* results = eval_expr(root, &count);
    delete root;

    if (results && count > 0) {
        // Сохраняем результаты
        last_results.doc_ids = results;
        last_results.total_count = count;
        last_results.current_offset = 0;
        
        // Выводим первую страницу
        print_results_page(0);
    } else {
        printf("Документов не найдено\n> ");
        fflush(stdout);
        delete[] results;
    }
}

// Обработка команд навигации
void handle_navigation_command(const char* cmd) {
    if (strcmp(cmd, "следующие 50") == 0 || strcmp(cmd, "next") == 0) {
        size_t new_offset = last_results.current_offset + 50;
        if (new_offset < last_results.total_count) {
            last_results.current_offset = new_offset;
            print_results_page(new_offset);
        } else {
            printf("Нет следующих результатов\n> ");
            fflush(stdout);
        }
    }
    else if (strcmp(cmd, "предыдущие 50") == 0 || strcmp(cmd, "prev") == 0) {
        if (last_results.current_offset >= 50) {
            size_t new_offset = last_results.current_offset - 50;
            last_results.current_offset = new_offset;
            print_results_page(new_offset);
        } else {
            printf("Нет предыдущих результатов\n> ");
            fflush(stdout);
        }
    }
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    if (argc < 3) {
        fprintf(stderr, "Использование:\n");
        fprintf(stderr, "  %s <индекс.txt> <url.jsonl>            # интерактивный режим\n", argv[0]);
        fprintf(stderr, "  %s <индекс.txt> <url.jsonl> <запросы.txt>  # пакетный режим\n", argv[0]);
        return 1;
    }

    // Загрузка индекса
    FILE* idx_f = fopen(argv[1], "r");
    if (!idx_f) {
        fprintf(stderr, "Ошибка: не удалось открыть файл индекса %s\n", argv[1]);
        return 1;
    }

    const size_t BUFFER_SIZE = 2097152;
    char* buf = new char[BUFFER_SIZE];
    size_t pos = 0;
    int c;
    size_t lines_loaded = 0;

    fprintf(stderr, "Загрузка индекса...\n");
    while ((c = fgetc(idx_f)) != EOF) {
        if (c == '\n') {
            buf[pos] = '\0';
            if (pos > 0) {
                char term[1024];
                if (parse_line(buf, term, sizeof(term))) {
                    lines_loaded++;
                    if (lines_loaded % 100000 == 0) {
                        fprintf(stderr, "  загружено %zu строк...\n", lines_loaded);
                    }
                }
            }
            pos = 0;
        } else if (pos < BUFFER_SIZE - 1) {
            buf[pos++] = (char)c;
        }
    }
    
    if (pos > 0) {
        buf[pos] = '\0';
        char term[1024];
        parse_line(buf, term, sizeof(term));
    }

    delete[] buf;
    fclose(idx_f);
    fprintf(stderr, "Загружено строк: %zu\n", lines_loaded);
    
    // Загрузка URL
    fprintf(stderr, "Загрузка URL...\n");
    load_urls(argv[2]);
    
    // Собираем все ID документов для поддержки NOT
    collect_all_doc_ids();

    // Пакетный режим
    if (argc >= 4) {
        FILE* queries_f = fopen(argv[3], "r");
        if (!queries_f) {
            fprintf(stderr, "Ошибка: не удалось открыть файл запросов %s\n", argv[3]);
        } else {
            char query[4096];
            while (fgets(query, sizeof(query), queries_f)) {
                query[strcspn(query, "\n")] = 0;
                if (strlen(query) > 0) {
                    // Парсим и выполняем запрос
                    Expr* root = nullptr;
                    char* q = query;
                    q = parse_expr(q, root);
                    
                    if (root) {
                        size_t count = 0;
                        size_t* results = eval_expr(root, &count);
                        delete root;
                        
                        if (results && count > 0) {
                            for (size_t i = 0; i < count; i++) {
                                size_t id = results[i];
                                if (id <= max_doc_id && doc_urls && doc_urls[id].url) {
                                    printf("%s\n", doc_urls[id].url);
                                }
                            }
                        }
                        delete[] results;
                    }
                }
            }
            fclose(queries_f);
        }
    } 
    // Интерактивный режим
    else {
        fprintf(stderr, "Интерактивный режим. Введите 'quit' для выхода.\n");
        fprintf(stderr, "Команды: 'следующие 50' или 'next', 'предыдущие 50' или 'prev'\n");
        
        char input[4096];
        while (true) {
            if (last_results.total_count == 0) {
                printf("> ");
            }
            fflush(stdout);

            if (!fgets(input, sizeof(input), stdin)) break;
            input[strcspn(input, "\n")] = 0;
            
            if (strcmp(input, "quit") == 0) break;
            if (strlen(input) == 0) continue;
            
            // Проверяем, является ли ввод командой навигации
            if (last_results.total_count > 0 && 
                (strcmp(input, "следующие 50") == 0 || 
                strcmp(input, "next") == 0 ||
                strcmp(input, "предыдущие 50") == 0 || 
                strcmp(input, "prev") == 0)) {
                handle_navigation_command(input);
            } else {
                // Это новый поисковый запрос
                process_query_with_pagination(input);
            }
        }
    }

    // Очистка памяти
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        IndexEntry* e = hash_table[i];
        while (e) {
            IndexEntry* next = e->next;
            DocNode* d = e->docs;
            while (d) {
                DocNode* nd = d->next;
                delete d;
                d = nd;
            }
            delete[] e->term;
            delete e;
            e = next;
        }
    }

    if (doc_urls) {
        for (size_t i = 0; i <= max_doc_id; i++) {
            delete[] doc_urls[i].url;
        }
        delete[] doc_urls;
    }
    
    if (all_doc_ids) {
        delete[] all_doc_ids;
    }

    return 0;
}
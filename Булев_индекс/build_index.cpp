#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#endif

bool is_valid_term(const char* term) {
    size_t len = strlen(term);
    if (len < 2) return false;
    for (size_t i = 0; i < len; i++) {
        if (!isalnum(term[i]) && term[i] != '\'') return false;
    }
    return true;
}

bool extract_doc_data(const char* line, size_t* id_out, char** url_out, char*** stems_out, int* count_out) {
    const char* id_pos = strstr(line, "\"id\":");
    if (!id_pos) return false;
    id_pos += 5;
    while (*id_pos == ' ') id_pos++;
    char* end;
    size_t doc_id = strtoull(id_pos, &end, 10);
    if (end == id_pos) return false;
    *id_out = doc_id;

    const char* url_start = strstr(line, "\"url\":\"");
    if (!url_start) return false;
    url_start += 8;
    const char* url_end = strchr(url_start, '"');
    if (!url_end) return false;
    size_t url_len = url_end - url_start;
    *url_out = new char[url_len + 1];
    strncpy(*url_out, url_start, url_len);
    (*url_out)[url_len] = '\0';

    const char* stems_start = strstr(line, "\"stems\":[");
    if (!stems_start) return false;
    stems_start += 10;
    const char* stems_end = strrchr(stems_start, ']');
    if (!stems_end) return false;

    size_t len = stems_end - stems_start;
    char* stems_str = new char[len + 1];
    strncpy(stems_str, stems_start, len);
    stems_str[len] = '\0';

    int capacity = 100;
    char** stems = new char*[capacity];
    int count = 0;

    char* token = strtok(stems_str, ",");
    while (token) {
        while (*token == ' ' || *token == '"') token++;
        char* end_token = token + strlen(token) - 1;
        while (end_token > token && (*end_token == '"' || *end_token == ']' || *end_token == ' ')) {
            *end_token = '\0';
            end_token--;
        }
        if (strlen(token) > 0 && is_valid_term(token)) {
            if (count >= capacity) {
                capacity *= 2;
                char** new_stems = new char*[capacity];
                for (int i = 0; i < count; i++) new_stems[i] = stems[i];
                delete[] stems;
                stems = new_stems;
            }
            stems[count] = new char[strlen(token) + 1];
            strcpy(stems[count], token);
            count++;
        }
        token = strtok(nullptr, ",");
    }

    delete[] stems_str;
    *stems_out = stems;
    *count_out = count;
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Использование: %s <вход.stems_with_urls.jsonl> <выход.inverted_index.txt> <выход.doc_urls.jsonl>\n", argv[0]);
        return 1;
    }

    FILE* infile = fopen(argv[1], "r");
    if (!infile) {
        perror("Не удалось открыть входной файл");
        return 1;
    }

    const char* temp_file = "temp_index.txt";
    FILE* temp_out = fopen(temp_file, "w");
    FILE* urls_out = fopen(argv[3], "w");
    if (!temp_out || !urls_out) {
        perror("Не удалось создать временные файлы");
        fclose(infile);
        return 1;
    }

    const size_t BUFFER_SIZE = 2097152;
    char* buffer = new char[BUFFER_SIZE];
    size_t pos = 0;
    int c;

    while ((c = fgetc(infile)) != EOF) {
        if (pos >= BUFFER_SIZE - 1) {
            pos = 0;
            while ((c = fgetc(infile)) != EOF && c != '\n') {}
            continue;
        }

        if (c == '\n') {
            buffer[pos] = '\0';
            pos = 0;

            if (strlen(buffer) == 0) continue;

            size_t doc_id;
            char* url = nullptr;
            char** stems = nullptr;
            int count = 0;

            if (extract_doc_data(buffer, &doc_id, &url, &stems, &count)) {
                fprintf(urls_out, "{\"id\":%zu,\"url\":\"%s\"}\n", doc_id, url);
                for (int i = 0; i < count; i++) {
                    fprintf(temp_out, "%s %zu\n", stems[i], doc_id);
                    delete[] stems[i];
                }
                delete[] stems;
                delete[] url;
            }
        } else {
            buffer[pos++] = (char)c;
        }
    }

    if (pos > 0) {
        buffer[pos] = '\0';
        size_t doc_id;
        char* url = nullptr;
        char** stems = nullptr;
        int count = 0;

        if (extract_doc_data(buffer, &doc_id, &url, &stems, &count)) {
            fprintf(urls_out, "{\"id\":%zu,\"url\":\"%s\"}\n", doc_id, url);
            for (int i = 0; i < count; i++) {
                fprintf(temp_out, "%s %zu\n", stems[i], doc_id);
                delete[] stems[i];
            }
            delete[] stems;
            delete[] url;
        }
    }

    delete[] buffer;
    fclose(infile);
    fclose(temp_out);
    fclose(urls_out);

    int sort_result = system("sort temp_index.txt -o temp_index_sorted.txt");
    if (sort_result != 0) {
        fprintf(stderr, "Ошибка сортировки временного файла.\n");
        return 1;
    }

    FILE* sorted_in = fopen("temp_index_sorted.txt", "r");
    FILE* final_out = fopen(argv[2], "w");
    if (!sorted_in || !final_out) {
        perror("Ошибка открытия итоговых файлов");
        return 1;
    }

    char current_term[1024] = {0};
    char line[2048];
    bool first = true;

    while (fgets(line, sizeof(line), sorted_in)) {
        char term[1024];
        size_t doc_id;
        if (sscanf(line, "%s %zu", term, &doc_id) != 2) continue;

        if (first) {
            strcpy(current_term, term);
            fprintf(final_out, "%s %zu", term, doc_id);
            first = false;
        } else if (strcmp(term, current_term) == 0) {
            fprintf(final_out, ",%zu", doc_id);
        } else {
            fprintf(final_out, "\n%s %zu", term, doc_id);
            strcpy(current_term, term);
        }
    }

    if (!first) {
        fprintf(final_out, "\n");
    }

    fclose(sorted_in);
    fclose(final_out);

    remove("temp_index.txt");
    remove("temp_index_sorted.txt");

    printf("Индекс и URL-ы сохранены.\n");
    return 0;
}
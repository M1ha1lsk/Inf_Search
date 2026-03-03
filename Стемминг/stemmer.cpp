#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#endif

char* my_strdup(const char* s) {
    if (!s) return nullptr;
    size_t len = strlen(s);
    char* copy = new char[len + 1];
    strcpy(copy, s);
    return copy;
}

bool is_consonant(const char* word, int i) {
    char c = word[i];
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') return false;
    if (c == 'y') return (i > 0) ? !is_consonant(word, i - 1) : true;
    return true;
}

int measure(const char* word) {
    int m = 0;
    int i = 0;
    int len = strlen(word);
    while (i < len && is_consonant(word, i)) i++;
    while (i < len) {
        while (i < len && !is_consonant(word, i)) i++;
        if (i >= len) break;
        m++;
        while (i < len && is_consonant(word, i)) i++;    }
    return m;
}

bool replace_ending(char* word, const char* suffix, const char* repl) {
    size_t wlen = strlen(word);
    size_t slen = strlen(suffix);
    if (wlen < slen) return false;
    if (strcmp(word + wlen - slen, suffix) != 0) return false;
    word[wlen - slen] = '\0';
    strcat(word, repl);
    return true;
}

bool replace_ending_if_m_gt0(char* word, const char* suffix, const char* repl) {
    size_t wlen = strlen(word);
    size_t slen = strlen(suffix);
    if (wlen < slen) return false;
    if (strcmp(word + wlen - slen, suffix) != 0) return false;

    char backup[1024];
    strcpy(backup, word);
    word[wlen - slen] = '\0';

    if (measure(word) > 0) {
        strcat(word, repl);
        return true;
    } else {
        strcpy(word, backup);
        return false;
    }
}

bool has_vowel(const char* word) {
    size_t len = strlen(word);
    for (size_t i = 0; i < len; ++i) {
        char c = word[i];
        if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') return true;
        if (c == 'y' && i > 0) return true;
    }
    return false;
}

void porter_stem_step1(char* word) {
    size_t len = strlen(word);
    if (len <= 3) return;

    if (!has_vowel(word)) return;

    char original[1024];
    strcpy(original, word);

    if (replace_ending(word, "sses", "ss")) {
    } else if (replace_ending(word, "ies", "i")) {
    } else {
        len = strlen(word);
        if (len >= 2 && word[len-1] == 's' && word[len-2] != 's') {
            word[len-1] = '\0';
        }
    }

    len = strlen(word);
    if (len >= 3 && replace_ending_if_m_gt0(word, "eed", "ee")) {
    } else {
        char before[1024];
        strcpy(before, word);
        bool ed_removed = replace_ending(word, "ed", "");
        bool ing_removed = !ed_removed && replace_ending(word, "ing", "");

        if (ed_removed || ing_removed) {
            if (has_vowel(word)) {
                len = strlen(word);
                if (len >= 2) {
                    char last1 = word[len-1];
                    char last2 = word[len-2];
                    if (last1 == last2 &&
                        last1 != 'l' && last1 != 's' && last1 != 'z' &&
                        is_consonant(word, len-1)) {
                    } else {
                        strcat(word, "e");
                    }
                } else {
                    strcat(word, "e");
                }
            } else {
                strcpy(word, before);
            }
        }
    }

    len = strlen(word);
    if (len > 1 && word[len-1] == 'y') {
        if (len >= 2) {
            char prev = word[len-2];
            if (prev != 'a' && prev != 'e' && prev != 'i' && prev != 'o' && prev != 'u') {
                word[len-1] = 'i';
            }
        }
    }

    if (strlen(word) <= 2) {
        strcpy(word, original);
    }
}

size_t extract_doc_id(const char* line) {
    const char* pos = strstr(line, "\"id\":");
    if (!pos) return 0;
    pos += 5;
    char* end;
    size_t id = strtoull(pos, &end, 10);
    if (end == pos) return 0;
    return id;
}

bool extract_field(const char* line, const char* field_name, char* out, size_t out_size) {
    char pattern[100];
    snprintf(pattern, sizeof(pattern), "\"%s\":", field_name);

    const char* pos = strstr(line, pattern);
    if (!pos) return false;

    pos += strlen(pattern);
    while (*pos == ' ') pos++;
    if (*pos != '"') return false;
    pos++;

    const char* start = pos;
    const char* end = start;
    while (*end && *end != '"') {
        if (*end == '\\' && *(end + 1)) end++;
        end++;
    }

    size_t len = end - start;
    if (len >= out_size) len = out_size - 1;
    strncpy(out, start, len);
    out[len] = '\0';
    return true;
}

bool extract_tokens(const char* line, char** tokens_str_out) {
    const char* pos = strstr(line, "\"tokens\":[");
    if (!pos) return false;

    const char* start = pos + 10;
    const char* end = start;
    int bracket_depth = 1;
    while (*end && bracket_depth > 0) {
        if (*end == '[') bracket_depth++;
        else if (*end == ']') bracket_depth--;
        if (bracket_depth == 0) break;
        end++;
    }
    if (bracket_depth != 0) return false;

    size_t len = end - start + 1;
    *tokens_str_out = new char[len + 1];
    strncpy(*tokens_str_out, start, len);
    (*tokens_str_out)[len] = '\0';
    return true;
}

char** split_tokens(char* tokens_str, int& count) {
    count = 0;
    int capacity = 10;
    char** tokens = new char*[capacity];

    char* token = strtok(tokens_str, ",");
    while (token) {
        while (*token == ' ' || *token == '"') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && (*end == '"' || *end == ' ')) {
            *end = '\0';
            end--;
        }
        if (strlen(token) > 0) {
            bool skip = false;
            if (strlen(token) < 3) skip = true;
            else {
                bool is_number = true;
                for (size_t i = 0; token[i]; i++) {
                    if (!isdigit(token[i])) {
                        is_number = false;
                        break;
                    }
                }
                if (is_number) skip = true;
                else if (strcmp(token, "book") == 0 || strcmp(token, "part") == 0) skip = true;
            }
            if (!skip) {
                if (count >= capacity) {
                    capacity *= 2;
                    char** new_tokens = new char*[capacity];
                    for (int i = 0; i < count; ++i) {
                        new_tokens[i] = tokens[i];
                    }
                    delete[] tokens;
                    tokens = new_tokens;
                }
                tokens[count] = my_strdup(token);
                count++;
            }
        }
        token = strtok(nullptr, ",");
    }
    return tokens;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Использование: %s <вход.tokens.jsonl> <выход.stems.jsonl>\n", argv[0]);
        return 1;
    }

    FILE* infile = fopen(argv[1], "r");
    FILE* outfile = fopen(argv[2], "w");
    if (!infile || !outfile) {
        perror("Ошибка открытия файла");
        return 1;
    }

    const size_t BUFFER_SIZE = 2097152;
    char* buffer = new char[BUFFER_SIZE];
    size_t doc_count = 0;
    size_t total_stems = 0;
    int c;
    size_t pos = 0;

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

            size_t doc_id = extract_doc_id(buffer);
            if (doc_id == 0) continue;

            char url[65536] = {0};
            if (!extract_field(buffer, "url", url, sizeof(url))) {
                continue;
            }

            char* tokens_str = nullptr;
            if (!extract_tokens(buffer, &tokens_str)) continue;

            int token_count = 0;
            char** tokens = split_tokens(tokens_str, token_count);

            char** stems = new char*[token_count];
            int stem_count = 0;
            for (int i = 0; i < token_count; ++i) {
                stems[stem_count] = my_strdup(tokens[i]);
                porter_stem_step1(stems[stem_count]);

                if (strlen(stems[stem_count]) >= 3) {
                    total_stems++;
                    stem_count++;
                } else {
                    delete[] stems[stem_count];
                }
            }

            if (stem_count > 0) {
                fprintf(outfile, "{\"id\":%zu,\"url\":\"%s\",\"stems\":[", doc_id, url);
                for (int i = 0; i < stem_count; ++i) {
                    if (i > 0) fprintf(outfile, ",");
                    fprintf(outfile, "\"%s\"", stems[i]);
                }
                fprintf(outfile, "]}\n");
                doc_count++;
            }

            for (int i = 0; i < token_count; ++i) delete[] tokens[i];
            delete[] tokens;
            for (int i = 0; i < stem_count; ++i) delete[] stems[i];
            delete[] stems;
            delete[] tokens_str;
        } else {
            buffer[pos++] = (char)c;
        }
    }

    if (pos > 0) {
        buffer[pos] = '\0';
        size_t doc_id = extract_doc_id(buffer);
        if (doc_id != 0) {
            char url[65536] = {0};
            if (!extract_field(buffer, "url", url, sizeof(url))) {
                goto cleanup_last;
            }

            char* tokens_str = nullptr;
            if (extract_tokens(buffer, &tokens_str)) {
                int token_count = 0;
                char** tokens = split_tokens(tokens_str, token_count);

                char** stems = new char*[token_count];
                int stem_count = 0;
                for (int i = 0; i < token_count; ++i) {
                    stems[stem_count] = my_strdup(tokens[i]);
                    porter_stem_step1(stems[stem_count]);
                    if (strlen(stems[stem_count]) >= 3) {
                        total_stems++;
                        stem_count++;
                    } else {
                        delete[] stems[stem_count];
                    }
                }

                if (stem_count > 0) {
                    fprintf(outfile, "{\"id\":%zu,\"url\":\"%s\",\"stems\":[", doc_id, url);
                    for (int i = 0; i < stem_count; ++i) {
                        if (i > 0) fprintf(outfile, ",");
                        fprintf(outfile, "\"%s\"", stems[i]);
                    }
                    fprintf(outfile, "]}\n");
                    doc_count++;
                }

                for (int i = 0; i < token_count; ++i) delete[] tokens[i];
                delete[] tokens;
                for (int i = 0; i < stem_count; ++i) delete[] stems[i];
                delete[] stems;
                delete[] tokens_str;
            }
        }
    }

cleanup_last:

    delete[] buffer;
    fclose(infile);
    fclose(outfile);

    printf("Обработано документов: %zu\n", doc_count);
    printf("Всего стемов: %zu\n", total_stems);

    return 0;
}
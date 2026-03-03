#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#endif

struct HashEntry {
    char* word;
    size_t count;
    HashEntry* next;
};

const size_t HASH_TABLE_SIZE = 1000003;
HashEntry* hash_table[HASH_TABLE_SIZE] = {nullptr};

unsigned long hash(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_TABLE_SIZE;
}

void add_word(const char* word) {
    if (!word || strlen(word) == 0) return;

    unsigned long idx = hash(word);
    HashEntry* entry = hash_table[idx];

    while (entry) {
        if (strcmp(entry->word, word) == 0) {
            entry->count++;
            return;
        }
        entry = entry->next;
    }

    entry = new HashEntry;
    entry->word = new char[strlen(word) + 1];
    strcpy(entry->word, word);
    entry->count = 1;
    entry->next = hash_table[idx];
    hash_table[idx] = entry;
}

bool extract_stems(const char* line, char*** stems_out, int* count_out) {
    const char* tokens_start = strstr(line, "\"stems\":[");
    if (!tokens_start) return false;
    tokens_start += 10;
    const char* tokens_end = strrchr(tokens_start, ']');
    if (!tokens_end) return false;

    size_t len = tokens_end - tokens_start;
    char* tokens_str = new char[len + 1];
    strncpy(tokens_str, tokens_start, len);
    tokens_str[len] = '\0';

    int capacity = 100;
    char** stems = new char*[capacity];
    int count = 0;

    char* token = strtok(tokens_str, ",");
    while (token) {
        while (*token == ' ' || *token == '"') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && (*end == '"' || *end == ' ')) {
            *end = '\0';
            end--;
        }
        if (strlen(token) > 0) {
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

    delete[] tokens_str;
    *stems_out = stems;
    *count_out = count;
    return true;
}

struct WordCount {
    char* word;
    size_t count;
};

WordCount* all_words = nullptr;
size_t word_count = 0;
size_t word_capacity = 100000;

void collect_all_words() {
    all_words = new WordCount[word_capacity];

    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry* entry = hash_table[i];
        while (entry) {
            if (word_count >= word_capacity) {
                word_capacity *= 2;
                WordCount* new_words = new WordCount[word_capacity];
                for (size_t j = 0; j < word_count; j++) {
                    new_words[j] = all_words[j];
                }
                delete[] all_words;
                all_words = new_words;
            }
            all_words[word_count].word = entry->word;
            all_words[word_count].count = entry->count;
            word_count++;
            entry = entry->next;
        }
    }
}

int compare_wordcount(const void* a, const void* b) {
    size_t count_a = ((WordCount*)a)->count;
    size_t count_b = ((WordCount*)b)->count;
    if (count_a > count_b) return -1;
    if (count_a < count_b) return 1;
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Использование: %s <вход.stems.jsonl> <выход.zipf_stats.csv>\n", argv[0]);
        return 1;
    }

    FILE* infile = fopen(argv[1], "r");
    if (!infile) {
        perror("Не удалось открыть входной файл");
        return 1;
    }

    const size_t BUFFER_SIZE = 1048576;
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

            char** stems;
            int count;
            if (extract_stems(buffer, &stems, &count)) {
                for (int i = 0; i < count; i++) {
                    add_word(stems[i]);
                    delete[] stems[i];
                }
                delete[] stems;
            }

        } else {
            buffer[pos++] = (char)c;
        }
    }

    if (pos > 0) {
        buffer[pos] = '\0';
        char** stems;
        int count;
        if (extract_stems(buffer, &stems, &count)) {
            for (int i = 0; i < count; i++) {
                add_word(stems[i]);
                delete[] stems[i];
            }
            delete[] stems;
        }
    }

    delete[] buffer;
    fclose(infile);

    collect_all_words();

    qsort(all_words, word_count, sizeof(WordCount), compare_wordcount);

    FILE* outfile = fopen(argv[2], "w");
    if (!outfile) {
        perror("Не удалось создать выходной файл");
        return 1;
    }

    fprintf(outfile, "rank,word,frequency\n");
    for (size_t i = 0; i < word_count; i++) {
        fprintf(outfile, "%zu,%s,%zu\n", i + 1, all_words[i].word, all_words[i].count);
    }

    fclose(outfile);

    printf("Обработано уникальных слов: %zu\n", word_count);
    printf("Результат сохранён в: %s\n", argv[2]);

    return 0;
}
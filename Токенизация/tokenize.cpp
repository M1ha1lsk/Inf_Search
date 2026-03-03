#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <ctime>
#ifdef _WIN32
#include <windows.h>
#endif

char** tokenize(const char* text, int* count_out) {
    size_t len = strlen(text);
    char* lower = new char[len + 1];
    for (size_t i = 0; i < len; i++) {
        lower[i] = (char)tolower((unsigned char)text[i]);
    }
    lower[len] = '\0';

    for (size_t i = 0; i < len; i++) {
        char c = lower[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\'')) {
            lower[i] = ' ';
        }
    }

    int count = 0;
    bool in_token = false;
    for (size_t i = 0; i < len; i++) {
        if (lower[i] != ' ') {
            if (!in_token) {
                count++;
                in_token = true;
            }
        } else {
            in_token = false;
        }
    }

    if (count == 0) {
        *count_out = 0;
        delete[] lower;
        return nullptr;
    }

    char** tokens = new char*[count];
    int index = 0;
    char* token = strtok(lower, " ");
    while (token && index < count) {
        size_t start = 0;
        while (token[start] == '\'') start++;
        size_t end = strlen(token);
        while (end > start && token[end - 1] == '\'') end--;
        if (end > start) {
            char* new_token = new char[end - start + 1];
            strncpy(new_token, token + start, end - start);
            new_token[end - start] = '\0';
            tokens[index] = new_token;
            index++;
        }
        token = strtok(nullptr, " ");
    }

    *count_out = index;
    delete[] lower;
    return tokens;
}

size_t extract_doc_id(const char* line) {
    const char* pos = strstr(line, "\"id\":");
    if (!pos) return 0;
    pos += 5;
    while (*pos == ' ') pos++;
    if (*pos == '"') pos++;
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

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.jsonl> <output.jsonl>\n", argv[0]);
        return 1;
    }

    FILE* infile = fopen(argv[1], "r");
    FILE* outfile = fopen(argv[2], "w");
    if (!infile || !outfile) {
        perror("Error opening file");
        return 1;
    }

    const size_t BUFFER_SIZE = 1048576;
    char* buffer = new char[BUFFER_SIZE];
    size_t doc_count = 0;
    size_t total_tokens = 0;
    size_t total_chars = 0;

    clock_t start_time = clock();

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
            if (doc_id == 0) {
                continue;
            }

            char title[65536] = {0};
            char text[65536] = {0};
            char url[65536] = {0};

            if (!extract_field(buffer, "title", title, sizeof(title)) ||
                !extract_field(buffer, "text", text, sizeof(text)) ||
                !extract_field(buffer, "url", url, sizeof(url))) {
                continue;
            }

            char* full_text = new char[strlen(title) + strlen(text) + 2];
            strcpy(full_text, title);
            strcat(full_text, " ");
            strcat(full_text, text);

            int token_count = 0;
            char** tokens = tokenize(full_text, &token_count);

            total_tokens += token_count;
            for (int i = 0; i < token_count; i++) {
                total_chars += strlen(tokens[i]);
            }

            fprintf(outfile, "{\"id\":%zu,\"url\":\"%s\",\"tokens\":[", doc_id, url);
            for (int i = 0; i < token_count; i++) {
                if (i > 0) fprintf(outfile, ",");
                fprintf(outfile, "\"%s\"", tokens[i]);
                delete[] tokens[i];
            }
            fprintf(outfile, "]}\n");

            fflush(outfile);

            delete[] tokens;
            delete[] full_text;
            doc_count++;

        } else {
            buffer[pos++] = (char)c;
        }
    }

    clock_t end_time = clock();
    double duration_sec = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    double avg_len = total_tokens ? (double)total_chars / total_tokens : 0.0;

    fseek(infile, 0, SEEK_END);
    long file_size = ftell(infile);
    rewind(infile);
    double speed_kb_sec = (file_size / 1024.0) / duration_sec;

    fprintf(stderr, "\nTokenization stats:\n");
    fprintf(stderr, "• Documents processed: %zu\n", doc_count);
    fprintf(stderr, "• Total tokens: %zu\n", total_tokens);
    fprintf(stderr, "• Average token length: %.2f chars\n", avg_len);
    fprintf(stderr, "• Execution time: %.2f sec\n", duration_sec);
    fprintf(stderr, "• Speed: %.1f KB/sec\n", speed_kb_sec);

    if (doc_count == 0) {
        fprintf(stderr, "No documents were processed!\n");
        fprintf(stderr, "Check input file format.\n");
    }

    delete[] buffer;
    fflush(outfile);
    fclose(outfile);
    fclose(infile);

    printf("Done: %zu documents processed.\n", doc_count);
    return 0;
}
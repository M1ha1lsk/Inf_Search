import json
import sys
import os

def deduplicate_jsonl(input_file, output_file, key="id"):
    seen = set()
    unique_count = 0

    output_dir = os.path.dirname(output_file)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)

    with open(input_file, "r", encoding="utf-8") as f_in, \
         open(output_file, "w", encoding="utf-8") as f_out:

        for line_num, line in enumerate(f_in, 1):
            line = line.strip()
            if not line:
                continue
            try:
                doc = json.loads(line)
                value = doc.get(key)
                if value is None:
                    print(f"Пропущена строка {line_num}: нет поля '{key}'")
                    continue
                if value not in seen:
                    seen.add(value)
                    f_out.write(line + "\n")
                    unique_count += 1
            except json.JSONDecodeError:
                print(f"Некорректный JSON в строке {line_num}")
                continue

    print(f"Обработано: {unique_count} уникальных документов.")
    print(f"Сохранено в: {output_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Использование: python dedup_corpus.py <входной.jsonl> <выходной.jsonl>")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]
    deduplicate_jsonl(input_path, output_path, key="id")
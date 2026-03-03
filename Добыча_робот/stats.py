import json
import os

total_docs = 0
total_chars = 0
with open(os.path.join("../data", "corpus_with_ids.jsonl"), "r", encoding="utf-8") as f:
    for line in f:
        doc = json.loads(line)
        total_docs += 1
        total_chars += len(doc["text"])

print(f"Документов: {total_docs}")
print(f"Символов: {total_chars:,}")
print(f"Средний размер: {total_chars / total_docs:.0f} символов")
import json
import sys

def assign_ids(input_path, output_corpus_path, output_map_path):
    with open(input_path, "r", encoding="utf-8") as infile, \
         open(output_corpus_path, "w", encoding="utf-8") as out_corpus, \
         open(output_map_path, "w", encoding="utf-8") as out_map:

        doc_id = 1
        processed_count = 0
        for line in infile:
            line = line.strip()
            if not line:
                continue
            try:
                doc = json.loads(line)
                original_id = doc["id"]
                
                out_map.write(f"{doc_id},{original_id}\n")
                
                new_doc = {
                    "id": doc_id,
                    "title": doc.get("title", ""),
                    "url": doc.get("url", ""),
                    "text": doc.get("text", "")
                }
                out_corpus.write(json.dumps(new_doc, ensure_ascii=False) + "\n")
                
                processed_count += 1
                doc_id += 1

            except Exception as e:
                print(f"Ошибка в строке: {e}", file=sys.stderr)
                continue

        print(f"Присвоено ID: {processed_count} документов")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Использование: python assign_doc_ids.py <вход.corpus.jsonl> <выход.corpus_with_ids.jsonl> <выход.doc_id_map.csv>")
        sys.exit(1)

    assign_ids(sys.argv[1], sys.argv[2], sys.argv[3])
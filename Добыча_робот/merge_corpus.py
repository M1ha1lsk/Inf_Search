import os

with open(os.path.join("../data", "corpus.jsonl"), "w", encoding="utf-8") as out:
    for part in ["corpus_wikipedia.jsonl", "corpus_gutenberg.jsonl"]:
        path = os.path.join("../data", part)
        if os.path.exists(path):
            with open(path, encoding="utf-8") as f:
                out.write(f.read())
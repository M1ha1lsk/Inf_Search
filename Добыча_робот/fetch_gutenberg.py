import requests
import csv
import re
import json
import time
import os
from tqdm import tqdm

KEYWORDS = [
    "naval", "navy", "fleet", "maritime", "sea power", "seapower", "blue water",
    "battleship", "battlecruiser", "cruiser", "destroyer", "frigate", "corvette",
    "submarine", "u-boat", "torpedo boat", "minelayer", "minesweeper",
    "aircraft carrier", "carrier", "amphibious", "landing craft", "patrol boat",
    "dreadnought", "pre-dreadnought", "monitor", "gunboat", "sloop",
    "convoy", "blockade", "amphibious assault", "naval warfare", "sea battle",
    "naval strategy", "naval tactics", "fleet action", "commerce raiding",
    "anti-submarine", "asw", "sonar", "radar", "signals intelligence", "sigint",
    "world war", "wwi", "wwii", "world war i", "world war ii",
    "jutland", "midway", "pearl harbor", "atlantic", "pacific", "mediterranean",
    "arctic convoy", "malta convoy", "battle of the atlantic", "battle of midway",
    "royal navy", "us navy", "kriegsmarine", "imperial japanese navy",
    "regia marina", "soviet navy", "french navy", "high seas fleet",
    "grand fleet", "home fleet", "mediterranean fleet", "pacific fleet",
    "admiral", "jellicoe", "beatty", "tirpitz", "yamamoto", "nelsen", "fisher",
    "churchill", "roosevelt", "doenitz", "raeder",
    "dreadnought", "treaty", "washington treaty", "london treaty", "naval arms race",
    "gunnery", "fire control", "torpedo", "depth charge", "hedgehog", "squid",
    "codebreaking", "ultra", "enigma", "radio intelligence", "direction finding",
    "shipbuilding", "dockyard", "naval architecture", "logistics", "supply",
    "naval history", "war at sea", "sea war", "ocean warfare"
]

OUTPUT_FILE = os.path.join("data", "corpus_gutenberg.jsonl")
CATALOG_URL = "https://gutenberg.org/cache/epub/feeds/pg_catalog.csv"
WORDS_PER_DOC = 2500
MIN_WORDS = 300

def download_and_filter_catalog():
    print("Скачивание каталога Project Gutenberg...")
    res = requests.get(CATALOG_URL, timeout=60)
    res.raise_for_status()
    
    lines = res.text.splitlines()
    reader = csv.DictReader(lines)
    matched_books = []

    for row in reader:
        if row.get("Language") != "en":
            continue
        title = (row.get("Title") or "").lower()
        subjects = (row.get("Subjects") or "").lower()
        book_id = row.get("Text#")
        if not book_id or not book_id.isdigit():
            continue
        
        text = title + " " + subjects
        if any(kw in text for kw in KEYWORDS):
            matched_books.append((book_id, title.strip()))
    
    print(f"Найдено {len(matched_books)} подходящих книг.")
    return matched_books[:1000]

def fetch_book_text(book_id):
    urls = [
        f"https://www.gutenberg.org/files/{book_id}/{book_id}-0.txt",
        f"https://www.gutenberg.org/ebooks/{book_id}.txt.utf-8"
    ]
    for url in urls:
        try:
            res = requests.get(url, timeout=15)
            if res.status_code == 200 and len(res.text) > 2000:
                text = res.text
                start = text.find("*** START OF")
                end = text.find("*** END OF")
                if start != -1 and end != -1:
                    text = text[start:end]
                return text.strip()
        except Exception:
            continue
    return None

def split_into_docs(text, book_id):
    words = text.split()
    docs = []
    for i in range(0, len(words), WORDS_PER_DOC):
        chunk = words[i:i + WORDS_PER_DOC]
        if len(chunk) < MIN_WORDS:
            break
        doc_text = " ".join(chunk)
        part_num = i // WORDS_PER_DOC + 1
        doc_id = f"gutenberg_{book_id}_part_{part_num}"
        title = f"Book #{book_id} (Part {part_num})"
        docs.append({"id": doc_id, "title": title, "text": doc_text})
    return docs

def main():
    books = download_and_filter_catalog()
    if not books:
        print("Не найдено подходящих книг.")
        return

    total_docs = 0
    with open(OUTPUT_FILE, "w", encoding="utf-8") as f_out:
        for book_id, title in tqdm(books, desc="Скачивание книг"):
            text = fetch_book_text(book_id)
            if not text:
                continue
            docs = split_into_docs(text, book_id)
            for doc in docs:
                f_out.write(json.dumps(doc, ensure_ascii=False) + "\n")
                total_docs += 1
            time.sleep(0.3)

    print(f"\nСохранено {total_docs} документов в {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
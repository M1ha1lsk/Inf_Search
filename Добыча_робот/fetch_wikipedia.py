import wikipediaapi
import json
import re
import os
import time
from requests.exceptions import ReadTimeout, ConnectionError, RequestException

USER_AGENT = "MAI_InfSearch_Lab1/1.0 (m.s.kalyuzhnyy@mai.ru)"
OUTPUT_FILE = os.path.join("data", "corpus_wikipedia.jsonl")

wiki = wikipediaapi.Wikipedia(
    language='en',
    user_agent=USER_AGENT,
    extract_format=wikipediaapi.ExtractFormat.WIKI
)

def get_category_members(category_name, max_pages=10000):
    category = wiki.page(f"Category:{category_name}")
    if not category.exists():
        print(f"Category not found: {category_name}")
        return []

    pages = []
    seen_titles = set()

    def recurse(page, depth=0):
        if len(pages) >= max_pages or depth > 3:
            return
        for member in page.categorymembers.values():
            if len(pages) >= max_pages:
                break
            if member.ns == 0 and not member.title.startswith(('List of', 'Template:', 'Category:', 'File:', 'Wikipedia:')):
                if member.title not in seen_titles:
                    seen_titles.add(member.title)
                    pages.append(member)
            elif member.ns == 14 and depth < 3:
                recurse(member, depth + 1)

    recurse(category)
    return pages[:max_pages]

def clean_text(text):
    text = re.split(r'\n==\s*(See also|References|External links|Bibliography|Notes|Footnotes)\s*==', text, flags=re.IGNORECASE)[0]
    text = re.sub(r'\[\[Category:.*?\]\]', '', text)
    text = re.sub(r'\{\{.*?\}\}', '', text, flags=re.DOTALL)
    return text.strip()

def load_existing_titles():
    titles = set()
    try:
        with open(OUTPUT_FILE, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line:
                    try:
                        doc = json.loads(line)
                        titles.add(doc["title"])
                    except json.JSONDecodeError:
                        continue
        print(f"Уже скачано {len(titles)} статей. Пропускаем их.")
    except FileNotFoundError:
        print("Файл корпуса ещё не существует — создаём новый.")
    return titles

def main():
    categories = [
        "Battleships",
        "Destroyers",
        "Submarines",
        "Naval battles of World War I",
        "Naval battles and operations of World War II",
        "Admirals",
        "Cruisers",
        "Aircraft carriers"
    ]

    all_pages = []
    seen_titles_global = set()

    for cat in categories:
        print(f"\nСбор статей из категории: {cat}")
        try:
            pages = get_category_members(cat, max_pages=10000)
            unique = [p for p in pages if p.title not in seen_titles_global]
            for p in unique:
                seen_titles_global.add(p.title)
            all_pages.extend(unique)
            print(f"→ Найдено {len(unique)} новых статей")
        except Exception as e:
            print(f"Ошибка при обходе {cat}: {e}")

    print(f"\nВсего найдено уникальных статей: {len(all_pages)}")

    existing_titles = load_existing_titles()

    new_pages = [p for p in all_pages if p.title not in existing_titles]
    print(f"Осталось скачать: {len(new_pages)} статей")

    if not new_pages:
        print("Всё уже скачано!")
        return

    with open(OUTPUT_FILE, "a", encoding="utf-8") as f:
        for i, page in enumerate(new_pages, start=1):
            try:
                raw_text = page.text
                if not raw_text or not raw_text.strip():
                    continue

                clean = clean_text(raw_text)
                if len(clean) < 200:
                    continue

                doc = {
                    "id": f"wikipedia_{page.title.replace(' ', '_')}",
                    "title": page.title,
                    "url": f"https://en.wikipedia.org/wiki/{page.title.replace(' ', '_')}",
                    "text": clean
                }
                f.write(json.dumps(doc, ensure_ascii=False) + "\n")
                f.flush()

                if i % 50 == 0:
                    print(f"Сохранено {i}/{len(new_pages)} новых статей...")

            except (ReadTimeout, ConnectionError, RequestException) as e:
                print(f"\nТаймаут при '{page.title}'. Пауза 3 сек...")
                time.sleep(3)
                continue
            except Exception as e:
                print(f"\nОшибка при '{page.title}': {e}")
                continue

    print(f"\nГотово! Всего добавлено: {len(new_pages)} статей.")
    print(f"Итоговый файл: {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
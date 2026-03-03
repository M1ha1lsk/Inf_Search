from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse, RedirectResponse
from fastapi.templating import Jinja2Templates
from contextlib import asynccontextmanager
import subprocess
import uvicorn
import os
import sys
import threading
import queue
import time

INDEX_FILE = "../data/inverted_index.txt"
URLS_FILE = "../data/doc_urls.jsonl"
SEARCH_PROGRAM = "../Булев_поиск/boolean_search"

search_process = None
output_queue = queue.Queue()
is_ready = False

def print_progress(message, current=None, total=None):
    if current is not None and total is not None:
        percent = (current / total) * 100
        bar_length = 30
        filled = int(bar_length * current // total)
        bar = '█' * filled + '░' * (bar_length - filled)
        print(f"\r[{bar}] {current}/{total} ({percent:.1f}%) - {message}", file=sys.stderr, end='')
        if current >= total:
            print(file=sys.stderr)
    else:
        print(f"{message}", file=sys.stderr)

def start_search_engine():
    global search_process, is_ready
    
    if not os.path.exists(SEARCH_PROGRAM):
        print(f"\nПрограмма не найдена: {SEARCH_PROGRAM}", file=sys.stderr)
        return False
    
    print(f"\n{'='*60}", file=sys.stderr)
    print(f"ЗАПУСК ПОИСКОВОЙ СИСТЕМЫ", file=sys.stderr)
    print(f"{'='*60}", file=sys.stderr)
    print(f"Программа: {SEARCH_PROGRAM}", file=sys.stderr)
    print(f"Индекс: {INDEX_FILE}", file=sys.stderr)
    print(f"URL: {URLS_FILE}", file=sys.stderr)
    print(f"{'='*60}\n", file=sys.stderr)
    
    search_process = subprocess.Popen(
        [SEARCH_PROGRAM, INDEX_FILE, URLS_FILE],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )
    
    total_lines = 731092
    loaded_lines = 0
    urls_loaded = False
    
    def stderr_reader():
        nonlocal loaded_lines, urls_loaded
        global is_ready
        while search_process and search_process.poll() is None:
            try:
                line = search_process.stderr.readline()
                if line:
                    line = line.strip()
                    if line:
                        print(f"{line}", file=sys.stderr)
                        
                        if "загружено" in line and "строк" in line:
                            try:
                                import re
                                numbers = re.findall(r'\d+', line)
                                if numbers:
                                    loaded_lines = int(numbers[0])
                                    print_progress("Загрузка индекса", loaded_lines, total_lines)
                            except:
                                pass
                        
                        if "Загружено URL:" in line:
                            urls_loaded = True
                            is_ready = True
                            print(f"\nПоисковая система готова к работе!", file=sys.stderr)
                            print(f"{'='*60}\n", file=sys.stderr)
            except:
                break
    
    threading.Thread(target=stderr_reader, daemon=True).start()
    
    def stdout_reader():
        while search_process and search_process.poll() is None:
            try:
                line = search_process.stdout.readline()
                if line:
                    line = line.strip()
                    if line:
                        output_queue.put(line)
            except:
                break
    
    threading.Thread(target=stdout_reader, daemon=True).start()
    
    print(f"Ожидание загрузки индекса и URL...", file=sys.stderr)
    
    while not urls_loaded:
        time.sleep(1)
    
    print(f"\nСистема полностью загружена!", file=sys.stderr)
    return True

def shutdown_search_engine():
    global search_process
    if search_process:
        print(f"\n{'='*60}", file=sys.stderr)
        print(f"ЗАВЕРШЕНИЕ ПОИСКОВОЙ СИСТЕМЫ", file=sys.stderr)
        print(f"{'='*60}", file=sys.stderr)
        try:
            search_process.stdin.write('quit\n')
            search_process.stdin.flush()
            search_process.terminate()
            search_process.wait(timeout=5)
            print(f"Поисковая система завершена", file=sys.stderr)
        except:
            search_process.kill()
            print(f"Поисковая система принудительно завершена", file=sys.stderr)
        search_process = None

def search_cpp(query: str):
    global search_process, is_ready, output_queue
    
    if not is_ready or not search_process:
        return {"error": "Поисковая система не готова", "urls": [], "total": 0}
    
    try:
        print(f"\n{'─'*40}", file=sys.stderr)
        print(f"Поисковый запрос: '{query}'", file=sys.stderr)
        print(f"{'─'*40}", file=sys.stderr)
        
        while not output_queue.empty():
            try:
                output_queue.get_nowait()
            except:
                pass
        
        search_process.stdin.write(query + '\n')
        search_process.stdin.flush()
        
        urls = []
        collecting = False
        total_results = 0
        
        print(f"Ожидание результатов...", file=sys.stderr)
        
        first_page_timeout = time.time() + 10
        
        while time.time() < first_page_timeout:
            try:
                line = output_queue.get(timeout=0.5)
                if not line:
                    continue
                
                print(f"{line}", file=sys.stderr)
                
                if '=== Результаты' in line and 'из' in line:
                    collecting = True
                    try:
                        parts = line.split('из')
                        if len(parts) > 1:
                            total_str = parts[1].strip().split()[0]
                            total_results = int(total_str)
                            print(f"Всего результатов: {total_results}", file=sys.stderr)
                    except Exception as e:
                        print(f"Ошибка парсинга количества: {e}", file=sys.stderr)
                    continue
                
                if collecting and '. ' in line and '://' in line and line[0].isdigit():
                    parts = line.split('. ', 1)
                    if len(parts) > 1:
                        url = parts[1].strip()
                        
                        if url.startswith('ttps://'):
                            url = 'https://' + url[6:]
                        elif url.startswith('ttp://'):
                            url = 'http://' + url[5:]
                        
                        urls.append(url)
                
                if collecting and '[следующие 50]' in line:
                    break
                    
            except queue.Empty:
                continue
        
        if total_results > 50:
            pages_needed = (total_results + 49) // 50 - 1
            
            for page in range(pages_needed):
                search_process.stdin.write('следующие 50\n')
                search_process.stdin.flush()
                
                page_timeout = time.time() + 5
                page_collecting = False
                
                while time.time() < page_timeout:
                    try:
                        line = output_queue.get(timeout=0.5)
                        if not line:
                            continue
                        
                        print(f"{line}", file=sys.stderr)
                        
                        if '=== Результаты' in line:
                            page_collecting = True
                            continue
                        
                        if page_collecting and '. ' in line and '://' in line and line[0].isdigit():
                            parts = line.split('. ', 1)
                            if len(parts) > 1:
                                url = parts[1].strip()
                                
                                if url.startswith('ttps://'):
                                    url = 'https://' + url[6:]
                                elif url.startswith('ttp://'):
                                    url = 'http://' + url[5:]
                                
                                urls.append(url)
                        
                        if '[следующие 50]' in line or '[предыдущие 50]' in line:
                            break
                            
                    except queue.Empty:
                        continue
        
        print(f"Найдено URL: {len(urls)} из {total_results}", file=sys.stderr)
        
        return {"urls": urls, "total": len(urls), "error": None}
        
    except Exception as e:
        print(f"Ошибка: {str(e)}", file=sys.stderr)
        return {"error": str(e), "urls": [], "total": 0}

@asynccontextmanager
async def lifespan(app: FastAPI):
    success = start_search_engine()
    if not success:
        print(f"КРИТИЧЕСКАЯ ОШИБКА: Поисковая система не запустилась", file=sys.stderr)
    yield
    shutdown_search_engine()

app = FastAPI(lifespan=lifespan)
templates = Jinja2Templates(directory="templates")

@app.get("/", response_class=HTMLResponse)
async def home(request: Request):
    return templates.TemplateResponse("search.html", {"request": request})

@app.get("/search", response_class=HTMLResponse)
async def search(request: Request, q: str = "", page: int = 1):
    print(f"\n{'='*50}", file=sys.stderr)
    print(f"HTTP запрос: /search?q={q}&page={page}", file=sys.stderr)
    
    if not q:
        return templates.TemplateResponse("search.html", {
            "request": request,
            "query": ""
        })
    
    if not is_ready:
        return templates.TemplateResponse("search.html", {
            "request": request,
            "query": q,
            "error": "Поисковая система еще загружается, подождите несколько секунд"
        })
    
    search_result = search_cpp(q)
    
    if search_result["error"]:
        return templates.TemplateResponse("search.html", {
            "request": request,
            "query": q,
            "error": search_result["error"]
        })
    
    all_urls = search_result["urls"]
    total = len(all_urls)
    
    start = (page - 1) * 50
    end = start + 50
    
    if start < total:
        page_urls = all_urls[start:end]
    else:
        page_urls = []
        if page > 1:
            return RedirectResponse(url=f"/search?q={q}&page=1")
    
    total_pages = (total + 49) // 50 if total > 0 else 1
    
    print(f"Ответ: страница {page}/{total_pages}, показано {len(page_urls)} URL", file=sys.stderr)
    
    return templates.TemplateResponse("search.html", {
        "request": request,
        "query": q,
        "results": page_urls,
        "total": total,
        "page": page,
        "total_pages": total_pages,
        "has_next": end < total,
        "has_prev": page > 1,
        "error": None
    })

if __name__ == "__main__":
    print(f"\n{'='*60}", file=sys.stderr)
    print(f"ЗАПУСК FASTAPI СЕРВЕРА", file=sys.stderr)
    print(f"{'='*60}", file=sys.stderr)
    uvicorn.run(app, host="0.0.0.0", port=8000)
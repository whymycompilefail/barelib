import requests
from bs4 import BeautifulSoup

# Check all files in LatestRelease
r = requests.get('https://mirrors.tuna.tsinghua.edu.cn/github-release/be5invis/Sarasa-Gothic/LatestRelease/', timeout=30)
soup = BeautifulSoup(r.text, 'html.parser')
for link in soup.find_all('a'):
    href = link.get('href')
    if href and 'SarasaGothicSC-TTF' in href:
        full_url = f'https://mirrors.tuna.tsinghua.edu.cn{href}'
        print(f'Full URL: {full_url}')
        # Try with a browser-like User-Agent
        headers = {'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'}
        r2 = requests.get(full_url, timeout=30, allow_redirects=True, headers=headers)
        print(f'Status: {r2.status_code}')
        ct = r2.headers.get('content-type', '')
        print(f'Content-Type: {ct}')
        if r2.status_code == 200:
            cl = r2.headers.get('content-length', 'unknown')
            print(f'Content-Length: {cl}')
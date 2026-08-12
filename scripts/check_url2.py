import requests
url = 'https://mirror.nju.edu.cn/github-release/be5invis/Sarasa-Gothic/LatestRelease/SarasaGothicSC-TTF-1.0.40.7z'
print('Trying:', url)
headers = {'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'}
r = requests.get(url, timeout=120, headers=headers, allow_redirects=True, stream=True)
print('Status:', r.status_code)
print('Content-Type:', r.headers.get('content-type', ''))
print('Content-Length:', r.headers.get('content-length', '0'))
print('Final URL:', r.url)
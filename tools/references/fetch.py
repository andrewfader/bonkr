#!/usr/bin/env python3
"""Fetches BONKR's reference recordings (references/manifest.tsv) into ~/.cache/bonkr-references,
pins or verifies their SHA-256 (references/checksums.sha256), and converts each to 48 kHz mono
32-bit float WAV (the channels summed) with ffmpeg for bonkr-reference to read.

  tools/references/fetch.py           download what is missing, verify against the pins
  tools/references/fetch.py --pin     download what is missing, write the pins

A URL of the form <archive.zip>!<member> is a single member of a remote zip, read with HTTP range
requests so the multi-gigabyte DrumGizmo kits are never downloaded whole. A download that fails
is reported and skipped; the rest carry on."""
import hashlib, io, os, shutil, subprocess, sys, time, urllib.request, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REFS = os.path.join(HERE, '..', '..', 'references')
CACHE = os.path.expanduser(os.environ.get('BONKR_REFERENCES', '~/.cache/bonkr-references'))
AGENT = 'bonkr-reference-fetch/1.0'


def request(url, method='GET', rng=None, tries=5):
    for attempt in range(tries):
        try:
            q = urllib.request.Request(url, method=method, headers={'User-Agent': AGENT})
            if rng: q.add_header('Range', 'bytes=%d-%d' % rng)
            return urllib.request.urlopen(q, timeout=90)
        except Exception as e:  # a firewall prompt or a slow host: wait and try again
            last = e
            time.sleep(2 + 3 * attempt)
    raise last


class RangeFile(io.RawIOBase):
    def __init__(self, url):
        self.url, self.pos = url, 0
        self.size = int(request(url, 'HEAD').headers['Content-Length'])
    def seekable(self): return True
    def readable(self): return True
    def tell(self): return self.pos
    def seek(self, offset, whence=0):
        self.pos = offset if whence == 0 else self.pos + offset if whence == 1 else self.size + offset
        return self.pos
    def readinto(self, b):
        if self.pos >= self.size or len(b) == 0: return 0
        end = min(self.size, self.pos + len(b)) - 1
        data = request(self.url, rng=(self.pos, end)).read()
        b[:len(data)] = data
        self.pos += len(data)
        return len(data)


def key(url): return hashlib.sha1(url.encode()).hexdigest()[:16]


def download(url, path):
    if '!' in url:
        archive, member = url.split('!', 1)
        z = zipfile.ZipFile(io.BufferedReader(RangeFile(archive), 1 << 16))
        data = z.read(member)
    else:
        data = request(url).read()
    tmp = path + '.part'
    with open(tmp, 'wb') as f: f.write(data)
    os.replace(tmp, path)


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''): h.update(chunk)
    return h.hexdigest()


def main():
    pin = '--pin' in sys.argv
    os.makedirs(os.path.join(CACHE, 'raw'), exist_ok=True)
    os.makedirs(os.path.join(CACHE, 'wav'), exist_ok=True)
    rows = [l.rstrip('\n').split('\t') for l in open(os.path.join(REFS, 'manifest.tsv')) if l.strip() and not l.startswith('#')]
    pins_path = os.path.join(REFS, 'checksums.sha256')
    pins = {}
    if os.path.exists(pins_path):
        for l in open(pins_path):
            digest, name = l.split()
            pins[name] = digest
    new_pins, failed = {}, 0
    for sound, url, *_ in rows:
        k = key(url)
        ext = os.path.splitext(url.split('!')[-1].split('?')[0])[1].lower() or '.bin'
        raw = os.path.join(CACHE, 'raw', k + ext)
        wav = os.path.join(CACHE, 'wav', k + '.wav')
        try:
            if not os.path.exists(raw):
                print(f'fetch {sound:16s} {url}', file=sys.stderr)
                download(url, raw)
            digest = sha256(raw)
            if not pin and k in pins and pins[k] != digest:
                print(f'CHECKSUM MISMATCH {sound} {url}', file=sys.stderr); failed += 1; continue
            new_pins[k] = digest
            if not os.path.exists(wav) and ext == '.wav' and subprocess.run(['ffprobe', '-v', 'quiet', raw]).returncode != 0:
                # ffmpeg will not open some multichannel float WAVs (no channel mask); bonkr-reference
                # reads them itself, averaging the channels, at their own rate.
                shutil.copyfile(raw, wav)
            if not os.path.exists(wav):
                # Every microphone summed (a multitrack kit's close, overhead and room mics alike).
                channels = int(subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'a:0', '-show_entries', 'stream=channels', '-of', 'csv=p=0', raw],
                                              capture_output=True, text=True, check=True).stdout.split()[0])
                mix = 'pan=mono|c0=' + '+'.join(f'{1.0 / channels:.6f}*c{i}' for i in range(channels))
                subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', raw, '-af', mix, '-ar', '48000', '-c:a', 'pcm_f32le', wav + '.part.wav'], check=True)
                os.replace(wav + '.part.wav', wav)
        except Exception as e:
            print(f'FAILED {sound} {url}: {e}', file=sys.stderr); failed += 1
    if pin:
        with open(pins_path, 'w') as f:
            for k in sorted(new_pins): f.write(f'{new_pins[k]}  {k}\n')
    print(f'{len(new_pins)} recordings ready in {CACHE}, {failed} failed', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())

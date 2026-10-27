import html
import re, os, glob, sys

os.chdir(sys.argv[1] if len(sys.argv) > 1 else '.')

strs = set()
for f in glob.glob('src/*.cpp') + glob.glob('src/*.h'):
    t = open(f, encoding='utf-8').read()
    strs |= set(re.findall(r'\btr\(\s*"((?:[^"\\]|\\.)+)"', t))
for f in glob.glob('qml/**/*.qml', recursive=True):
    t = open(f, encoding='utf-8').read()
    strs |= set(re.findall(r'\bqsTr\(\s*"((?:[^"\\]|\\.)+)"', t))

ts = sorted(glob.glob('translations/*.ts'))
print('unique translatable strings:', len(strs), '| ts files:', len(ts))

missing = {}
for f in ts:
    t = open(f, encoding='utf-8').read()
    have = {html.unescape(s) for s in re.findall(r'<source>(.*?)</source>', t, re.S)}
    missing[os.path.basename(f)] = sorted(strs - have)

for k, v in missing.items():
    if v:
        print(k, len(v), v[:8])

common = set.intersection(*[set(v) for v in missing.values()]) if missing else set()
print('MISSING IN ALL TS:')
for s in sorted(common):
    print('  ', repr(s))

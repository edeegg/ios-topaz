#!/usr/bin/env python3
"""Exercise actual patched FTS contact and polling code against recorded outcomes."""
import ctypes
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'research/cepheus/touch-contact-check'

def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)

def section(text, name):
    start = text.index('static void\n' + name)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

def main():
    target = OUT / 'bsd/dev/cepheus_touch.c'
    target.parent.mkdir(parents=True, exist_ok=True)
    original = (ROOT / 'xnu/patches/0068-cepheus-st-fts521-touch.patch').read_text()
    source = original.split('+++ b/bsd/dev/cepheus_touch.c\n', 1)[1]
    target.write_text(''.join(line[1:] + '\n' for line in source.splitlines()
                              if line.startswith('+')), encoding='utf-8', newline='\n')
    for name in ['0070-cepheus-touch-hwcg-console.patch', '0072-cepheus-pmic-keys.patch',
                 '0074-cepheus-touch-small-pin-maps.patch', '0076-cepheus-touch-contact-lifetime.patch']:
        text = (ROOT / 'xnu/patches' / name).read_text()
        parts = text.split('--- ')
        filtered = ''.join('--- ' + part for part in parts[1:]
                           if part.startswith('a/bsd/dev/cepheus_touch.c\n'))
        patch = OUT / name
        patch.write_text(filtered, encoding='utf-8', newline='\n')
        run('git', 'apply', '--unsafe-paths', '--directory=' + OUT.relative_to(ROOT).as_posix(), str(patch))
    source = target.read_text()
    functions = section(source, 'touch_release_all(void)') + '\n' + section(source, 'touch_event(')
    loop = source.split('    unsigned errors=0;\n', 1)[1].split('\nout:\n', 1)[0]
    test = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define TOUCH_MAX 10
static struct { unsigned active; int x[10], y[10]; bool dead; } st;
static unsigned downs, ups, step, fatal, premature;
static void sweet_hid_touch(int id, int down, int x, int y)
{ (void)id; (void)x; (void)y; if (down) ++downs; else ++ups; }
'''+functions+r'''
static int touch_msg(unsigned a,const uint8_t *tx,size_t n,uint8_t *rx,size_t count,bool stretch)
{
    (void)a; (void)tx; (void)n; (void)stretch;
    if (fatal || step < 2) return -1;
    if (rx) {
        for(size_t i=0;i<count;++i) rx[i]=0;
        rx[1]=1;
        if(step==3) { rx[0]=0x23; rx[2]=50; }
        if(step==4) rx[0]=0x33;
    }
    return 0;
}
static void touch_wait(unsigned ms)
{
    (void)ms;
    if(step<4 && !(st.active&1)) ++premature;
    ++step;
    if(!fatal && step==5) st.dead=true;
}
static void polling(void)
{
    int err;
    unsigned errors=0;
'''+loop+r'''
}
int test(void)
{
    const uint8_t enter[8]={0x13,1,20,0,2,0,0,0};
    const uint8_t empty[8]={0};
    const uint8_t reset[8]={0x03,0xf0};
    touch_event(enter); touch_event(empty);
    if(st.active!=1 || downs!=1 || ups) return 1;
    polling();
    if(premature || st.active || downs!=2 || ups!=1) return 2;
    st.dead=false; step=0; fatal=1; downs=ups=0;
    touch_event(enter); polling();
    if(step!=9 || st.active || ups!=1) return 3;
    touch_event(enter); touch_event(reset);
    if(st.active || ups!=2) return 4;
    return 0;
}
'''
    cfile = OUT / 'contacts.c'
    cfile.write_text(test, encoding='utf-8')
    run('clang', '--target=x86_64-pc-windows-msvc', '-ffreestanding', '-fno-builtin',
        '-fno-stack-protector', '-O2', '-Wall', '-Wextra', '-Werror', '-c', str(cfile),
        '-o', str(OUT / 'contacts.obj'))
    run('lld-link', '/dll', '/noentry', '/nodefaultlib', '/export:test',
        '/out:' + str(OUT / 'contacts.dll'), str(OUT / 'contacts.obj'))
    result = ctypes.CDLL(str(OUT / 'contacts.dll')).test()
    if result:
        raise SystemExit('Contact test failed: ' + str(result))
    print('PASS: actual FTS enter/hold/move/up, transient read errors, fatal stop and reset.')
    print('Transport is mocked; this does not validate GPI/SMMU or physical touch on iOS.')

if __name__ == '__main__':
    main()

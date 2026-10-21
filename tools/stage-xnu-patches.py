#!/usr/bin/env python3
"""Stage the full Topaz series or the minimal Mi 9 console bring-up series."""
import argparse
from pathlib import Path


def gpu_backend_patch(root):
    """Compile the backend only on cepheus-ios; no hardware activation hook."""
    directory = root / 'drivers/cepheus'
    files = sorted(p for p in directory.rglob('*') if p.suffix in ('.c', '.h'))
    sources = [p for p in files if p.suffix == '.c']
    patch = ('--- a/bsd/conf/files\n+++ b/bsd/conf/files\n'
             f'@@ -110,2 +110,{2 + len(sources)} @@\n'
             ' bsd/dev/cepheus_keys.c\t\t\tstandard\n')
    for path in sources:
        target = 'bsd/dev/cepheus_gpu/' + path.relative_to(directory).as_posix()
        patch += '+' + target + '\t\tstandard\n'
    patch += ' bsd/dev/mem.c\t\t\t\tstandard\n'
    for path in files:
        target = 'bsd/dev/cepheus_gpu/' + path.relative_to(directory).as_posix()
        lines = path.read_text(encoding='utf-8').splitlines()
        patch += (f'--- /dev/null\n+++ b/{target}\n'
                  f'@@ -0,0 +1,{len(lines)} @@\n' +
                  ''.join('+' + line + '\n' for line in lines))
    patch += '''--- a/bsd/dev/sweet_ufs.c
+++ b/bsd/dev/sweet_ufs.c
@@ -47,5 +47,6 @@
 extern int hz;
 static void sweet_log_start(void);
 extern void sweet_sample_start(void);
 extern void cepheus_touch_init(void);
+extern void cepheus_gpu_probe_init(void);
'''
    patch += ' \n'
    patch += '''\
@@ -606,5 +607,6 @@
 \tsweet_log_start();
 \tsweet_sample_start();
 \tcepheus_touch_init();
+\tcepheus_gpu_probe_init();
 \treturn 0;
 }
'''
    return patch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=('topaz', 'cepheus', 'cepheus-ufs', 'cepheus-ios'), required=True)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1] / 'xnu/patches'
    cepheus_display = {'0061-cepheus-logical-display.patch', '0067-sweet-console-mute.patch',
                       '0068-cepheus-st-fts521-touch.patch',
                       '0070-cepheus-touch-hwcg-console.patch', '0072-cepheus-pmic-keys.patch',
                       '0074-cepheus-touch-small-pin-maps.patch'}
    selected = sorted(p for p in source.iterdir()
                      if p.suffix in ('.patch', '.sh') and
                      ((args.board == 'topaz' and p.name not in
                        {'0061-cepheus-logical-display.patch', '0068-cepheus-st-fts521-touch.patch',
                       '0070-cepheus-touch-hwcg-console.patch', '0072-cepheus-pmic-keys.patch',
                       '0074-cepheus-touch-small-pin-maps.patch'}) or
                       (args.board == 'cepheus-ios' and p.name in cepheus_display) or
                       int(p.name[:4]) <= (59 if args.board == 'cepheus-ios' else
                                          34 if args.board == 'cepheus-ufs' else 18)))
    if not args.destination.is_dir():
        parser.error('destination must be the existing upstream patches directory')
    for path in selected:
        text = path.read_text(encoding='utf-8', errors='surrogateescape')
        if args.board.startswith('cepheus'):
            if path.name.startswith('0010-'):
                # Candidate ABL stride, still awaiting physical measurement.
                # Keep stage-zero marker inside the Mi 9's 2340-line surface.
                for old, new in (('2368 * 4320', '2308 * 4320'),
                                 ('#define EARLY_HEIGHT 2400u',
                                  '#define EARLY_HEIGHT 2340u')):
                    if text.count(old) != 1:
                        raise ValueError(f'{path.name}: expected exactly one {old!r}')
                    text = text.replace(old, new)
            elif path.name.startswith('0018-'):
                old = 'withCStringNoCopy("Kryo 470")'
                if text.count(old) != 1:
                    raise ValueError('CPU label patch context changed')
                text = text.replace(old, 'withCStringNoCopy("Kryo 485")')
        (args.destination / path.name).write_text(text, encoding='utf-8',
                                                errors='surrogateescape', newline='\n')
    print(f'{args.board}: staged {len(selected)} patch/helper files')
    if args.board == 'cepheus-ios':
        # Compiled backend primitives only. Never call GPU programming routines
        # until the platform establishes their documented hardware prerequisites.
        (args.destination / '0075-cepheus-gpu-backend.patch').write_text(
            gpu_backend_patch(source.parents[1]), encoding='utf-8', newline='\n')
        # Authorised target is the EXISTING userdata on this Mi 9, alongside
        # Mindows. Refuse enabling writes for any other partition geometry.
        patch = '''--- a/bsd/dev/sweet_ufs.c
+++ b/bsd/dev/sweet_ufs.c
@@ -588,3 +588,9 @@
 \t/* Only now, with the partition bounds known, can writes be allowed. */
+\tif (rw && (strcmp(want, "userdata") != 0 || su.lun != 0 ||
+\t    su.first_block != 14707712ULL || su.nblocks != 14947072ULL)) {
+\t\tprintf("CEPHEUS UFS: refusing writes: userdata geometry mismatch\\n");
+\t\treturn ENXIO;
+\t}
+\t/* SCSI writes remain bounded to this partition by ufs_write_blocks. */
 \tsu.writable = (rw != 0);
 \tsu.ready = true;
'''
        (args.destination / '0060-cepheus-userdata-write-guard.patch').write_text(
            patch, encoding='utf-8', newline='\n')
        print('Single CPU iOS bring-up; writes require exact authorised userdata geometry.')
    elif args.board == 'cepheus-ufs':
        # The first storage test must never issue a SCSI WRITE command, even
        # if a stale boot command line accidentally requests ufsrw=1.
        patch = '''--- a/bsd/dev/sweet_ufs.c
+++ b/bsd/dev/sweet_ufs.c
@@ -588,3 +588,3 @@
 \t/* Only now, with the partition bounds known, can writes be allowed. */
-\tsu.writable = (rw != 0);
+\tsu.writable = false; /* cepheus: initial storage bring-up is read-only */
 \tsu.ready = true;
'''
        (args.destination / '0035-cepheus-read-only-ufs.patch').write_text(
            patch, encoding='utf-8', newline='\n')
        print('Single CPU, HFS+, portable crypto, UFS forced read-only; no input/modem drivers.')
    elif args.board == 'cepheus':
        print('Single CPU console only; no UFS, touch, USB, modem or iOS root driver.')


if __name__ == '__main__':
    main()

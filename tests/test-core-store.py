#!/usr/bin/env python3
"""Temporary filesystem fault injection. No router or NAND access."""
import gzip, hashlib, json, os, pathlib, shutil, subprocess, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
SOURCE=ROOT/'userspace/core-store'
import shlex
SHELL=shlex.split(os.environ.get('SR7100_TEST_SHELL', 'busybox sh'))
results=[]
with tempfile.TemporaryDirectory(prefix='core-store-check-') as td:
 p=pathlib.Path(td); ov=p/'overlay'; ov.mkdir(); tmp=p/'tmp'; tmp.mkdir(); etc=p/'etc';etc.mkdir(); commands=p/'commands';commands.mkdir()
 (ov/'.sr7100-layout').write_text('sr7100-plugin-expanded-v2\n')
 (p/'mounts').write_text(f'ubi0:rootfs_data {ov} ubifs rw,relatime 0 0\n')
 config=etc/'config';config.write_text('valid config')
 (commands/'uci').write_text(f'#!/bin/sh\necho {config}\n')
 (commands/'df').write_text('''#!/bin/sh
if [ -n "$LOW_SPACE" ]; then
 echo 'fixture 1000 999 1 99% overlay'
else
 echo 'fixture 100000 1 99999 1% overlay'
fi
''')
 (commands/'sync').write_text('''#!/bin/sh
[ -n "$CUT_AFTER" ] || exit 0
n=$(cat "$COUNTER" 2>/dev/null || echo 0); n=$((n+1)); echo "$n" > "$COUNTER"
if [ "$n" = "$CUT_AFTER" ]; then kill -KILL "$PPID"; fi
exit 0
''')
 for f in commands.iterdir():f.chmod(0o755)
 source=SOURCE.read_text().replace('/tmp/',str(tmp)+'/').replace('/proc/mounts',str(p/'mounts')).replace(' -d /etc/openclash', ' -d '+str(etc))
 # Some BusyBox builds prefer built-in applets over PATH. Explicit wrappers
 # make space and sync fault injection deterministic on both configurations.
 wrappers='\n'.join(f'{name}() {{ "{commands/name}" "$@"; }}' for name in ('df','sync','uci'))
 source=source.replace('set -eu', 'set -eu\n'+wrappers, 1)
 script=p/'core-store';script.write_text(source)
 env=dict(os.environ,PATH=str(commands)+':'+os.environ['PATH'],SR7100_CORE_VOLUME=str(ov),COUNTER=str(p/'counter'))
 cores=[]
 for version in range(1,5):
  c=p/f'core{version}';c.write_text(f'#!/bin/sh\n# Version {version}\n[ "$1" != -t ] || [ ! -f "{p}/reject" ] || exit 1\necho "Mihomo test version {version}"\n');c.chmod(0o755);cores.append(c)
 digest=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
 store=ov/'sr7100-core'; runtime=tmp/'etc/openclash/core/clash_meta'
 def run(*args,extra=None,ok=True):
  r=subprocess.run(SHELL+[str(script),*map(str,args)],env=env|dict(extra or {}),capture_output=True,text=True)
  if ok:assert r.returncode==0,(args,r.returncode,r.stdout,r.stderr)
  return r
 def restored(expected):
  runtime.unlink(missing_ok=True); run('restore'); assert digest(runtime)==digest(expected)
 def one_core(expected):
  assert sorted(x.name for x in store.glob('*.gz'))==[digest(expected)+'.gz']
  assert not (store/'previous').exists()
 run('install',cores[0]);restored(cores[0]);one_core(cores[0]);results.append('initial install and restore')
 # Migrate an existing two-version store without changing the current version.
 (store/(digest(cores[3])+'.gz')).write_bytes(gzip.compress(cores[3].read_bytes()))
 (store/'previous').write_text(digest(cores[3])+'\n')
 run('prune');one_core(cores[0]);results.append('migration deletes previous only after current archive validation')
 run('install',cores[1]);one_core(cores[1]);restored(cores[1]);results.append('new version persisted; old archive removed')
 (p/'reject').touch();before=(store/'current').read_bytes();assert run('install',cores[2],ok=False).returncode!=0;assert (store/'current').read_bytes()==before;(p/'reject').unlink();results.append('configuration rejection retains committed version')
 archive=store/(digest(cores[1])+'.gz');good=archive.read_bytes();archive.write_bytes(good[:10]);runtime.unlink()
 # Even a legacy previous pointer must not be used by the new policy.
 (store/(digest(cores[0])+'.gz')).write_bytes(gzip.compress(cores[0].read_bytes()))
 (store/'previous').write_text(digest(cores[0])+'\n')
 assert run('restore',ok=False).returncode!=0 and not runtime.exists()
 assert run('prune',ok=False).returncode!=0 and (store/'previous').exists()
 results.append('corrupt current returns failure to ROM fallback; never restores legacy previous')
 archive.write_bytes(good);run('prune');one_core(cores[1])
 assert run('install',cores[2],extra={'LOW_SPACE':'1'},ok=False).returncode!=0
 restored(cores[1]);one_core(cores[1]);results.append('insufficient space rejects update without deleting current')
 run('install',cores[2]);restored(cores[2]);one_core(cores[2])
 snapshot=p/'snapshot';shutil.copytree(store,snapshot)
 for cut in range(1,9):
  shutil.rmtree(store);shutil.copytree(snapshot,store);(p/'counter').unlink(missing_ok=True)
  run('install',cores[3],extra={'CUT_AFTER':str(cut)},ok=False)
  runtime.unlink(missing_ok=True);run('restore');assert digest(runtime) in (digest(cores[2]),digest(cores[3]))
  current=(store/'current').read_text().strip();assert (store/(current+'.gz')).exists()
  assert len(list(store.glob('*.gz')))==1 and not list(store.glob('.*.gz'))
 results.append('8 forced process kills retain committed core; restore cleans interrupted update archives')
 (ov/'.sr7100-layout').write_text('old layout\n');assert run('install',cores[0],ok=False).returncode!=0;results.append('old layout cannot be modified')
report=dict(passed=True,checks=results,script_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),scope='Selected BusyBox shell; mocked mount/space/config, synthetic executable cores, temporary host filesystem; not NAND power-failure proof')
print(json.dumps(report,indent=2))

"""Compile the production telemetry handlers against host measurement fixtures."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

repo=Path(__file__).resolve().parents[2]
source=(repo/'comm/commands.c').read_text()
normal=source[source.index('\tcase COMM_GET_VALUES:'):source.index('\tcase COMM_SET_DUTY:')]
start=source.index('\tcase COMM_GET_VALUES_SETUP:')
setup=source[start:source.index('\tcase COMM_SET_ODOMETER:',start)]
hw=(repo/'hwconf/SPEEDPED75100/hw_SPEEDPED75100.c').read_text()
start=hw.index('float speedped_get_speed(void)')
speed=hw[start:hw.index('unsigned char speedped_pin_levels',start)]
compiler=os.environ.get('CC') or shutil.which('gcc')
if not compiler:
    raise SystemExit('Set CC to a native host GCC compiler')
with tempfile.TemporaryDirectory(prefix='speedped-telemetry-') as tmp:
    tmp=Path(tmp)
    handlers='#define HW_SPEEDPED75100\nstatic void telemetry(unsigned packet_id,uint8_t *data) { switch(packet_id) {\n'+normal+setup+'} }\n'
    handlers+='#undef HW_SPEEDPED75100\nstatic void telemetry_standard(unsigned packet_id,uint8_t *data) { switch(packet_id) {\n'+normal+'} }\n'
    (tmp/'telemetry_handlers.inc').write_text(handlers)
    (tmp/'telemetry_speed.inc').write_text(speed)
    exe=tmp/('test.exe' if os.name=='nt' else 'test')
    subprocess.run([compiler,'-std=gnu99','-Wall','-Wextra','-Werror','-I'+str(tmp),'-I'+str(repo/'util'),'-I'+str(repo/'hwconf/SPEEDPED75100'),str(Path(__file__).with_suffix('.c')),str(repo/'util/buffer.c'),'-lm','-o',str(exe)],check=True)
    env=dict(os.environ)
    env['PATH']=str(Path(compiler).parent)+os.pathsep+env.get('PATH','')
    subprocess.run([str(exe)],env=env,check=True)

"""Exercise the production current getters with simulated hardware ADC samples."""
from pathlib import Path
import os,shutil,subprocess,tempfile
repo=Path(__file__).resolve().parents[2]
def function(source,name):
    start=source.index('float '+name+'(')
    opening=source.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]+'\n'
compiler=os.environ.get('CC') or shutil.which('gcc')
if not compiler: raise SystemExit('Set CC to a host GCC compiler')
with tempfile.TemporaryDirectory(prefix='speedped-battery-') as tmp:
    tmp=Path(tmp);fragments=function((repo/'hwconf/SPEEDPED75100/hw_SPEEDPED75100.c').read_text(),'speedped_battery_current_read')
    names={
      'motor/mcpwm.c':['mcpwm_get_tot_current_in','mcpwm_get_tot_current_in_filtered'],
      'motor/mcpwm_foc.c':['mcpwm_foc_get_tot_current_in','mcpwm_foc_get_tot_current_in_filtered','mcpwm_foc_get_tot_current_in_motor','mcpwm_foc_get_tot_current_in_filtered_motor'],
      'motor/mc_interface.c':['mc_interface_get_tot_current_in','mc_interface_get_tot_current_in_filtered']}
    for path,functions in names.items():
        source=(repo/path).read_text()
        fragments+=''.join(function(source,name) for name in functions)
    (tmp/'battery_getters.inc').write_text(fragments)
    adc=(repo/'applications/app_adc.c').read_text()
    end=adc.index('        decoded_level2 = brake;')+len('        decoded_level2 = brake;')
    start=adc.rfind('#ifdef HW_SPEEDPED75100',0,end)
    (tmp/'battery_adc2.inc').write_text(adc[start:end])
    exe=tmp/('test.exe' if os.name=='nt' else 'test')
    subprocess.run([compiler,'-std=c99','-Wall','-Wextra','-Werror','-I'+str(tmp),'-I'+str(repo/'hwconf/SPEEDPED75100'),str(Path(__file__).with_suffix('.c')),'-lm','-o',str(exe)],check=True)
    env=dict(os.environ);env['PATH']=str(Path(compiler).parent)+os.pathsep+env.get('PATH','')
    subprocess.run([str(exe)],env=env,check=True)

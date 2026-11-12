"""Verify configuration writes through the real custom-command length guard."""
from pathlib import Path
import os,shutil,subprocess,tempfile
repo=Path(__file__).resolve().parents[2]
commands=(repo/'conf_custom.c').read_text()
a=commands.index('\tcase COMM_SET_CUSTOM_CONFIG:')
b=commands.index('\tcase COMM_GET_CUSTOM_CONFIG_XML:',a)
case=commands[a:b]
fixture=Path(__file__).with_name('test_storage.c').read_text()
wrapper="""
#define HW_SPEEDPED75100
#define COMM_SET_CUSTOM_CONFIG 93
static unsigned acknowledgements;
static bool (*m_set_cfg)(uint8_t *data)=bike_set_cfg;
static void reply_func(uint8_t *data,int32_t len) { assert(len==1 && data[0]==93); acknowledgements++; }
static void set_command(uint8_t *data,unsigned len) {
    unsigned packet_id=COMM_SET_CUSTOM_CONFIG;
    switch(packet_id) {
"""+case+'} }\n'
fixture=fixture.replace('int main(void) {',wrapper+'\nint main(void) {')
checks="""
    uint8_t command[BIKE_WIRE_SIZE+1]={0};
    bike_conf=bike_defaults(); bike_conf.v[BIKE_ADC2_V1]=1300; bike_conf.v[BIKE_ADC2_I1]=0;
    bike_get_cfg(command+1,false); acknowledgements=0;
    set_command(command,66); assert(acknowledgements==0);
    set_command(command,BIKE_WIRE_SIZE); assert(acknowledgements==0);
    set_command(command,BIKE_WIRE_SIZE+1); assert(acknowledgements==1);
    reload(); assert(bike_conf.v[BIKE_ADC2_V1]==1300 && bike_conf.v[BIKE_ADC2_I1]==0);
    command[0]=1; set_command(command,BIKE_WIRE_SIZE+1); assert(acknowledgements==1);
"""
fixture=fixture.replace('    puts("Bike storage:',checks+'\n    puts("Bike storage:')
compiler=os.environ.get('CC') or shutil.which('gcc')
if not compiler: raise SystemExit('Set CC to host GCC')
with tempfile.TemporaryDirectory(prefix='speedped-config-') as tmp:
    tmp=Path(tmp);source=tmp/'test.c';source.write_text(fixture)
    exe=tmp/('test.exe' if os.name=='nt' else 'test')
    subprocess.run([compiler,'-std=c99','-Wall','-Wextra','-Werror','-I'+str(repo/'tests/speedped75100'),'-I'+str(repo/'tests/speedped75100/stubs'),str(source),'-lm','-o',str(exe)],check=True)
    env=dict(os.environ);env['PATH']=str(Path(compiler).parent)+os.pathsep+env.get('PATH','')
    subprocess.run([str(exe)],env=env,check=True)

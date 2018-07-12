from building import *

cwd = GetCurrentDir()
    
if GetDepend(['AT_DEVICE_M26']):
    src = Glob('at_socket_m26.c')
    
if GetDepend(['AT_DEVICE_ESP8266']):
    src = Glob('at_socket_esp8266.c')
    
if GetDepend(['AT_DEVICE_NOT_SELECTED']):
   src = Glob('*.c')
    
group = DefineGroup('at_device', src, depend = ['PKG_USING_AT_DEVICE','RT_AT_USING_CLIENT'], CPPPATH = [cwd])

Return('group')

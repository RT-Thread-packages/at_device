from building import *

cwd = GetCurrentDir()
path = [cwd + '/inc']
src  = Glob('src/*.c')

# M26/MC20
if GetDepend(['AT_DEVICE_USING_M26']):
    path += [cwd + '/class/m26']
    src += Glob('class/m26/at_device_m26.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/m26/at_socket_m26.c')
    if GetDepend(['AT_DEVICE_M26_SAMPLE']):
        src += Glob('samples/at_sample_m26.c')

# EC20
if GetDepend(['AT_DEVICE_USING_EC20']):
    path += [cwd + '/class/ec20']
    src += Glob('class/ec20/at_device_ec20.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/ec20/at_socket_ec20.c')
    if GetDepend(['AT_DEVICE_EC20_SAMPLE']):
        src += Glob('samples/at_sample_ec20.c')

# ESP8266
if GetDepend(['AT_DEVICE_USING_ESP8266']):
    path += [cwd + '/class/esp8266']
    src += Glob('class/esp8266/at_device_esp8266.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/esp8266/at_socket_esp8266.c')
    if GetDepend(['AT_DEVICE_ESP8266_SAMPLE']):
        src += Glob('samples/at_sample_esp8266.c')

# MW31
if GetDepend(['AT_DEVICE_USING_MW31']):
    path += [cwd + '/class/mw31']
    src += Glob('class/mw31/at_device_mw31.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/mw31/at_socket_mw31.c')
    if GetDepend(['AT_DEVICE_MW31_SAMPLE']):
        src += Glob('samples/at_sample_mw31.c')

# RW007
if GetDepend(['AT_DEVICE_USING_RW007']):
    path += [cwd + '/class/rw007']
    src += Glob('class/rw007/at_device_rw007.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/rw007/at_socket_rw007.c')
    if GetDepend(['AT_DEVICE_RW007_SAMPLE']):
        src += Glob('samples/at_sample_rw007.c')

# SIM800C
if GetDepend(['AT_DEVICE_USING_SIM800C']):
    path += [cwd + '/class/sim800c']
    src += Glob('class/sim800c/at_device_sim800c.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/sim800c/at_socket_sim800c.c')
    if GetDepend(['AT_DEVICE_SIM800C_SAMPLE']):
        src += Glob('samples/at_sample_sim800c.c')

# SIM76XX
if GetDepend(['AT_DEVICE_USING_SIM76XX']):
    path += [cwd + '/class/sim76xx']
    src += Glob('class/sim76xx/at_device_sim76xx.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/sim76xx/at_socket_sim76xx.c')
    if GetDepend(['AT_DEVICE_SIM76XX_SAMPLE']):
        src += Glob('samples/at_sample_sim76xx.c')

group = DefineGroup('at_device', src, depend = ['PKG_USING_AT_DEVICE'], CPPPATH = path)

Return('group')

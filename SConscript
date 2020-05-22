from building import *

cwd = GetCurrentDir()
path = [cwd + '/inc']
src  = Glob('src/*.c')

# A9G
if GetDepend(['AT_DEVICE_USING_A9G']):
    path += [cwd + '/class/a9g']
    src += Glob('class/a9g/at_device_a9g.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/a9g/at_socket_a9g.c')
    if GetDepend(['AT_DEVICE_A9G_SAMPLE']):
        src += Glob('samples/at_sample_a9g.c')

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

# ESP32
if GetDepend(['AT_DEVICE_USING_ESP32']):
    path += [cwd + '/class/esp32']
    src += Glob('class/esp32/at_device_esp32.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/esp32/at_socket_esp32.c')
    if GetDepend(['AT_DEVICE_ESP32_SAMPLE']):
        src += Glob('samples/at_sample_esp32.c')

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

# W60X
if GetDepend(['AT_DEVICE_USING_W60X']):
    path += [cwd + '/class/w60x']
    src += Glob('class/w60x/at_device_w60x.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/w60x/at_socket_w60x.c')
    if GetDepend(['AT_DEVICE_W60X_SAMPLE']):
        src += Glob('samples/at_sample_w60x.c')
        
# BC26
if GetDepend(['AT_DEVICE_USING_BC26']):
    path += [cwd + '/class/bc26']
    src += Glob('class/bc26/at_device_bc26.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/bc26/at_socket_bc26.c')
    if GetDepend(['AT_DEVICE_BC26_SAMPLE']):
        src += Glob('samples/at_sample_bc26.c')
        
# air720
if GetDepend(['AT_DEVICE_USING_AIR720']):
    path += [cwd + '/class/air720']
    src += Glob('class/air720/at_device_air720.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/air720/at_socket_air720.c')
    if GetDepend(['AT_DEVICE_AIR720_SAMPLE']):
        src += Glob('samples/at_sample_air720.c')
        
# ME3616
if GetDepend(['AT_DEVICE_USING_ME3616']):
    path += [cwd + '/class/me3616']
    src += Glob('class/me3616/at_device_me3616.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/me3616/at_socket_me3616.c')
    if GetDepend(['AT_DEVICE_ME3616_SAMPLE']):
        src += Glob('samples/at_sample_me3616.c')

# M6315
if GetDepend(['AT_DEVICE_USING_M6315']):
    path += [cwd + '/class/m6315']
    src += Glob('class/m6315/at_device_m6315.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/m6315/at_socket_m6315.c')
    if GetDepend(['AT_DEVICE_M6315_SAMPLE']):
        src += Glob('samples/at_sample_m6315.c')            

# BC28
if GetDepend(['AT_DEVICE_USING_BC28']):
    path += [cwd + '/class/bc28']
    src += Glob('class/bc28/at_device_bc28.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/bc28/at_socket_bc28.c')
    if GetDepend(['AT_DEVICE_BC28_SAMPLE']):
        src += Glob('samples/at_sample_bc28.c')

# EC200X
if GetDepend(['AT_DEVICE_USING_EC200X']):
    path += [cwd + '/class/ec200x']
    src += Glob('class/ec200x/at_device_ec200x.c')
    if GetDepend(['AT_USING_SOCKET']):
        src += Glob('class/ec200x/at_socket_ec200x.c')
    if GetDepend(['AT_DEVICE_EC200X_SAMPLE']):
        src += Glob('samples/at_sample_ec200x.c')
        
group = DefineGroup('at_device', src, depend = ['PKG_USING_AT_DEVICE'], CPPPATH = path)

Return('group')

#Uncomment the line below to automatically load code after building
import bootloadercmd as b

env = Environment(PIC = '24FJ128GB206', 
                  CC = 'xc16-gcc', 
                  PROGSUFFIX = '.elf', 
                  CFLAGS = '-g -omf=elf -x c -mcpu=$PIC', 
                  LINKFLAGS = '-omf=elf -mcpu=$PIC -Wl,--script="app_p24FJ128GB206.gld"', 
                  CPPPATH = '../lib')
#Path for OSX
#env.PrependENVPath('PATH', '/Applications/microchip/xc16/v1.23/bin')
#Path for Linux
env.PrependENVPath('PATH', '/opt/microchip/xc16/v1.25/bin')
bin2hex = Builder(action = 'xc16-bin2hex $SOURCE -omf=elf',
                  suffix = 'hex', 
                  src_suffix = 'elf')
env.Append(BUILDERS = {'Hex' : bin2hex})
list = Builder(action = 'xc16-objdump -S -D $SOURCE > $TARGET', 
               suffix = 'lst', 
               src_suffix = 'elf')
env.Append(BUILDERS = {'List' : list})

env.Program('mp2', ['mp2.c',
                    '../lib/descriptors.c',
                    '../lib/timer.c',
                    '../lib/ui.c',
                    '../lib/pin.c',
                    '../lib/spi.c',
                    '../lib/oc.c',
                    '../lib/md.c',
                    '../lib/common.c'])

print('Creating builder to load hex file via bootloader...')
def load_function(target, source, env):
    bl = b.bootloadercmd()
    bl.import_hex(source[0].rstr())
    bl.write_device()
    bl.bootloader.start_user()
    bl.bootloader.close()
    return None

load = Builder(action=load_function,
               suffix = 'none',
               src_suffix = 'hex')

env.Append(BUILDERS = {'Load' : load})

env.Hex('mp2')
env.List('mp2')
# To automatically load the hex file, you need to run scons like this:
# >scons --site-dir ../site_scons
env.Load('mp2')

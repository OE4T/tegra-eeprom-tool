project('tegra-eeprom', ['c'],
        version: '1.3.0',
        meson_version: '>= 0.50.0',
        default_options: [
          'buildtype=debugoptimized',
          'warning_level=1' ],
        license : 'MIT')

# Configure the build environment.
cc = meson.get_compiler('c')

# dependency definitions
deps = []
libedit = cc.find_library('edit', required: true)
if libedit.found()
   deps += [ libedit ]
endif

# source definitions
src_tegra_eeprom_tool = ['tegra-eeprom-tool.c']
src_tegra_boardspec = ['tegra-boardspec.c']
src_tegra_eeprom = [
        'eeprom.c',
        'eeprom.h',
        'cvm.c',
        'cvm.h',
        'boardspec.c',
        'boardspec.h']

# Shared Library
lib_tegra_eeprom = shared_library('tegra-eeprom', src_tegra_eeprom,
        version: meson.project_version(),
        install : true)
install_headers('boardspec.h', 'cvm.h', 'eeprom.h', subdir : 'tegra-eeprom')

# Executables
args_tegra_eeprom_tool = [ '-D_DEFAULT_SOURCE', '-D_BSD_SOURCE' ]
executable('tegra-eeprom-tool', src_tegra_eeprom_tool,
        link_with : lib_tegra_eeprom,
        c_args : args_tegra_eeprom_tool,
        dependencies: deps,
        install : true)

executable('tegra-boardspec', src_tegra_boardspec,
        link_with : lib_tegra_eeprom,
        install : true)

# Pkg config file generation
pkg = import('pkgconfig')
pkg.generate(libraries: lib_tegra_eeprom,
             version: meson.project_version(),
             name: 'tegra-eeprom',
             description: 'Library for reading and writing Tegra EEPROMs')

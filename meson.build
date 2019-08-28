project(
	'glava',
	'c',
	version: run_command('git', 'describe', '--tags').stdout().strip()
)

cc = meson.get_compiler('c')

if get_option('buildtype').startswith('debug')
	add_project_arguments(
		'-O0',
		'-ggdb',
		'-Wall',
		'-DGLAVA_DEBUG',
		language: 'c'
	)
	if get_option('glad')
		run_command('./glad_generate.sh', 'c-debug')
	endif
else
	add_project_arguments(
		'-O2',
		'-Wstringop-overflow=0',
		language: 'c'
	)
	if get_option('glad')
		run_command('./glad_generate.sh')
	endif
endif

glava_dependencies = [
	dependency('threads'),
	cc.find_library('pulse'),
	cc.find_library('pulse-simple'),
	cc.find_library('dl'),
	cc.find_library('m'),
	cc.find_library('X11'),
	cc.find_library('Xext')
]

glava_version = meson.project_version()
if glava_version == ''
	glava_version = 'unknown'
endif

shaderdir = get_option('shaderdir')
if host_machine.system() == 'linux'
	add_project_arguments(
		'-DGLAVA_UNIX',
		language: 'c'
	)
endif
if host_machine.system() == 'darwin'
	add_project_arguments(
		'-DGLAVA_OSX',
		language: 'c'
	)
	shaderdir = '/Library/glava/'
endif

if get_option('enable_glfw')
	add_project_arguments(
		'-DGLAVA_GLFW',
		language: 'c'
	)
	glava_dependencies += cc.find_library('glfw')
endif

if not get_option('disable_glx')
	add_project_arguments(
		'-DGLAVA_GLX',
		language: 'c'
	)
	glava_dependencies += cc.find_library('Xrender')
endif

if get_option('standalone')
	add_project_arguments(
		'-DGLAVA_STANDALONE',
		language: 'c')
endif

add_project_arguments(
	'-DGLAVA_VERSION="'+glava_version+'"',
	'-DSHADER_INSTALL_PATH="'+shaderdir+'/glava"',
    '-I/usr/include/obs',
    '-fvisibility=hidden',
	language: 'c'
)

libglava = shared_library(
	'glava',
	sources: run_command('find', 'glava', '-type', 'f', '-name', '*.c', '-print').stdout().strip().split('\n'),
	dependencies: glava_dependencies,
	install: true
)

executable(
    'glava',
    sources: run_command('find', 'glava-cli', '-type', 'f', '-name', '*.c', '-print').stdout().strip().split('\n'),
    link_with: libglava,
    include_directories: 'glava',
    install: true
)

executable(
	'glava-config',
	sources: run_command('find', 'glava-config', '-type', 'f', '-name', '*.c', '-print').stdout().strip().split('\n'),
	dependencies: [
		cc.find_library('GL'),
		cc.find_library('X11')
	],
	c_args: '-Wunused-but-set-variable',
	install: true
)

if not get_option('disable_obs')
    shared_library(
        'glava-obs',
        sources: run_command('find', 'glava-obs', '-type', 'f', '-name', '*.c', '-print').stdout().strip().split('\n'),
        link_with: libglava,
        dependencies: [
	        dependency('threads'),
		    cc.find_library('GL'),
		    cc.find_library('X11'),
            cc.find_library('obs'),
	        cc.find_library('dl')
	    ],
	    install: true,
        install_dir : '/usr/lib/obs-plugins'
    )
endif

install_subdir('shaders/glava', install_dir: shaderdir)
install_headers('glava/glava.h')

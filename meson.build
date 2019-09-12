project(
  'glava',
  ['c', 'cpp'],
  version: run_command('git', 'describe', '--tags').stdout().strip(),
  default_options:['buildtype=release', 'strip=true', 'optimization=2'])

cc = meson.get_compiler('c')

if get_option('glad')
  if get_option('buildtype').startswith('debug')
	run_command('./glad_generate.sh', 'c-debug')
  else
    run_command('./glad_generate.sh')
  endif
endif

if get_option('buildtype').startswith('debug')
  add_project_arguments('-DGLAVA_DEBUG', language: 'c')
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

if cc.get_id() == 'clang'
  add_project_arguments('-fblocks', language: 'c')
  glava_dependencies += cc.find_library('BlocksRuntime')
endif

shader_dir = get_option('shader_install_dir')
glava_version = meson.project_version()
if glava_version == ''
  glava_version = 'unknown'
endif

if host_machine.system() == 'linux' or host_machine.system() == 'bsd'
  add_project_arguments('-DGLAVA_UNIX', language: ['cpp', 'c'])
endif

# Note: the OSX install directives only exist for future platform support
if host_machine.system() == 'darwin'
  add_project_arguments('-DGLAVA_OSX', language: ['cpp', 'c'])
  error('OSX targets are not supported, see issue #86.')
  # shader_dir = '/Library/glava/'
endif

if get_option('enable_glfw')
  add_project_arguments('-DGLAVA_GLFW', language: ['cpp', 'c'])
  glava_dependencies += cc.find_library('glfw')
endif

if not get_option('disable_glx')
  add_project_arguments('-DGLAVA_GLX', language: ['cpp', 'c'])
  glava_dependencies += cc.find_library('Xrender')
endif

if get_option('standalone')
  add_project_arguments('-DGLAVA_STANDALONE', language: ['cpp', 'c'])
endif

resource_dir = get_option('resource_install_dir')
if get_option('standalone')
  resource_dir = '..'
endif

add_project_arguments(
  '-DGLAVA_VERSION="' + glava_version + '"',
  '-DSHADER_INSTALL_PATH="' + shader_dir + '/glava"',
  '-DGLAVA_RESOURCE_PATH="' + resource_dir + '/resources"',
  # todo: add back
  # '-fvisibility=hidden',
  language: ['cpp', 'c'])

glfft = static_library(
  'glfft',
  sources: run_command('find', 'glfft', '-type', 'f', '-name', '*.cpp', '-print')
           .stdout().strip().split('\n'),
  c_args: ['-std=c++11'],
  dependencies: [ cc.find_library('dl') ])

libglava = shared_library(
  'glava',
  sources: run_command('find', 'glava', '-type', 'f', '-name', '*.c', '-print')
           .stdout().strip().split('\n'),
  dependencies: glava_dependencies,
  install:      true)

executable(
  'glava',
  sources:   'glava-cli/cli.c',
  link_with: libglava,
  c_args:    ['-I' + meson.source_root() + '/glava', '-std=c++11'],
  install:   true)

if not get_option('disable_config')
  
  # Generator and target for lua objects used by `glava-config`.
  # This has been written such that ninja can detect when sources
  # need to be rebuilt.

  luac_input_ext  = 'lua'
  luac_output_ext = 'lua'
  
  glava_config_lua_sources = run_command(
    'find', 'glava-config', '-type', 'f', '-name', '*.' + luac_input_ext, '-print'
  ).stdout().strip().split('\n')
  glava_config_lua_targets = []
  foreach s: run_command(
    'basename', '-s.' + luac_input_ext, glava_config_lua_sources
  ).stdout().strip().split('\n')
    glava_config_lua_targets += s + '.' + luac_output_ext
  endforeach
  
  luac_name = 'luac' + get_option('lua_version')
  luac_args = ['-o', '@OUTPUT@', '@INPUT@']
  lua_dir   = get_option('lua_implementation')
  lua_ver   = get_option('lua_version')
  lua_impl  = get_option('lua_implementation')
  lua_inc   = get_option('lua_implementation') + get_option('lua_version')
  if get_option('lua_implementation') == 'luajit'
    # LuaJIT compiler produces better bytecode; use that
    luac_name = 'luajit'
    lua_impl += '-'
    luac_args = ['-b', '@INPUT@', '@OUTPUT@']
    if get_option('buildtype').startswith('debug')
      luac_args += '-g'
    endif
    # LuaJIT uses /usr/share/lua/5.1; ignore version
    lua_dir = 'lua'
    lua_ver = '5.1'
    lua_inc = 'luajit-2.0'
  elif not get_option('buildtype').startswith('debug')
    luac_args = ['-s'] + luac_args
  endif
  
  luac_target = custom_target(
    'glava-config-luac',
    input: generator(
      find_program(luac_name),
      output:    '@BASENAME@.' + luac_output_ext,
      arguments: luac_args).process(glava_config_lua_sources),
    output:           glava_config_lua_targets,
    command:          [find_program('cp'), '-t', '@OUTDIR@', '@INPUT@'],
    build_by_default: true,
    install:          true,
    install_dir:      get_option('lua_install_dir') + '/' + lua_dir + '/'
                      + lua_ver + '/' + 'glava-config')
  
  executable(
    'glava-config',
    install: true,
    sources: 'glava-config/entry.c',
    c_args:  '-I/usr/include/' + lua_inc,
    dependencies: [
	  cc.find_library('X11'),
      cc.find_library(lua_impl + lua_ver)
    ])
  
  # Local glava-config environment symlink for standalone execution
  if get_option('standalone')
    env_target = custom_target(
      'glava-config-env', input: [], output: 'glava-env',
      depends:          luac_target,
      command:          [find_program('mkdir'), '-p', 'glava-env'],
      build_by_default: true)
    
    custom_target(
      'glava-config-ln', input: [], output: '.PHONY',
      depends:          env_target,
      command:          [find_program('ln'), '-sfT',
                         '../glava-config-luac@cus',
                         'glava-env/glava-config'],
      build_by_default: true)
  endif
endif

if not get_option('disable_obs')
  shared_library(
    'glava-obs',
	install:     true,
    install_dir: get_option('obs_plugin_install_dir'),
    sources:     'glava-obs/entry.c',
    link_with:   libglava,
    c_args:      '-I/usr/include/obs',
    dependencies: [
	  dependency('threads'),
	  cc.find_library('GL'),
	  cc.find_library('X11'),
      cc.find_library('obs'),
	  cc.find_library('dl')
	])
endif

install_subdir('shaders/glava', install_dir: shader_dir)
install_subdir('resources', install_dir: resource_dir)
install_headers('glava/glava.h')

project(
    'vkdevicechooser',
    'cpp',
    version: '1',
    license: 'MIT',
)

cpp = meson.get_compiler('cpp')

if not cpp.has_header('vulkan/vulkan.h')
    error('Vulkan development files not present (vulkan/vulkan.h)')
endif
if not cpp.has_header('vulkan/vk_layer.h')
    error('Vulkan development files not present (vulkan/vk_layer.h)')
endif
if not cpp.has_header('vulkan/vk_layer_dispatch_table.h')
    error('Vulkan development files not present (vulkan/vk_layer_dispatch_table.h)')
endif

libdir = get_option('libdir')
datadir = get_option('datadir')
vulkanlayerdir = join_paths(datadir, 'vulkan', 'implicit_layer.d')

lib_src = files(
    'layer.cpp',
)

json_src = files(
    'vkdevicechooser.json',
)

vkdevicechooser_lib = library(
    'vkdevicechooser',
    lib_src,
    install: libdir,
)

install_data(
    json_src,
    install_dir: vulkanlayerdir,
)

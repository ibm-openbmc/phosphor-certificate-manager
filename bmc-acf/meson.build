cxx = meson.get_compiler('cpp')
celogin_dep =  cxx.find_library('celogin')
if cxx.has_header('jsmn.h')
    jsmn_dep = declare_dependency()
else
    jsmn_dep = dependency('jsmn')
endif
openssl_dep = dependency('openssl')
tacf_dep = dependency('tacf', required : true)

bmc_acf_deps = [
    celogin_dep,
    jsmn_dep,
    openssl_dep,
    phosphor_dbus_interfaces_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
    sdeventplus_dep,
    tacf_dep,
]

bmc_acf_lib = static_library(
    'bmc_acf',
    [
        'acf_manager.cpp',
    ],
    include_directories: '..',
    dependencies: bmc_acf_deps,
)

bmc_acf_dep = declare_dependency(
    link_with: bmc_acf_lib,
    dependencies: bmc_acf_deps,
)

executable(
    'bmc-acf',
    'mainapp.cpp',
    include_directories: '..',
    dependencies: bmc_acf_dep,
    install: true,
)

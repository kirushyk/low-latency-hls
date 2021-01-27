project('rtsp2hls', 'cpp',
    default_options: [
        'cpp_std=c++14'
    ]
)

lhls_service_sources = [
    'input' / 'rtsp-input.cpp',
    'hls' / 'base-segment.cpp',
    'hls' / 'partial-segment.cpp',
    'hls' / 'segment.cpp',
    'hls' / 'output.cpp',
    'http' / 'file-endpoint.cpp',
    'http' / 'api.cpp',
    'main.cpp'
]

config = configuration_data()
config.set_quoted('APPLICATION_NAME', 'RTSP-to-HLS Demo')
config.set('PLAYLIST_CHUNK_SIZE', 4096)
configure_file(output: 'config.h', configuration: config)

executable('rtsp2hls',
    lhls_service_sources,
    dependencies: [
        dependency('zlib'),
        dependency('libsoup-2.4'),
        dependency('gstreamer-1.0'),
        dependency('gstreamer-app-1.0'),
        dependency('gstreamer-video-1.0')
    ]
)

subdir('ui')

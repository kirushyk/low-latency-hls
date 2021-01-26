lhls_service_sources = [
    'file-endpoint.cpp',
    'hlspartialsegment.cpp',
    'hlssegment.cpp',
    'hls.cpp',
    'main.cpp'
]

executable('lhls-service',
    lhls_service_sources,
    dependencies: [
        dependency('zlib'),
        dependency('libsoup-2.4'),
        dependency('gstreamer-1.0'),
        dependency('gstreamer-app-1.0'),
        dependency('gstreamer-video-1.0')
    ]
)

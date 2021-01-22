lhls_service_sources = [
    'file-endpoint.cpp',
    'hls.cpp',
    'main.cpp'
]

executable('lhls-service',
    lhls_service_sources,
    dependencies: [
        dependency('libsoup-2.4'),
        dependency('gstreamer-1.0'),
        dependency('gstreamer-app-1.0')
    ]
)

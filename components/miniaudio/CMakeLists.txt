idf_component_register(
    SRCS "miniaudio/extras/miniaudio_split/miniaudio.c"
    INCLUDE_DIRS "miniaudio/extras/miniaudio_split"
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    MA_ENABLE_ONLY_SPECIFIC_BACKENDS
    MA_NO_DECODING
    MA_NO_ENCODING
    MA_NO_WAV
    MA_NO_FLAC
    MA_NO_MP3
    MA_NO_DEVICE_IO
    MA_NO_RESOURCE_MANAGER
    MA_NO_GENERATION
    MA_NO_THREADING
    MA_NO_RUNTIME_LINKING
)

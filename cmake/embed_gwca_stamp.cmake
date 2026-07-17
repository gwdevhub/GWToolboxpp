if(NOT EXISTS "${GWCA_EMBED_DLL}")
    message(FATAL_ERROR "gwca.dll not found for embedding: ${GWCA_EMBED_DLL}")
endif()

file(SHA256 "${GWCA_EMBED_DLL}" _gwca_hash)
set(_content "#define GWCA_EMBED_HASH \"${_gwca_hash}\"\n")

set(_old "")
if(EXISTS "${GWCA_EMBED_STAMP}")
    file(READ "${GWCA_EMBED_STAMP}" _old)
endif()

if(NOT _old STREQUAL _content)
    file(WRITE "${GWCA_EMBED_STAMP}" "${_content}")
    message(STATUS "Embedded gwca.dll refreshed (${_gwca_hash})")
endif()

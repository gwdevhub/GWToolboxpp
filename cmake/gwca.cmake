include_guard()

set(GWCA_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/")

add_library(gwca)

file(GLOB SOURCES
	"${GWCA_FOLDER}/source/*.cpp"
	"${GWCA_FOLDER}/include/gwca/constants/*.h"
	"${GWCA_FOLDER}/include/gwca/context/*.h"
	"${GWCA_FOLDER}/include/gwca/gamecontainers/*.h"
	"${GWCA_FOLDER}/include/gwca/gameentities/*.h"
	"${GWCA_FOLDER}/include/gwca/managers/*.h"
	"${GWCA_FOLDER}/include/gwca/packets/*.h"
	"${GWCA_FOLDER}/include/gwca/utilities/*.h")
source_group(TREE "${GWCA_FOLDER}" FILES ${SOURCES})
target_sources(gwca PRIVATE ${SOURCES})
target_include_directories(gwca PUBLIC "${GWCA_FOLDER}/include/")

include(minhook)

target_link_libraries(gwca PUBLIC
	minhook)
	
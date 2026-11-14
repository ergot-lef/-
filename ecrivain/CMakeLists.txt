cmake_minimum_required(VERSION 3.10)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
endif()
project(ecrivain C)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Écrivain")
set(VITA_TITLEID  "ZZZZ00002")
set(VITA_VERSION  "01.00")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -ffast-math")

# Le livre (domaine public) est téléchargé et préparé avant la compilation : voir la CI.
set(LIVRE "${CMAKE_SOURCE_DIR}/livre.txt")
if(NOT EXISTS ${LIVRE})
  message(FATAL_ERROR "livre.txt manquant : lancer tools/prepare_livre.py (voir README)")
endif()

add_executable(ecrivain src/main_vita.c src/model.c ../common/gfx.c)
target_link_libraries(ecrivain SceCtrl_stub SceDisplay_stub ScePower_stub m)

vita_create_self(eboot.bin ecrivain UNSAFE)
vita_create_vpk(ecrivain.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
       ${LIVRE} livre.txt
)

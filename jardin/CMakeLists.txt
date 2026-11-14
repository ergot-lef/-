cmake_minimum_required(VERSION 3.10)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
endif()
project(jardin C)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Jardin")
set(VITA_TITLEID  "ZZZZ00001")
set(VITA_VERSION  "01.00")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -ffast-math")

add_executable(jardin src/main_vita.c src/garden.c src/view.c ../common/gfx.c)
target_link_libraries(jardin SceDisplay_stub SceCtrl_stub SceTouch_stub ScePower_stub m)

vita_create_self(eboot.bin jardin UNSAFE)
vita_create_vpk(jardin.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
)

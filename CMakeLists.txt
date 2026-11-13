cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Definir VITASDK")
  endif()
endif()

project(monde C)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "-")
set(VITA_TITLEID  "ZZZZ00000")
set(VITA_VERSION  "01.00")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall")

add_executable(monde src/main_vita.c src/world.c)

target_link_libraries(monde
  SceDisplay_stub
  SceCtrl_stub
  SceTouch_stub
  SceMotion_stub
  SceAudio_stub
  SceAudioIn_stub
  ScePower_stub
  m
)

vita_create_self(eboot.bin monde UNSAFE)
vita_create_vpk(monde.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
)

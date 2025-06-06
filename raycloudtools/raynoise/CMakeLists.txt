set(SOURCES
  raynoise.cpp
)

ras_add_executable(raynoise
  LIBS raylib
  SOURCES ${SOURCES}
  PROJECT_FOLDER "raycloudtools"
)

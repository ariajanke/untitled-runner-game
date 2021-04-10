QT      -= core gui
CONFIG  -= c++11

QMAKE_CXXFLAGS += -std=c++17 -pedantic -Wall -DMACRO_DEBUG
QMAKE_LFLAGS   += -std=c++17
INCLUDEPATH    += ../lib/cul/inc \
                  ../lib/tmap/inc \
                  ../lib/ecs/inc
#                 have to use absolute file paths
LIBS           += "-L$$PWD/../lib/cul" \
                  "-L$$PWD/../lib/tmap" \
                  "-L$$PWD/../lib/ecs"
LIBS           += -lsfml-graphics -lsfml-window -lsfml-system \
                  -ltmap \
                  -lz -ltinyxml2 -lcommon -lpthread

DEFINES += MACRO_COMPILER_GCC

SOURCES += \
    ../src/main.cpp \
    ../src/Defs.cpp \
    ../src/GameDriver.cpp \
    ../src/GraphicalEffects.cpp \
    ../src/get_8x8_char.cpp \
    ../src/GraphicsDrawer.cpp \
    ../src/GenBuiltinTileSet.cpp \
    ../src/TreeGraphics.cpp \
    ../src/ForestDecor.cpp \
    ../src/Flower.cpp \
    \ # maps
    ../src/maps/Maps.cpp \
    ../src/maps/MapObjectLoader.cpp \
    ../src/maps/LineMapLoader.cpp \
    ../src/maps/SurfaceRef.cpp \
    ../src/maps/MapLinks.cpp \
    ../src/maps/MapMultiplexer.cpp \
    \ # components
    ../src/components/ComponentsMisc.cpp \
    ../src/components/Platform.cpp \
    ../src/components/ComponentsComplete.cpp \
    ../src/components/DisplayFrame.cpp \
    ../src/components/PhysicsComponent.cpp \
    \ # systems
    ../src/systems/SystemsDefs.cpp \
    ../src/systems/SystemsMisc.cpp \
    ../src/systems/LineTrackerPhysics.cpp \
    ../src/systems/EnvironmentCollisionSystem.cpp \
    ../src/systems/FreeBodyPhysics.cpp \
    ../src/systems/DrawSystems.cpp

#SOURCES += \
#    ../lib/tmap/src/TileSet.cpp \
#    ../lib/tmap/src/Base64.cpp \
#    ../lib/tmap/src/TiXmlHelpers.cpp \
#    ../lib/tmap/src/TileEffect.cpp \
#    ../lib/tmap/src/ColorLayer.cpp \
#    ../lib/tmap/src/TiledMapImpl.cpp \
#    ../lib/tmap/src/TileLayer.cpp \
#    ../lib/tmap/src/TiledMap.cpp \
#    ../lib/tmap/src/ZLib.cpp

HEADERS += \
    ../src/GridRange.hpp \
    ../src/Defs.hpp \
    ../src/Systems.hpp \
    ../src/GameDriver.hpp \
    ../src/GraphicalEffects.hpp \
    ../src/get_8x8_char.hpp \
    ../src/Components.hpp \
    ../src/GraphicsDrawer.hpp \
    ../src/GenBuiltinTileSet.hpp \
    ../src/FillIterate.hpp \
    ../src/TreeGraphics.hpp \
    ../src/BresenhamView.hpp \
    ../src/ForestDecor.hpp \
    ../src/Flower.hpp \
    \ # maps
    ../src/maps/Maps.hpp \
    ../src/maps/MapObjectLoader.hpp \
    ../src/maps/LineMapLoader.hpp \
    ../src/maps/SurfaceRef.hpp \
    ../src/maps/MapLinks.hpp \
    ../src/maps/MapMultiplexer.hpp \
    \ # components
    ../src/components/ComponentsComplete.hpp \
    ../src/components/DisplayFrame.hpp \
    ../src/components/Platform.hpp \
    ../src/components/ComponentsMisc.hpp \
    ../src/components/Defs.hpp \
    ../src/components/PhysicsComponent.hpp \
    \ # systems
    ../src/systems/FreeBodyPhysics.hpp \
    ../src/systems/SystemsComplete.hpp \
    ../src/systems/EnvironmentCollisionSystem.hpp \
    ../src/systems/LineTrackerPhysics.hpp \
    ../src/systems/DrawSystems.hpp \
    ../src/systems/SystemsDefs.hpp \
    ../src/systems/SystemsMisc.hpp

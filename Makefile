# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

# FLAGS will be passed to both the C and C++ compiler
FLAGS +=
CFLAGS +=
CXXFLAGS +=

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine.
LDFLAGS +=

# Add .cpp and .c files to the build
SOURCES += $(wildcard src/*.cpp)
# SOURCES += $(wildcard src/ComputerscareSVGPanel.cpp)
# SOURCES += $(wildcard src/ComputerscareSvgPort.cpp)


# SOURCES += $(wildcard src/ComputerscarePatchSequencer.cpp)
# SOURCES += $(wildcard src/ComputerscareLaundrySoup.cpp)
# SOURCES += $(wildcard src/ComputerscareILoveCookies.cpp)
# SOURCES += $(wildcard src/ComputerscareDebug.cpp)
# SOURCES += $(wildcard src/ComputerscareOhPeas.cpp)

# SOURCES += $(wildcard src/ComputerscareKnolyPobs.cpp)
# SOURCES += $(wildcard src/ComputerscareBolyPuttons.cpp)
# SOURCES += $(wildcard src/ComputerscareRolyPouter.cpp)
# SOURCES += $(wildcard src/ComputerscareTolyPools.cpp)
# SOURCES += $(wildcard src/ComputerscareSolyPequencer.cpp)
# SOURCES += $(wildcard src/ComputerscareFolyPace.cpp)
# SOURCES += $(wildcard src/ComputerscareBlank.cpp)
# SOURCES += $(wildcard src/ComputerscareStolyFickPigure.cpp)
# SOURCES += $(wildcard src/ComputerscareGolyPenerator.cpp)

# SOURCES += $(wildcard src/Computerscare.cpp)
# SOURCES += $(wildcard src/dtpulse.cpp)
# SOURCES += $(wildcard src/golyFunctions.cpp)


# Add files to the ZIP package when running `make dist`
# The compiled plugin is automatically added.
DISTRIBUTABLES += $(wildcard LICENSE*) res presets

# Include the VCV Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk

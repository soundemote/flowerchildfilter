# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

# Collection layout: ship SuperLove; other modules stay in src/modules/ uncompiled.
FLAGS += -Isrc -Isrc/common -Isrc/modules/SuperLove
SOURCES += src/plugin.cpp
SOURCES += src/common/FmdWidgets.cpp
SOURCES += src/modules/SuperLove/SuperLove.cpp

# Dist zip: SuperLove assets only (Flower Child / Shaped Resonator stay in git).
DISTRIBUTABLES += res/common
DISTRIBUTABLES += res/SuperLove
DISTRIBUTABLES += res/panels/SuperLove.png
DISTRIBUTABLES += res/labels/SuperLove.svg
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk

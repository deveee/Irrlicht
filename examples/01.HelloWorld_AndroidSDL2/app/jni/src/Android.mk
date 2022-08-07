LOCAL_PATH := $(call my-dir)

#SDL2

include $(CLEAR_VARS)

SDL2_PATH ?= $(error SDL2_PATH variable is not set)

CURRENT_LOCAL_PATH := $(LOCAL_PATH)
include $(SDL2_PATH)/Android.mk
LOCAL_PATH := $(CURRENT_LOCAL_PATH)

#Irrlicht

include $(CLEAR_VARS)

LOCAL_MODULE := Irrlicht
LOCAL_SRC_FILES := $(LOCAL_PATH)/../../../../../lib/Android-SDL2/$(TARGET_ARCH_ABI)/libIrrlicht.a
include $(PREBUILT_STATIC_LIBRARY)

# Build main

include $(CLEAR_VARS)

LOCAL_MODULE := main
IRRLICHT_PATH := $(LOCAL_PATH)/../../../../../source/Irrlicht
LOCAL_C_INCLUDES := $(IRRLICHT_PATH)/SDL2/include \
					$(IRRLICHT_PATH) \
					$(IRRLICHT_PATH)/../../include \
					$(IRRLICHT_PATH)/bzip2 \
					$(IRRLICHT_PATH)/jpeglib \
					$(IRRLICHT_PATH)/libpng \
					$(IRRLICHT_PATH)/lzma \

LOCAL_SRC_FILES := main.cpp
LOCAL_SHARED_LIBRARIES := SDL2
LOCAL_STATIC_LIBRARIES := Irrlicht
LOCAL_LDLIBS := -lEGL -lz -lGLESv1_CM -lGLESv2 -llog

include $(BUILD_SHARED_LIBRARY)

# Copy assets

include $(CLEAR_VARS)

IRRLICHT_MEDIA_PATH := $(LOCAL_PATH)/../../../../../media
PROJECT_ASSETS_PATH := $(LOCAL_PATH)/../../src/main

$(shell mkdir -p $(PROJECT_ASSETS_PATH)/assets)
$(shell mkdir -p $(PROJECT_ASSETS_PATH)/assets/media)
$(shell mkdir -p $(PROJECT_ASSETS_PATH)/assets/media/Shaders)
$(shell cp $(IRRLICHT_MEDIA_PATH)/Shaders/*.* $(PROJECT_ASSETS_PATH)/assets/media/Shaders/)
$(shell cp $(IRRLICHT_MEDIA_PATH)/irrlichtlogo3.png $(PROJECT_ASSETS_PATH)/assets/media/)
$(shell cp $(IRRLICHT_MEDIA_PATH)/dwarf.x $(PROJECT_ASSETS_PATH)/assets/media/)
$(shell cp $(IRRLICHT_MEDIA_PATH)/dwarf.jpg $(PROJECT_ASSETS_PATH)/assets/media/)
$(shell cp $(IRRLICHT_MEDIA_PATH)/axe.jpg $(PROJECT_ASSETS_PATH)/assets/media/)
$(shell cp $(IRRLICHT_MEDIA_PATH)/fonthaettenschweiler.bmp $(PROJECT_ASSETS_PATH)/assets/media/)
$(shell cp $(IRRLICHT_MEDIA_PATH)/bigfont.png $(PROJECT_ASSETS_PATH)/assets/media/)

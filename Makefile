# source files
SRCS := \
    Main.cpp \
	VulkanRenderer.cpp \
	Mesh.cpp \
	MeshModel.cpp \
	stb_image.h


ifeq ($(OS),Windows_NT)
	include win.mak
else
	include linux.mak
endif
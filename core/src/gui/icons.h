#pragma once
#include <imgui/imgui.h>
#include <string>

#include <utils/opengl_include_code.h>

namespace icons {
    extern ImTextureID CENTER_TUNING;
    extern ImTextureID LEFTMUTED;
    extern ImTextureID LEFTUNMUTED;
    extern ImTextureID LOCK;
    extern ImTextureID LOGO;
    extern ImTextureID MENU;
    extern ImTextureID MUTED;
    extern ImTextureID NORMAL_TUNING;
    extern ImTextureID PLAY;
    extern ImTextureID STOP;
    extern ImTextureID UNLOCK;
    extern ImTextureID UNMUTED;

    GLuint loadTexture(std::string path);
    bool load(std::string resDir);
}
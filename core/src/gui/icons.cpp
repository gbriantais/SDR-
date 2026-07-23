#include <gui/icons.h>
#include <stdint.h>
#include <config.h>

#define STB_IMAGE_IMPLEMENTATION
#include <imgui/stb_image.h>
#include <filesystem>
#include <utils/flog.h>

namespace icons {
    ImTextureID CENTER_TUNING;
    ImTextureID LEFTMUTED;
    ImTextureID LEFTUNMUTED;
    ImTextureID LOCK;
    ImTextureID LOGO;
    ImTextureID MENU;
    ImTextureID MUTED;
    ImTextureID NORMAL_TUNING;
    ImTextureID PLAY;
    ImTextureID STOP;
    ImTextureID UNLOCK;
    ImTextureID UNMUTED;

    GLuint loadTexture(std::string path) {
        int w, h, n;
        stbi_uc* data = stbi_load(path.c_str(), &w, &h, &n, 0);
        GLuint texId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, (uint8_t*)data);
        stbi_image_free(data);
        return texId;
    }

    bool load(std::string resDir) {
        if (!std::filesystem::is_directory(resDir)) {
            flog::error("Invalid resource directory: {0}", resDir);
            return false;
        }

        CENTER_TUNING = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/center_tuning.png");
        LEFTMUTED = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/leftmuted.png");
        LEFTUNMUTED = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/leftunmuted.png");
        LOCK = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/Lock.png");
        LOGO = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/sdrpp.png");
        MENU = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/menu.png");
        MUTED = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/muted.png");
        NORMAL_TUNING = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/normal_tuning.png");
        PLAY = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/play.png");
        STOP = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/stop.png");
        UNLOCK = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/UnLock.png");
        UNMUTED = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/unmuted.png");

        return true;
    }
}
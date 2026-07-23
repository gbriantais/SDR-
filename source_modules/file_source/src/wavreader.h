#pragma once
#include <stdint.h>
#include <string.h>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

#define WAV_SIGNATURE       "RIFF"
#define WAV_TYPE            "WAVE"
#define WAV_FORMAT_MARK     "fmt "
#define WAV_DATA_MARK       "data"
#define WAV_SAMPLE_TYPE_PCM 1

#ifdef _WIN32
// Convertit un chemin UTF-8 en UTF-16 pour les API Windows.
// Nécessaire car std::ifstream(const char*) utilise l'encodage ANSI
// sous MSVC, ce qui casse les chemins contenant des caractères accentués.
static inline std::wstring _wavreader_utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) { return {}; }
    int len = MultiByteToWideChar(CP_UTF8, 0,
                                   utf8.c_str(), (int)utf8.size(),
                                   nullptr, 0);
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0,
                         utf8.c_str(), (int)utf8.size(),
                         &wide[0], len);
    return wide;
}
#endif

class WavReader {
public:
    WavReader(std::string path) {
#ifdef _WIN32
        // Sur Windows, std::ifstream(const char*) interprète le chemin
        // en ANSI (CP-1252), ce qui échoue dès qu'un répertoire ou nom
        // de fichier contient un caractère accentué (é, à, ê, ü...).
        // La surcharge std::ifstream(const wchar_t*) de MSVC utilise
        // l'API Unicode (CreateFileW) et supporte pleinement l'UTF-8
        // converti en UTF-16.
        file = std::ifstream(_wavreader_utf8_to_wide(path), std::ios::binary);
#else
        // Linux / macOS : le système de fichiers est nativement UTF-8,
        // pas de conversion nécessaire.
        file = std::ifstream(path.c_str(), std::ios::binary);
#endif
        file.read((char*)&hdr, sizeof(WavHeader_t));
        valid = false;
        if (memcmp(hdr.signature, "RIFF", 4) != 0) { return; }
        if (memcmp(hdr.fileType, "WAVE", 4) != 0) { return; }
        valid = true;
    }

    uint16_t getBitDepth() {
        return hdr.bitDepth;
    }

    uint16_t getChannelCount() {
        return hdr.channelCount;
    }

    uint32_t getSampleRate() {
        return hdr.sampleRate;
    }

    bool isValid() {
        return valid;
    }

    void readSamples(void* data, size_t size) {
        char* _data = (char*)data;
        file.read(_data, size);
        int read = file.gcount();
        if (read < size) {
            file.clear();
            file.seekg(sizeof(WavHeader_t));
            file.read(&_data[read], size - read);
        }
        bytesRead += size;
    }

    void rewind() {
        file.seekg(sizeof(WavHeader_t));
    }

    void close() {
        file.close();
    }

private:
    struct WavHeader_t {
        char signature[4];           // "RIFF"
        uint32_t fileSize;           // data bytes + sizeof(WavHeader_t) - 8
        char fileType[4];            // "WAVE"
        char formatMarker[4];        // "fmt "
        uint32_t formatHeaderLength; // Always 16
        uint16_t sampleType;         // PCM (1)
        uint16_t channelCount;
        uint32_t sampleRate;
        uint32_t bytesPerSecond;
        uint16_t bytesPerSample;
        uint16_t bitDepth;
        char dataMarker[4];          // "data"
        uint32_t dataSize;
    };

    bool valid = false;
    std::ifstream file;
    size_t bytesRead = 0;
    WavHeader_t hdr;
};

#include <utils/networking.h>
#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <signal_path/sink.h>
#include <dsp/buffer/packer.h>
#include <dsp/convert/stereo_to_mono.h>
#include <dsp/sink/handler_sink.h>
#include <utils/flog.h>
#include <config.h>
#include <gui/style.h>
#include <core.h>
#include <atomic>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "network_sink",
    /* Description:     */ "Network sink module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

enum {
    SINK_MODE_TCP,
    SINK_MODE_UDP,
    SINK_MODE_UDP_MULTICAST
};

const char* sinkModesTxt = "TCP\0UDP\0UDP Multicast\0";

#define MAX_PACKET_SAMPLES 8192

class NetworkSink : SinkManager::Sink {
public:
    NetworkSink(SinkManager::Stream* stream, std::string streamName) {
        _stream = stream;
        _streamName = streamName;

        // Load config
        config.acquire();
        if (!config.conf.contains(_streamName)) {
            config.conf[_streamName]["hostname"] = "localhost";
            config.conf[_streamName]["port"] = 7355;
            config.conf[_streamName]["protocol"] = SINK_MODE_UDP; // UDP
            config.conf[_streamName]["sampleRate"] = 48000.0;
            config.conf[_streamName]["stereo"] = false;
            config.conf[_streamName]["listening"] = false;
        }
        std::string host = config.conf[_streamName]["hostname"];
        strcpy(hostname, host.c_str());
        port = config.conf[_streamName]["port"];
        modeId = config.conf[_streamName]["protocol"];
        if (modeId < 0 || modeId > SINK_MODE_UDP_MULTICAST) { modeId = SINK_MODE_UDP; }
        if (config.conf[_streamName].contains("multicastInterface")) {
            std::string iface = config.conf[_streamName]["multicastInterface"];
            strncpy(mcastIface, iface.c_str(), sizeof(mcastIface) - 1);
            mcastIface[sizeof(mcastIface) - 1] = 0;
        }
        sampleRate = config.conf[_streamName]["sampleRate"];
        stereo = config.conf[_streamName]["stereo"];
        bool startNow = config.conf[_streamName]["listening"];
        config.release(true);

        netBuf = new int16_t[STREAM_BUFFER_SIZE];

        packer.init(_stream->sinkOut, 512);
        s2m.init(&packer.out);
        monoSink.init(&s2m.out, monoHandler, this);
        stereoSink.init(&packer.out, stereoHandler, this);


        // Create a list of sample rates
        for (int sr = 12000; sr < 200000; sr += 12000) {
            sampleRates.push_back(sr);
        }
        for (int sr = 11025; sr < 192000; sr += 11025) {
            sampleRates.push_back(sr);
        }

        // Sort sample rate list
        std::sort(sampleRates.begin(), sampleRates.end(), [](double a, double b) { return (a < b); });

        // Débits élevés ajoutés en fin de liste (après le tri) pour le transport de flux large bande
        sampleRates.push_back(1000000);
        sampleRates.push_back(2000000);

        // Generate text list for UI
        char buffer[128];
        int id = 0;
        int _48kId;
        bool found = false;
        for (auto sr : sampleRates) {
            sprintf(buffer, "%d", (int)sr);
            sampleRatesTxt += buffer;
            sampleRatesTxt += '\0';
            if (sr == sampleRate) {
                srId = id;
                found = true;
            }
            if (sr == 48000.0) { _48kId = id; }
            id++;
        }
        if (!found) {
            srId = _48kId;
            sampleRate = 48000.0;
        }
        _stream->setSampleRate(sampleRate);

        // Start if needed
        if (startNow) { startServer(); }
    }

    ~NetworkSink() {
        stopServer();
        delete[] netBuf;
    }

    void start() {
        if (running) {
            return;
        }
        doStart();
        running = true;
    }

    void stop() {
        if (!running) {
            return;
        }
        doStop();
        running = false;
    }

    void menuHandler() {
        float menuWidth = ImGui::GetContentRegionAvail().x;

        bool listening = (listener && listener->isListening()) || (conn && conn->isOpen());

        if (listening) { style::beginDisabled(); }
        if (ImGui::InputText(CONCAT("##_network_sink_host_", _streamName), hostname, 1023)) {
            config.acquire();
            config.conf[_streamName]["hostname"] = hostname;
            config.release(true);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt(CONCAT("##_network_sink_port_", _streamName), &port, 0, 0)) {
            config.acquire();
            config.conf[_streamName]["port"] = port;
            config.release(true);
        }

        ImGui::LeftLabel("Protocol");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##_network_sink_mode_", _streamName), &modeId, sinkModesTxt)) {
            config.acquire();
            config.conf[_streamName]["protocol"] = modeId;
            config.release(true);
        }

        if (modeId == SINK_MODE_UDP_MULTICAST) {
            ImGui::LeftLabel("Interface");
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::InputText(CONCAT("##_network_sink_mcast_iface_", _streamName), mcastIface, sizeof(mcastIface) - 1)) {
                config.acquire();
                config.conf[_streamName]["multicastInterface"] = mcastIface;
                config.release(true);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Adresse IP de la carte reseau utilisee pour emettre\n(0.0.0.0 = choix automatique par le systeme)");
            }
        }

        if (listening) { style::endDisabled(); }

        ImGui::LeftLabel("Samplerate");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##_network_sink_sr_", _streamName), &srId, sampleRatesTxt.c_str())) {
            sampleRate = sampleRates[srId];
            _stream->setSampleRate(sampleRate);
            // Un datagramme UDP ne peut pas dépasser 65507 octets : on plafonne la taille des blocs
            // (8192 échantillons = 16 Ko en mono, 32 Ko en stéréo) pour les débits élevés (1 et 2 MS/s)
            packer.setSampleCount(std::min<int>(sampleRate / 60, MAX_PACKET_SAMPLES));
            config.acquire();
            config.conf[_streamName]["sampleRate"] = sampleRate;
            config.release(true);
        }

        if (ImGui::Checkbox(CONCAT("Stereo##_network_sink_stereo_", _streamName), &stereo)) {
            stop();
            start();
            config.acquire();
            config.conf[_streamName]["stereo"] = stereo;
            config.release(true);
        }

        if (listening && ImGui::Button(CONCAT("Stop##_network_sink_stop_", _streamName), ImVec2(menuWidth, 0))) {
            stopServer();
            config.acquire();
            config.conf[_streamName]["listening"] = false;
            config.release(true);
        }
        else if (!listening && ImGui::Button(CONCAT("Start##_network_sink_stop_", _streamName), ImVec2(menuWidth, 0))) {
            startServer();
            config.acquire();
            config.conf[_streamName]["listening"] = true;
            config.release(true);
        }

        ImGui::TextUnformatted("Status:");
        ImGui::SameLine();
        if (conn && conn->isOpen()) {
            ImGui::TextColored(ImVec4(0.0, 1.0, 0.0, 1.0), (modeId == SINK_MODE_TCP) ? "Connected" : "Sending");
        }
        else if (listening) {
            ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), "Listening");
        }
        else {
            ImGui::TextUnformatted("Idle");
        }

        // Dernière erreur (ouverture du socket ou envoi) : sans ça, le statut reste sur Idle sans explication
        if (!listening) {
            std::string err = lastError;
            if (err.empty() && sendFailed) {
                err = "Echec d'envoi : verifiez l'adresse/l'interface reseau (voir la console)";
            }
            if (!err.empty()) {
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Erreur : %s", err.c_str());
                ImGui::PopTextWrapPos();
            }
        }
    }

private:
    void doStart() {
        packer.start();
        if (stereo) {
            stereoSink.start();
        }
        else {
            flog::warn("Starting");
            s2m.start();
            monoSink.start();
        }
    }

    void doStop() {
        packer.stop();
        s2m.stop();
        monoSink.stop();
        stereoSink.stop();
    }

    void startServer() {
        lastError.clear();
        sendFailed = false;
        try {
            if (modeId == SINK_MODE_TCP) {
                listener = net::listen(hostname, port);
                if (listener) {
                    listener->acceptAsync(clientHandler, this);
                }
            }
            else if (modeId == SINK_MODE_UDP_MULTICAST) {
                flog::info("Network sink: multicast group {0}:{1} via interface {2}", hostname, port, mcastIface);
                conn = net::openUDPMulticast(hostname, port, mcastIface);
            }
            else {
                conn = net::openUDP("0.0.0.0", port, hostname, port, false);
            }
        }
        catch (const std::exception& e) {
            flog::error("Failed to open socket: {}", e.what());
            lastError = e.what();
        }
    }

    void stopServer() {
        if (conn) { conn->close(); }
        if (listener) { listener->close(); }
    }

    static void monoHandler(float* samples, int count, void* ctx) {
        NetworkSink* _this = (NetworkSink*)ctx;
        std::lock_guard lck(_this->connMtx);
        if (!_this->conn || !_this->conn->isOpen()) { return; }

        volk_32f_s32f_convert_16i(_this->netBuf, (float*)samples, 32768.0f, count);

        if (!_this->conn->write(count * sizeof(int16_t), (uint8_t*)_this->netBuf)) {
            _this->sendFailed = true;
            flog::error("Network sink: send failed ({} bytes), connection closed", count * (int)sizeof(int16_t));
        }
    }

    static void stereoHandler(dsp::stereo_t* samples, int count, void* ctx) {
        NetworkSink* _this = (NetworkSink*)ctx;
        std::lock_guard lck(_this->connMtx);
        if (!_this->conn || !_this->conn->isOpen()) { return; }

        volk_32f_s32f_convert_16i(_this->netBuf, (float*)samples, 32768.0f, count * 2);

        if (!_this->conn->write(count * 2 * sizeof(int16_t), (uint8_t*)_this->netBuf)) {
            _this->sendFailed = true;
            flog::error("Network sink: send failed ({} bytes), connection closed", count * 2 * (int)sizeof(int16_t));
        }
    }

    static void clientHandler(net::Conn client, void* ctx) {
        NetworkSink* _this = (NetworkSink*)ctx;

        {
            std::lock_guard lck(_this->connMtx);
            _this->conn = std::move(client);
        }

        if (_this->conn) {
            _this->conn->waitForEnd();
            _this->conn->close();
        }
        else {
        }

        _this->listener->acceptAsync(clientHandler, _this);
    }

    SinkManager::Stream* _stream;
    dsp::buffer::Packer<dsp::stereo_t> packer;
    dsp::convert::StereoToMono s2m;
    dsp::sink::Handler<float> monoSink;
    dsp::sink::Handler<dsp::stereo_t> stereoSink;

    std::string _streamName;

    int srId = 0;
    bool running = false;

    char hostname[1024];
    char mcastIface[64] = "0.0.0.0";
    std::string lastError;
    std::atomic<bool> sendFailed{ false };
    int port = 4242;

    int modeId = 1;

    std::vector<unsigned int> sampleRates;
    std::string sampleRatesTxt;
    unsigned int sampleRate = 48000;
    bool stereo = false;

    int16_t* netBuf;

    net::Listener listener;
    net::Conn conn;
    std::mutex connMtx;
};

class NetworkSinkModule : public ModuleManager::Instance {
public:
    NetworkSinkModule(std::string name) {
        this->name = name;
        provider.create = create_sink;
        provider.ctx = this;

        sigpath::sinkManager.registerSinkProvider("Network", provider);
    }

    ~NetworkSinkModule() {
        // Unregister sink, this will automatically stop and delete all instances of the audio sink
        sigpath::sinkManager.unregisterSinkProvider("Network");
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static SinkManager::Sink* create_sink(SinkManager::Stream* stream, std::string streamName, void* ctx) {
        return (SinkManager::Sink*)(new NetworkSink(stream, streamName));
    }

    std::string name;
    bool enabled = true;
    SinkManager::SinkProvider provider;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(core::args["root"].s() + "/network_sink_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    NetworkSinkModule* instance = new NetworkSinkModule(name);
    return instance;
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (NetworkSinkModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
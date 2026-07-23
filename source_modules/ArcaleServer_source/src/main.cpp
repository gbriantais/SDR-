#include <ArcaleServer.h>
#include <config.h>
#include <core.h>
#include <gui/gui.h>
#include <gui/smgui.h>
#include <gui/style.h>
#include <gui/widgets/stepped_slider.h>
#include <imgui.h>
#include <module.h>
#include <signal_path/signal_path.h>
#include <utils/flog.h>
#include <utils/optionlist.h>


#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "ArcaleServer_source",
    /* Description:     */ "ArcaleServer source module for SDR++",
    /* Author:          */ "sdaviet",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

const double BANDWITH[] = {
    100000,
    250000,
    500000,
    1000000,
    2000000,
    5000000,
    10000000
};

const char* BANDWITHTXT[] = {
    "100KHz",
    "200KHz",
    "500KHz",
    "1MHz",
    "2MHz",
    "5MHz",
    "10MHz"
};



class ARCALESERVERSourceModule : public ModuleManager::Instance {
public:
    ARCALESERVERSourceModule(std::string name) {
        this->name = name;

        int srCount = sizeof(BANDWITHTXT) / sizeof(char*);
        for (int i = 0; i < srCount; i++) {
            srTxt += BANDWITHTXT[i];
            srTxt += '\0';
        }
        srId = 2;

        config.acquire();
        std::string hostStr = config.conf["host"];
        port = config.conf["port"];
        bandwith = config.conf["bandwith"];
        channel = config.conf["channel"];
        hostStr = hostStr.substr(0, 1023);
        strcpy(ip, hostStr.c_str());
        config.release();
        
        srId = std::distance(BANDWITH, std::find(std::begin(BANDWITH), std::end(BANDWITH), bandwith));

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("ArcaleServer", &handler);
    }

    ~ARCALESERVERSourceModule() {
        stop(this); 
        sigpath::sourceManager.unregisterSource("ArcaleServer");
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
    static void menuSelected(void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        
        flog::info("ArcaleServerSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        flog::info("ArcaleServerSourceModule '{0}': Menu Deselect!", _this->name);

    }
    
    static void start(void* ctx) {
        double fmin, fmax;

        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        if (_this->running) {
            return;
        }
        _this->client = new ArcaleRfTCPServer(_this->channel + 10040, _this->port, _this->timeOut);

        if (_this->client->Connect(_this->ip, _this->freq)) {
            flog::error("Could not connect to {0}:{1}", _this->ip, _this->port);
            return;
        }

        // Récupérer les limites de fréquence du serveur
        _this->client->getFrequencyLimits(&fmin, &fmax);
        flog::info("ArcaleServerSourceModule freq: '{0}', fmin: {1}, fmax: {2}", _this->freq, fmin, fmax);

        // Appliquer les limites dans MainWindow
        gui::mainWindow.setFrequencyLimits(true, fmin, fmax);

        flog::info("ArcaleServerSourceModule freq: '{0}', bandwith: {1}, sampleRate: {2}", _this->freq, _this->bandwith, _this->sampleRate);

        _this->IQPairs = _this->client->ConnectToChannel(_this->freq, _this->bandwith, &_this->sampleRate);
        core::setInputSampleRate(_this->sampleRate);
        flog::info("Setting sample rate to {0}", _this->sampleRate);
        flog::info("ArcaleServerSourceModule IQPairs : '{0}'", _this->IQPairs);

        if (_this->IQPairs > 0) {
            _this->running = true;
            _this->threadstate = true;
            _this->workerThread = std::thread(worker, _this);
            flog::info("ArcaleServerSourceModule '{0}': Start!", _this->name);
        }
        else {
            // Si la connexion échoue, nettoyer et désactiver les limites
            delete _this->client;
            _this->client = nullptr;
            gui::mainWindow.setFrequencyLimits(false, 0.0, 0.0);
            flog::error("Failed to connect to channel, IQPairs = 0");
        }
    }
    
    static void stop(void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        if (!_this->running) {
            return;
        }
        _this->running = false;
        _this->threadstate = false;
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();
        _this->client->disconnect();
        delete _this->client;
        _this->client = nullptr;

        // Désactiver les limites de fréquence
        gui::mainWindow.setFrequencyLimits(false, 0.0, 0.0);

        flog::info("ArcaleServerSourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        _this->freq = freq;
        flog::info("ArcaleServer'{0}' request Tune: {1}!", _this->name, _this->freq);
        if (_this->running) {
            _this->client->sendChannelInfo(_this->freq, _this->bandwith, &_this->sampleRate, false);
            core::setInputSampleRate(_this->sampleRate);
        }
        flog::info("ArcaleServer'{0}' Tune: {1}!", _this->name, _this->freq);
    }
    
    static void menuHandler(void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;

        if (_this->running) {
            SmGui::BeginDisabled();
        }

        // IP input field
        if (SmGui::InputText(CONCAT("##_ip_select_", _this->name), _this->ip, 1024)) {
            config.acquire();
            config.conf["host"] = _this->ip;
            config.release(true);
        }

        // Port input field
        SmGui::SameLine();
        SmGui::FillWidth();
        if (SmGui::InputInt(CONCAT("##_port_select_", _this->name), &_this->port, 0)) {
            config.acquire();
            config.conf["port"] = _this->port;
            config.release(true);
        }
        
        // Channel selection
        SmGui::LeftLabel("Channel");
        SmGui::FillWidth();
        if (SmGui::InputInt(CONCAT("Server Module (1-8)##ChannelNumber", _this->name), &_this->channel, 1, 1)) {
            _this->channel = std::clamp(_this->channel, 1, 8);
            config.acquire();
            config.conf["channel"] = _this->channel;
            config.release(true);
        }

        // Bandwidth / Sample rate selection
        SmGui::LeftLabel("Bandwidth");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("Bandwidth##Bandwidth_", _this->name), &_this->srId, _this->srTxt.c_str())) {
            _this->bandwith = BANDWITH[_this->srId];
            config.acquire();
            config.conf["bandwith"] = _this->bandwith;
            config.release(true);
        }

        if (_this->running) {
            SmGui::EndDisabled();
        }
    }

    static void worker(void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        int i;
        int bytesRead;
        int dataRead;
        int bytesleft;
        char* ptr_i = NULL;
        char* ptr_q = NULL;


        int blockSize = _this->IQPairs*4;
        char* inBuf = new char[blockSize];

        flog::info("ArcaleServer Worker started");
        while (_this->threadstate) {
            // Read samples here
            dataRead = 0;
            bytesleft = blockSize;
            while (bytesleft > 0) {
                bytesRead = _this->client->receiveSDRPP(&inBuf[dataRead], bytesleft);
                if (bytesRead <= 0) break;
                bytesleft -= bytesRead;
                dataRead += bytesRead;
            }
            if (dataRead!=blockSize) flog::info("ArcaleServerSourceModule data_read {0}", dataRead);

            ptr_q = &inBuf[0];
            ptr_i = &inBuf[2];

            for (i = 0; i < (dataRead/4); i++) {
                _this->stream.writeBuf[i].im = ((float)((ptr_i[0] << 8) | (ptr_i[1] & 0xFF))) / 32768.0f;
                _this->stream.writeBuf[i].re = ((float)((ptr_q[0] << 8) | (ptr_q[1] & 0xFF))) / 32768.0f;

                ptr_i = ptr_i + 4;
                ptr_q = ptr_q + 4;
            }
            if (!_this->stream.swap(dataRead / 4)) { break; };
            //flog::info("ArcaleServerSourceModule data_read : '{0}', '{1}'", dataRead, i);
            
        }
        delete[] inBuf;
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    ArcaleRfTCPServer* client = nullptr;
    std::thread workerThread;
    bool running = false;
    bool threadstate = false;
    double freq;
    char ip[1024] = "localhost";
    int port = 1234;
    int channel = 1;
    double bandwith = 100000;
    int timeOut = 1000;
    int srId = 0;
    double sampleRate;
    std::string srTxt = "";
    uint32_t IQPairs;
    float downFactor = 32768.0f;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(core::args["root"].s() + "/ArcaleServer_config.json");
    def["host"] = "localhost";
    def["port"] = 1234;
    def["bandwith"] = 1000000;
    def["channel"] = 1;
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new ARCALESERVERSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (ARCALESERVERSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}

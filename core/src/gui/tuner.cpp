#include <signal_path/signal_path.h>
#include <gui/gui.h>
#include <gui/tuner.h>
#include <string>

namespace tuner {

    void centerTuning(std::string vfoName, double freq) {
        if (vfoName != "") {
            if (gui::waterfall.vfos.find(vfoName) == gui::waterfall.vfos.end()) { return; }
            sigpath::vfoManager.setOffset(vfoName, 0);
        }
        double BW = gui::waterfall.getBandwidth();
        double viewBW = gui::waterfall.getViewBandwidth();
        gui::waterfall.setViewOffset((BW / 2.0) - (viewBW / 2.0));
        gui::waterfall.setCenterFrequency(freq);
        gui::waterfall.setViewOffset(0);
        gui::freqSelect.setFrequency(freq);
        sigpath::sourceManager.tune(freq);
    }

    void normalTuning(std::string vfoName, double freq) {
        if (vfoName == "") {
            centerTuning(vfoName, freq);
            return;
        }
        if (gui::waterfall.vfos.find(vfoName) == gui::waterfall.vfos.end()) { return; }

        double viewBW = gui::waterfall.getViewBandwidth();
        double BW = gui::waterfall.getBandwidth();

        ImGui::WaterfallVFO* vfo = gui::waterfall.vfos[vfoName];

        double currentOff = vfo->centerOffset;
        double currentTune = gui::waterfall.getCenterFrequency() + vfo->generalOffset;
        double delta = freq - currentTune;

        double newVFO = currentOff + delta;
        double vfoBW = vfo->bandwidth;
        double vfoBottom = newVFO - (vfoBW / 2.0);
        double vfoTop = newVFO + (vfoBW / 2.0);

        double view = gui::waterfall.getViewOffset();
        double viewBottom = view - (viewBW / 2.0);
        double viewTop = view + (viewBW / 2.0);

        double bottom = -(BW / 2.0);
        double top = (BW / 2.0);

        // VFO still fints in the view
        if (vfoBottom > viewBottom && vfoTop < viewTop) {
            sigpath::vfoManager.setCenterOffset(vfoName, newVFO);
            return;
        }

        // VFO too low for current SDR tuning
        if (vfoBottom < bottom) {
            double newVFOOffset = (BW / 2.0) - (vfoBW / 2.0) - (viewBW / 10.0);
            double rawCenterFreq = freq - newVFOOffset;
            double newCenterFreq = gui::mainWindow.clampFrequency(rawCenterFreq);
            if (newCenterFreq != rawCenterFreq) {
                // La source ne peut pas retuner jusque là (limite de bande IQ
                // atteinte) : on ne déplace ni la vue ni le bandeau, on plaque
                // simplement le VFO contre le bord de la bande reçue.
                double clampedVFO = bottom + (vfoBW / 2.0);
                sigpath::vfoManager.setCenterOffset(vfoName, clampedVFO);
                return;
            }
            gui::waterfall.setViewOffset((BW / 2.0) - (viewBW / 2.0));
            sigpath::vfoManager.setOffset(vfoName, newVFOOffset);
            gui::waterfall.setCenterFrequency(newCenterFreq);
            sigpath::sourceManager.tune(newCenterFreq);
            return;
        }

        // VFO too high for current SDR tuning
        if (vfoTop > top) {
            double newVFOOffset = (vfoBW / 2.0) - (BW / 2.0) + (viewBW / 10.0);
            double rawCenterFreq = freq - newVFOOffset;
            double newCenterFreq = gui::mainWindow.clampFrequency(rawCenterFreq);
            if (newCenterFreq != rawCenterFreq) {
                // Idem, côté haut de bande.
                double clampedVFO = top - (vfoBW / 2.0);
                sigpath::vfoManager.setCenterOffset(vfoName, clampedVFO);
                return;
            }
            gui::waterfall.setViewOffset((viewBW / 2.0) - (BW / 2.0));
            sigpath::vfoManager.setOffset(vfoName, newVFOOffset);
            gui::waterfall.setCenterFrequency(newCenterFreq);
            sigpath::sourceManager.tune(newCenterFreq);
            return;
        }

        // VFO is still without the SDR's bandwidth
        if (delta < 0) {
            double newViewOff = vfoTop - (viewBW / 2.0) + (viewBW / 10.0);
            double newViewBottom = newViewOff - (viewBW / 2.0);

            if (newViewBottom > bottom) {
                gui::waterfall.setViewOffset(newViewOff);
                sigpath::vfoManager.setCenterOffset(vfoName, newVFO);
                return;
            }

            double newVFOOffset = (BW / 2.0) - (vfoBW / 2.0) - (viewBW / 10.0);
            double rawCenterFreq = freq - newVFOOffset;
            double newCenterFreq = gui::mainWindow.clampFrequency(rawCenterFreq);
            if (newCenterFreq != rawCenterFreq) {
                double clampedVFO = bottom + (vfoBW / 2.0);
                sigpath::vfoManager.setCenterOffset(vfoName, clampedVFO);
                return;
            }
            gui::waterfall.setViewOffset((BW / 2.0) - (viewBW / 2.0));
            sigpath::vfoManager.setCenterOffset(vfoName, newVFOOffset);
            gui::waterfall.setCenterFrequency(newCenterFreq);
            sigpath::sourceManager.tune(newCenterFreq);
        }
        else {
            double newViewOff = vfoBottom + (viewBW / 2.0) - (viewBW / 10.0);
            double newViewTop = newViewOff + (viewBW / 2.0);

            if (newViewTop < top) {
                gui::waterfall.setViewOffset(newViewOff);
                sigpath::vfoManager.setCenterOffset(vfoName, newVFO);
                return;
            }

            double newVFOOffset = (vfoBW / 2.0) - (BW / 2.0) + (viewBW / 10.0);
            double rawCenterFreq = freq - newVFOOffset;
            double newCenterFreq = gui::mainWindow.clampFrequency(rawCenterFreq);
            if (newCenterFreq != rawCenterFreq) {
                double clampedVFO = top - (vfoBW / 2.0);
                sigpath::vfoManager.setCenterOffset(vfoName, clampedVFO);
                return;
            }
            gui::waterfall.setViewOffset((viewBW / 2.0) - (BW / 2.0));
            sigpath::vfoManager.setCenterOffset(vfoName, newVFOOffset);
            gui::waterfall.setCenterFrequency(newCenterFreq);
            sigpath::sourceManager.tune(newCenterFreq);
        }
    }

    void iqTuning(double freq) {
        gui::waterfall.setCenterFrequency(freq);
        gui::waterfall.centerFreqMoved = true;
        sigpath::sourceManager.tune(freq);
    }

    void tune(int mode, std::string vfoName, double freq) {
        switch (mode) {
        case TUNER_MODE_CENTER:
            centerTuning(vfoName, freq);
            break;
        case TUNER_MODE_NORMAL:
            normalTuning(vfoName, freq);
            break;
        case TUNER_MODE_LOWER_HALF:
            normalTuning(vfoName, freq);
            break;
        case TUNER_MODE_UPPER_HALF:
            normalTuning(vfoName, freq);
            break;
        case TUNER_MODE_IQ_ONLY:
            iqTuning(freq);
            break;
        }
    }
}
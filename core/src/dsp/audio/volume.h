#pragma once
#include "../processor.h"
#include <cmath>
#include <mutex>
#include <cassert>

namespace dsp::audio {
    class Volume : public Processor<stereo_t, stereo_t> {
        using base_type = Processor<stereo_t, stereo_t>;
    public:
        Volume() : _leftVolume(1.0f), _rightVolume(1.0f), _leftMuted(false), _rightMuted(false) {}

        Volume(stream<stereo_t>* in, double leftVolume, double rightVolume, bool leftMuted, bool rightMuted) {
            init(in, leftVolume, rightVolume, leftMuted, rightMuted);
        }

        void init(stream<stereo_t>* in, double leftVolume, double rightVolume, bool leftMuted, bool rightMuted) {
            _leftVolume = powf(static_cast<float>(leftVolume), 2.0f);
            _rightVolume = powf(static_cast<float>(rightVolume), 2.0f);
            _leftMuted = leftMuted;
            _rightMuted = rightMuted;
            base_type::init(in);
        }

        void setLeftVolume(double volume) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _leftVolume = powf(static_cast<float>(volume), 2.0f);
        }

        void setRightVolume(double volume) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _rightVolume = powf(static_cast<float>(volume), 2.0f);
        }

        void setLeftMuted(bool muted) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _leftMuted = muted;
        }

        void setRightMuted(bool muted) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _rightMuted = muted;
        }

        bool getLeftMuted() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            return _leftMuted;
        }

        bool getRightMuted() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            return _rightMuted;
        }

        double getLeftVolume() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            return sqrtf(_leftVolume);
        }

        double getRightVolume() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            return sqrtf(_rightVolume);
        }

        void setMuted(bool muted) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            _leftMuted = muted;
            _rightMuted = muted;
        }

        bool getMuted() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            return _leftMuted && _rightMuted;
        }

        inline int process(int count, const stereo_t* in, stereo_t* out) {
            float leftGain = _leftMuted ? 0.0f : _leftVolume;
            float rightGain = _rightMuted ? 0.0f : _rightVolume;

            for (int i = 0; i < count; i++) {
                out[i].l = in[i].l * leftGain;
                out[i].r = in[i].r * rightGain;
            }
            return count;
        }

        virtual int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    private:
        float _leftVolume;
        float _rightVolume;
        bool _leftMuted;
        bool _rightMuted;
    };
}

#pragma once
/*
 * Gathers data from CO2 Monitor device.
 *
 * Based on https://hackaday.io/project/5301-reverse-engineering-a-low-cost-usb-co-monitor
*/

#include "hidapi++.h"

#include <memory>
#include <list>
#include <thread>
#include <iostream>
#include <chrono>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace co2 {

    struct GatherContext {

        struct DataPoint {
            unsigned short co2;
            float temperature;
            std::chrono::system_clock::time_point time;
        };

        bool stop;

        void add(unsigned short co2, float temperature) {
            std::unique_lock lk(_mutex);
            _data.push_back({
                           co2,
                           temperature,
                           std::chrono::system_clock::now()
                       });

            if(_data.size() > 500) {
                _data.pop_front();
            }
        }

        std::list<DataPoint> _data;
        std::mutex _mutex;

        std::unique_ptr<std::thread> _gatherThread;
    };

    void error(std::string message) {
        json j = {
            {"error", message}
        };
        std::cout << j.dump() << std::endl;
    }

    void output(unsigned short co2, float temperature) {
        json j = {
            {"temperature", temperature},
            {"co2", co2}
        };
        std::cout << j.dump() << std::endl;
    }

    bool decrypt(HidApi::Device::Data key, HidApi::Device::Data& data) {

        if(data.size() != 8)
            return false;

        static std::array<unsigned char, 8> cstate { 0x48, 0x74, 0x65, 0x6D, 0x70, 0x39, 0x39, 0x65 };
        static std::array<unsigned char, 8> shuffle { 2, 4, 0, 7, 1, 6, 5, 3 };

        std::array<unsigned char, 8> phase1 {0,0,0,0,0,0,0,0};
        std::array<unsigned char, 8> phase2 {0,0,0,0,0,0,0,0};

        std::transform(phase1.begin(), phase1.end(), shuffle.begin(), phase1.begin(), [&data](auto p, auto s) { return data[s]; });
        std::transform(phase1.begin(), phase1.end(), key.begin(), phase1.begin(), [](auto p, auto k) { return p ^ k; });

        for(int i=0;i<8;i++) {
            phase2[i] = ( (phase1[i] >> 3) | (phase1[ (i-1+8)%8 ] << 5) ) & 0xff;
        }

        std::transform(phase2.begin(), phase2.end(), cstate.begin(), data.begin(), [](auto p, auto c) {
            return (0x100 + p - (( (c >> 4) | (c<<4) ) & 0xff) ) & 0xff;
        });

        int sum = std::accumulate(data.begin(), data.begin()+3, 0);

        if (data[4] != 0x0d || (sum & 0xff) != data[3]) {
            return false;
        }

        return true;
    }

    void fetchData(std::shared_ptr<GatherContext> ctxt) {

        while(!ctxt->stop) {

            auto start = std::chrono::steady_clock::now();

            auto hid = HidApi::create();

            if(!hid) {
                error("Failed opening hidapi!");
                std::this_thread::sleep_for(1s);
                continue;
            }

            HidApi::Enumerate enumerator(hid);

            auto it = std::find_if(enumerator.begin(), enumerator.end(), [](auto deviceInfo) { return deviceInfo.vendor_id == 0x4d9 && deviceInfo.product_id == 0xa052;});

            if( it == enumerator.end()) {
                error("Couldn't find CO2 Monitor!");
                std::this_thread::sleep_for(1s);
                continue;
            }

            auto device = hid->openDevice(*it);

            if(!device) {
                error("Couldn't open CO2 Monitor!");
                std::this_thread::sleep_for(1s);
                continue;
            }

            HidApi::Device::Data key = {0x0, 0xc4, 0xc6, 0xc0, 0x92, 0x40, 0x23, 0xdc, 0x96};

            if(device->send_feature_report(key) != key.size()) {
                error("Failed sending key!");
                std::this_thread::sleep_for(1s);
                continue;
            }

            key.erase(key.begin()); // Remove the 0x0 from the key

            std::array<unsigned short, 256> values;
            values.fill(0);

            bool hasTemp {false};
            bool hasCo2 {false};

            while(!hasTemp || !hasCo2) {
                auto data = device->read(8);

                if(!decrypt(key, data)) {
                    //std::cout << "Failed decrypting data!" << std::endl;
                    continue;
                }

                unsigned char op = data[0];

                unsigned short value = data[1] << 8 | data[2];
                values[op] = value;

                hasTemp |= (op == 0x42);
                hasCo2 |= (op == 0x50);
            }

            unsigned short co2 = values[0x50];
            float temperature = ((float)values[0x42]/16.0f-273.15f);

            ctxt->add(co2, temperature);

            output(co2, temperature);

            auto elapsed = std::chrono::steady_clock::now() - start;

            std::this_thread::sleep_for(5s-elapsed);
        }
    }

    std::shared_ptr<GatherContext> start() {
        auto gatherContext = std::make_shared<GatherContext>();
        gatherContext->stop = false;
        gatherContext->_gatherThread = std::make_unique<std::thread>(std::bind(&fetchData, gatherContext));

        return gatherContext;
    }

    void stop(std::shared_ptr<GatherContext> ctxt) {
        ctxt->stop = true;
        ctxt->_gatherThread->join();
    }

}

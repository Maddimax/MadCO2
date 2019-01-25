#pragma once

#include <vector>
#include <map>

#include <ostream>
#include <codecvt>

#include <hidapi.h>


std::ostream& operator<<(std::ostream& stream, const hid_device_info& info ) {
    stream << "{ " << info.path << "}";
    return stream;
}

class HidApi {
    struct CreateProtect {};
public:
    static std::shared_ptr<HidApi> create() {
        static std::weak_ptr<HidApi> hidApi;

        if(std::shared_ptr<HidApi> p = hidApi.lock()) {
            return p;
        }

        std::shared_ptr<HidApi> p = std::make_shared<HidApi>(CreateProtect());

        if(hid_init() != 0)
            return nullptr;

        hidApi = p;
        return p;
    }

    class Enumerate {
    public:
        class iterator {
        public:
            iterator(hid_device_info* deviceInfo) : _currentDevice(deviceInfo) { }

            hid_device_info operator*() {
                return *_currentDevice;
            }

            void operator++() {
                if(_currentDevice) {
                    _currentDevice = _currentDevice->next;
                }
            }

            bool operator!=(const iterator& it) {
                return it._currentDevice != _currentDevice;
            }
            bool operator==(const iterator& it) {
                return it._currentDevice == _currentDevice;
            }

            hid_device_info* _currentDevice;
        };
    public:
        Enumerate(std::shared_ptr<HidApi> hidApi, unsigned short vendorId = 0, unsigned short productId = 0)
            : _hidApi(hidApi)
        {
            _start = hid_enumerate(vendorId, productId);
        }

        Enumerate(const Enumerate&) = delete;

        ~Enumerate() {
            hid_free_enumeration(_start);
        }
    public:

        iterator begin() {
            return iterator(_start);
        }

        iterator end() {
            return iterator(nullptr);
        }

    private:
        hid_device_info* _start;
        std::shared_ptr<HidApi> _hidApi;
    };

    class Device {
    public:
        using Data = std::vector<unsigned char>;
    public:
        Device(CreateProtect, const hid_device_info& device_info, hid_device* hidDevice)
            : deviceInfo(device_info)
            , device(hidDevice)
        {
            using convert_type = std::codecvt_utf8<wchar_t>;
            std::wstring_convert<convert_type, wchar_t> converter;

            if(hid_get_manufacturer_string(device, wcharBuffer, 1024) == 0) {
                manufacturer = converter.to_bytes( std::wstring(wcharBuffer) );
            }

            if(hid_get_product_string(device, wcharBuffer, 1024) == 0) {
                product = converter.to_bytes( std::wstring(wcharBuffer) );
            }

            if(hid_get_serial_number_string(device, wcharBuffer, 1024) == 0) {
                serial = converter.to_bytes( std::wstring(wcharBuffer) );
            }
        }

        ~Device() {
            hid_close(device);
        }

        size_t send_feature_report(std::vector<unsigned char> data) {
            return (size_t)hid_send_feature_report(device, &data[0], data.size());
        }

        Data read(size_t numberOfBytes) {
            if(numberOfBytes > 1024) {
                return {};
            }

            int nRead = hid_read(device, charBuffer, numberOfBytes );

            if(nRead > 0) {
                return Data(charBuffer, charBuffer + nRead);
            }

            return {};
        }

    private:
        hid_device_info deviceInfo;
        hid_device* device;

        wchar_t wcharBuffer[1024];
        unsigned char charBuffer[1024];

        std::string manufacturer;
        std::string product;
        std::string serial;
    };

public:
    HidApi(CreateProtect) {
    }

public:
    ~HidApi() {
        hid_exit();
    }

    std::shared_ptr<Device> openDevice(const hid_device_info& info) {
        auto it = _openedDevices.find(info.path);
        if(it != _openedDevices.end()) {
            if(std::shared_ptr<Device> device = it->second.lock()) {
                return device;
            }
        }
        auto hidDevice = hid_open_path(info.path);

        if(!hidDevice)
            return nullptr;

        auto device = std::make_shared<Device>(CreateProtect(), info, hidDevice);
        _openedDevices[info.path] = device;
        return device;
    }

private:
    std::map<std::string, std::weak_ptr<Device> > _openedDevices;
};

std::ostream& operator<<(std::ostream& stream, const HidApi::Device::Data& data) {

    stream << "{";
    for(auto c : data) {
        stream << " 0x" << std::hex << (int)c;
    }
    stream << " }";

    return stream;
}

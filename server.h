#pragma once

#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_listener.h>
#include <cpprest/uri.h>
#include <cpprest/asyncrt_utils.h>
#include <cpprest/json.h>
#include <cpprest/filestream.h>
#include <cpprest/containerstream.h>
#include <cpprest/producerconsumerstream.h>

#include <boost/dll.hpp>

class server
{
public:
    server() = delete;
    server(utility::string_t url, std::shared_ptr<co2::GatherContext> gatherCtxt) : _listener(url), _gatherCtxt(gatherCtxt) {

        auto path = boost::dll::program_location().parent_path();

        std::ifstream bundle;
        bundle.open((path / "Chart.bundle.min.js").string(), std::ifstream::in);
        _chartBundle = std::string((std::istreambuf_iterator<char>(bundle)),
                                   std::istreambuf_iterator<char>());

        std::ifstream index;
        index.open((path / "index.html").string(), std::ifstream::in);
        _index = std::string((std::istreambuf_iterator<char>(index)),
                                   std::istreambuf_iterator<char>());

        _listener.support(web::http::methods::GET, std::bind(&server::handle_get, this, std::placeholders::_1));
    }
    ~server() {}

    pplx::task<void>open(){return _listener.open();}
    pplx::task<void>close(){return _listener.close();}

protected:

private:
    void handle_get(web::http::http_request message) {

        std::cout << "GET: " << message.relative_uri().path() <<std::endl;

        if(message.relative_uri().path() == "/data.json") {

            std::unique_lock lk(_gatherCtxt->_mutex);

            json j;

            for(auto& datapoint : _gatherCtxt->_data)
            {
                auto reltime = std::chrono::duration_cast<std::chrono::seconds>( std::chrono::system_clock::now() - datapoint.time);

                json jdata = {
                    {"co2", datapoint.co2},
                    {"temp", datapoint.temperature},
                    {"time", reltime.count() }
                };
                j.push_back(jdata);
            }

            message.reply(web::http::status_codes::OK,j.dump());
        }
        else if(message.relative_uri().path() == "/" || message.relative_uri().path() == "/index.html") {
            message.reply(web::http::status_codes::OK, _index, "text/html");
        }
        else if(message.relative_uri().path() == "/Chart.bundle.min.js") {
            message.reply(web::http::status_codes::OK, _chartBundle);
        }
        else {
            message.reply(web::http::status_codes::NotFound);
        }
    }
    void handle_error(pplx::task<void>& t){}

    web::http::experimental::listener::http_listener _listener;
    std::shared_ptr<co2::GatherContext> _gatherCtxt;

    std::string _chartBundle;
    std::string _index;
};

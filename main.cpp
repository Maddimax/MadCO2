#include "co2gatherer.h"
#include "server.h"

#include <iostream>
#include <memory>

#include <nlohmann/json.hpp>

int main(int argc, char** argv) {
    auto ctxt = co2::start();

    std::unique_ptr<server> httpServer;

    web::uri_builder uri("http://0.0.0.0:35842");

    auto addr = uri.to_uri().to_string();
    httpServer = std::unique_ptr<server>(new server(addr, ctxt));
    httpServer->open().wait();

    std::cout << "Press ENTER to exit." << std::endl;

    std::string line;
    std::getline(std::cin, line);

    co2::stop(ctxt);

    return 0;
}

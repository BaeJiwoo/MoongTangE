#include "Server/Network/AsioServer.h"
#include "Common/Utility/CommandLine.h"
#include <csignal>
#include <iostream>
namespace {
volatile std::sig_atomic_t stopping=0;
void requestStop(int) { stopping=1; }
}
int main(int argc,char** argv) {
    try {
        std::uint16_t port=27015;
        unsigned duration=0;
        bool publicBind=false;
        for (int i=1; i<argc; ++i) {
            const std::string_view arg(argv[i]);
            if (arg=="--help") { std::cout << "DedicatedServer [--port 27015] [--public] [--run-for seconds]\n"; return 0; }
            if (arg=="--public") { publicBind=true; continue; }
            if ((arg=="--port" || arg=="--run-for") && i+1<argc) {
                const auto value=sandbox::unsignedOption(argv[++i],arg=="--port" ? 65535u : 86400u,arg=="--port");
                if (arg=="--port") port=static_cast<std::uint16_t>(value); else duration=value;
                continue;
            }
            throw std::invalid_argument("Unknown or incomplete option: "+std::string(arg));
        }
        std::signal(SIGINT,requestStop); std::signal(SIGTERM,requestStop);
        auto core=std::make_shared<sandbox::ServerHost>(); core->start();
        const std::string address=publicBind ? "0.0.0.0" : "127.0.0.1";
        {
            sandbox::AsioServer server(core,address,port);
            std::cout << "Listening " << address << ':' << server.port() << std::endl;
            if (publicBind) std::cerr << "Warning: unauthenticated test server; use trusted networks only.\n";
            const auto start=std::chrono::steady_clock::now();
            while (!stopping && (!duration || std::chrono::steady_clock::now()-start<std::chrono::seconds(duration)))
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        core->stop(); return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

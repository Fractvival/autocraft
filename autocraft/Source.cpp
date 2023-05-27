#include "Header.h"

#include <Windows.h>
#include <wininet.h>
#include <vector>
#include <sstream> // Pøidáno pro použití getline
#include <nlohmann/json.hpp>

#pragma comment(lib, "wininet.lib")

using namespace std;

using json = nlohmann::json;

std::string GetWebPageContent(const std::string& url) {
    std::string content;

    HINTERNET hInternet = InternetOpen(L"HTTPGET", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (hConnect) {
            const int bufferSize = 4096;
            char buffer[bufferSize];
            DWORD bytesRead;
            while (InternetReadFile(hConnect, buffer, bufferSize - 1, &bytesRead) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                content += buffer;
            }
            InternetCloseHandle(hConnect);
        }
        InternetCloseHandle(hInternet);
    }

    return content;
}


int main()
{
    std::string url = "https://launchermeta.mojang.com/mc/game/version_manifest.json";
    std::string jsonStr = GetWebPageContent(url);

    if (!jsonStr.empty()) 
    {
        json data = json::parse(jsonStr);
        //cout<<data.dump(4);
        
        std::string snapshot = data["latest"]["snapshot"];
        std::string release = data["latest"]["release"];
        std::string urlSnapshot;
        std::string urlRelease;
        std::string urlSnapshotServer;
        std::string urlReleaseServer;

        cout << snapshot << endl;
        cout << release << endl<<endl;

        std::string searchId = snapshot;
        std::string searchType = "snapshot";

        // Vyhledání odpovídající položky
        for (const auto& item : data["versions"]) {
            if (item["id"] == searchId && item["type"] == searchType) {
                urlSnapshot = item["url"];
            }
        }

        searchId = release;
        searchType = "release";

        // Vyhledání odpovídající položky
        for (const auto& item : data["versions"]) {
            if (item["id"] == searchId && item["type"] == searchType) {
                urlRelease = item["url"];
            }
        }

        cout << urlSnapshot << endl;
        cout << urlRelease << endl<<endl;

        std::string jsonStrSnap = GetWebPageContent(urlSnapshot);
        std::string jsonStrRel = GetWebPageContent(urlRelease);

        json dataSnap = json::parse(jsonStrSnap);
        json dataRel = json::parse(jsonStrRel);

        urlSnapshotServer = dataSnap["downloads"]["server"]["url"];
        urlReleaseServer = dataRel["downloads"]["server"]["url"];
        cout <<endl<< urlSnapshotServer<<endl;
        cout <<urlReleaseServer<<endl<<endl;
    }
    else 
    {
        std::cout << "Chyba cteni stranky." << std::endl;
    }

    return 0;
}
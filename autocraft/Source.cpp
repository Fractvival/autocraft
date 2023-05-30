#include "Header.h"

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string.h>
#include <Windows.h>
#include <wininet.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>
#include <condition_variable>

#pragma comment(lib, "wininet.lib")

using json = nlohmann::json;
json data;
json dataSnapshot;
json dataRelease;

std::string configFilename = "config.txt";
std::map<std::string, std::string> configData;
std::mutex g_consoleMutex;
std::condition_variable g_exitSignal;
std::string jsonStr;
std::string snapshotVersion;
std::string releaseVersion;
std::string urlSnapshotJson;
std::string urlReleaseJson;
std::string urlSnapshotDown;
std::string urlReleaseDown;
std::string searchId;
std::string searchType;
std::string jsonStrSnapshot;
std::string jsonStrRelease;

std::string urlManifest = "https://launchermeta.mojang.com/mc/game/version_manifest.json";

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

using namespace std::filesystem;
namespace fs = std::filesystem;

bool createDirectory(const std::string& directoryPath) {
    if (exists(directoryPath)) {
        return true;
    }

    if (create_directory(directoryPath)) {
        return true;
    }
    else {
        return false;
    }
}


bool GetFileAndSave(const std::string& url, const std::string& filename, const std::string& folder) {
    // Inicializace WinINet
    HINTERNET hInternet = InternetOpen(L"File Downloader", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet == NULL) {
        std::cerr << "Chyba pøi inicializaci WinINet." << std::endl;
        return false;
    }
    // Otevøení pøipojení k URL
    HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (hConnect == NULL) {
        std::cerr << "Chyba pøi otevírání URL." << std::endl;
        InternetCloseHandle(hInternet);
        return false;
    }
    // Otevøení souboru pro zápis
    std::string foldername = "";
    std::string filenameWithExtension = filename + ".jar";
    if (createDirectory(folder))
    {
        foldername += folder+"\\"+filenameWithExtension;
    }
    FILE* file = fopen(foldername.c_str(), "wb");
    if (file == NULL) {
        std::cerr << "Chyba pøi otevírání souboru pro zápis." << std::endl;
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }
    // Stahování dat a zápis do souboru
    char buffer[1024];
    DWORD bytesRead = 0;
    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        fwrite(buffer, 1, bytesRead, file);
    }
    // Uzavøení zdrojù
    fclose(file);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return true;
}


std::map<std::string, std::string> LoadConfig(const std::string& filename) {
    std::ifstream configFile(filename); // Otevøít konfiguraèní soubor pro ètení

    std::map<std::string, std::string> configData; // Mapa pro uložení klíè-hodnota

    if (configFile) {
        std::string line;
        while (std::getline(configFile, line)) {
            // Ignorovat prázdné øádky a øádky zaèínající znakem komentáøe (#)
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Najít pozici rovnítka, které oddìluje klíè a hodnotu
            size_t equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                // Extrahovat klíè a hodnotu
                std::string key = line.substr(0, equalPos);
                std::string value = line.substr(equalPos + 1);

                // Odstranit pøebyteèné mezery kolem klíèe a hodnoty
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));

                // Pøidat klíè a hodnotu do mapy
                configData[key] = value;
            }
        }

        configFile.close(); // Uzavøít konfiguraèní soubor
    }
    else {
        std::cout << "Nepodaøilo se otevøít konfiguraèní soubor." << std::endl;
    }

    // Kontrola existence a pøidání defaultních hodnot
    if (configData.find("snapshot_version") == configData.end()) {
        configData["snapshot_version"] = "none";
    }
    if (configData.find("release_version") == configData.end()) {
        configData["release_version"] = "none";
    }
    if (configData.find("timeout_second") == configData.end()) {
        configData["timeout_second"] = "15";
    }

    return configData;
}


void SaveConfig(const std::string& filename, const std::map<std::string, std::string>& configData) {
    std::ofstream configFile(filename); // Otevøít konfiguraèní soubor pro zápis

    if (configFile) {
        for (const auto& pair : configData) {
            configFile << pair.first << " = " << pair.second << std::endl;
        }

        configFile.close(); // Uzavøít konfiguraèní soubor
    }
    else {
        std::cout << "Nepodaøilo se otevøít konfiguraèní soubor pro zápis." << std::endl;
    }
}



// Funkce pro smyèku hlavního vlákna
void MainLoop() {
    while (true) {
        // Kontrola aktuálnosti souboru na webu a aktualizace konfigurace
        // ...
        std::cout << "A check is in progress..." << std::endl;
        // KONTROLA ZDE
        jsonStr = "";
        std::string jsonStr = GetWebPageContent(urlManifest);
        if (!jsonStr.empty())
        {
            data = json::parse(jsonStr);
            snapshotVersion = data["latest"]["snapshot"];
            releaseVersion = data["latest"]["release"];

            if (configData["snapshot_version"] == snapshotVersion)
            {
                std::cout << "Snapshot is current (" << snapshotVersion << ")" << std::endl;
            }
            else
            {
                std::cout << "New version of Snapshot detected (" << snapshotVersion << ")" << std::endl;
                searchId = snapshotVersion;
                searchType = "snapshot";
                for (const auto& item : data["versions"]) {
                    if (item["id"] == searchId && item["type"] == searchType) {
                        urlSnapshotJson = item["url"];
                    }
                }
                jsonStrSnapshot = GetWebPageContent(urlSnapshotJson);
                dataSnapshot = json::parse(jsonStrSnapshot);
                urlSnapshotDown = dataSnapshot["downloads"]["server"]["url"];
                std::cout << "Downloading Snapshot version...";
                if (GetFileAndSave(urlSnapshotDown, snapshotVersion, "snapshot"))
                {
                    std::cout << "Success" << std::endl << std::endl;
                    configData["snapshot_version"] = snapshotVersion;
                    SaveConfig(configFilename, configData);
                }
                else
                {
                    std::cout << "Failure!" << std::endl << std::endl;
                }
            }

            if (configData["release_version"] == releaseVersion)
            {
                std::cout << "Release is current (" << releaseVersion << ")" << std::endl;
            }
            else
            {
                std::cout << "New version of Release detected (" << releaseVersion << ")" << std::endl;
                searchId = releaseVersion;
                searchType = "release";
                for (const auto& item : data["versions"]) {
                    if (item["id"] == searchId && item["type"] == searchType) {
                        urlReleaseJson = item["url"];
                    }
                }
                jsonStrRelease = GetWebPageContent(urlReleaseJson);
                dataRelease = json::parse(jsonStrRelease);
                urlReleaseDown = dataRelease["downloads"]["server"]["url"];
                std::cout << "Downloading Release version...";
                if (GetFileAndSave(urlReleaseDown, releaseVersion, "release"))
                {
                    std::cout << "Success" << std::endl << std::endl;
                    configData["release_version"] = releaseVersion;
                    SaveConfig(configFilename, configData);
                }
                else
                {
                    std::cout << "Failure!" << std::endl << std::endl;
                }
            }

        }
        else
        {
            std::cout << "Unable to load JSON file! Check your internet connection." << std::endl;
        }
        // KONTROLA KONEC
        // Èekání na ukonèení programu nebo pøíští kontrolu
        {
            std::unique_lock<std::mutex> lock(g_consoleMutex);
            if (g_exitSignal.wait_for(lock, std::chrono::seconds(std::stoi(configData["timeout_second"]))) == std::cv_status::no_timeout) {
                // Byl zadán pøíkaz pro ukonèení programu
                break;
            }
        }
    }
}

// Funkce pro smyèku vlákna pro ètení uživatelského vstupu
void ConsoleLoop() {
    std::string command;
    while (true) {
        // Pøeètení uživatelského vstupu
        std::getline(std::cin, command);

        // Zpracování uživatelského pøíkazu
        {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            // Zde mùžete provést akce na základì uživatelského vstupu
            // Napøíklad ovládat druhou konzoli nebo ukonèit program
            if (command == "exit" || command == "quit") {
                // Ukonèení programu
                g_exitSignal.notify_all();
                return;
            }
            else {
                // Zde mùžete provést další akce na základì uživatelského vstupu
                // Napøíklad ovládat druhou konzoli
                continue;
            }
        }
    }
}


int main() {

    configData = LoadConfig(configFilename);
    SaveConfig(configFilename, configData);

    std::cout << "AutoCraft v1.0-290523 (C) PROGMaxi software" << std::endl<<std::endl;
    std::cout << "To exit, write the command \"exit\"" << std::endl;
    std::cout << "A new version will be checked every " << configData["timeout_second"]<< " second" << std::endl <<std::endl;

    // Spuštìní hlavního vlákna pro smyèku
    std::thread mainThread(MainLoop);

    // Spuštìní vlákna pro ètení uživatelského vstupu
    std::thread consoleThread(ConsoleLoop);

    // Èekání na ukonèení programu
    mainThread.join();
    consoleThread.join();

    return 0;
}



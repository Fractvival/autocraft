#include "Header.h"
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string.h>
#include <Windows.h>
#include <shellapi.h>
#include <wininet.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <chrono>
#include <thread>
#include <random>
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


class MinecraftServer {
public:
    MinecraftServer()
        : m_serverProcess(nullptr), m_workingDirectory(""), m_xms(""), m_xmx(""), m_serverJarName("") {}

    bool StartServer() {
        std::string command = "cd " + m_workingDirectory + " && java " + m_xms + " " + m_xmx + " -jar " + m_serverJarName + " nogui";
        m_serverProcess = _popen(command.c_str(), "w");
        return m_serverProcess != nullptr;
    }

    void StopServer() {
        if (m_serverProcess) {
            fprintf(m_serverProcess, "stop\n");
            fflush(m_serverProcess);
            _pclose(m_serverProcess);
            m_serverProcess = nullptr;
        }
    }

    bool IsServerRunning() const {
        return m_serverProcess != nullptr;
    }

    void SendCommand(const std::string& command) {
        if (m_serverProcess) {
            fprintf(m_serverProcess, "%s\n", command.c_str());
            fflush(m_serverProcess);
        }
    }

    std::string ReadOutput() {
        std::string output;
        if (m_serverProcess) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), m_serverProcess)) {
                output += buffer;
            }
        }
        return output;
    }

    void SetWorkingDirectory(const std::string& workingDirectory) {
        m_workingDirectory = workingDirectory;
    }

    void SetXms(const std::string& xms) {
        m_xms = "-Xms" + xms;
    }

    void SetXmx(const std::string& xmx) {
        m_xmx = "-Xmx" + xmx;
    }

    void SetServerJarName(const std::string& serverJarName) {
        m_serverJarName = serverJarName;
    }

private:
    FILE* m_serverProcess;
    std::string m_workingDirectory;
    std::string m_xms;
    std::string m_xmx;
    std::string m_serverJarName;
};

MinecraftServer server;


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
        configData["timeout_second"] = "30";
    }
    if (configData.find("run_server") == configData.end()) {
        configData["run_server"] = "true";
    }
    if (configData.find("type_server") == configData.end()) {
        configData["type_server"] = "snapshot";
    }
    if (configData.find("xms") == configData.end()) {
        configData["xms"] = "1G";
    }
    if (configData.find("xmx") == configData.end()) {
        configData["xmx"] = "4G";
    }
    if (configData.find("server_name") == configData.end()) {
        configData["server_name"] = "AutoCraft";
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
        std::cout << "The settings file is not currently created." << std::endl;
    }
}


bool fileExists(const std::string& filename) {
    std::ifstream file(filename.c_str());
    return file.good();
}


void createAndWriteToFile(const std::string& filename, const std::string& text) {
    try {
        std::ofstream file(filename.c_str());
        if (file.is_open()) {
            file << text;
            file.close();
            std::cout << "A new file " << filename << " has been created." << std::endl;
        }
        else {
            throw std::ofstream::failure("Failed to create the file "+filename);
        }
    }
    catch (const std::ofstream::failure& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// Funkce pro generování náhodného èísla s urèeným poètem cifer a rozsahem
std::string generateRandomNumber(int digits, int min, int max) {
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(min, max);

    int factor = 1;
    for (int i = 1; i < digits; ++i) {
        factor *= 10;
    }

    int randomNumber = distribution(generator) * factor;
    return std::to_string(randomNumber);
}

void renameFileIfExists(const std::string& filename) {
    std::filesystem::path filePath(filename);
    if (std::filesystem::exists(filePath)) {
        std::filesystem::path newFilePath = filePath.parent_path() / ("__old_" + filePath.filename().string());
        std::error_code ec;
        std::filesystem::rename(filePath, newFilePath, ec);
        if (!ec) {
            std::cout << "The file " << filePath << " has been renamed to: " << newFilePath << std::endl;
        }
        else {
            std::cout << "File " << filePath << " renaming failed. Error code: " << ec.value() << std::endl;
        }
    }
    else {
        std::cout << "The file " << filePath << " does not exist." << std::endl;
    }
}


bool isRunning = true;

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

            configData.clear();
            configData = LoadConfig(configFilename);

            if (configData["snapshot_version"] == snapshotVersion)
            {
                std::cout << "Snapshot is current (" << snapshotVersion << ")" << std::endl;
            }
            else
            {
                if (server.IsServerRunning())
                {
                    std::cout << "\nStopping the server for the purpose of downloading a new version\n";
                    server.StopServer();
                }
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
                    createAndWriteToFile("snapshot\\run.bat", "java -Xms" + configData["xms"] + " -Xmx" + configData["xmx"] + " -jar " + snapshotVersion + ".jar nogui");
                    renameFileIfExists("snapshot\\" + configData["snapshot_version"] + ".jar");
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
                if (server.IsServerRunning())
                {
                    std::cout << "\nStopping the server for the purpose of downloading a new version\n";
                    server.StopServer();
                }
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
                    createAndWriteToFile("release\\run.bat", "java -Xms" + configData["xms"] + " -Xmx" + configData["xmx"] + " -jar " + releaseVersion + ".jar nogui");
                    renameFileIfExists("release\\" + configData["release_version"] + ".jar");
                    configData["release_version"] = releaseVersion;
                    SaveConfig(configFilename, configData);
                }
                else
                {
                    std::cout << "Failure!" << std::endl << std::endl;
                }
            }

            if (configData["run_server"] == "true" && !server.IsServerRunning() && isRunning )
            {
                std::string cmd = "";
                std::string rnd = generateRandomNumber(10, -999999999, 999999999);
                std::string nam = "";
                std::cout << "\nAutomatic execution of the """ << configData["type_server"] << """ version is currently in progress.\n" << std::endl;
                if (configData["type_server"] == "snapshot")
                {
                    nam = "\u00a7f" + configData["server_name"] + "\u00a78-\u00a77Snapshot server " + snapshotVersion;
                    cmd = "cd snapshot && java -Xms"+configData["xms"]+" -Xmx"+configData["xmx"]+" -jar "+snapshotVersion+".jar nogui";
                    if (!fileExists("snapshot\\eula.txt"))
                    {
                        createAndWriteToFile("snapshot\\eula.txt", "#Confirmed automatically by Autocraft.\neula=true");
                    }
                    if (!fileExists("snapshot\\server.properties"))
                    {
                        createAndWriteToFile("snapshot\\server.properties",
                            "#Minecraft server properties\n"
                            "#Created automatically by Autocraft.\n"
                            "#Please modify these values to your own.\n"
                            "enable-jmx-monitoring=false\n"
                            "rcon.port=27575\n"
                            "level-seed=" + rnd + "\n"
                            "gamemode=survival\n"
                            "enable-command-block=false\n"
                            "enable-query=false\n"
                            "generator-settings={}\n"
                            "enforce-secure-profile=true\n"
                            "level-name=world\n"
                            "motd=" + nam + "\n"
                            "query.port=25565\n"
                            "pvp=true\n"
                            "generate-structures=true\n"
                            "max-chained-neighbor-updates=1000000\n"
                            "difficulty=hard\n"
                            "network-compression-threshold=256\n"
                            "max-tick-time=60000\n"
                            "require-resource-pack=false\n"
                            "use-native-transport=true\n"
                            "max-players=5\n"
                            "online-mode=true\n"
                            "enable-status=true\n"
                            "allow-flight=false\n"
                            "initial-disabled-packs=\n"
                            "broadcast-rcon-to-ops=true\n"
                            "view-distance=10\n"
                            "server-ip=\n"
                            "resource-pack-prompt=\n"
                            "allow-nether=true\n"
                            "server-port=25565\n"
                            "enable-rcon=false\n"
                            "sync-chunk-writes=true\n"
                            "op-permission-level=4\n"
                            "prevent-proxy-connections=false\n"
                            "hide-online-players=false\n"
                            "resource-pack=\n"
                            "entity-broadcast-range-percentage=100\n"
                            "simulation-distance=10\n"
                            "rcon.password=\n"
                            "player-idle-timeout=0\n"
                            "force-gamemode=false\n"
                            "rate-limit=0\n"
                            "hardcore=false\n"
                            "white-list=false\n"
                            "broadcast-console-to-ops=true\n"
                            "spawn-npcs=true\n"
                            "spawn-animals=true\n"
                            "function-permission-level=2\n"
                            "initial-enabled-packs=vanilla\n"
                            "level-type=minecraft\\:normal\n"
                            "text-filtering-config=\n"
                            "spawn-monsters=true\n"
                            "enforce-whitelist=false\n"
                            "spawn-protection=16\n"
                            "resource-pack-sha1=\n"
                            "max-world-size=29999984"
                        );
                    }
                    server.SetWorkingDirectory("snapshot");
                    server.SetServerJarName(snapshotVersion+".jar");
                    server.SetXms(configData["xms"]);
                    server.SetXmx(configData["xmx"]);
                    server.StartServer();
                }
                else
                {
                    nam = "\u00a7f" + configData["server_name"] + "\u00a78-\u00a77Release server " + releaseVersion;
                    cmd = "cd release && java -Xms" + configData["xms"] + " -Xmx" + configData["xmx"] + " -jar " + releaseVersion + ".jar nogui";
                    if (!fileExists("release\\eula.txt"))
                    {
                        createAndWriteToFile("release\\eula.txt", "#Confirmed automatically by Autocraft.\neula=true");
                    }
                    if (!fileExists("release\\server.properties"))
                    {
                        createAndWriteToFile("release\\server.properties",
                            "#Minecraft server properties\n"
                            "#Created automatically by Autocraft.\n"
                            "#Please modify these values to your own.\n"
                            "enable-jmx-monitoring=false\n"
                            "rcon.port=27575\n"
                            "level-seed=" + rnd + "\n"
                            "gamemode=survival\n"
                            "enable-command-block=false\n"
                            "enable-query=false\n"
                            "generator-settings={}\n"
                            "enforce-secure-profile=true\n"
                            "level-name=world\n"
                            "motd=" + nam + "\n"
                            "query.port=25565\n"
                            "pvp=true\n"
                            "generate-structures=true\n"
                            "max-chained-neighbor-updates=1000000\n"
                            "difficulty=hard\n"
                            "network-compression-threshold=256\n"
                            "max-tick-time=60000\n"
                            "require-resource-pack=false\n"
                            "use-native-transport=true\n"
                            "max-players=5\n"
                            "online-mode=true\n"
                            "enable-status=true\n"
                            "allow-flight=false\n"
                            "initial-disabled-packs=\n"
                            "broadcast-rcon-to-ops=true\n"
                            "view-distance=10\n"
                            "server-ip=\n"
                            "resource-pack-prompt=\n"
                            "allow-nether=true\n"
                            "server-port=25565\n"
                            "enable-rcon=false\n"
                            "sync-chunk-writes=true\n"
                            "op-permission-level=4\n"
                            "prevent-proxy-connections=false\n"
                            "hide-online-players=false\n"
                            "resource-pack=\n"
                            "entity-broadcast-range-percentage=100\n"
                            "simulation-distance=10\n"
                            "rcon.password=\n"
                            "player-idle-timeout=0\n"
                            "force-gamemode=false\n"
                            "rate-limit=0\n"
                            "hardcore=false\n"
                            "white-list=false\n"
                            "broadcast-console-to-ops=true\n"
                            "spawn-npcs=true\n"
                            "spawn-animals=true\n"
                            "function-permission-level=2\n"
                            "initial-enabled-packs=vanilla\n"
                            "level-type=minecraft\\:normal\n"
                            "text-filtering-config=\n"
                            "spawn-monsters=true\n"
                            "enforce-whitelist=false\n"
                            "spawn-protection=16\n"
                            "resource-pack-sha1=\n"
                            "max-world-size=29999984"
                        );
                    }
                    server.SetWorkingDirectory("release");
                    server.SetServerJarName(releaseVersion + ".jar");
                    server.SetXms(configData["xms"]);
                    server.SetXmx(configData["xmx"]);
                    server.StartServer();
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
            if (command == "exit") 
            {
                server.StopServer();
                g_exitSignal.notify_all();
                return;
            }
            else if (command == "stop")
            {
                if (server.IsServerRunning())
                {
                    std::cout << "\nStop server!\n";
                    isRunning = false;
                    server.StopServer();
                }
                else
                {
                    std::cout << "\nServer NOT running!!\n";
                }
                continue;
            }
            else if (command == "start")
            {
                if (server.IsServerRunning())
                {
                    std::cout << "\nThe server is running.\n";
                }
                else
                {
                    std::cout << "\nThe server start will occur during the next version check.\n";
                    isRunning = true;
                }
                continue;
            }
            else 
            {
                server.SendCommand(command);
                continue;
            }
        }
    }
}


int main() {

    configData = LoadConfig(configFilename);
    SaveConfig(configFilename, configData);

    std::cout << "AutoCraft v1.0-290523 (C) PROGMaxi software" << std::endl<<std::endl;
    std::cout << "To stop server and exit, write the command \"exit\"" << std::endl;
    std::cout << "To stop server, write the command \"stop\"" << std::endl;
    std::cout << "To run server, write the command \"start\"" << std::endl;
    std::cout << "Other commands are being sent to the server." << std::endl;
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



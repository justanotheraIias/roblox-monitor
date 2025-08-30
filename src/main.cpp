#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <windows.h>
#include <wininet.h>
#include <limits>
#include "json.hpp"
using json = nlohmann::json;

// Color helpers for Windows console
void setColor(WORD color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}
void resetColor() {
    setColor(7); // Default gray
}

struct GameData {
    int ccu;
    double rating;
    std::string timestamp;
};

class RobloxGameMonitor {
private:
    std::string gameId;
    int monitorMinutes;
    std::vector<GameData> dataPoints;
    bool skipInfoPrint = false;
    std::vector<std::string> logLines; // NEW

public:
    struct GameInfo {
        std::string name;
        std::string description;
        std::string created;
        std::string creatorName;
        std::string creatorType;
    };

    RobloxGameMonitor(const std::string& id, int minutes) 
        : gameId(id), monitorMinutes(minutes), dataPoints() {}

    std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);

        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    std::string httpGet(const std::string& url) {
        HINTERNET hInternet = InternetOpenA("RobloxMonitor", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) {
            std::cerr << "InternetOpenA failed. Error: " << GetLastError() << std::endl;
            return "";
        }

        HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (!hConnect) {
            std::cerr << "InternetOpenUrlA failed for URL: " << url << " Error: " << GetLastError() << std::endl;
            InternetCloseHandle(hInternet);
            return "";
        }

        std::string response;
        char buffer[4096];
        DWORD bytesRead;

        while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            response.append(buffer, bytesRead);
        }

        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return response;
    }

    // Fetch game info from universe API
    GameInfo fetchGameInfo() {
        GameInfo info = {"N/A", "N/A", "N/A", "N/A", "N/A"};
        std::string url = "https://games.roblox.com/v1/games?universeIds=" + gameId;
        std::string response = httpGet(url);

        if (!response.empty()) {
            try {
                json root = json::parse(response);
                if (root.contains("data") && root["data"].is_array() && !root["data"].empty()) {
                    auto gameData = root["data"][0];
                    if (gameData.contains("name"))
                        info.name = gameData["name"].get<std::string>();
                    if (gameData.contains("description"))
                        info.description = gameData["description"].get<std::string>();
                    if (gameData.contains("created"))
                        info.created = gameData["created"].get<std::string>();
                    if (gameData.contains("creator") && gameData["creator"].is_object()) {
                        auto creator = gameData["creator"];
                        if (creator.contains("name"))
                            info.creatorName = creator["name"].get<std::string>();
                        if (creator.contains("type"))
                            info.creatorType = creator["type"].get<std::string>();
                    }
                }
            } catch (...) {
                // ignore parse errors
            }
        }
        return info;
    }

    // Print game info table
    // Print game info table
    void printGameInfoTable(const GameInfo& info, WORD color = 11) {
        setColor(color);
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "GAME INFO" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        resetColor();
        std::cout << "Name:        " << info.name << std::endl;
        std::cout << "Created:     " << info.created << std::endl;
        std::cout << "Creator:     " << info.creatorName << " (" << info.creatorType << ")" << std::endl;
        std::cout << "Description: " << info.description << std::endl;
        setColor(color);
        std::cout << std::string(60, '=') << std::endl << std::endl;
        resetColor();
    }


    GameData fetchGameData() {
        GameData data = {0, 0.0, getCurrentTime()};
        
        // Only use Universe ID endpoints
        std::string universeUrl = "https://games.roblox.com/v1/games?universeIds=" + gameId;
        std::string universeResponse = httpGet(universeUrl);
        
        if (!universeResponse.empty()) {
            try {
                json universeRoot = json::parse(universeResponse);
                if (universeRoot.contains("data") && universeRoot["data"].is_array() && !universeRoot["data"].empty()) {
                    auto gameData = universeRoot["data"][0];
                    if (gameData.contains("playing")) {
                        data.ccu = gameData["playing"].get<int>();
                    }
                }
            } catch (const std::exception& e) {
                // Handle parsing error
            }
        }

        // Get votes for rating (Universe ID endpoint)
        std::string voteUrl = "https://games.roblox.com/v1/games/votes?universeIds=" + gameId;
        std::string voteResponse = httpGet(voteUrl);
        
        if (!voteResponse.empty()) {
            try {
                json voteRoot = json::parse(voteResponse);
                if (voteRoot.contains("data") && voteRoot["data"].is_array() && !voteRoot["data"].empty()) {
                    auto voteData = voteRoot["data"][0];
                    if (voteData.contains("upVotes") && voteData.contains("downVotes")) {
                        int upVotes = voteData["upVotes"].get<int>();
                        int downVotes = voteData["downVotes"].get<int>();
                        int totalVotes = upVotes + downVotes;
                        if (totalVotes > 0) {
                            data.rating = (double(upVotes) / totalVotes) * 100.0;
                        }
                    }
                }
            } catch (const std::exception& e) {
                // Handle parsing error
            }
        }

        return data;
    }

    void setSkipInfoPrint(bool skip) { skipInfoPrint = skip; }

    // Store logs instead of printing
    void startMonitoring(const std::string& logPrefix = "", WORD logColor = 11) {
        if (!skipInfoPrint) {
            GameInfo info = fetchGameInfo();
            printGameInfoTable(info);
        }

        int minute = 0;
        int last_minute = -1;
        // Efficiently wait until the start of the next minute
        auto now = std::chrono::system_clock::now();
        auto next_minute = std::chrono::time_point_cast<std::chrono::minutes>(now) + std::chrono::minutes(1);
        std::this_thread::sleep_until(next_minute);

        // Update last_minute after waking up
        auto time_t_now = std::chrono::system_clock::to_time_t(next_minute);
        auto tm_now = *std::localtime(&time_t_now);
        last_minute = tm_now.tm_min;


        while (minute < monitorMinutes) {
            GameData data = fetchGameData();
            dataPoints.push_back(data);
     
            // Prepare log line as string
            std::ostringstream oss;
            oss << logPrefix << " Minute " << (minute + 1) << "/" << monitorMinutes << " - "
                << "CCU: " << data.ccu
                << ", Rating: " << std::fixed << std::setprecision(1) << data.rating << "%"
                << " [" << data.timestamp << "]";

            // Print to console (real-time)
            setColor(logColor);
            std::cout << oss.str() << std::endl;
            resetColor();

            // Store in logLines for later retrieval
            logLines.push_back(oss.str());

            minute++;

            auto now = std::chrono::system_clock::now();
            auto next_minute = std::chrono::time_point_cast<std::chrono::minutes>(now) + std::chrono::minutes(1);
            std::this_thread::sleep_until(next_minute);

            // Update last_minute
            auto time_t_now = std::chrono::system_clock::to_time_t(next_minute);
            auto tm_now = *std::localtime(&time_t_now);
            last_minute = tm_now.tm_min;
        }
        // Do NOT call showResults() here!
    }

    // Returns average CCU
    double getAverageCCU() const {
        if (dataPoints.empty()) return 0.0;
        int sum = 0;
        for (const auto& d : dataPoints) sum += d.ccu;
        return static_cast<double>(sum) / dataPoints.size();
    }

    // Returns index of peak CCU
    size_t getPeakCCUIndex() const {
        if (dataPoints.empty()) return 0;
        return std::distance(dataPoints.begin(), std::max_element(dataPoints.begin(), dataPoints.end(),
            [](const GameData& a, const GameData& b) { return a.ccu < b.ccu; }));
    }

    // Returns index of lowest CCU
    size_t getLowestCCUIndex() const {
        if (dataPoints.empty()) return 0;
        return std::distance(dataPoints.begin(), std::min_element(dataPoints.begin(), dataPoints.end(),
            [](const GameData& a, const GameData& b) { return a.ccu < b.ccu; }));
    }

    void showResults(const std::string& gameName = "") {
        setColor(10); // Green
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "MONITORING COMPLETE - RESULTS SUMMARY" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        resetColor();

        if (dataPoints.empty()) {
            std::cout << "No data collected!" << std::endl;
            return;
        }

        // Find CCU peaks
        auto ccuMinMax = std::minmax_element(dataPoints.begin(), dataPoints.end(),
            [](const GameData& a, const GameData& b) { return a.ccu < b.ccu; });
        size_t peakIdx = getPeakCCUIndex();
        size_t lowIdx = getLowestCCUIndex();

        // Find rating peaks
        auto ratingMinMax = std::minmax_element(dataPoints.begin(), dataPoints.end(),
            [](const GameData& a, const GameData& b) { return a.rating < b.rating; });

        // Display summary
        if (!gameName.empty())
            std::cout << "Game: " << gameName << std::endl;
        std::cout << "Game ID: " << gameId << std::endl;
        std::cout << "Monitoring Duration: " << monitorMinutes << " minutes" << std::endl;
        std::cout << "Total Data Points: " << dataPoints.size() << std::endl;

        setColor(14); // Yellow for section headers
        std::cout << "\nCONCURRENT USERS (CCU) ANALYSIS:" << std::endl;
        resetColor();
        if (!dataPoints.empty()) {
            std::cout << "Starting CCU: " << dataPoints.front().ccu 
                      << " [" << dataPoints.front().timestamp << "]" << std::endl;
            std::cout << "Ending CCU: " << dataPoints.back().ccu 
                      << " [" << dataPoints.back().timestamp << "]" << std::endl;
        }
        std::cout << "Lowest CCU: " << ccuMinMax.first->ccu 
                  << " [" << ccuMinMax.first->timestamp << "] (Minute " << (lowIdx+1) << ")" << std::endl;
        std::cout << "Highest CCU: " << ccuMinMax.second->ccu 
                  << " [" << ccuMinMax.second->timestamp << "] (Minute " << (peakIdx+1) << ")" << std::endl;
        std::cout << "CCU Average: " << std::fixed << std::setprecision(2) << getAverageCCU() << std::endl;

        int ccuChange = (!dataPoints.empty()) ? (dataPoints.back().ccu - dataPoints.front().ccu) : 0;
        std::cout << "Net CCU Change: " << (ccuChange >= 0 ? "+" : "") << ccuChange;
        if (!dataPoints.empty() && dataPoints.front().ccu != 0) {
            std::cout << " (" << std::fixed << std::setprecision(1)
                      << ((double)ccuChange / dataPoints.front().ccu * 100) << "%)";
        } else {
            std::cout << " (N/A%)";
        }
        std::cout << std::endl;

        setColor(14);
        std::cout << "\nRATING ANALYSIS:" << std::endl;
        resetColor();
        if (!dataPoints.empty()) {
            std::cout << "Starting Rating: " << std::fixed << std::setprecision(1) 
                      << dataPoints.front().rating << "% [" << dataPoints.front().timestamp << "]" << std::endl;
            std::cout << "Ending Rating: " << dataPoints.back().rating 
                      << "% [" << dataPoints.back().timestamp << "]" << std::endl;
        }
        std::cout << "Lowest Rating: " << ratingMinMax.first->rating 
                  << "% [" << ratingMinMax.first->timestamp << "]" << std::endl;
        std::cout << "Highest Rating: " << ratingMinMax.second->rating 
                  << "% [" << ratingMinMax.second->timestamp << "]" << std::endl;
        
        double ratingChange = (!dataPoints.empty()) ? (dataPoints.back().rating - dataPoints.front().rating) : 0.0;
        std::cout << "Net Rating Change: " << (ratingChange >= 0 ? "+" : "") 
                  << std::setprecision(2) << ratingChange << "%" << std::endl;

        setColor(13); // Magenta for detailed points
        std::cout << "\nDETAILED DATA POINTS:" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        for (size_t i = 0; i < dataPoints.size(); i++) {
            std::cout << "Point " << (i + 1) << ": CCU=" << dataPoints[i].ccu 
                      << ", Rating=" << std::fixed << std::setprecision(1) << dataPoints[i].rating 
                      << "% [" << dataPoints[i].timestamp << "]" << std::endl;
        }
        setColor(10);
        std::cout << std::string(60, '=') << std::endl;
        resetColor();
    }

    const std::vector<GameData>& getDataPoints() const { return dataPoints; }
    // Getter for log lines
    const std::vector<std::string>& getLogLines() const { return logLines; }
};

int main() {

    SetConsoleOutputCP(CP_UTF8);   // ✅ add this line here

    setColor(11); // Cyan
    std::cout << "Roblox Game Monitoring Tool" << std::endl;
    std::cout << std::string(40, '=') << std::endl;
    resetColor();

    std::string compareModeInput;
    std::cout << "Initiate compare mode? (y/n): ";
    std::getline(std::cin, compareModeInput);

    bool compareMode = (compareModeInput.size() > 0 && (compareModeInput[0] == 'y' || compareModeInput[0] == 'Y'));

    auto isGameInfoValid = [](const RobloxGameMonitor::GameInfo& info) {
        return info.name != "N/A" && info.created != "N/A" && info.creatorName != "N/A";
    };

    if (compareMode) {
        std::string gameId1, gameId2;
        int minutes;

        // Prompt for first Universe ID and validate
        while (true) {
            do {
                std::cout << "Enter first Roblox Universe ID: ";
                std::getline(std::cin, gameId1);
            } while (gameId1.empty());
            RobloxGameMonitor monitorTest(gameId1, 1);
            auto infoTest = monitorTest.fetchGameInfo();
            if (isGameInfoValid(infoTest)) break;
            setColor(12); // Red
            std::cout << "Invalid Universe ID (N/A returned). Please try again.\n";
            resetColor();
        }

        // Prompt for second Universe ID and validate
        while (true) {
            do {
                std::cout << "Enter second Roblox Universe ID: ";
                std::getline(std::cin, gameId2);
            } while (gameId2.empty());
            RobloxGameMonitor monitorTest(gameId2, 1);
            auto infoTest = monitorTest.fetchGameInfo();
            if (isGameInfoValid(infoTest)) break;
            setColor(12);
            std::cout << "Invalid Universe ID (N/A returned). Please try again.\n";
            resetColor();
        }

        std::cout << "Enter monitoring duration (minutes): ";
        if (!(std::cin >> minutes)) {
            setColor(12);
            std::cout << "Invalid input. Please enter a number." << std::endl;
            resetColor();
            system("pause");
            return 1;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (minutes <= 0) {
            setColor(12);
            std::cout << "Invalid duration. Please enter a positive number." << std::endl;
            resetColor();
            system("pause");
            return 1;
        }
        std::cout << std::endl;

        RobloxGameMonitor monitor1(gameId1, minutes);
        RobloxGameMonitor monitor2(gameId2, minutes);

        // Print game info for both games
        setColor(11);
        std::cout << "=== GAME 1 INFO ===\n";
        resetColor();
        auto info1 = monitor1.fetchGameInfo();
        monitor1.printGameInfoTable(info1, 11);  // Cyan

        setColor(12);
        std::cout << "=== GAME 2 INFO ===\n";
        resetColor();
        auto info2 = monitor2.fetchGameInfo();
        monitor2.printGameInfoTable(info2, 12);  // Red


        // Prevent duplicate info print in threads
        monitor1.setSkipInfoPrint(true);
        monitor2.setSkipInfoPrint(true);

        // Monitor both games in parallel, store logs only
        std::thread t1([&monitor1]() { monitor1.startMonitoring("[GAME 1]", 9); }); // Blue
        std::thread t2([&monitor2]() { monitor2.startMonitoring("[GAME 2]", 12); }); // Red
        t1.join();
        t2.join();

        // Print logs in alternating order, always GAME 1 first, with a small delay for sync
        const auto& logs1 = monitor1.getLogLines();
        const auto& logs2 = monitor2.getLogLines();
        size_t maxLen = std::max(logs1.size(), logs2.size());
        for (size_t i = 0; i < maxLen; ++i) {
            if (i < logs1.size()) {
                setColor(9); // Blue for GAME 1
                std::cout << logs1[i] << std::endl;
                resetColor();
                std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 1 second delay
            }
            if (i < logs2.size()) {
                setColor(12); // Red for GAME 2
                std::cout << logs2[i] << std::endl;
                resetColor();
            }
        }

        // Now print results for each game, clearly separated
        setColor(11);
        std::cout << "\n\n=== GAME 1 RESULTS ===\n";
        resetColor();
        monitor1.showResults(info1.name);

        setColor(11);
        std::cout << "\n\n=== GAME 2 RESULTS ===\n";
        resetColor();
        monitor2.showResults(info2.name);

        // Comparison summary
        setColor(10);
        std::cout << "\n\n=== COMPARISON SUMMARY ===\n";
        std::cout << std::string(60, '=') << std::endl;
        resetColor();
        std::cout << "Game 1: " << info1.name << "\nGame 2: " << info2.name << std::endl;
        std::cout << "Monitoring Duration: " << minutes << " minutes" << std::endl;

        std::cout << "\nCCU Averages:\n";
        std::cout << info1.name << ": " << std::fixed << std::setprecision(2) << monitor1.getAverageCCU() << std::endl;
        std::cout << info2.name << ": " << std::fixed << std::setprecision(2) << monitor2.getAverageCCU() << std::endl;

        size_t peak1 = monitor1.getPeakCCUIndex();
        size_t peak2 = monitor2.getPeakCCUIndex();
        size_t low1 = monitor1.getLowestCCUIndex();
        size_t low2 = monitor2.getLowestCCUIndex();

        const auto& dp1 = monitor1.getDataPoints();
        const auto& dp2 = monitor2.getDataPoints();

        std::cout << "\nPeak CCU:\n";
        if (!dp1.empty())
            std::cout << info1.name << ": " << dp1[peak1].ccu << " (Minute " << (peak1+1) << ", " << dp1[peak1].timestamp << ")" << std::endl;
        else
            std::cout << info1.name << ": N/A" << std::endl;
        if (!dp2.empty())
            std::cout << info2.name << ": " << dp2[peak2].ccu << " (Minute " << (peak2+1) << ", " << dp2[peak2].timestamp << ")" << std::endl;
        else
            std::cout << info2.name << ": N/A" << std::endl;

        std::cout << "\nLowest CCU:\n";
        if (!dp1.empty())
            std::cout << info1.name << ": " << dp1[low1].ccu << " (Minute " << (low1+1) << ", " << dp1[low1].timestamp << ")" << std::endl;
        else
            std::cout << info1.name << ": N/A" << std::endl;
        if (!dp2.empty())
            std::cout << info2.name << ": " << dp2[low2].ccu << " (Minute " << (low2+1) << ", " << dp2[low2].timestamp << ")" << std::endl;
        else
            std::cout << info2.name << ": N/A" << std::endl;

        setColor(10);
        std::cout << std::string(60, '=') << std::endl;
        resetColor();
        system("pause");
        return 0;
    } else {
        std::string gameId;
        int minutes;

        // Prompt for Universe ID and validate
        while (true) {
            do {
                std::cout << "Enter Roblox Universe ID: ";
                std::getline(std::cin, gameId);
            } while (gameId.empty());
            RobloxGameMonitor monitorTest(gameId, 1);
            auto infoTest = monitorTest.fetchGameInfo();
            if (isGameInfoValid(infoTest)) break;
            setColor(12);
            std::cout << "Invalid Universe ID (N/A returned). Please try again.\n";
            resetColor();
        }

        std::cout << "Enter monitoring duration (minutes): ";
        if (!(std::cin >> minutes)) {
            setColor(12);
            std::cout << "Invalid input. Please enter a number." << std::endl;
            resetColor();
            system("pause");
            return 1;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (minutes <= 0) {
            setColor(12);
            std::cout << "Invalid duration. Please enter a positive number." << std::endl;
            resetColor();
            system("pause");
            return 1;
        }

        std::cout << std::endl;

        RobloxGameMonitor monitor(gameId, minutes);
        auto info = monitor.fetchGameInfo();
        monitor.printGameInfoTable(info);
        monitor.startMonitoring(); // No prefix, default color (cyan)
        // Print logs after monitoring
        const auto& logs = monitor.getLogLines();
        for (const auto& line : logs) {
            setColor(11);
            std::cout << line << std::endl;
            resetColor();
        }
        monitor.showResults(info.name);

        setColor(10);
        std::cout << "\nMonitoring completed successfully!" << std::endl;
        resetColor();
        system("pause");
        return 0;
    }
}
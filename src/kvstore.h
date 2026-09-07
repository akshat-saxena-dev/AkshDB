#ifndef KVSTORE_H
#define KVSTORE_H

#include <iostream>
#include <string>
#include <utility>
#include <unordered_map>
#include <optional>
#include <sstream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>


class KVStore {
private:
    std::unordered_map<std::string, std::string> data;

    bool transactionActive = false;
    std::unordered_map<std::string, std::string> transactionBackup;

public:
    bool set(const std::string& key, const std::string& value) {
        data[key] = value;

        if (!logOperation("SET", key, value)) {
            std::cerr << "Warning: failed to write SET log\n";
            return false;
        }

        return true;
    }

    std::optional<std::string> get(const std::string &key) const {
        auto it = data.find(key);

        if (it == data.end()) {
            return std::nullopt;
        }

        return it->second;
    }

    bool remove(const std::string& key) {
        auto it = data.find(key);

        if (it == data.end()) {
            return false;
        }

        data.erase(it);

        if (!logOperation("DELETE", key)) {
            std::cerr << "Warning: failed to write DELETE log\n";
            return false;
        }

        return true;
    }

    bool exists(const std::string& key) {
        return data.find(key) != data.end();
    }

    std::vector<std::string> keys() {
        std::vector<std::string> result;

        for (const auto& pair : data) {
            result.push_back(pair.first);
        }

        return result;
    }

    void clear() {
        data.clear();
    }

    std::size_t size() {
        return data.size();
    }

    void stats() const {
        std::cout << "Total keys: " << data.size() << '\n';

        if (data.empty()) {
            std::cout << "Database is empty\n";
        }
        else {
            std::cout << "Database contains data\n";
        }
    }

    bool rename(
        const std::string& oldKey,
        const std::string& newKey
    ) {
        auto it = data.find(oldKey);

        if (it == data.end()) {
            return false;
        }

        if (data.find(newKey) != data.end()) {
            return false;
        }

        std::string value = it->second;

        data.erase(it);
        data[newKey] = value;

        if (!logOperation("RENAME", oldKey, newKey)) {
            std::cerr << "Warning: failed to write RENAME log\n";
            return false;
        }

        return true;
    }

    bool save(const std::string& filename) {
        std::string tempFilename = filename + ".tmp";
        std::string backupFilename = filename + ".bak";

        std::ofstream file(tempFilename);

        if (!file.is_open()) {
            return false;
        }

        int savedCount = 0;

        for (const auto& pair : data) {
            if (pair.first.empty()) {
                continue;
            }

            file << pair.first << '|' << pair.second << '\n';
            savedCount++;
        }

        if (!file.good()) {
            file.close();
            std::remove(tempFilename.c_str());
            return false;
        }

        file.close();

        std::remove(backupFilename.c_str());

        if (std::rename(filename.c_str(), backupFilename.c_str()) != 0) {
            if (std::ifstream(filename).good()) {
                std::remove(tempFilename.c_str());
                return false;
            }
        }

        if (std::rename(tempFilename.c_str(), filename.c_str()) != 0) {
            std::remove(filename.c_str());
            std::rename(backupFilename.c_str(), filename.c_str());
            std::remove(tempFilename.c_str());
            return false;
        }

        std::cout << "Saved entries: " << savedCount << '\n';

        return true;
    }

    bool load(const std::string& filename) {
        std::ifstream file(filename);

        if (!file.is_open()) {
            return false;
        }
        
        std::unordered_map<std::string, std::string> loadedData;

        std::string line;
        int loadedCount = 0;
        int skippedCount = 0;

        while (std::getline(file, line)) {
            std::size_t separator = line.find('|');

            if (separator == std::string::npos) {
                skippedCount++;
                continue;
            }

            std::string key = line.substr(0, separator);
            std::string value = line.substr(separator + 1);

            if (key.empty() || !isValidKey(key)) {
                skippedCount++;
                continue;
            }

            loadedData[key] = value;
            loadedCount++;
        }

        data = std::move(loadedData);

        file.close();

        if (loadedCount == 0 && skippedCount == 0) {
            std::cout << "Database file is empty\n";
        }
        else {
            std::cout << "Loaded entries: " << loadedCount << '\n';

            if (skippedCount > 0) {
                std::cout << "Skipped invalid entries: "
                        << skippedCount << '\n';
            }
        }

        return true;
    }

    bool isValidKey(const std::string& key) {
        if (key.empty()) {
            return false;
        }

        if (key.find('|') != std::string::npos) {
            return false;
        }

        return true;
    }

    bool beginTransaction() {
        if (transactionActive) {
            return false;
        }

        transactionBackup = data;
        transactionActive = true;

        if (!logOperation("BEGIN", "")) {
            transactionBackup.clear();
            transactionActive = false;
            return false;
        }

        return true;
    }

    bool rollback() {
        if (!transactionActive) {
            return false;
        }

        data = transactionBackup;
        transactionBackup.clear();
        transactionActive = false;

        if (!logOperation("ROLLBACK", "")) {
            std::cerr << "Warning: failed to write ROLLBACK log\n";
        }

        return true;
    }

    bool commit() {
        if (!transactionActive) {
            return false;
        }

        transactionBackup.clear();
        transactionActive = false;

        if (!logOperation("COMMIT", "")) {
            std::cerr << "Warning: failed to write COMMIT log\n";
        }

        return true;
    }

    bool isTransactionActive() const {
        return transactionActive;
    }

    bool logOperation(
        const std::string& operation,
        const std::string& key,
        const std::string& value = ""
    ) {
        std::ofstream logFile("akshdb.log", std::ios::app);

        if (!logFile.is_open()) {
            return false;
        }

        auto now = std::chrono::system_clock::now();
        std::time_t currentTime =
            std::chrono::system_clock::to_time_t(now);

        std::tm localTime{};

    #ifdef _WIN32
        localtime_s(&localTime, &currentTime);
    #else
        localtime_r(&currentTime, &localTime);
    #endif

        logFile << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
                << '|'
                << operation
                << '|'
                << key
                << '|'
                << value
                << '\n';

        if (!logFile) {
            return false;
        }

        logFile.flush();

        return static_cast<bool>(logFile);
    }
};

#endif
#ifndef KVSTORE_H
#define KVSTORE_H

#include <iostream>
#include <string>
#include <utility>
#include <unordered_map>
#include <tuple>
#include <optional>
#include <sstream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <chrono>
#include <shared_mutex>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <sstream>


class KVStore {
private:
    std::unordered_map<std::string, std::string> data;
    mutable std::shared_mutex mutex;

    bool persistenceEnabled = true;

    bool transactionActive = false;
    std::unordered_map<std::string, std::string> transactionBackup;

public:
    KVStore(bool enablePersistence = true)
        : persistenceEnabled(enablePersistence) {}

    bool set(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> lock(mutex);

        if (persistenceEnabled) {
            if (!writeWAL("SET", key, value)) {
                std::cerr << "Error: failed to write WAL\n";
                return false;
            }
        }

        data[key] = value;

        if (persistenceEnabled) {
            if (!logOperation("SET", key, value)) {
                std::cerr << "Warning: failed to write operation log\n";
            }
        }

        return true;
    }

    std::optional<std::string> get(const std::string &key) const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        auto it = data.find(key);

        if (it == data.end()) {
            return std::nullopt;
        }

        return it->second;
    }

    bool remove(const std::string& key) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        auto it = data.find(key);

        if (it == data.end()) {
            return false;
        }

        if (persistenceEnabled) {
            if (!writeWAL("DELETE", key)) {
                std::cerr << "Error: failed to write WAL\n";
                return false;
            }
        }

        data.erase(it);

        if (persistenceEnabled) {
            if (!logOperation("DELETE", key)) {
                std::cerr << "Warning: failed to write operation log\n";
            }
        }

        return true;
    }

    bool exists(const std::string& key) {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return data.find(key) != data.end();
    }

    std::vector<std::string> keys() {
        std::shared_lock<std::shared_mutex> lock(mutex);
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
        std::shared_lock<std::shared_mutex> lock(mutex);
        return data.size();
    }

    void stats() const {
        std::shared_lock<std::shared_mutex> lock(mutex);

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
        std::unique_lock<std::shared_mutex> lock(mutex);
        auto it = data.find(oldKey);

        if (it == data.end()) {
            return false;
        }

        if (data.find(newKey) != data.end()) {
            return false;
        }

        std::string value = it->second;

        if (persistenceEnabled) {
            if (!writeWAL("RENAME", oldKey, newKey)) {
                std::cerr << "Error: failed to write WAL\n";
                return false;
            }
        }

        data.erase(it);
        data[newKey] = value;

        if (persistenceEnabled) {
            if (!logOperation("RENAME", oldKey, newKey)) {
                std::cerr << "Warning: failed to write operation log\n";
            }
        }

        return true;
    }

    bool save(const std::string& filename) {
        std::unordered_map<std::string, std::string> snapshot;

        {
            std::shared_lock<std::shared_mutex> lock(mutex);
            snapshot = data;
        }

        std::string tempFilename = filename + ".tmp";
        std::string backupFilename = filename + ".bak";

        std::ofstream file(tempFilename);

        if (!file.is_open()) {
            return false;
        }

        int savedCount = 0;

        for (const auto& pair : snapshot) {
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

        if (!clearWAL()) {
            std::cerr << "Warning: failed to clear WAL\n";
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

        {
            std::unique_lock<std::shared_mutex> lock(mutex);
            data = loadedData;
        }

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
        std::unique_lock<std::shared_mutex> lock(mutex);

        if (transactionActive) {
            return false;
        }

        if (!writeWAL("BEGIN", "")) {
            std::cerr << "Error: failed to write WAL\n";
            return false;
        }

        transactionBackup = data;
        transactionActive = true;

        if (!logOperation("BEGIN", "")) {
            std::cerr << "Warning: failed to write operation log\n";
        }

        return true;
    }

    bool rollback() {
        std::unique_lock<std::shared_mutex> lock(mutex);

        if (!transactionActive) {
            return false;
        }

        if (!writeWAL("ROLLBACK", "")) {
            std::cerr << "Error: failed to write WAL\n";
            return false;
        }

        data = transactionBackup;

        transactionBackup.clear();
        transactionActive = false;

        if (!logOperation("ROLLBACK", "")) {
            std::cerr << "Warning: failed to write operation log\n";
        }

        return true;
    }

    bool commit() {
        std::unique_lock<std::shared_mutex> lock(mutex);

        if (!transactionActive) {
            return false;
        }

        if (!writeWAL("COMMIT", "")) {
            std::cerr << "Error: failed to write WAL\n";
            return false;
        }

        transactionBackup.clear();
        transactionActive = false;

        if (!logOperation("COMMIT", "")) {
            std::cerr << "Warning: failed to write operation log\n";
        }

        return true;
    }

    bool isTransactionActive() const {
        std::shared_lock<std::shared_mutex> lock(mutex);

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

    bool writeWAL(
        const std::string& operation,
        const std::string& key,
        const std::string& value = ""
    ) {
        std::ofstream wal("akshdb.wal", std::ios::app);

        if (!wal.is_open()) {
            return false;
        }

        wal << operation << '|'
            << key << '|'
            << value << '\n';

        wal.flush();

        return static_cast<bool>(wal);
    }

    bool clearWAL() {
        std::ofstream wal("akshdb.wal", std::ios::trunc);

        if (!wal.is_open()) {
            return false;
        }

        wal.flush();

        return static_cast<bool>(wal);
    }

    bool replayWAL(const std::string& filename = "akshdb.wal") {
        std::ifstream wal(filename);

        if (!wal.is_open()) {
            return true;
        }

        std::unique_lock<std::shared_mutex> lock(mutex);

        std::string line;

        bool transactionActive = false;

        std::vector<std::tuple<std::string, std::string, std::string>>
            transactionOperations;

        int replayedCount = 0;

        while (std::getline(wal, line)) {
            if (line.empty()) {
                continue;
            }

            std::stringstream ss(line);

            std::string operation;
            std::string key;
            std::string value;

            std::getline(ss, operation, '|');
            std::getline(ss, key, '|');
            std::getline(ss, value);

            if (operation.empty()) {
                continue;
            }

            if (operation == "BEGIN") {
                if (transactionActive) {
                    continue;
                }

                transactionActive = true;
                transactionOperations.clear();
                continue;
            }

            if (operation == "COMMIT") {
                if (!transactionActive) {
                    continue;
                }

                for (const auto& op : transactionOperations) {
                    const auto& opType = std::get<0>(op);
                    const auto& opKey = std::get<1>(op);
                    const auto& opValue = std::get<2>(op);

                    if (opType == "SET") {
                        data[opKey] = opValue;
                        replayedCount++;
                    }
                    else if (opType == "DELETE") {
                        data.erase(opKey);
                        replayedCount++;
                    }
                    else if (opType == "RENAME") {
                        auto it = data.find(opKey);

                        if (it == data.end()) {
                            continue;
                        }

                        if (data.find(opValue) != data.end()) {
                            continue;
                        }

                        std::string storedValue = it->second;

                        data.erase(it);
                        data[opValue] = storedValue;

                        replayedCount++;
                    }
                }

                transactionOperations.clear();
                transactionActive = false;

                continue;
            }

            if (operation == "ROLLBACK") {
                transactionOperations.clear();
                transactionActive = false;
                continue;
            }

            if (transactionActive) {
                if (operation != "SET" &&
                    operation != "DELETE" &&
                    operation != "RENAME") {
                    continue;
                }

                if (key.empty() || !isValidKey(key)) {
                    continue;
                }

                if (operation == "RENAME" &&
                    (value.empty() || !isValidKey(value))) {
                    continue;
                }

                transactionOperations.emplace_back(
                    operation,
                    key,
                    value
                );

                continue;
            }

            if (operation == "SET") {
                if (key.empty() || !isValidKey(key)) {
                    continue;
                }

                data[key] = value;
                replayedCount++;
            }
            else if (operation == "DELETE") {
                if (key.empty() || !isValidKey(key)) {
                    continue;
                }

                data.erase(key);
                replayedCount++;
            }
            else if (operation == "RENAME") {
                if (key.empty() ||
                    value.empty() ||
                    !isValidKey(key) ||
                    !isValidKey(value)) {
                    continue;
                }

                auto it = data.find(key);

                if (it == data.end()) {
                    continue;
                }

                if (data.find(value) != data.end()) {
                    continue;
                }

                std::string storedValue = it->second;

                data.erase(it);
                data[value] = storedValue;

                replayedCount++;
            }
        }

        /*
            If the WAL ends while a transaction is active,
            the transaction was never committed.
            Therefore, discard its operations.
        */

        transactionOperations.clear();
        transactionActive = false;

        std::cout << "Replayed WAL entries: "
                << replayedCount << '\n';

        return true;
    }
};

#endif
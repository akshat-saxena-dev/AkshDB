#include "kvstore.h"

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>

struct Command {
    std::string name;
    std::string key;
    std::string value;
};

Command parseCommand(const std::string& line) {
    std::istringstream iss(line);

    Command command;

    iss >> command.name;
    iss >> command.key;

    std::getline(iss, command.value);

    // Remove the first space before the value.
    if (!command.value.empty() && command.value[0] == ' ') {
        command.value.erase(0, 1);
    }

    // Convert command name to uppercase.
    std::transform(
        command.name.begin(),
        command.name.end(),
        command.name.begin(),
        [](unsigned char ch) {
            return std::toupper(ch);
        }
    );

    return command;
}

void printHelp() {
    std::cout << "Available commands:\n";
    std::cout << "SET <key> <value>\n";
    std::cout << "GET <key>\n";
    std::cout << "DELETE <key>\n";
    std::cout << "DEL <key>\n";
    std::cout << "EXISTS <key>\n";
    std::cout << "KEYS\n";
    std::cout << "SIZE\n";
    std::cout << "CLEAR\n";
    std::cout << "STATS\n";
    std::cout << "INFO\n";
    std::cout << "SAVE\n";
    std::cout << "LOAD\n";
    std::cout << "RENAME <oldKey> <newKey>\n";
    std::cout << "BEGIN\n";
    std::cout << "COMMIT\n";
    std::cout << "ROLLBACK\n";
    std::cout << "VERISON\n";
    std::cout << "HELP\n";
    std::cout << "EXIT\n";
    std::cout << "QUIT\n";
}

void processCommand(
    KVStore& db,
    const std::string& line,
    bool& shouldExit
) {
    Command parsed = parseCommand(line);

    std::string command = parsed.name;
    std::string key = parsed.key;
    std::string value = parsed.value;

    if (command.empty()) {
        std::cout << "Empty command\n";
        return;
    }

    /*
        EXIT and QUIT
    */
    if (command == "EXIT" || command == "QUIT") {
        if (db.isTransactionActive()) {
            db.rollback();
            std::cout << "Active transaction rolled back\n";
        }

        if (db.save("akshdb.data")) {
            std::cout << "Database saved\n";
        }
        else {
            std::cout << "Failed to save database\n";
        }

        shouldExit = true;
        return;
    }

    /*
        SET
    */
    if (command == "SET") {
        if (key.empty() || value.empty()) {
            std::cout << "Usage: SET <key> <value>\n";
        }
        else if (!db.isValidKey(key)) {
            std::cout << "Invalid key\n";
        }
        else {
            if (db.set(key, value)) {
                std::cout << "OK\n";
            } else {
                std::cout << "Operation completed, but logging failed\n";
            }
        }

        return;
    }

    /*
        GET
    */
    if (command == "GET") {
        if (key.empty()) {
            std::cout << "Usage: GET <key>\n";
        }
        else if (!db.isValidKey(key)) {
            std::cout << "Invalid key\n";
        }
        else {
            auto result = db.get(key);

            if (result.has_value()) {
                std::cout << result.value() << '\n';
            }
            else {
                std::cout << "Key not found\n";
            }
        }

        return;
    }

    /*
        DELETE and DEL
    */
    if (command == "DELETE" || command == "DEL") {
        if (key.empty()) {
            std::cout << "Usage: DELETE <key>\n";
        }
        else if (!db.isValidKey(key)) {
            std::cout << "Invalid key\n";
        }
        else if (db.remove(key)) {
            std::cout << "Deleted\n";
        }
        else {
            std::cout << "Key not found\n";
        }

        return;
    }

    /*
        EXISTS
    */
    if (command == "EXISTS") {
        if (key.empty()) {
            std::cout << "Usage: EXISTS <key>\n";
        }
        else if (!db.isValidKey(key)) {
            std::cout << "Invalid key\n";
        }
        else {
            std::cout << std::boolalpha
                      << db.exists(key)
                      << '\n';
        }

        return;
    }

    /*
        RENAME
    */
    if (command == "RENAME") {
        std::istringstream renameStream(line);

        std::string renameCommand;
        std::string oldKey;
        std::string newKey;

        renameStream >> renameCommand >> oldKey >> newKey;

        if (oldKey.empty() || newKey.empty()) {
            std::cout << "Usage: RENAME <oldKey> <newKey>\n";
        }
        else if (!db.isValidKey(oldKey) ||
                 !db.isValidKey(newKey)) {
            std::cout << "Invalid key\n";
        }
        else if (!db.exists(oldKey)) {
            std::cout << "Old key not found\n";
        }
        else if (db.exists(newKey)) {
            std::cout << "New key already exists\n";
        }
        else if (db.rename(oldKey, newKey)) {
            std::cout << "OK\n";
        }
        else {
            std::cout << "Rename failed\n";
        }

        return;
    }

    /*
        KEYS
    */
    if (command == "KEYS") {
        auto allKeys = db.keys();

        for (const auto& currentKey : allKeys) {
            std::cout << currentKey << '\n';
        }

        return;
    }

    /*
        CLEAR
    */
    if (command == "CLEAR") {
        db.clear();
        std::cout << "OK\n";
        return;
    }

    /*
        SIZE
    */
    if (command == "SIZE") {
        std::cout << db.size() << '\n';
        return;
    }

    /*
        STATS
    */
    if (command == "STATS") {
        db.stats();
        return;
    }

    /*
        INFO
    */
    if (command == "INFO") {
        std::cout << "Database size: "
                  << db.size()
                  << '\n';

        if (db.isTransactionActive()) {
            std::cout << "Transaction: active\n";
        }
        else {
            std::cout << "Transaction: inactive\n";
        }

        return;
    }

    /*
        SAVE
    */
    if (command == "SAVE") {
        if (db.isTransactionActive()) {
            std::cout << "Cannot save during an active transaction\n";
        }
        else if (db.save("akshdb.data")) {
            std::cout << "Database saved\n";
        }
        else {
            std::cout << "Failed to save database\n";
        }

        return;
    }

    /*
        LOAD
    */
    if (command == "LOAD") {
        if (db.isTransactionActive()) {
            std::cout << "Cannot load during an active transaction\n";
        }
        else if (db.load("akshdb.data")) {
            std::cout << "Database loaded\n";
        }
        else {
            std::cout << "Failed to load database\n";
        }

        return;
    }

    /*
        BEGIN
    */
    if (command == "BEGIN") {
        if (db.beginTransaction()) {
            std::cout << "Transaction started\n";
        }
        else {
            std::cout << "Transaction already active\n";
        }

        return;
    }

    /*
        COMMIT
    */
    if (command == "COMMIT") {
        if (db.commit()) {
            std::cout << "Transaction committed\n";
        }
        else {
            std::cout << "No active transaction\n";
        }

        return;
    }

    /*
        ROLLBACK
    */
    if (command == "ROLLBACK") {
        if (db.rollback()) {
            std::cout << "Transaction rolled back\n";
        }
        else {
            std::cout << "No active transaction\n";
        }

        return;
    }

    /*
        VERSION
    */

    if (command == "VERSION") {
        std::cout << "AkshDB version 1.0\n";
        return;
    }

    /*
        HELP
    */
    if (command == "HELP") {
        printHelp();
        return;
    }

    /*
        Unknown command
    */
    std::cout << "Unknown command\n";
}

int main() {
    KVStore db;

    if (db.load("akshdb.data")) {
        std::cout << "Previous database loaded\n";
    }
    else if (db.load("akshdb.data.bak")) {
        std::cout << "Backup database loaded\n";
    }
    else {
        std::cout << "Starting with an empty database\n";
    }

    std::string line;
    bool shouldExit = false;

    while (!shouldExit) {
        std::cout << "> ";

        if (!std::getline(std::cin, line)) {
            break;
        }

        processCommand(db, line, shouldExit);
    }

    return 0;
}
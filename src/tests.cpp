#include "kvstore.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdio>

void testSafeLoad() {
    KVStore db;

    db.set("name", "Akshat");
    db.set("city", "Lucknow");

    std::ofstream file("safe_load_test.data");
    file << "validKey|validValue\n";
    file << "invalidLineWithoutSeparator\n";
    file.close();

    bool loaded = db.load("safe_load_test.data");

    assert(loaded);
    assert(db.exists("validKey"));
    assert(db.get("validKey").value() == "validValue");
    assert(!db.exists("name"));
    assert(!db.exists("city"));

    std::remove("safe_load_test.data");

    std::cout << "Safe load test passed\n";
}

void testTransactions() {
    KVStore db;

    db.set("name", "Akshat");

    assert(db.beginTransaction());
    assert(db.isTransactionActive());

    db.set("name", "Temporary");
    assert(db.get("name").value() == "Temporary");

    assert(db.rollback());
    assert(!db.isTransactionActive());
    assert(db.get("name").value() == "Akshat");

    assert(db.beginTransaction());

    db.set("name", "Permanent");

    assert(db.commit());
    assert(!db.isTransactionActive());
    assert(db.get("name").value() == "Permanent");

    std::cout << "Transaction test passed\n";
}

int main() {
    KVStore db;

    // SET and GET
    db.set("name", "Akshat");
    assert(db.get("name").value() == "Akshat");

    // Update existing key
    db.set("name", "Akshat Saxena");
    assert(db.get("name").value() == "Akshat Saxena");

    // EXISTS
    assert(db.exists("name"));
    assert(!db.exists("city"));

    // SIZE
    assert(db.size() == 1);

    // RENAME
    assert(db.rename("name", "username"));
    assert(db.exists("username"));
    assert(!db.exists("name"));
    assert(db.get("username").value() == "Akshat Saxena");

    // DELETE
    assert(db.remove("username"));
    assert(!db.exists("username"));

    // CLEAR
    db.set("a", "1");
    db.set("b", "2");
    assert(db.size() == 2);

    db.clear();
    assert(db.size() == 0);

    // Invalid key
    assert(!db.isValidKey(""));
    assert(!db.isValidKey("hello|world"));
    assert(db.isValidKey("hello"));

    // SAVE and LOAD
    const std::string testFile = "test_database.data";

    KVStore saveDb;
    saveDb.set("name", "Akshat");
    saveDb.set("city", "Varanasi");

    assert(saveDb.save(testFile));

    KVStore loadDb;
    assert(loadDb.load(testFile));

    assert(loadDb.get("name").value() == "Akshat");
    assert(loadDb.get("city").value() == "Varanasi");
    assert(loadDb.size() == 2);

    // Clean up the test file
    std::remove(testFile.c_str());


    // Invalid file handling
    const std::string invalidFile = "invalid_database.data";

    {
        std::ofstream file(invalidFile);

        file << "valid|entry\n";
        file << "invalid line without separator\n";
        file << "|missing_key\n";
        file << "another|valid entry\n";
    }

    KVStore invalidDb;
    assert(invalidDb.load(invalidFile));

    assert(invalidDb.get("valid").value() == "entry");
    assert(invalidDb.get("another").value() == "valid entry");
    assert(invalidDb.size() == 2);

    std::remove(invalidFile.c_str());


    // Backup recovery test
    const std::string recoveryFile = "recovery_database.data";
    const std::string recoveryBackup = recoveryFile + ".bak";

    {
        std::ofstream file(recoveryBackup);

        file << "name|Akshat\n";
        file << "city|Varanasi\n";
    }

    KVStore recoveryDb;

    assert(!recoveryDb.load(recoveryFile));
    assert(recoveryDb.load(recoveryBackup));

    assert(recoveryDb.get("name").value() == "Akshat");
    assert(recoveryDb.get("city").value() == "Varanasi");
    assert(recoveryDb.size() == 2);

    std::remove(recoveryFile.c_str());
    std::remove(recoveryBackup.c_str());

    testSafeLoad();
    testTransactions();

    std::cout << "All tests passed\n";

    return 0;
}
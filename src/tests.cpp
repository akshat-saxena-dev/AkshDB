#include "kvstore.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <thread>
#include <filesystem>
#include <vector>

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

void testWALSetRecovery() {
    const std::string filename = "test_set.wal";

    std::remove(filename.c_str());

    {
        std::ofstream wal(filename);

        assert(wal.is_open());

        wal << "SET|name|Akshat\n";
    }

    KVStore db;

    assert(db.replayWAL(filename));

    auto value = db.get("name");

    assert(value.has_value());
    assert(value.value() == "Akshat");

    std::remove(filename.c_str());

    std::cout << "WAL SET recovery test passed\n";
}

void testWALDeleteRecovery() {
    const std::string filename = "test_delete.wal";

    std::remove(filename.c_str());

    {
        std::ofstream wal(filename);

        assert(wal.is_open());

        wal << "SET|name|Akshat\n";
        wal << "DELETE|name|\n";
    }

    KVStore db;

    assert(db.replayWAL(filename));

    assert(!db.exists("name"));

    std::remove(filename.c_str());

    std::cout << "WAL DELETE recovery test passed\n";
}

void testWALRenameRecovery() {
    const std::string filename = "test_rename.wal";

    std::remove(filename.c_str());

    {
        std::ofstream wal(filename);

        assert(wal.is_open());

        wal << "SET|name|Akshat\n";
        wal << "RENAME|name|username\n";
    }

    KVStore db;

    assert(db.replayWAL(filename));

    assert(!db.exists("name"));

    auto value = db.get("username");

    assert(value.has_value());
    assert(value.value() == "Akshat");

    std::remove(filename.c_str());

    std::cout << "WAL RENAME recovery test passed\n";
}

void testWALCommittedTransactionRecovery() {
    const std::string filename = "test_transaction.wal";

    std::remove(filename.c_str());

    {
        std::ofstream wal(filename);

        assert(wal.is_open());

        wal << "BEGIN||\n";
        wal << "SET|name|Akshat\n";
        wal << "SET|city|Delhi\n";
        wal << "COMMIT||\n";
    }

    KVStore db;

    assert(db.replayWAL(filename));

    auto name = db.get("name");
    auto city = db.get("city");

    assert(name.has_value());
    assert(city.has_value());

    assert(name.value() == "Akshat");
    assert(city.value() == "Delhi");

    std::remove(filename.c_str());

    std::cout << "WAL committed transaction recovery test passed\n";
}

void testWALIncompleteTransactionRecovery() {
    const std::string filename = "test_incomplete_transaction.wal";

    std::remove(filename.c_str());

    {
        std::ofstream wal(filename);

        assert(wal.is_open());

        wal << "BEGIN||\n";
        wal << "SET|name|Akshat\n";
        wal << "SET|city|Delhi\n";
    }

    KVStore db;

    assert(db.replayWAL(filename));

    assert(!db.exists("name"));
    assert(!db.exists("city"));

    std::remove(filename.c_str());

    std::cout << "WAL incomplete transaction recovery test passed\n";
}

void testWALCheckpoint() {
    const std::string dbFile = "test_checkpoint.data";
    const std::string walFile = "test_checkpoint.wal";

    std::remove(dbFile.c_str());
    std::remove((dbFile + ".bak").c_str());
    std::remove((dbFile + ".tmp").c_str());
    std::remove(walFile.c_str());

    {
        KVStore db;

        assert(db.set("name", "Akshat"));
        assert(db.set("city", "Delhi"));

        assert(db.save(dbFile));

        std::ifstream wal(walFile);
        assert(!wal.is_open() || wal.peek() == std::ifstream::traits_type::eof());
    }

    {
        KVStore db;

        assert(db.load(dbFile));
        assert(db.replayWAL(walFile));

        auto name = db.get("name");
        auto city = db.get("city");

        assert(name.has_value());
        assert(city.has_value());

        assert(name.value() == "Akshat");
        assert(city.value() == "Delhi");
    }

    std::remove(dbFile.c_str());
    std::remove((dbFile + ".bak").c_str());
    std::remove((dbFile + ".tmp").c_str());
    std::remove(walFile.c_str());

    std::cout << "WAL checkpoint test passed\n";
}

void testConcurrentSet() {
    KVStore db;

    const int threadCount = 8;
    const int operationsPerThread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < threadCount; t++) {
        threads.emplace_back([&db, t]() {
            for (int i = 0; i < operationsPerThread; i++) {
                std::string key =
                    "thread_" + std::to_string(t) +
                    "_key_" + std::to_string(i);

                std::string value =
                    "value_" + std::to_string(i);

                assert(db.set(key, value));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    assert(db.size() == threadCount * operationsPerThread);

    std::cout << "Concurrent SET test passed\n";
}

void testConcurrentReadWrite() {
    KVStore db;

    const int writerCount = 4;
    const int readerCount = 4;
    const int operationsPerThread = 1000;

    std::vector<std::thread> threads;

    // Writers
    for (int t = 0; t < writerCount; t++) {
        threads.emplace_back([&db, t]() {
            for (int i = 0; i < operationsPerThread; i++) {
                std::string key =
                    "key_" + std::to_string(t) +
                    "_" + std::to_string(i);

                db.set(key, "value");
            }
        });
    }

    // Readers
    for (int t = 0; t < readerCount; t++) {
        threads.emplace_back([&db]() {
            for (int i = 0; i < operationsPerThread; i++) {
                db.get("key_0_" + std::to_string(i));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    assert(db.size() == writerCount * operationsPerThread);

    std::cout << "Concurrent read/write test passed\n";
}

void testConcurrentDelete() {
    KVStore db;

    const int threadCount = 4;
    const int operationsPerThread = 250;

    for (int t = 0; t < threadCount; t++) {
        for (int i = 0; i < operationsPerThread; i++) {
            std::string key =
                "key_" + std::to_string(t) +
                "_" + std::to_string(i);

            assert(db.set(key, "value"));
        }
    }

    assert(db.size() == threadCount * operationsPerThread);

    std::vector<std::thread> threads;

    for (int t = 0; t < threadCount; t++) {
        threads.emplace_back([&db, t]() {
            for (int i = 0; i < operationsPerThread; i++) {
                std::string key =
                    "key_" + std::to_string(t) +
                    "_" + std::to_string(i);

                assert(db.remove(key));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    assert(db.size() == 0);

    std::cout << "Concurrent DELETE test passed\n";
}

void testConcurrentRename() {
    KVStore db;

    const int threadCount = 4;
    const int operationsPerThread = 250;

    for (int t = 0; t < threadCount; t++) {
        for (int i = 0; i < operationsPerThread; i++) {
            std::string key =
                "old_" + std::to_string(t) +
                "_" + std::to_string(i);

            assert(db.set(key, "value"));
        }
    }

    assert(db.size() == threadCount * operationsPerThread);

    std::vector<std::thread> threads;

    for (int t = 0; t < threadCount; t++) {
        threads.emplace_back([&db, t]() {
            for (int i = 0; i < operationsPerThread; i++) {
                std::string oldKey =
                    "old_" + std::to_string(t) +
                    "_" + std::to_string(i);

                std::string newKey =
                    "new_" + std::to_string(t) +
                    "_" + std::to_string(i);

                assert(db.rename(oldKey, newKey));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    assert(db.size() == threadCount * operationsPerThread);

    for (int t = 0; t < threadCount; t++) {
        for (int i = 0; i < operationsPerThread; i++) {
            std::string oldKey =
                "old_" + std::to_string(t) +
                "_" + std::to_string(i);

            std::string newKey =
                "new_" + std::to_string(t) +
                "_" + std::to_string(i);

            assert(!db.exists(oldKey));
            assert(db.exists(newKey));
        }
    }

    std::cout << "Concurrent RENAME test passed\n";
}

void testWALAutoCheckpoint() {
    KVStore db(true, 1024); // 1 KB WAL threshold

    std::string value(256, 'x');

    for (int i = 0; i < 10; i++) {
        db.set("checkpoint_key_" + std::to_string(i), value);
    }

    std::ifstream wal("akshdb.wal", std::ios::ate | std::ios::binary);

    if (!wal.is_open()) {
        std::cout << "WAL auto-checkpoint test passed\n";
        return;
    }

    size_t walSize = static_cast<size_t>(wal.tellg());

    if (walSize >= 1024) {
        std::cerr << "WAL auto-checkpoint test failed\n";
        return;
    }

    KVStore recovered(false);

    if (!recovered.load("akshdb.data")) {
        std::cerr << "WAL auto-checkpoint recovery test failed\n";
        return;
    }

    recovered.replayWAL();

    if (recovered.size() != 10) {
        std::cerr << "WAL auto-checkpoint recovery test failed\n";
        return;
    }

    std::cout << "WAL auto-checkpoint test passed\n";
}

void testCustomDataDirectory()
{
    const std::string directory = "test_storage";

    std::filesystem::remove_all(directory);

    {
        KVStore db(true, 1024, directory);

        assert(db.set("name", "Akshat"));
        assert(db.set("city", "Delhi"));
        assert(db.save(directory + "/akshdb.data"));
    }

    assert(std::filesystem::exists(directory));
    assert(std::filesystem::exists(directory + "/akshdb.data"));
    assert(std::filesystem::exists(directory + "/akshdb.log"));
    assert(std::filesystem::exists(directory + "/akshdb.wal"));

    KVStore recovered(false, 1024, directory);

    assert(recovered.load(directory + "/akshdb.data"));
    assert(recovered.get("name").value() == "Akshat");
    assert(recovered.get("city").value() == "Delhi");

    std::filesystem::remove_all(directory);

    std::cout << "Custom data directory test passed\n";
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
    testWALSetRecovery();
    testWALDeleteRecovery();
    testWALRenameRecovery();
    testWALCommittedTransactionRecovery();
    testWALIncompleteTransactionRecovery();
    testWALCheckpoint();
    testConcurrentSet();
    testConcurrentReadWrite();
    testConcurrentDelete();
    testConcurrentRename();
    testWALAutoCheckpoint();
    testCustomDataDirectory();

    std::cout << "All tests passed\n";

    return 0;
}
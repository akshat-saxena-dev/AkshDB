# AkshDB

AkshDB is a lightweight key-value storage engine written in modern C++.

The project is being developed incrementally to explore practical systems
programming and database internals, including persistence, transactions,
crash recovery, concurrency, testing, and performance optimization.

> Current version: **1.0 — In-memory key-value store with persistence and transactions**

## Features

- Set and update key-value pairs
- Get values by key
- Delete keys
- Check whether a key exists
- List all keys
- Check database size
- Clear the database
- Display database statistics
- Display database information
- Rename keys
- Prevent duplicate-key renaming
- Validate keys
- Save data to disk
- Load data from disk
- Temporary-file based saving
- Backup file creation
- Backup recovery during startup
- Safe loading through a temporary in-memory map
- Skip malformed records during loading
- Operation logging
- Transactions with `BEGIN`, `COMMIT`, and `ROLLBACK`
- Block `SAVE` and `LOAD` during active transactions
- Automatically roll back active transactions on exit
- Interactive command-line interface
- Case-insensitive commands
- Automated tests for core functionality

## Supported Commands

| Command | Description |
|---|---|
| `SET <key> <value>` | Insert or update a key-value pair |
| `GET <key>` | Retrieve the value associated with a key |
| `DELETE <key>` | Delete a key |
| `DEL <key>` | Alias for `DELETE` |
| `EXISTS <key>` | Check whether a key exists |
| `KEYS` | Display all stored keys |
| `SIZE` | Display the number of stored keys |
| `CLEAR` | Remove all key-value pairs |
| `STATS` | Display basic database statistics |
| `INFO` | Display database size and transaction status |
| `SAVE` | Persist the database to disk |
| `LOAD` | Load the database from disk |
| `RENAME <oldKey> <newKey>` | Rename an existing key |
| `BEGIN` | Start a transaction |
| `COMMIT` | Commit the active transaction |
| `ROLLBACK` | Restore the state from before the transaction |
| `HELP` | Display available commands |
| `VERSION` | Display the current AkshDB version |
| `EXIT` | Save and exit the database |
| `QUIT` | Alias for `EXIT` |

## Example

```text
> SET name Akshat
OK

> SET city Lucknow
OK

> GET name
Akshat

> EXISTS city
true

> RENAME city location
Key renamed

> BEGIN
Transaction started

> SET name Temporary
OK

> GET name
Temporary

> ROLLBACK
Transaction rolled back

> GET name
Akshat

> SAVE
Saved entries: 2
Database saved
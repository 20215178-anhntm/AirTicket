#include <iostream>
#include <sqlite3.h>

using namespace std;

static int callback(void *data, int argc, char **argv, char **azColName)
{
    for (int i = 0; i < argc; i++)
    {
        cout << azColName[i] << ": " << (argv[i] ? argv[i] : "NULL") << endl;
    }
    cout << endl;
    return 0;
}

void viewFlightsTable(sqlite3 *db)
{
    char *errMsg = nullptr;
    const char *sql = "SELECT * FROM Flights;"; // Querying the Flights table
    if (sqlite3_exec(db, sql, callback, 0, &errMsg) != SQLITE_OK)
    {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
}

void viewUsertable(sqlite3 *db)
{
    char *errMsg = nullptr;
    const char *sql = "SELECT * FROM Users;";
    if (sqlite3_exec(db, sql, callback, 0, &errMsg) != SQLITE_OK)
    {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
}

void viewTicketstable(sqlite3 *db)
{
    char *errMsg = nullptr;
    const char *sql = "SELECT * FROM Tickets;";
    if (sqlite3_exec(db, sql, callback, 0, &errMsg) != SQLITE_OK)
    {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
}

void viewNotificationstable(sqlite3 *db)
{
    char *errMsg = nullptr;
    const char *sql = "SELECT * FROM Notifications;";
    if (sqlite3_exec(db, sql, callback, 0, &errMsg) != SQLITE_OK)
    {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
}
int main()
{
    sqlite3 *db;
    int rc;

    rc = sqlite3_open("air_ticket.db", &db);
    if (rc)
    {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return 1;
    }

    cout << "Contents of Flights table:" << endl;
    viewFlightsTable(db);
    cout << "\n";
    cout << "Contents of Users table:" << endl;
    viewUsertable(db);
    cout << "\n";
    cout << "Contents of Tickets table:" << endl;
    viewTicketstable(db);
    cout << "\n";
    cout << "Contents of Notifications table:" << endl;
    viewNotificationstable(db);

    sqlite3_close(db);
    return 0;
}

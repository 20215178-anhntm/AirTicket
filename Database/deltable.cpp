
#include <iostream>
#include <sqlite3.h>

using namespace std;

bool deleteFlight(sqlite3 *db, const int &del)
{
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM Flights WHERE number_of_passenger = ?;";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    sqlite3_bind_int(stmt, 1, del);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    cout << "Flights with " << del << " passengers deleted successfully." << endl;
    return true;
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

    int del = 170;
    if (!deleteFlight(db, del))
    {
        cerr << "Failed to delete flight." << endl;
    }

    sqlite3_close(db);
    return 0;
}

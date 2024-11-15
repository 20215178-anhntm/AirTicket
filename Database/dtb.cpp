#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <tuple>

using namespace std;

int main()
{
    sqlite3 *conn;
    int rc;

    rc = sqlite3_open("air_ticket.db", &conn);
    if (rc)
    {
        cerr << "Can't open database: " << sqlite3_errmsg(conn) << endl;
        return 1;
    }

    sqlite3_stmt *stmt;

    const char *createFlightsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS Flights (
            company VARCHAR(20),
            flight_num VARCHAR(20) PRIMARY KEY,
            seat_class_A INT,
            seat_class_B INT,
            price_A INT,
            price_B INT,
            departure_point VARCHAR(50),
            destination_point VARCHAR(50),
            departure_date TEXT,
            return_date TEXT
        )
    )";

    rc = sqlite3_exec(conn, createFlightsTableSQL, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK)
    {
        cerr << "SQL error: " << sqlite3_errmsg(conn) << endl;
        sqlite3_close(conn);
        return 1;
    }

    const char *insertFlightsSQL = "INSERT INTO Flights (company, flight_num, seat_class_A, seat_class_B, price_A, price_B, departure_point, destination_point, departure_date, return_date) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    vector<tuple<string, string, int, int, int, int, string, string, string, string>> flights_data = {
        {"VietJettAir", "ABC123", 49, 100, 300000, 200000, "CaMau", "Vinh", "2023-01-15 08:00", "2023-01-20 19:30"},
        {"BambooAirWay", "DEF456", 80, 119, 350000, 250000, "HaNoi", "HoChiMinh", "2023-02-10 09:45", "2023-02-15 18:15"},
        {"VietNamAirLine", "GHI789", 39, 60, 280000, 180000, "NgheAn", "CaoBang", "2023-03-05 07:30", "2023-03-10 20:00"},
        {"VietNamAirLine", "JKL012", 70, 109, 320000, 220000, "QuyNhon", "HaiPhong", "2023-04-20 06:15", "2023-04-25 21:45"},
        {"VietJettAir", "MNO345", 59, 90, 300000, 200000, "ThanhHoa", "KhanhHoa", "2023-05-12 10:00", "2023-05-18 22:30"},
        {"BambooAirWay", "HJS383", 0, 12, 350000, 120000, "DaNang", "CaoBang", "2023-11-10 10:45", "2024-02-15 20:22"}};

    for (const auto &row : flights_data)
    {
        rc = sqlite3_prepare_v2(conn, insertFlightsSQL, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "SQL error: " << sqlite3_errmsg(conn) << endl;
            sqlite3_close(conn);
            return 1;
        }

        // Bind values to the insert statement
        sqlite3_bind_text(stmt, 1, get<0>(row).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, get<1>(row).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, get<2>(row));
        sqlite3_bind_int(stmt, 4, get<3>(row));
        sqlite3_bind_int(stmt, 5, get<4>(row));
        sqlite3_bind_int(stmt, 6, get<5>(row));
        sqlite3_bind_text(stmt, 7, get<6>(row).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, get<7>(row).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, get<8>(row).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 10, get<9>(row).c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            cerr << "SQL error: " << sqlite3_errmsg(conn) << endl;
            sqlite3_finalize(stmt);
            sqlite3_close(conn);
            return 1;
        }

        sqlite3_finalize(stmt);
    }

    const char *createUsersTableSQL = R"(
        CREATE TABLE IF NOT EXISTS Users (
            user_id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT,
            password TEXT
        )
    )";

    rc = sqlite3_exec(conn, createUsersTableSQL, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK)
    {
        cerr << "SQL error: " << sqlite3_errmsg(conn) << endl;
        sqlite3_close(conn);
        return 1;
    }

    const char *insertUsersSQL = "INSERT INTO Users (user_id, username, password) VALUES (?, ?, ?)";

    vector<tuple<int, string, string>> users_data = {
        {1, "user1", "abc123"},
        {2, "user2", "deg456"},
        {3, "user3", "abc@123"},
        {4, "user4", "456hhh"},
        {5, "user5", "998kkk"}};

    for (const auto &row : users_data)
    {
        rc = sqlite3_prepare_v2(conn, insertUsersSQL, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "SQL error: " << sqlite3_errmsg(conn) << endl;
            sqlite3_close(conn);
            return 1;
        }

        sqlite3_bind_int(stmt, 1, get<0>(row));
        sqlite3_bind_text(stmt, 2, get<1>(row).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, get<2>(row).c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            cerr << "SQL error: " << sqlite3_errmsg(conn) << endl;
            sqlite3_finalize(stmt);
            sqlite3_close(conn);
            return 1;
        }

        sqlite3_finalize(stmt);
    }

    const char *createTicketsTableSQL = R"(
    CREATE TABLE IF NOT EXISTS Tickets (
        ticket_code TEXT,
        user_id INTEGER,
        flight_num VARCHAR(20),
        seat_class TEXT,
        ticket_price INTEGER,
        payment TEXT CHECK(payment IN ('PAID', 'NOT_PAID')),
        FOREIGN KEY (user_id) REFERENCES Users(user_id),
        FOREIGN KEY (flight_num) REFERENCES Flights(flight_num)
    )
)";

    rc = sqlite3_exec(conn, createTicketsTableSQL, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK)
    {
        cerr << "SQL error: " << sqlite3_errmsg(conn) << endl;
        sqlite3_close(conn);
        return 1;
    }

    const char *insertTicketsSQL = "INSERT INTO Tickets (ticket_code, user_id, flight_num, seat_class, ticket_price, payment) VALUES (?, ?, ?, ?, ?, ?)";

    vector<tuple<string, int, string, string, int, string>> tickets_data = {
        {"TCKT123", 1, "ABC123", "A", 300000, "PAID"},
        {"TCKT456", 2, "DEF456", "B", 250000, "NOT_PAID"},
        {"TCKT789", 3, "GHI789", "A", 280000, "PAID"},
        {"TCKT012", 4, "JKL012", "B", 220000, "NOT_PAID"},
        {"TCKT345", 5, "MNO345", "A", 300000, "NOT_PAID"}};

    for (const auto &row : tickets_data)
    {
        rc = sqlite3_prepare_v2(conn, insertTicketsSQL, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            cerr << "SQL error: " << sqlite3_errmsg(conn) << endl;
            sqlite3_close(conn);
            return 1;
        }

        sqlite3_bind_text(stmt, 1, get<0>(row).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, get<1>(row));
        sqlite3_bind_text(stmt, 3, get<2>(row).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, get<3>(row).c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 5, get<4>(row));
        sqlite3_bind_text(stmt, 6, get<5>(row).c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            cerr << "SQL error: " << sqlite3_errmsg(conn) << endl;
            sqlite3_finalize(stmt);
            sqlite3_close(conn);
            return 1;
        }

        sqlite3_finalize(stmt);
    }
    sqlite3_exec(conn, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(conn);

    return 0;
}

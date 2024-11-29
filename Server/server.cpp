#include "server.h"

sqlite3 *db;

int main()
{
    if (sqlite3_config(SQLITE_CONFIG_MULTITHREAD) != SQLITE_OK)
    {
        cerr << "Failed to set SQLite to multi-threaded mode." << endl;
        return 1;
    }
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        cerr << "Error creating server socket" << endl;
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        cerr << "Error binding server socket" << endl;
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) == -1)
    {
        cerr << "Error listening on server socket" << endl;
        close(server_socket);
        return 1;
    }

    std::cout << "Server listening on port " << PORT << "..." << endl;

    while (true)
    {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1)
        {
            cerr << "Error accepting client connection" << endl;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN) == NULL)
        {
            cerr << "Error converting IP address" << endl;
            close(client_socket);
            continue;
        }
        std::cout << "Received request from " << client_ip << endl;
        {
            std::lock_guard<std::mutex> lock(clientNotifMapMutex);
            clientNotifMap[client_socket] = ""; // save client connection
        }
        thread client_thread(connect_client, client_socket); // create a new thread to serve new client
        client_thread.detach(); // ensure that thread can run independently, do not depend on primary thread
    }

    close(server_socket);
    return 0;
}
void connect_client(int client_socket)
{
    char buffer[BUFFER_SIZE];
    int bytes_received;

    if (sqlite3_open("../Database/air_ticket.db", &db) != SQLITE_OK)
    {
        cerr << "Error opening database: " << sqlite3_errmsg(db) << endl;
        close(client_socket);
        return;
    }
    char *errMsg;
    if (sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, 0, &errMsg) != SQLITE_OK)
    {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
    std::cout << "Connected to client" << endl;

    while (true)
    {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            break;
        }
        buffer[bytes_received] = '\0';

        string received(buffer);
        cout << "Received: " << received << "\n";
        if (received == "exit")
        {
            break;
        }

        vector<string> type1 = split(received, '/');

        if (type1[0] == "login")
        {
            log_in(client_socket, type1[1], type1[2]);
        }
        else if (type1[0] == "register")
        {
            register_user(client_socket, type1[1], type1[2]);
        }
    }

    std::cout << "Connection closed" << endl;
    sqlite3_close(db);
    db = nullptr;
    close(client_socket);
}

void log_in(int client_socket, const string &username, const string &password) // Log in function
{
    if (userSocketMap.find(username) != userSocketMap.end())
    {
        std::cout << "Send: N_login1\n"
                  << endl;
        send(client_socket, "N_login1", strlen("N_login1"), 0);
        return;
    }

    if (username == "admin" && password == "1")
    {
        std::cout << "Send: Y_admin -> Admin\n";
        send(client_socket, "Y_admin", strlen("Y_admin"), 0);
        admin_mode(client_socket);
    }
    else
    {
        sqlite3_stmt *stmt;
        string query = "SELECT username, password FROM Users WHERE username = ? AND password = ?";
        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            User user;
            user.username = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            user.password = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            std::cout << "Send: Y_login\n";
            send(client_socket, "Y_login", strlen("Y_login"), 0);
            {
                std::lock_guard<std::mutex> lock(mapMutex);
                userSocketMap[username] = client_socket;
            }
            sqlite3_finalize(stmt);
            functions(client_socket, user);
        }
        else
        {
            std::cout << "Send: N_login" << endl;
            sqlite3_finalize(stmt);
            send(client_socket, "N_login", strlen("N_login"), 0);
        }
    }
}

void register_user(int client_socket, const string &username, const string &password)
{
    sqlite3_stmt *stmt;
    string query = "SELECT username FROM Users WHERE username = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        return;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        std::cout << "Send: N_register" << endl;
        sqlite3_finalize(stmt);
        send(client_socket, "N_register", strlen("N_register"), 0);
    }
    else
    {
        query = "INSERT INTO Users (username, password) VALUES (?, ?)";
        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);

            return;
        }
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            cerr << "Error inserting user data: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
        }
        User newUser;
        newUser.username = username;
        newUser.password = password;
        std::cout << newUser.username << "Y_register" << endl;
        send(client_socket, "Y_register", strlen("Y_register"), 0);
        {
            std::lock_guard<std::mutex> lock(mapMutex);
            userSocketMap[username] = client_socket;
        }
        sqlite3_finalize(stmt);
        functions(client_socket, newUser);
    }
}

void admin_mode(int client_socket)
{
    
}

void functions(int client_socket, const User &user)
{
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while (true)
    {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            break;
        }
        buffer[bytes_received] = '\0';

        string received(buffer);
        cout << "Received: " << received << " from " << user.username << endl;
        if (received == "logout")
        {
            cout << "Send: O_log ->" << user.username << "\n";
            send(client_socket, "O_log", strlen("O_log"), 0);
            {
                std::lock_guard<std::mutex> lock(mapMutex);
                userSocketMap.erase(user.username);
            }
            return;
        }
        vector<string> type1 = split(received, '/');

        if (lower(type1[0]) == "search1")
        {
            search_flight1(client_socket, type1[1], type1[2], type1[3], user);
        }
        if (lower(type1[0]) == "search3")
        {
            search_flight3(client_socket, type1[1], type1[2], type1[3], user);
        }
        if (lower(type1[0]) == "search2")
        {
            search_flight2(client_socket, type1[1], type1[2], type1[3], type1[4], user);
        }
        if (lower(type1[0]) == "search4")
        {
            search_flight4(client_socket, type1[1], type1[2], type1[3], type1[4], type1[5], user);
        }
    }
}

void search_flight1(int client_socket, const string &departure_point, const string &destination_point, const string &departure_date, const User &user)
{
    string msg;
    string noti = checknoti(client_socket);
    sqlite3_stmt *stmt;
    bool found = false;

    string result_str = "Y_found/";
    string query2 =
        "SELECT "
        "F1.*, F2.*"
        "FROM "
        "Flights F1 "
        "JOIN "
        "Flights F2 ON F1.destination_point = F2.departure_point AND F1.departure_date < F2.departure_date "
        "WHERE "
        "F1.departure_point = ? AND F2.destination_point = ? AND F1.departure_date<=?;";

    if (sqlite3_prepare_v2(db, query2.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_found" + noti;
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        sqlite3_finalize(stmt);
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destination_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, departure_date.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;

        Flights flight1;
        flight1.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight1.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight1.num_A = sqlite3_column_int(stmt, 2);
        flight1.num_B = sqlite3_column_int(stmt, 3);
        flight1.price_A = sqlite3_column_int(stmt, 4);
        flight1.price_B = sqlite3_column_int(stmt, 5);
        flight1.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight1.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight1.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight1.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight1.company + "," + flight1.flight_num + "," +
                      std::to_string(flight1.num_A) + "," + std::to_string(flight1.num_B) + "," +
                      std::to_string(flight1.price_A) + " VND," + std::to_string(flight1.price_B) + " VND," +
                      flight1.departure_point + "," + flight1.destination_point + "," +
                      flight1.departure_date + "," + flight1.return_date + ";";

        Flights flight2;
        flight2.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 10));
        flight2.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 11));
        flight2.num_A = sqlite3_column_int(stmt, 12);
        flight2.num_B = sqlite3_column_int(stmt, 13);
        flight2.price_A = sqlite3_column_int(stmt, 14);
        flight2.price_B = sqlite3_column_int(stmt, 15);
        flight2.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 16));
        flight2.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 17));
        flight2.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 18));
        flight2.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 19));

        result_str += flight2.company + "," + flight2.flight_num + "," +
                      std::to_string(flight2.num_A) + "," + std::to_string(flight2.num_B) + "," +
                      std::to_string(flight2.price_A) + " VND," + std::to_string(flight2.price_B) + " VND," +
                      flight2.departure_point + "," + flight2.destination_point + "," +
                      flight2.departure_date + "," + flight2.return_date + ";";
    }

    string query = "SELECT * FROM Flights WHERE departure_point = ? AND destination_point = ? AND departure_date <= ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        msg = "N_found" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destination_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, departure_date.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;
        Flights flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.num_A = sqlite3_column_int(stmt, 2);
        flight.num_B = sqlite3_column_int(stmt, 3);
        flight.price_A = sqlite3_column_int(stmt, 4);
        flight.price_B = sqlite3_column_int(stmt, 5);
        flight.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight.company + ",";
        result_str += flight.flight_num + ",";
        result_str += to_string(flight.num_A) + ",";
        result_str += to_string(flight.num_B) + ",";
        result_str += to_string(flight.price_A) + " VND" + ",";
        result_str += to_string(flight.price_B) + " VND" + ",";
        result_str += flight.departure_point + ",";
        result_str += flight.destination_point + ",";
        result_str += flight.departure_date + ",";
        result_str += flight.return_date + ";";
    }

    if (!found)
    {
        msg = "N_found" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        msg = result_str + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }

    sqlite3_finalize(stmt);
}

void search_flight3(int client_socket, const string &company, const string &departure_point, const string &destination_point, const User &user)
{
    string msg;
    string noti = checknoti(client_socket);
    sqlite3_stmt *stmt;

    bool found = false;

    string result_str = "Y_found/";
    string query2 =
        "SELECT "
        "F1.*, F2.* "
        "FROM "
        "Flights F1 "
        "JOIN "
        "Flights F2 ON F1.destination_point = F2.departure_point AND F1.departure_date < F2.departure_date "
        "WHERE "
        "F1.departure_point = ? AND F2.destination_point = ? AND F1.company=? AND F2.company=?;";

    if (sqlite3_prepare_v2(db, query2.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_found" + noti;
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        sqlite3_finalize(stmt);
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destination_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, company.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, company.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;

        Flights flight1;
        flight1.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight1.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight1.num_A = sqlite3_column_int(stmt, 2);
        flight1.num_B = sqlite3_column_int(stmt, 3);
        flight1.price_A = sqlite3_column_int(stmt, 4);
        flight1.price_B = sqlite3_column_int(stmt, 5);
        flight1.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight1.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight1.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight1.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight1.company + "," + flight1.flight_num + "," +
                      std::to_string(flight1.num_A) + "," + std::to_string(flight1.num_B) + "," +
                      std::to_string(flight1.price_A) + " VND," + std::to_string(flight1.price_B) + " VND," +
                      flight1.departure_point + "," + flight1.destination_point + "," +
                      flight1.departure_date + "," + flight1.return_date + ";";

        Flights flight2;
        flight2.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 10));
        flight2.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 11));
        flight2.num_A = sqlite3_column_int(stmt, 12);
        flight2.num_B = sqlite3_column_int(stmt, 13);
        flight2.price_A = sqlite3_column_int(stmt, 14);
        flight2.price_B = sqlite3_column_int(stmt, 15);
        flight2.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 16));
        flight2.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 17));
        flight2.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 18));
        flight2.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 19));

        result_str += flight2.company + "," + flight2.flight_num + "," +
                      std::to_string(flight2.num_A) + "," + std::to_string(flight2.num_B) + "," +
                      std::to_string(flight2.price_A) + " VND," + std::to_string(flight2.price_B) + " VND," +
                      flight2.departure_point + "," + flight2.destination_point + "," +
                      flight2.departure_date + "," + flight2.return_date + ";";
    }

    string query = "SELECT * FROM Flights WHERE company = ? AND departure_point = ? AND destination_point = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        msg = "N_found" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, company.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, destination_point.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;
        Flights flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.num_A = sqlite3_column_int(stmt, 2);
        flight.num_B = sqlite3_column_int(stmt, 3);
        flight.price_A = sqlite3_column_int(stmt, 4);
        flight.price_B = sqlite3_column_int(stmt, 5);
        flight.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight.company + ",";
        result_str += flight.flight_num + ",";
        result_str += to_string(flight.num_A) + ",";
        result_str += to_string(flight.num_B) + ",";
        result_str += to_string(flight.price_A) + " VND" + ",";
        result_str += to_string(flight.price_B) + " VND" + ",";
        result_str += flight.departure_point + ",";
        result_str += flight.destination_point + ",";
        result_str += flight.departure_date + ",";
        result_str += flight.return_date + ";";
    }
    if (!found)
    {
        msg = "N_found" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        msg = result_str + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }

    sqlite3_finalize(stmt);
}
void search_flight2(int client_socket, const string &departure_point, const string &destination_point, const string &departure_date, const string &return_date, const User &user)
{
    cout << departure_point << destination_point << endl;
    string msg;
    string noti = checknoti(client_socket);
    sqlite3_stmt *stmt;

    bool found = false;

    string result_str = "Y_found/";
    string query2 =
        "SELECT "
        "F1.*, F2.* "
        "FROM "
        "Flights F1 "
        "JOIN "
        "Flights F2 ON F1.destination_point = F2.departure_point AND F1.departure_date < F2.departure_date "
        "WHERE "
        "F1.departure_point = ? AND F2.destination_point = ? AND F1.departure_date<=? AND F2.return_date<=?;";
    cout << query2 << endl;

    if (sqlite3_prepare_v2(db, query2.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_found" + noti;
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        sqlite3_finalize(stmt);
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }
    cout << query2 << endl;

    sqlite3_bind_text(stmt, 1, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destination_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, departure_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, return_date.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;

        Flights flight1;
        flight1.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight1.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight1.num_A = sqlite3_column_int(stmt, 2);
        flight1.num_B = sqlite3_column_int(stmt, 3);
        flight1.price_A = sqlite3_column_int(stmt, 4);
        flight1.price_B = sqlite3_column_int(stmt, 5);
        flight1.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight1.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight1.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight1.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight1.company + "," + flight1.flight_num + "," +
                      std::to_string(flight1.num_A) + "," + std::to_string(flight1.num_B) + "," +
                      std::to_string(flight1.price_A) + " VND," + std::to_string(flight1.price_B) + " VND," +
                      flight1.departure_point + "," + flight1.destination_point + "," +
                      flight1.departure_date + "," + flight1.return_date + ";";

        Flights flight2;
        flight2.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 10));
        flight2.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 11));
        flight2.num_A = sqlite3_column_int(stmt, 12);
        flight2.num_B = sqlite3_column_int(stmt, 13);
        flight2.price_A = sqlite3_column_int(stmt, 14);
        flight2.price_B = sqlite3_column_int(stmt, 15);
        flight2.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 16));
        flight2.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 17));
        flight2.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 18));
        flight2.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 19));

        result_str += flight2.company + "," + flight2.flight_num + "," +
                      std::to_string(flight2.num_A) + "," + std::to_string(flight2.num_B) + "," +
                      std::to_string(flight2.price_A) + " VND," + std::to_string(flight2.price_B) + " VND," +
                      flight2.departure_point + "," + flight2.destination_point + "," +
                      flight2.departure_date + "," + flight2.return_date + ";";
    }
    string query = "SELECT * FROM Flights WHERE departure_point = ? AND destination_point = ? AND departure_date <= ? AND return_date <= ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_found" + noti;
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }
    
    cout << query << endl;

    sqlite3_bind_text(stmt, 1, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destination_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, departure_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, return_date.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;
        Flights flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.num_A = sqlite3_column_int(stmt, 2);
        flight.num_B = sqlite3_column_int(stmt, 3);
        flight.price_A = sqlite3_column_int(stmt, 4);
        flight.price_B = sqlite3_column_int(stmt, 5);
        flight.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight.company + ",";
        result_str += flight.flight_num + ",";
        result_str += to_string(flight.num_A) + ",";
        result_str += to_string(flight.num_B) + ",";
        result_str += to_string(flight.price_A) + " VND" + ",";
        result_str += to_string(flight.price_B) + " VND" + ",";
        result_str += flight.departure_point + ",";
        result_str += flight.destination_point + ",";
        result_str += flight.departure_date + ",";
        result_str += flight.return_date + ";";
    }

    if (!found)
    {
        msg = "N_found" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        msg = result_str + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }

    sqlite3_finalize(stmt);
}

void search_flight4(int client_socket, const string &company, const string &departure_point, const string &destination_point, const string &departure_date, const string &return_date, const User &user)
{
    string msg;
    string noti = checknoti(client_socket);
    sqlite3_stmt *stmt;

    bool found = false;

    string result_str = "Y_found/";
    string query2 =
        "SELECT "
        "F1.*, F2.* "
        "FROM "
        "Flights F1 "
        "JOIN "
        "Flights F2 ON F1.destination_point = F2.departure_point AND F1.departure_date < F2.departure_date "
        "WHERE "
        "F1.departure_point = ? AND F2.destination_point = ? AND F1.departure_date <=? AND F2.return_date<=? AND F1.company=? AND F2.company=?;";

    if (sqlite3_prepare_v2(db, query2.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_found" + noti;
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        sqlite3_finalize(stmt);
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destination_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, departure_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, return_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, company.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, company.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;

        Flights flight1;
        flight1.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight1.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight1.num_A = sqlite3_column_int(stmt, 2);
        flight1.num_B = sqlite3_column_int(stmt, 3);
        flight1.price_A = sqlite3_column_int(stmt, 4);
        flight1.price_B = sqlite3_column_int(stmt, 5);
        flight1.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight1.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight1.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight1.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight1.company + "," + flight1.flight_num + "," +
                      std::to_string(flight1.num_A) + "," + std::to_string(flight1.num_B) + "," +
                      std::to_string(flight1.price_A) + " VND," + std::to_string(flight1.price_B) + " VND," +
                      flight1.departure_point + "," + flight1.destination_point + "," +
                      flight1.departure_date + "," + flight1.return_date + ";";

        Flights flight2;
        flight2.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 10));
        flight2.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 11));
        flight2.num_A = sqlite3_column_int(stmt, 12);
        flight2.num_B = sqlite3_column_int(stmt, 13);
        flight2.price_A = sqlite3_column_int(stmt, 14);
        flight2.price_B = sqlite3_column_int(stmt, 15);
        flight2.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 16));
        flight2.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 17));
        flight2.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 18));
        flight2.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 19));

        result_str += flight2.company + "," + flight2.flight_num + "," +
                      std::to_string(flight2.num_A) + "," + std::to_string(flight2.num_B) + "," +
                      std::to_string(flight2.price_A) + " VND," + std::to_string(flight2.price_B) + " VND," +
                      flight2.departure_point + "," + flight2.destination_point + "," +
                      flight2.departure_date + "," + flight2.return_date + ";";
    }
    string query = "SELECT * FROM Flights WHERE company = ? AND departure_point = ? AND destination_point = ? AND departure_date <= ? AND return_date <= ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_found" + noti;
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);

        return;
    }

    sqlite3_bind_text(stmt, 1, company.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, destination_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, departure_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, return_date.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;
        Flights flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.num_A = sqlite3_column_int(stmt, 2);
        flight.num_B = sqlite3_column_int(stmt, 3);
        flight.price_A = sqlite3_column_int(stmt, 4);
        flight.price_B = sqlite3_column_int(stmt, 5);
        flight.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight.company + ",";
        result_str += flight.flight_num + ",";
        result_str += to_string(flight.num_A) + ",";
        result_str += to_string(flight.num_B) + ",";
        result_str += to_string(flight.price_A) + " VND" + ",";
        result_str += to_string(flight.price_B) + " VND" + ",";
        result_str += flight.departure_point + ",";
        result_str += flight.destination_point + ",";
        result_str += flight.departure_date + ",";
        result_str += flight.return_date + ";";
    }

    if (!found)
    {
        msg = "N_found" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        msg = result_str + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }

    sqlite3_finalize(stmt);
}


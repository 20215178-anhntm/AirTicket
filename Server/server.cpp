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
/*

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
            // Gửi thông báo chưa đọc
            fetchUnreadNotifications(client_socket, db, username);

            functions(client_socket, user);
        }
        else
        {
            std::cout << "Send: N_login" << endl;
            sqlite3_finalize(stmt);
            send(client_socket, "N_login", strlen("N_login"), 0);
        }
    }
} */

void log_in(int client_socket, const std::string &username, const std::string &password) // Log in function
{
    if (userSocketMap.find(username) != userSocketMap.end())
    {
        std::cout << "Send: N_login1\n";
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
        std::string query = "SELECT username, password FROM Users WHERE username = ? AND password = ?";
        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::cerr << "Error preparing query: " << sqlite3_errmsg(db) << std::endl;
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

            // Lấy thông báo chưa đọc
            std::vector<std::string> unreadNotifications = fetchUnreadNotifications(db, username);

            // Tạo chuỗi thông báo kết hợp
            std::string combinedMessage = "Y_login";
            for (const auto &message : unreadNotifications)
            {
                combinedMessage += "&" + message;
            }

            // Gửi chuỗi thông báo kết hợp
            std::cout << "Send: " << combinedMessage << "\n";
            send(client_socket, combinedMessage.c_str(), combinedMessage.length(), 0);

            // Thêm vào bản đồ người dùng đã đăng nhập
            {
                std::lock_guard<std::mutex> lock(mapMutex);
                userSocketMap[username] = client_socket;
            }

            sqlite3_finalize(stmt);

            // Chuyển đến các chức năng chính sau khi đăng nhập
            functions(client_socket, user);
        }
        else
        {
            std::cout << "Send: N_login\n";
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
        // Gửi thông báo chưa đọc
        //fetchUnreadNotifications(client_socket, db, username);

        functions(client_socket, newUser);
    }
}

void admin_mode(int client_socket)
{
    char buffer[BUFFER_SIZE];
    int bytes_received;
    while (true)
    {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0)
        {
            break;
        }

        buffer[bytes_received] = '\0';
        string received(buffer);
        cout << "Received: " << received << " from admin\n";
        if (lower(received) == "logout")
        {
            std::cout << "Admin logged out requested" << endl;
            send(client_socket, "O_log", strlen("O_log"), 0);
            return;
        }
        vector<string> type1 = split(received, '/');
        if (lower(type1[0]) == "add_flight")
        {
            sqlite3_stmt *stmt;

            Flights newFlight;
            try
            {
                newFlight.company = type1[1];
                newFlight.flight_num = type1[2];
                newFlight.num_A = stoi(type1[3]);
                newFlight.num_B = stoi(type1[4]);
                newFlight.price_A = stoi(type1[5]);
                newFlight.price_B = stoi(type1[6]);
                newFlight.departure_point = type1[7];
                newFlight.destination_point = type1[8];
                newFlight.departure_date = type1[9];
                newFlight.return_date = type1[10];
            }
            catch (const std::invalid_argument &ia)
            {
                std::cout << "Invalid argument: " << ia.what() << endl;
                std::cout << "Send: N_add -> Admin\n";
                send(client_socket, "N_add", strlen("N_add"), 0);
                continue;
            }
            string check_query = "SELECT flight_num FROM Flights WHERE flight_num = ?";
            if (sqlite3_prepare_v2(db, check_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            {
                std::cout << "Error preparing query: " << sqlite3_errmsg(db) << endl;
                std::cout << "Send: N_add -> Admin\n";
                send(client_socket, "N_add", strlen("N_add"), 0);
                continue;
            }
            sqlite3_bind_text(stmt, 1, newFlight.flight_num.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                std::cout << "Flight number already exists" << endl;
                std::cout << "Send: N_add -> Admin\n";
                send(client_socket, "N_add", strlen("N_add"), 0);
                sqlite3_finalize(stmt);
                continue;
            }
            sqlite3_finalize(stmt);

            string insert_query = "INSERT INTO Flights (company, flight_num, seat_class_A,seat_class_B,price_A,price_B, departure_point, destination_point, departure_date, return_date) VALUES (?, ?, ?, ?, ?, ?, ?,?,?,?)";
            if (sqlite3_prepare_v2(db, insert_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            {
                cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
                std::cout << "Send: N_add -> Admin\n";
                send(client_socket, "N_add", strlen("N_add"), 0);
                continue;
            }

            // Binding parameters to avoid SQL injection
            sqlite3_bind_text(stmt, 1, newFlight.company.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, newFlight.flight_num.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, newFlight.num_A);
            sqlite3_bind_int(stmt, 4, newFlight.num_B);
            sqlite3_bind_int(stmt, 5, newFlight.price_A);
            sqlite3_bind_int(stmt, 6, newFlight.price_B);
            sqlite3_bind_text(stmt, 7, newFlight.departure_point.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 8, newFlight.destination_point.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 9, newFlight.departure_date.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 10, newFlight.return_date.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_DONE)
            {
                cerr << "Error inserting data: " << sqlite3_errmsg(db) << endl;
                std::cout << "Send: N_add -> Admin\n";
                send(client_socket, "N_add", strlen("N_add"), 0);
            }
            else
            {
                cout << "Admin: Flight inserted\n";
                std::cout << "Send: Y_add -> Admin\n";
                send(client_socket, "Y_add", strlen("Y_add"), 0);
            }
            sqlite3_finalize(stmt);
        }
        else if (lower(type1[0]) == "del_flight")
        {
            sqlite3_stmt *stmt;
            string flight_num = type1[1];
            const char *flight_check_sql = "SELECT COUNT(*) FROM Flights WHERE flight_num = ?;";
            if (sqlite3_prepare_v2(db, flight_check_sql, -1, &stmt, NULL) != SQLITE_OK)
            {
                cerr << "Error preparing flight check statement" << endl;
                return;
            }
            else
            {
                sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    if (sqlite3_column_int(stmt, 0) == 0)
                    {
                        std::cout << "Send: N_del ->Admin\n";
                        send(client_socket, "N_del", strlen("N_del"), 0);
                        sqlite3_finalize(stmt);
                        continue;
                    }
                }
                sqlite3_finalize(stmt);
            }
            vector<int> affected_user_ids = get_affected_user_id(flight_num);
            // Delete the flight
            string delete_query = "DELETE FROM Flights WHERE flight_num = ?";
            if (sqlite3_prepare_v2(db, delete_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            {
                cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
                std::cout << "Send: N_del ->Admin\n";
                send(client_socket, "N_del", strlen("N_del"), 0);
                continue;
            }
            sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_DONE)
            {
                cerr << "Error deleting flight: " << sqlite3_errmsg(db) << endl;
                std::cout << "Send: N_del ->Admin\n";
                send(client_socket, "N_del", strlen("N_del"), 0);
            }
            else
            {
                std::cout << "Flight deleted successfully" << endl;
                std::cout << "Send: Y_del ->Admin\n";
                send(client_socket, "Y_del", strlen("Y_del"), 0);
                string noti1 = "Flight cancelled: " + flight_num + "'s has been cancelled&" ;
                const char *insertNotificationSQL = R"(
            INSERT INTO Notifications (user_id, message, status) VALUES (?, ?, 'UNREAD')
        )";

        for (int user_id : affected_user_ids)
        {
            string username = get_username_from_id(user_id);
            {
                std::lock_guard<std::mutex> lockMap(mapMutex);
                auto it = userSocketMap.find(username);
                if (it != userSocketMap.end())
                {
                    int user_socket = it->second;
                    std::lock_guard<std::mutex> lockNotif(clientNotifMapMutex);
                    clientNotifMap[user_socket] += "Y_notif_cancelled" + noti1;
                }
                else
                {
                    cerr << "User " << username << " not connected. Saving notification to database." << endl;
                    sqlite3_stmt *insertStmt = nullptr;
                    int rc = sqlite3_prepare_v2(db, insertNotificationSQL, -1, &insertStmt, nullptr);
                    string notificationMessage = "Y_notif_cancelled" + noti1;

                    if (rc == SQLITE_OK)
                    {
                        sqlite3_bind_int(insertStmt, 1, user_id);
                        sqlite3_bind_text(insertStmt, 2, notificationMessage.c_str(), -1, SQLITE_STATIC);
                        rc = sqlite3_step(insertStmt);
                        if (rc != SQLITE_DONE)
                        {
                            cerr << "Failed to insert notification: " << sqlite3_errmsg(db) << endl;
                        }
                        sqlite3_finalize(insertStmt);
                    }
                    else
                    {
                        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
                    }
                }
            }
        }
                
            }
            sqlite3_finalize(stmt);

            string del_ticket_query = "DELETE FROM Tickets WHERE flight_num = ?";
            if (sqlite3_prepare_v2(db, del_ticket_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            {
                cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
                std::cout << "Send: N_del ->Admin\n";
                send(client_socket, "N_del", strlen("N_del"), 0);
                continue;
            }
            sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_DONE)
            {
                cerr << "Error deleting tickets: " << sqlite3_errmsg(db) << endl;
                std::cout << "Send: N_del ->Admin\n";
                send(client_socket, "N_del", strlen("N_del"), 0);
            }
            else
            {
                std::cout << "Tickets for flight " << flight_num << " deleted successfully" << endl;
            }
            sqlite3_finalize(stmt);
        }
        else if (lower(type1[0]) == "modify1")
        {
            update_flight1(client_socket, type1[1], type1[2]);
        }
        else if (lower(type1[0]) == "modify2")
        {
            update_flight2(client_socket, type1[1], type1[2]);
        }
        else if (lower(type1[0]) == "modify3")
        {
            update_flight3(client_socket, type1[1], type1[2], type1[3]);
        }
    }   
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
        else if (lower(type1[0]) == "book")
        {
            book_flight(client_socket, type1[1], type1[2], user);
        } 
        else if (type1[0] == "view")
        {
            string noti = checknoti(client_socket);
            string msg;
            cout << "view\n";
            sqlite3_stmt *stmt;
            string query = "SELECT T.ticket_code, T.flight_num, T.seat_class, T.ticket_price, T.payment, F.company, F.departure_date, F.return_date, F.departure_point, F.destination_point "
                           "FROM Tickets T "
                           "JOIN Flights F ON T.flight_num = F.flight_num "
                           "JOIN Users U ON T.user_id = U.user_id "
                           "WHERE U.username = ?";

            if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            {
                msg = "N_view" + noti;
                cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(client_socket, msg.c_str(), msg.length(), 0);
                return;
            }

            sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_STATIC);

            string result_str = "Y_view/";
            bool found = false;

            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                found = true;
                Ticket ticket;
                Flights flight;
                ticket.ticket_code = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
                ticket.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
                ticket.seat_class = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
                ticket.ticket_price = sqlite3_column_int(stmt, 3);
                ticket.payment = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
                flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
                flight.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
                flight.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
                flight.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
                flight.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

                stringstream ss;
                ss << ticket.ticket_price;
                string str = ss.str();
                string str_ticket_price = str.substr(0, 3) + "." + str.substr(3, 3);

                result_str += ticket.flight_num + ",";
                result_str += ticket.ticket_code + ",";
                result_str += flight.company + ",";
                result_str += flight.departure_point + ',';
                result_str += flight.destination_point + ',';
                result_str += flight.departure_date + ",";
                result_str += flight.return_date + ",";
                result_str += ticket.seat_class + ",";
                result_str += str_ticket_price + " VND" + ",";
                result_str += ticket.payment + ";";
            }

            sqlite3_finalize(stmt);
            if (!found)
            {
                msg = "N_view" + noti;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                cout << user.username << ": Found no ticket\n";
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
            else
            {
                msg = result_str + noti;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
        }
        else if (lower(type1[0]) == "cancel")
        {
            string ticket_code = type1[1];
            cancel_flight(client_socket, ticket_code, user);
        }
        else if (lower(type1[0]) == "print" && lower(type1[1]) == "all")
        {
            print_all(client_socket, user);
        }
        else if (lower(type1[0]) == "print" && type1[1] != "all")
        {
            print_ticket(client_socket, type1[1], user);
        }
        else if (lower(type1[0]) == "pay") // chưa có trường hợp vé đã thanh toán rồi
        {
            string noti = checknoti(client_socket);
            string msg;
            int ticket_price;
            sqlite3_stmt *stmt;
            string check = "SELECT ticket_price FROM Tickets WHERE ticket_code = ?";

            sqlite3_prepare_v2(db, check.c_str(), -1, &stmt, NULL);
            sqlite3_bind_text(stmt, 1, type1[1].c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                ticket_price = sqlite3_column_int(stmt, 0);
            }

            sqlite3_finalize(stmt);

            string update_pay = "UPDATE Tickets SET payment = 'PAID' WHERE ticket_code = ?";

            if (sqlite3_prepare_v2(db, update_pay.c_str(), -1, &stmt, NULL) != SQLITE_OK)
            {
                msg = "N_pay" + noti;
                cerr << "Error preparing update statement" << endl;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
            else
            {
                if (sqlite3_bind_text(stmt, 1, type1[1].c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
                {
                    msg = "N_pay" + noti;
                    cerr << "Error binding ticket_code to update statement" << endl;
                    cout << "Send: " << msg << " ->" << user.username << "\n";
                    send(client_socket, msg.c_str(), msg.length(), 0);
                }
                else
                {
                    if (sqlite3_step(stmt) != SQLITE_DONE)
                    {
                        msg = "N_pay" + noti;
                        cerr << "Error executing update statement" << endl;
                        cout << "Send: " << msg << " ->" << user.username << "\n";
                        send(client_socket, msg.c_str(), msg.length(), 0);
                    }
                }
                sqlite3_finalize(stmt);
                msg = "Y_pay/" + to_string(ticket_price) + type1[1] + noti;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
        }
        else if (lower(type1[0]) == "change")
        {
            {
                change_flight(client_socket, type1[1], type1[2], type1[3], user);
            }
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
        "F1.departure_point = ? AND F2.destination_point = ? AND F1.departure_date <=? AND F2.return_date <=? AND F1.company=? AND F2.company=?;";

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

void book_flight(int client_socket, const string flight_num, const string seat_class, const User &user)
{
    string msg;
    string noti = checknoti(client_socket);
    sqlite3_stmt *stmt;
    int ticket_price = 0;
    string query_price = "SELECT ";
    query_price += (seat_class == "A" ? "price_A" : "price_B");
    query_price += " FROM Flights WHERE flight_num = ?";

    sqlite3_prepare_v2(db, query_price.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ticket_price = sqlite3_column_int(stmt, 0);
    }
    else
    {
        cerr << "Failed to retrieve ticket price." << endl;
    }

    string query_seat;
    int available_seats;

    if (seat_class == "A")
    {
        query_seat = "SELECT seat_class_A FROM Flights WHERE flight_num = ?";
    }
    else if (seat_class == "B")
    {
        query_seat = "SELECT seat_class_B FROM Flights WHERE flight_num = ?";
    }
    else
    {
        msg = "N_invalid_class" + noti;
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    if (sqlite3_prepare_v2(db, query_seat.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        available_seats = sqlite3_column_int(stmt, 0);
    }
    else
    {
        msg = "N_flight_not_found" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);
    std::cout << "Seat available: " << available_seats << endl;
    if (available_seats == 0)
    {
        msg = "N_no_seats/" + seat_class + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }
    string query = "SELECT flight_num FROM Flights WHERE flight_num = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_book" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        sqlite3_finalize(stmt);

        int user_id = -1;
        string query1 = "SELECT user_id FROM Users WHERE username = ?";
        if (sqlite3_prepare_v2(db, query1.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                user_id = sqlite3_column_int(stmt, 0);
            }
            else
            {
                msg = "N_book" + noti;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(client_socket, msg.c_str(), msg.length(), 0);
                return;
            }
            sqlite3_finalize(stmt);
        }
        else
        {
            cerr << "Error preparing user query: " << sqlite3_errmsg(db) << endl;
            msg = "N_book" + noti;
            cout << "Send: " << msg << " ->" << user.username << "\n";
            send(client_socket, msg.c_str(), msg.length(), 0);
            sqlite3_finalize(stmt);
            return;
        }

        string ticket_code = generate_ticket_code();

        string payment_status = "NOT_PAID";
        string query2 = "INSERT INTO Tickets (ticket_code, user_id, flight_num, seat_class, ticket_price,payment) VALUES (?, ?, ?, ?, ?,?)";
        if (sqlite3_prepare_v2(db, query2.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, ticket_code.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, user_id);
            sqlite3_bind_text(stmt, 3, flight_num.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, seat_class.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 5, ticket_price);
            sqlite3_bind_text(stmt, 6, payment_status.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                std::cout << "Finished booking\n";
                string success_book = "Y_book/";
                success_book += ticket_code;
                success_book += to_string(ticket_price) + "VND";
                msg = success_book + noti;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(client_socket, msg.c_str(), msg.length(), 0);

                update_seat_count(db, flight_num, seat_class, -1);
                sqlite3_finalize(stmt);
            }
            else
            {
                msg = "N_book" + noti;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                cerr << "Error inserting ticket data: " << sqlite3_errmsg(db) << endl;
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
        }
        else
        {
            msg = "N_book" + noti;
            cout << "Send: " << msg << " ->" << user.username << "\n";
            cerr << "Error preparing insert query: " << sqlite3_errmsg(db) << endl;
            send(client_socket, msg.c_str(), msg.length(), 0);
        }
    }
    else
    {
        msg = "N_book" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
}

void update_seat_count(sqlite3 *db, const string &flight_num, const string &seat_class, int adjustment)
{
    sqlite3_stmt *stmt;
    string sql;
    if (seat_class == "A")
    {
        sql = "UPDATE Flights SET seat_class_A = seat_class_A + ? WHERE flight_num = ?";
    }
    else if (seat_class == "B")
    {
        sql = "UPDATE Flights SET seat_class_B = seat_class_B + ? WHERE flight_num = ?";
    }
    else
    {
        cerr << "Invalid seat class" << endl;
        return;
    }

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_int(stmt, 1, adjustment);
    sqlite3_bind_text(stmt, 2, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        cerr << "SQL error in updating seat count: " << sqlite3_errmsg(db) << endl;
    }

    sqlite3_finalize(stmt);
}

void cancel_flight(int client_socket, const string ticket_code, const User &user)
{
    sqlite3_stmt *stmt;
    string flight_num, seat_class;
    string msg;
    string noti = checknoti(client_socket);

    string query = "SELECT flight_num, seat_class FROM Tickets WHERE ticket_code = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_cancel_err" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(client_socket, msg.c_str(), msg.length(), 0);

        return;
    }

    sqlite3_bind_text(stmt, 1, ticket_code.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        seat_class = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    }
    else
    {
        msg = "N_cancel_notfound" + noti;
        cerr << "Ticket not found" << endl;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);

    query = "DELETE FROM Tickets WHERE ticket_code = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_cancel_err" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Error preparing delete query: " << sqlite3_errmsg(db) << endl;
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, ticket_code.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        msg = "N_cancel_err" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Error deleting ticket: " << sqlite3_errmsg(db) << endl;
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        std::cout << "Ticket cancelled successfully" << endl;
        string cancel_success = "Y_cancel/" + ticket_code;
        msg = cancel_success + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);

        update_seat_count(db, flight_num, seat_class, 1);
    }
    sqlite3_finalize(stmt);
}

void print_all(int client_socket, const User &user)
{
    string msg;
    string noti = checknoti(client_socket);
    sqlite3_stmt *stmt;
    string query = "SELECT T.ticket_code, T.flight_num, T.seat_class, T.ticket_price, T.payment, F.company, F.departure_date, F.return_date, F.departure_point, F.destination_point "
                   "FROM Tickets T "
                   "JOIN Flights F ON T.flight_num = F.flight_num "
                   "JOIN Users U ON T.user_id = U.user_id "
                   "WHERE U.username = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_view" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_STATIC);

    string result_str = "Y_print/";
    bool found = false;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;
        Ticket ticket;
        Flights flight;
        ticket.ticket_code = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        ticket.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        ticket.seat_class = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        ticket.ticket_price = sqlite3_column_int(stmt, 3);
        ticket.payment = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
        flight.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        stringstream ss;
        ss << ticket.ticket_price;
        string str = ss.str();
        string str_ticket_price = str.substr(0, 3) + "." + str.substr(3, 3);

        result_str += ticket.flight_num + ",";
        result_str += ticket.ticket_code + ",";
        result_str += flight.company + ",";
        result_str += flight.departure_point + ',';
        result_str += flight.destination_point + ',';
        result_str += flight.departure_date + ",";
        result_str += flight.return_date + ",";
        result_str += ticket.seat_class + ",";
        result_str += str_ticket_price + " VND" + ",";
        result_str += ticket.payment + ";";
    }

    sqlite3_finalize(stmt);

    if (!found)
    {
        msg = "N_view" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        msg = result_str + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
}

void print_ticket(int client_socket, const string ticket_code, const User &user)
{
    sqlite3_stmt *stmt;
    string msg;
    string noti = checknoti(client_socket);
    string query = "SELECT T.ticket_code, T.flight_num, T.seat_class, T.ticket_price, T.payment, F.company, F.departure_date, F.return_date, F.departure_point, F.destination_point "
                   "FROM Tickets T "
                   "JOIN Flights F ON T.flight_num = F.flight_num "
                   "WHERE T.ticket_code = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_print_cer" + noti;
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, ticket_code.c_str(), -1, SQLITE_STATIC);

    string result_str = "Y_print_cer/";
    bool found = false;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;
        Ticket ticket;
        Flights flight;
        ticket.ticket_code = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        ticket.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        ticket.seat_class = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        ticket.ticket_price = sqlite3_column_int(stmt, 3);
        ticket.payment = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
        flight.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        stringstream ss;
        ss << ticket.ticket_price;
        string str = ss.str();
        string str_ticket_price = str.substr(0, 3) + "." + str.substr(3, 3);

        result_str += ticket.flight_num + ",";
        result_str += ticket.ticket_code + ",";
        result_str += flight.company + ",";
        result_str += flight.departure_point + ',';
        result_str += flight.destination_point + ',';
        result_str += flight.departure_date + ",";
        result_str += flight.return_date + ",";
        result_str += ticket.seat_class + ",";
        result_str += str_ticket_price + " VND" + ",";
        result_str += ticket.payment + ";";
    }

    sqlite3_finalize(stmt);

    if (!found)
    {
        msg = "N_print_cerr" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        msg = result_str + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
}

void change_flight(int client_socket, const string old_ticket_code, const string flight_num_new, const string seat_class_new, const User &user)
{
    string noti = checknoti(client_socket);
    string msg;
    string change_success = "Y_change/";
    sqlite3_stmt *stmt;
    string old_flight_num, old_seat_class;

    string can_query = "SELECT flight_num, seat_class FROM Tickets WHERE ticket_code = ?";
    if (sqlite3_prepare_v2(db, can_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_cancel_err" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(client_socket, msg.c_str(), msg.length(), 0);

        return;
    }

    sqlite3_bind_text(stmt, 1, old_ticket_code.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        old_flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        old_seat_class = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    }
    else
    {
        msg = "N_found_change" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Ticket not found" << endl;
        send(client_socket, msg.c_str(), msg.length(), 0);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);
    can_query = "DELETE FROM Tickets WHERE ticket_code = ?";
    if (sqlite3_prepare_v2(db, can_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        string msg = "N_cancel_err" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Error preparing delete query: " << sqlite3_errmsg(db) << endl;
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, old_ticket_code.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        string msg = "N_cancel_err" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Error deleting ticket: " << sqlite3_errmsg(db) << endl;
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    std::cout << "Ticket cancelled successfully" << endl;
    change_success += old_ticket_code;

    update_seat_count(db, old_flight_num, old_seat_class, 1);
    sqlite3_finalize(stmt);

    int ticket_price = 0;
    string query_price = "SELECT ";
    query_price += (seat_class_new == "A" ? "price_A" : "price_B");
    query_price += " FROM Flights WHERE flight_num = ?";

    sqlite3_prepare_v2(db, query_price.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, flight_num_new.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ticket_price = sqlite3_column_int(stmt, 0);
    }
    else
    {
        cerr << "Failed to retrieve ticket price." << endl;
    }

    string query_seat;
    int available_seats;

    if (seat_class_new == "A")
    {
        query_seat = "SELECT seat_class_A FROM Flights WHERE flight_num = ?";
    }
    else if (seat_class_new == "B")
    {
        query_seat = "SELECT seat_class_B FROM Flights WHERE flight_num = ?";
    }
    else
    {
        msg = "N_invalid_class" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    if (sqlite3_prepare_v2(db, query_seat.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, flight_num_new.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        available_seats = sqlite3_column_int(stmt, 0);
    }
    else
    {
        msg = "N_flight_not_found" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);
    std::cout << "Seat available: " << available_seats << endl;
    if (available_seats == 0)
    {
        string N_book_avail = "N_no_seats/" + seat_class_new;
        msg = N_book_avail + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    string book_query = "SELECT flight_num FROM Flights WHERE flight_num = ?";
    if (sqlite3_prepare_v2(db, book_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_book" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, flight_num_new.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        sqlite3_finalize(stmt);

        int user_id = -1;
        string query1 = "SELECT user_id FROM Users WHERE username = ?";
        if (sqlite3_prepare_v2(db, query1.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                user_id = sqlite3_column_int(stmt, 0);
            }
            else
            {
                msg = "N_book" + noti;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                cerr << "No user found" << endl;
                send(client_socket, msg.c_str(), msg.length(), 0);
                return;
            }
            sqlite3_finalize(stmt);
        }
        else
        {
            msg = "N_book" + noti;
            cout << "Send: " << msg << " ->" << user.username << "\n";
            cerr << "Error preparing user query: " << sqlite3_errmsg(db) << endl;
            send(client_socket, msg.c_str(), msg.length(), 0);
            sqlite3_finalize(stmt);
            return;
        }

        string new_ticket_code = generate_ticket_code();

        // Payment
        string payment_status = "NOT_PAID";
        string query2 = "INSERT INTO Tickets (ticket_code, user_id, flight_num, seat_class, ticket_price, payment) VALUES (?, ?, ?, ?, ?,?)";
        if (sqlite3_prepare_v2(db, query2.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, new_ticket_code.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, user_id);
            sqlite3_bind_text(stmt, 3, flight_num_new.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, seat_class_new.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 5, ticket_price);
            sqlite3_bind_text(stmt, 6, payment_status.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                std::cout << "Finished booking\n";
                change_success += new_ticket_code;
                change_success += to_string(ticket_price) + "VND";
                msg = change_success + noti;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(client_socket, msg.c_str(), msg.length(), 0);
                update_seat_count(db, flight_num_new, seat_class_new, -1);
            }
            else
            {
                msg = "N_book" + noti;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                cerr << "Error inserting ticket data: " << sqlite3_errmsg(db) << endl;
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
        }
        else
        {
            msg = "N_book" + noti;
            cout << "Send: " << msg << " ->" << user.username << "\n";
            cerr << "Error preparing insert query: " << sqlite3_errmsg(db) << endl;
            send(client_socket, msg.c_str(), msg.length(), 0);
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        msg = "N_book" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
}

std::string get_username_from_id(int user_id)
{
    sqlite3_stmt *stmt;
    std::string username;
    std::string query = "SELECT username FROM Users WHERE user_id = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, user_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char *username_raw = sqlite3_column_text(stmt, 0);
            username = reinterpret_cast<const char *>(username_raw);
        }
        else
        {
            std::cerr << "User not found for ID: " << user_id << std::endl;
        }

        sqlite3_finalize(stmt);
    }
    else
    {
        std::cerr << "SQL error: " << sqlite3_errmsg(db) << std::endl;
    }

    return username;
}

void update_flight1(int client_socket, const string &flight_num, const string &new_departure_date)
{
    if (!flight_num_exists(flight_num))
    {
        cout << "Flight number does not exist: " << flight_num << endl;
        send(client_socket, "N_modify", strlen("N_modify"), 0);
        return;
    }

    vector<int> affected_ids = get_affected_user_id(flight_num);
    pair<string, string> old = get_old_dates(flight_num);
    DateDifference diff = calculate_date_difference(old.first, new_departure_date);

    string noti1;
    if (diff.days != 0)
    {
        noti1 = "Schedule changed: " + flight_num + "'s departure date has changed to " + new_departure_date + "&";
    }
    else if (diff.hours != 0 && diff.days == 0)
    {
        stringstream ss;
        ss << diff.hours;
        noti1 = "Flight " + flight_num + " has been delayed for " + ss.str() + " hours&";
    }

    sqlite3_stmt *stmt = nullptr;
    string query = "UPDATE Flights SET departure_date = ? WHERE flight_num = ?";
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        cerr << "Error preparing update statement: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, new_departure_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        cerr << "Error executing update statement: " << sqlite3_errmsg(db) << endl;
    }
    else
    {
        const char *insertNotificationSQL = R"(
            INSERT INTO Notifications (user_id, message, status) VALUES (?, ?, 'UNREAD')
        )";

        for (int user_id : affected_ids)
        {
            string username = get_username_from_id(user_id);
            {
                std::lock_guard<std::mutex> lockMap(mapMutex);
                auto it = userSocketMap.find(username);
                if (it != userSocketMap.end())
                {
                    int user_socket = it->second;
                    std::lock_guard<std::mutex> lockNotif(clientNotifMapMutex);
                    clientNotifMap[user_socket] += "Y_modified1" + noti1;
                }
                else
                {
                    cerr << "User " << username << " not connected. Saving notification to database." << endl;
                    sqlite3_stmt *insertStmt = nullptr;
                    rc = sqlite3_prepare_v2(db, insertNotificationSQL, -1, &insertStmt, nullptr);
                    string notificationMessage = "Y_modified1" + noti1;

                    if (rc == SQLITE_OK)
                    {
                        sqlite3_bind_int(insertStmt, 1, user_id);
                        sqlite3_bind_text(insertStmt, 2, notificationMessage.c_str(), -1, SQLITE_STATIC);
                        rc = sqlite3_step(insertStmt);
                        if (rc != SQLITE_DONE)
                        {
                            cerr << "Failed to insert notification: " << sqlite3_errmsg(db) << endl;
                        }
                        sqlite3_finalize(insertStmt);
                    }
                    else
                    {
                        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
                    }
                }
            }
        }
	
	std::cout << "Send: Y_modify ->Admin\n";
        send(client_socket, "Y_modify", strlen("Y_modify"), 0);
    }

    sqlite3_finalize(stmt);
}

void update_flight2(int client_socket, string &flight_num, const string &new_return_date)
{
    vector<int> affected_ids = get_affected_user_id(flight_num);
    if (!flight_num_exists(flight_num))
    {
        cout << "Flight number does not exist: " << flight_num << endl;
        std::cout << "Send: N_modify ->Admin\n";
        send(client_socket, "N_modify", strlen("N_modify"), 0);
        return;
    }
    sqlite3_stmt *stmt;
    string query = "UPDATE Flights SET return_date = ? WHERE flight_num = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing update statement: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, new_return_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        cerr << "Error executing update statement: " << sqlite3_errmsg(db) << endl;
    }
    else
    {
        const char *insertNotificationSQL = R"(
            INSERT INTO Notifications (user_id, message, status) VALUES (?, ?, 'UNREAD')
        )";

        for (int user_id : affected_ids)
        {
            string username = get_username_from_id(user_id);
            {
                std::lock_guard<std::mutex> lockMap(mapMutex);
                auto it = userSocketMap.find(username);
                if (it != userSocketMap.end())
                {
                    int user_socket = it->second;
                    std::string notifMessage = "Y_modified2Scheduled change: " + flight_num + "'s return date has changed to " + new_return_date + "&";
                    std::lock_guard<std::mutex> lockNotif(clientNotifMapMutex);
                    clientNotifMap[user_socket] += notifMessage;
                }
                else
                {
                    cerr << "User " << username << " not connected. Saving notification to database." << endl;
                    sqlite3_stmt *insertStmt = nullptr;
                    int rc = sqlite3_prepare_v2(db, insertNotificationSQL, -1, &insertStmt, nullptr);
                    std::string notifMessage = "Y_modified2Scheduled change: " + flight_num + "'s return date has changed to " + new_return_date + "&";

                    if (rc == SQLITE_OK)
                    {
                        sqlite3_bind_int(insertStmt, 1, user_id);
                        sqlite3_bind_text(insertStmt, 2, notifMessage.c_str(), -1, SQLITE_STATIC);
                        rc = sqlite3_step(insertStmt);
                        if (rc != SQLITE_DONE)
                        {
                            cerr << "Failed to insert notification: " << sqlite3_errmsg(db) << endl;
                        }
                        sqlite3_finalize(insertStmt);
                    }
                    else
                    {
                        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
                    }
                }
            }
        }
        
        std::cout << "Send: Y_modify ->Admin\n";
        send(client_socket, "Y_modify", strlen("Y_modify"), 0);
    }
    sqlite3_finalize(stmt);
}

void update_flight3(int client_socket, string &flight_num, const string &new_departure_date, const string &new_return_date)
{
    if (!flight_num_exists(flight_num))
    {
        cout << "Flight number does not exist: " << flight_num << endl;
        std::cout << "Send: N_modify ->Admin\n";
        send(client_socket, "N_modify", strlen("N_modify"), 0);
        return;
    }
    vector<int> affected_ids = get_affected_user_id(flight_num);
    pair<string, string> old = get_old_dates(flight_num);
    DateDifference diff = calculate_date_difference(old.first, new_departure_date);
    string noti1;
    if (diff.days != 0)
    {
        noti1 = "Schedule changed: " + flight_num + "'s departure date has changed to " + new_departure_date + " and return date has changed to " + new_return_date + "&";
    }
    else if (diff.hours != 0 && diff.days == 0)
    {
        stringstream ss;
        ss << diff.hours;
        string num_hours = ss.str();
        noti1 = "Flight " + flight_num + " has been delayed for " + num_hours + "hours" + "&";
    }
    sqlite3_stmt *stmt;
    string query = "UPDATE Flights SET departure_date = ?, return_date = ? WHERE flight_num = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing update statement: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, new_departure_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, new_return_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        cerr << "Error executing update statement: " << sqlite3_errmsg(db) << endl;
    }
    else
    {
        const char *insertNotificationSQL = R"(
            INSERT INTO Notifications (user_id, message, status) VALUES (?, ?, 'UNREAD')
        )";

        for (int user_id : affected_ids)
        {
            string username = get_username_from_id(user_id);
            {
                std::lock_guard<std::mutex> lockMap(mapMutex);
                auto it = userSocketMap.find(username);
                if (it != userSocketMap.end())
                {
                    int user_socket = it->second;
                    std::lock_guard<std::mutex> lockNotif(clientNotifMapMutex);
                    clientNotifMap[user_socket] += "Y_modified3" + noti1;
                }
                else
                {
                    cerr << "User " << username << " not connected. Saving notification to database." << endl;
                    sqlite3_stmt *insertStmt = nullptr;
                    int rc = sqlite3_prepare_v2(db, insertNotificationSQL, -1, &insertStmt, nullptr);
                    string notificationMessage = "Y_modified3" + noti1;

                    if (rc == SQLITE_OK)
                    {
                        sqlite3_bind_int(insertStmt, 1, user_id);
                        sqlite3_bind_text(insertStmt, 2, notificationMessage.c_str(), -1, SQLITE_STATIC);
                        rc = sqlite3_step(insertStmt);
                        if (rc != SQLITE_DONE)
                        {
                            cerr << "Failed to insert notification: " << sqlite3_errmsg(db) << endl;
                        }
                        sqlite3_finalize(insertStmt);
                    }
                    else
                    {
                        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
                    }
                }
            }
        }
        std::cout << "Send: Y_modify ->Admin\n";
        send(client_socket, "Y_modify", strlen("Y_modify"), 0);
    }
    sqlite3_finalize(stmt);
}

std::vector<int> get_affected_user_id(const std::string &flight_num)
{
    std::vector<int> affected_user_ids;
    sqlite3_stmt *stmt;

    std::string ticket_check_query = "SELECT user_id FROM Tickets WHERE flight_num = ?";
    if (sqlite3_prepare_v2(db, ticket_check_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "Error preparing query: " << sqlite3_errmsg(db) << std::endl;
        return affected_user_ids;
    }
    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        affected_user_ids.push_back(sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);

    return affected_user_ids;
}

pair<string, string> get_old_dates(const string &flight_num)
{
    sqlite3_stmt *stmt;
    string query = "SELECT departure_date, return_date FROM Flights WHERE flight_num = ?";
    pair<string, string> old_dates;

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing statement: " << sqlite3_errmsg(db) << endl;
        return old_dates;
    }

    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        old_dates.first = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        old_dates.second = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    }

    sqlite3_finalize(stmt);
    return old_dates;
}

bool flight_num_exists(const string &flight_num)
{
    cout << "Checking flight number: " << flight_num << endl;
    sqlite3_stmt *stmt;
    string query = "SELECT 1 FROM Flights WHERE flight_num = ? LIMIT 1";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing select statement: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return true;
    }
    else
    {
        cerr << "Flight number not found or error executing select statement: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        return false;
    }
}

std::vector<std::string> fetchUnreadNotifications(sqlite3 *conn, const std::string &username)
{
    std::vector<std::string> messages;

    const char *fetchUnreadNotificationsSQL = R"(
        SELECT notification_id, message FROM Notifications WHERE user_id = (
            SELECT user_id FROM Users WHERE username = ?
        ) AND status = 'UNREAD'
    )";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(conn, fetchUnreadNotificationsSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int notification_id = sqlite3_column_int(stmt, 0);
            std::string message(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));

            // Lưu thông báo vào vector
            messages.push_back(message);

            // Đánh dấu thông báo là 'READ'
            const char *markAsReadSQL = R"(
                UPDATE Notifications SET status = 'READ' WHERE notification_id = ?
            )";
            sqlite3_stmt *updateStmt;
            rc = sqlite3_prepare_v2(conn, markAsReadSQL, -1, &updateStmt, nullptr);
            if (rc == SQLITE_OK)
            {
                sqlite3_bind_int(updateStmt, 1, notification_id);
                sqlite3_step(updateStmt);
                sqlite3_finalize(updateStmt);
            }
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        std::cerr << "SQL error: " << sqlite3_errmsg(conn) << std::endl;
    }

    return messages;
}




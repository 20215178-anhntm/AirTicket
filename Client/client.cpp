#include "client.h"

int main()
{
    std::string tmp_noti;
    std::cout << "Enter IP: ";

    std::string chost;
    getline(cin, chost);
    const char *host = chost.c_str();
    struct sockaddr_in server_addr;
    int client_socket;
    Role cur_role = Role::none;

    try
    {
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1)
        {
            throw runtime_error("Error creating client socket");
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERV_PORT);
        if (inet_pton(AF_INET, host, &(server_addr.sin_addr)) <= 0)
        {
            cerr << "Invalid IP address" << endl;
            return 1;
        }

        if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        {
            throw runtime_error("Error connecting to the server");
        }

        char buffer[BUFFER_SIZE];

        while (true)
        {
            print_main_menu();
            string choice;
            getline(cin, choice);
            string lower_choice = trim(lower(choice));
            if (lower_choice == "3") // exit
            {
                send(client_socket, lower_choice.c_str(), choice.length(), 0);
                break;
            }
            else if (lower_choice == "1") // log in
            {
                string username, password;
                cout << "Enter username: ";
                getline(cin, username);
                cout << "Enter password: ";
                getline(cin, password);
                string msg = "login/" + username + "/" + password;
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
            else if (lower_choice == "2") // register
            { 
                string username, password;
                cout << "Enter new username: ";
                getline(cin, username);
                cout << "Enter new password: ";
                getline(cin, password);
                string msg = "register/" + username + "/" + password;
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
            else
            {
                std::cout << "Invalid choice!\n";
                continue;
            }
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0)
            {
                break;
            }
            buffer[bytes_received] = '\0';

            string message(buffer);
            string response = string(buffer);

            if (response == "Y_login" || response == "Y_register")
            {
                cout << "You're currently online!" << endl;
                cur_role = Role::user;
                while (true)
                {
                    if (tmp_noti.length() != 0)
                    {
                        cout << "Notification: \n";
                        cout << tmp_noti;
                        tmp_noti = "";
                    }
                    print_functions();

                    string choice1;
                    getline(cin, choice1);
                    string lower_choice1 = trim(lower(choice1));
                    if (lower_choice1 == "5")
                    {
                        send(client_socket, lower_choice.c_str(), choice.length(), 0);
                        break;
                    }
                    else if (lower_choice1 == "1") // search
                    {
                        string company, destination_point, departure_point, departure_date, return_date;
                        string search_msg;
                        while (1)
                        {
                            int choice2;
                            print_menu_search();
                            cin >> choice2;
                            cin.ignore(numeric_limits<streamsize>::max(), '\n');
                            if (choice2 == 1) // search3
                            {
                                cout << "Enter company : ";
                                getline(cin, company);

                                cout << "Enter departure point : ";
                                getline(cin, departure_point);

                                cout << "Enter destination point : ";
                                getline(cin, destination_point);
                                search_msg += "search3/" + company + "/" + departure_point + "/" + destination_point;
                                break;
                            }
                            else if (choice2 == 2) //search1
                            {
                                cout << "Enter departure point : ";
                                getline(cin, departure_point);

                                cout << "Enter destination point : ";
                                getline(cin, destination_point);

                                cout << "Enter departure date (format YYYY-MM-DD): ";
                                getline(cin, departure_date);
                                search_msg += "search1/" + departure_point + "/" + destination_point + "/" + departure_date;
                                break;
                            }
                            else if (choice2 == 3) //search2
                            {
                                cout << "Enter departure point : ";
                                getline(cin, departure_point);

                                cout << "Enter destination point : ";
                                getline(cin, destination_point);

                                cout << "Enter departure date (or leave blank for any, format YYYY-MM-DD): ";
                                getline(cin, departure_date);

                                cout << "Enter return date (format YYYY-MM-DD): ";
                                getline(cin, return_date);
                                search_msg += "search2/" + departure_point + "/" + destination_point + "/" + departure_date + "/" + return_date;
                                break;
                            }
                            else if (choice2 == 4) //search4
                            {
                                cout << "Enter company : ";
                                getline(cin, company);

                                cout << "Enter departure point : ";
                                getline(cin, departure_point);

                                cout << "Enter destination point : ";
                                getline(cin, destination_point);

                                cout << "Enter departure date (format YYYY-MM-DD): ";
                                getline(cin, departure_date);

                                cout << "Enter return date (or leave blank for any, format YYYY-MM-DD): ";
                                getline(cin, return_date);
                                search_msg += "search4/" + company + "/" + departure_point + "/" + destination_point + "/" + departure_date + "/" + return_date;
                                break;
                            }
                            else if (choice2 == 5) // exit
                            {
                            	break;
                            }
                            else
                            {
                                std::cout << "Invalid choice!\n";
                            }
                        }
                        send(client_socket, search_msg.c_str(), search_msg.length(), 0);
                    }
                    else if (lower_choice1 == "2") // book ticket
                    {
                     	break;
                    }
                    else if (lower_choice1 == "3") // view ticket detail
                    {
                        break;
                    }
                    else if (lower_choice1 == "4") // cancel tickets
                    {
                        break;
                    }
                    else if (lower_choice1 == "6") // print tickets
                    {
                        break;
                    }
                    else if (lower_choice1 == "7") // pay ticket
                    {
                        break;
                    }
                    else if (lower_choice1 == "5") // change ticket
                    {
                        break;
                    }
                    else if (lower_choice1 == "8") // log out
                    {
                        send(client_socket, "logout", strlen("logout"), 0);
                        memset(buffer, 0, BUFFER_SIZE);
                        int bytes_received1 = recv(client_socket, buffer, BUFFER_SIZE, 0);
                        if (bytes_received1 <= 0)
                        {
                            break;
                        }
                        buffer[bytes_received1] = '\0';

                        string response1 = string(buffer);
                        if (response1 == "O_log")
                        {
                            cur_role = Role::none; // Resetting the role to none
                            std::cout << "You've logged out successfully!" << endl;
                            break;
                        }
                    }
                    else
                    {
                        std::cout << "Invalid choice\n";
                        continue;
                    }
                    memset(buffer, 0, BUFFER_SIZE);
                    int bytes_received1 = recv(client_socket, buffer, BUFFER_SIZE, 0);
                    if (bytes_received1 <= 0)
                    {
                        break;
                    }
                    buffer[bytes_received1] = '\0';

                    string response1 = string(buffer);

                    if (response1.find("Y_found/") == 0)
                    {
                        string flight_data = response1.substr(8);
                        std::cout << "Flight data:" << endl;
                        display_search(flight_data);
                    }
                    else if (response1.find("N_found") == 0)
                    {
                        std::cout << "Can't find the flight!\n";
                    }
                }
            }
            else if (response == "N_login")
            {
                cout << "Login failed. Please check your username and password." << endl;
            }
            else if (response == "N_register")
            {
                cout << "Registration failed. Username might already exist." << endl;
            }
            else if (response == "N_login1")
            {
                std::cout << "This account is currently online\n";
            }
            else if (response == "N_register")
            {
                std::cout << "Your username has already existed!" << endl;
            }
            else if (response == "Y_admin")
            {
                string admin_choice;
                cur_role = Role::admin;
                std::cout << "You're the admin\n";
            }
        }

        close(client_socket);
        std::cout << "Closed the connection." << endl;
    }
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}

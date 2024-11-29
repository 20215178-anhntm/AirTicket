#include <iostream>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include <cstring>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <map>
#include <mutex>
#include <chrono>
#include <queue>

using namespace std;
#define PORT 3000
#define BUFFER_SIZE 1024

std::map<int, string> clientNotifMap;
std::mutex clientNotifMapMutex;
std::map<std::string, int> userSocketMap; // client socket, user_id
std::mutex mapMutex;

struct Flights
{
    string company;
    string flight_num;
    int num_A;
    int num_B;
    int price_A;
    int price_B;
    string departure_point;
    string destination_point;
    string departure_date;
    string return_date;
};

struct User
{
    int user_id;
    string username;
    string password;
};

struct Ticket
{
    string ticket_code;
    int user_id;
    string flight_num;
    string seat_class;
    double ticket_price;
    string payment;
};

vector<string> split(const string &input, char delimiter)
{
    vector<string> result;
    stringstream ss(input);
    string item;

    while (getline(ss, item, delimiter))
    {
        result.push_back(item);
    }

    return result;
}
string lower(const string &input)
{
    string result = input;

    for (char &c : result)
    {
        c = tolower(c);
    }

    return result;
}
std::string checknoti(int client_socket)
{
    string notification;
    {
        std::lock_guard<std::mutex> lock(clientNotifMapMutex);
        auto it = clientNotifMap.find(client_socket); // find noti of specific client socket
        if (it != clientNotifMap.end())
        {
            notification = it->second;
            clientNotifMap.erase(it);
        }
    }
    return notification;
}
void log_in(int client_socket, const string &username, const string &password);
void register_user(int client_socket, const string &username, const string &password);
void search_flight1(int client_socket, const string &departure_point, const string &destination_point, const string &departure_date, const User &user);
void search_flight3(int client_socket, const string &company, const string &departure_point, const string &destination_point, const User &user);
void search_flight2(int client_socket, const string &departure_point, const string &destination_point, const string &departure_date, const string &return_date, const User &user);
void search_flight4(int client_socket, const string &company, const string &departure_point, const string &destination_point, const string &departure_date, const string &return_date, const User &user);
void functions(int client_socket, const User &user);
void connect_client(int client_socket);
void admin_mode(int client_socket);
void notify_affected_users(const vector<int> &affected_user_ids, const string &noti, int c);
std::vector<int> get_affected_user_id(const std::string &flight_num);
std::string get_username_from_id(int user_id);
void handle_notifications(int client_socket, vector<int> affected_ids, const string noti, int c);

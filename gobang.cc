#include "server.hpp"
#define HOST "127.0.0.1"
#define USER "root"
#define PSW "xzx@2003"
#define DBNAME "gobang"
void mysql_test()
{
    MYSQL *mysql = mysql_util::mysql_create(HOST, USER, PSW, DBNAME);
    std::string sql = "insert stu values(null,'小明',18,53,68,87);";
    mysql_util::mysql_exec(mysql, sql);
    mysql_util::mysql_destroy(mysql);
}

void json_test()
{
    Json::Value root;
    root["name"] = "zhixin";
    root["age"] = 18;
    root["sex"] = "女";
    root["grade"].append(100);
    root["grade"].append(98);
    root["grade"].append(99);
    std::string body;
    json_util::serialize(root, body);
    DLOG("%s", body.c_str());

    Json::Value val;
    json_util::unserialize(body, val);
    std::cout << "反序列化结果:\n"
              << "age:" << val["age"].asString() << " "
              << "grade:" << val["grade"][0].asString() << "," << val["grade"][1].asString() << "," << val["grade"][2].asString() << " "
              << "name:" << val["name"].asString() << " "
              << "sex:" << val["sex"].asString() << " "
              << std::endl;
}

void str_test()
{
    std::string str = "123,,45,678,,9";
    std::vector<std::string> array;
    string_util::split(str, ",", array);
    for (auto &e : array)
    {
        DLOG("%s", e.c_str());
    }
}

void file_test()
{
    std::string body;
    std::string filename = "./makefile";
    file_util::read(filename, body);
    std::cout << body << std::endl;
}

void db_test()
{
    user_table ut(HOST, USER, PSW, DBNAME);
    Json::Value user;
    user["username"] = "xiaoming";
    // user["password"] = "123123";
    ut.insert(user);
    // bool ret = ut.login(user);
    // if (ret == false)
    // {
    //     DLOG("login failed");
    // }
    // ut.select_by_id(1, user);
    // std::string body;
    // json_util::serialize(user, body);
    // DLOG("%s", body.c_str());
    // ut.win(1);
    // ut.lose(1);
}

void online_test()
{
    online_manager om;
    wsserver_t::connection_ptr conn;
    uint64_t uid = 2;
    om.enter_game_hall(uid, conn);
    if (om.in_game_hall(uid))
    {
        DLOG("IN GAME HALL");
    }
    else
    {
        DLOG("NOT IN GAME HALL");
    }
    om.exit_game_hall(uid);
    if (om.in_game_hall(uid))
    {
        DLOG("IN GAME HALL");
    }
    else
    {
        DLOG("NOT IN GAME HALL");
    }
}

int main()
{
    gobang_server _server(HOST, USER, PSW, DBNAME);
    _server.start(8082);
    return 0;
}
#ifndef __M_UTIL_H__
#define __M_UTIL_H__
#include <mysql/mysql.h>
#include "logger.hpp"
#include <string>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <sstream>
#include <vector>
#include <fstream>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
typedef websocketpp::server<websocketpp::config::asio> wsserver_t;

class mysql_util;
class json_util;
class string_util;
class mysql_util
{
public:
    static MYSQL *mysql_create(const std::string &host,
                               const std::string &username,
                               const std::string &psw,
                               const std::string &dbname,
                               uint16_t port = 3306)
    {
        // 1.初始化句柄
        MYSQL *mysql = mysql_init(NULL);
        if (mysql == NULL)
        {
            ELOG("mysql init fail");
            return NULL;
        }
        // 2.连接服务器
        if (mysql_real_connect(mysql, host.c_str(), username.c_str(), psw.c_str(), dbname.c_str(), port, NULL, 0) == NULL)
        {
            ELOG("connet mysql server fail :%s", mysql_errno(mysql));
            mysql_close(mysql);
            return NULL;
        }
        // 3.设置客户端字符集
        if (mysql_set_character_set(mysql, "utf8") != 0)
        {
            ELOG("set client character failed :%s", mysql_errno(mysql));
            mysql_close(mysql);
            return NULL;
        }
        return mysql;
    }

    static bool mysql_exec(MYSQL *mysql, const std::string &sql)
    {
        if (mysql_query(mysql, sql.c_str()) != 0)
        {
            ELOG("%s", sql.c_str());
            ELOG("mysql query failed :%s", mysql_error(mysql));
            // mysql_close(mysql);  这地方不能close,否则句柄被置空，但是整个服务器用的时一个user_table,里面是同一个mysql
            // std::cout << "mysql close" << std::endl;
            return false;
        }
        return true;
    }

    static void mysql_destroy(MYSQL *mysql)
    {
        if (mysql != NULL)
        {
            mysql_close(mysql);
        }
    }
};

class json_util
{
public:
    static bool serialize(const Json::Value &root, std::string &str)
    {
        Json::StreamWriterBuilder swb;
        Json::StreamWriter *sw = swb.newStreamWriter();
        std::stringstream ss;
        int ret = sw->write(root, &ss);
        if (ret != 0)
        {
            ELOG("json serialize failed!\n");
            return false;
        }
        str = ss.str();
        return true;
    }

    static bool unserialize(const std::string str, Json::Value &root)
    {
        Json::CharReaderBuilder crb;
        Json::CharReader *cr = crb.newCharReader();
        bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &root, nullptr);
        if (ret == false)
        {
            ELOG("json unserialize failed!\n");
            return false;
        }
        return true;
    }
};

class string_util
{
public:
    static int split(const std::string &src, const std::string &sep, std::vector<std::string> &res)
    {
        // 123,4,,5,678,9104
        int pos = 0, start = 0;
        while (start < src.size())
        {
            pos = src.find(sep, start);
            if (pos == std::string::npos)
            {
                //[start，npos] 没有sep出现了
                res.push_back(src.substr(start));
                break;
            }
            if (pos == start) // 特殊情况处理
            {
                //,,,
                start += sep.size();
                continue;
            }
            std::string cur = src.substr(start, pos - start);
            res.push_back(cur);
            start = pos + sep.size();
        }
    }
};

class file_util
{
public:
    static bool read(const std::string &filename, std::string &body)
    {
        // 1.打开文件（二进制）
        std::ifstream ifs(filename, std::ios::binary);
        if (ifs.is_open() == false)
        {
            ELOG("%s file open failed!", filename.c_str());
            return false;
        }
        // 2.获取文件大小
        ifs.seekg(0, std::ios::end);
        size_t sz = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        // std::cout << sz << std::endl;
        body.resize(sz);
        // 3.读取数据
        ifs.read(&body[0], sz);
        // 4.获取上一次操作状态
        if (ifs.good() == false)
        {
            ELOG("read %s file content fail!", filename.c_str());
            ifs.close();
            return false;
        }
        ifs.close();
        return true;
    }
};
#endif
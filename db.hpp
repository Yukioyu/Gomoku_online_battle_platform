#ifndef __M_DB_H__
#define __M_DB_H__
#include "util.hpp"
#include <mutex>
#include <assert.h>
class user_table
{
private:
    MYSQL *_mysql;
    std::mutex _mutex;

public:
    user_table(const std::string &host,
               const std::string &username,
               const std::string &psw,
               const std::string &dbname,
               uint16_t port = 3306)
    {
        _mysql = mysql_util::mysql_create(host, username, psw, dbname, port);
        assert(_mysql != NULL);
    }
    ~user_table()
    {
        mysql_util::mysql_destroy(_mysql);
    }
    // 新增用户
    bool insert(Json::Value &user)
    {
// sprintf(buffer,"%s",...)
#define INSERT_USER "insert user values(null,'%s',password('%s'),1000,0,0);"
        /*判断一下用户是否存在*/ // UNIQUE_key 数据库排错
        // Json::Value val;
        // bool ret = select_by_name(user["name"].asCString(), val);
        // if (ret == true)
        // {
        //     DLOG("user:%s is already exsits", user["name"].asCString());
        //     return false;
        // }
        /*插入*/
        char sql[4096] = {0};
        if (user["username"].isNull() || user["password"].isNull())
        {
            DLOG("INPUT PASSWORD OR USERNAME");
            return false;
        }
        sprintf(sql, INSERT_USER, user["username"].asCString(), user["password"].asCString());
        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            ELOG("insert user info failed!!\n");
            return false;
        }
        return true;
    }
    // 登录验证
    bool login(Json::Value &user)
    {
        // 以用户名和密码共同作为查询过滤条件
#define LOGIN_USER "select id,score,total_count,win_count from user where username='%s' and password=password('%s');"
        char sql[4096] = {0};
        if (user["username"].isNull() || user["password"].isNull())
        {
            DLOG("INPUT PASSWORD OR USERNAME");
            return false;
        }
        sprintf(sql, LOGIN_USER, user["username"].asCString(), user["password"].asCString());
        MYSQL_RES *res = NULL;
        {
            std::unique_lock<std::mutex> lov(_mutex);
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("user login failed!!\n");
                return false;
            }
            res = mysql_store_result(_mysql);
            /*要么有数据且只有一条，要么没有数据*/
            if (res == NULL)
            {
                DLOG("have no login user info!!\n");
                return false;
            }
        }
        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            DLOG("the user information is not unique!!\n");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);

        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res);
        return true;
    }

    // 获取用户信息
    bool select_by_name(const std::string &name, Json::Value &user)
    {
        // 以用户名作为查询过滤条件
#define SELECT_BY_NAME "select id,score,total_count,win_count from user where username='%s';"
        char sql[4096] = {0};
        sprintf(sql, SELECT_BY_NAME, name.c_str());
        MYSQL_RES *res = NULL;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("select_by_name failed!!\n");
                return false;
            }
            res = mysql_store_result(_mysql);
            /*要么有数据且只有一条，要么没有数据*/
            if (res == NULL)
            {
                DLOG("have no login user info!!\n");
                return false;
            }
        }
        int row_num = mysql_num_rows(res);
        std::cout << row_num << std::endl;
        if (row_num != 1)
        {
            DLOG("the user information is not unique!!\n");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["username"] = name;
        user["score"] = (Json::Int64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res);
        return true;
    }

    bool select_by_id(uint64_t id, Json::Value &user)
    {
        // 以用户名作为查询过滤条件
#define SELECT_BY_ID "select username,score,total_count,win_count from user where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, SELECT_BY_ID, id);
        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("select_by_id failed!!\n");
            return false;
        }
        MYSQL_RES *res = mysql_store_result(_mysql);
        /*要么有数据且只有一条，要么没有数据*/
        if (res == NULL)
        {
            DLOG("have no login user info!!\n");
            return false;
        }
        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            DLOG("the user information is not unique!!\n");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)id;
        user["username"] = row[0]; //?
        user["score"] = (Json::Int64)std::stol(row[1]);
        std::cout << "db.hpp: score(row[1])=" << std::stol(row[1]) << std::endl;
        std::cout << "db.hpp: score(user)=" << user["score"] << std::endl;
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res);
        return true;
    }
    // 赛后结果统计
    bool win(uint64_t id)
    {
#define USER_WIN "update user set score=score+30,total_count=total_count+1,win_count=win_count+1 where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, USER_WIN, id);
        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("update win user info failed!!");
            return false;
        }
        return true;
    }

    bool lose(uint64_t id)
    {
#define USER_LOSE "update user set score=score-30,total_count=total_count+1,win_count=win_count where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, USER_LOSE, id);
        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("update lose user info failed!!");
            return false;
        }
        return true;
    }
};
#endif
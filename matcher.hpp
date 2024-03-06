#ifndef __M_MATCHER_H__
#define __M_MATCHER_H__
#include "util.hpp"
#include "db.hpp"
#include "online.hpp"
#include "room.hpp"
#include <list>
#include <mutex>
#include <condition_variable>
template <class T>
class match_queue // 自定义对战队列
{
private:
    std::list<T> _list;            // 使用链表——>随时删除
    std::mutex _mutex;             // 使用锁-->多线程
    std::condition_variable _cond; // 主要是为了阻塞消费者，使用的时候 队列中的元素<2则阻塞
public:
    int size()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _list.size();
    }

    bool empty()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _list.empty();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock);
    }

    void push(const T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _list.push_back(data);
        _cond.notify_all();
    }

    bool pop(T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_list.empty())
        {
            return false;
        }
        data = _list.front();
        _list.pop_front();
        return true;
    }

    void remove(T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _list.remove(data);
    }
};

class matcher
{
private:
    match_queue<uint64_t> _q_normal;
    match_queue<uint64_t> _q_high;
    match_queue<uint64_t> _q_super;
    std::thread _th_normal;
    std::thread _th_high;
    std::thread _th_super;
    room_manager *_rm;   // 创建房间
    user_table *_ut;     // 操纵用户表
    online_manager *_om; // 推送消息
public:
    void handler_match(match_queue<uint64_t> &mq)
    {
        while (1)
        {
            while (mq.size() < 2)
            {
                // 1. 判断队列人数是否大于2，<2则阻塞
                mq.wait();
            }
            // 2.走下来代表人数够了，出队两个
            uint64_t uid1, uid2;
            bool ret = mq.pop(uid1);
            if (ret == false)
            {
                continue;
            }
            ret = mq.pop(uid2);
            if (ret == false)
            {
                this->add(uid1);
                continue;
            }
            // 3.校验两个玩家是否在线
            wsserver_t::connection_ptr conn1;
            bool cret = _om->get_conn_from_game_hall(uid1, conn1);
            if (conn1.get() == nullptr)
            {
                this->add(uid2);
                continue;
            }
            wsserver_t::connection_ptr conn2;
            cret = _om->get_conn_from_game_hall(uid2, conn2);
            if (conn2.get() == nullptr)
            {
                this->add(uid1);
                continue;
            }
            // 4.为两个玩家创建房间，并将玩家加入房间中
            room_ptr rp = _rm->create_room(uid1, uid2);
            if (rp.get() == nullptr)
            {
                this->add(uid1);
                this->add(uid2);
                continue;
            }
            // 5.广播
            Json::Value resp;
            resp["optype"] = "match_success";
            resp["result"] = true;
            resp["uid1"] = (Json::UInt64)uid1;
            resp["uid2"] = (Json::UInt64)uid2;
            std::string body;
            json_util::serialize(resp, body);
            std::cout << body << std::endl;
            conn1->send(body);
            conn2->send(body);
        }
    }

    void th_normal_entry() { return handler_match(_q_normal); }
    void th_high_entry() { return handler_match(_q_high); }
    void th_super_entry() { return handler_match(_q_super); }

public:
    matcher(room_manager *rm, user_table *ut, online_manager *om)
        : _th_normal(std::thread(&matcher::th_normal_entry, this)), _th_high(std::thread(&matcher::th_high_entry, this)), _th_super(std::thread(&matcher::th_super_entry, this)), _rm(rm), _ut(ut), _om(om)
    {
        DLOG("游戏匹配模块初始化完成...");
    }

    bool add(uint64_t uid)
    {
        Json::Value user;
        bool ret = _ut->select_by_id(uid, user);
        if (ret == false)
        {
            DLOG("获取玩家:%d 信息失败!!", uid);
            return false;
        }
        int score = user["score"].asInt();
        if (score < 2000)
        {
            _q_normal.push(uid);
        }
        else if (score >= 2000 && score < 3000)
        {
            _q_high.push(uid);
        }
        else
        {
            _q_super.push(uid);
        }
        return true;
    }

    bool del(uint64_t uid)
    {
        Json::Value user;
        bool ret = _ut->select_by_id(uid, user);
        if (ret == false)
        {
            DLOG("获取玩家:%d 信息失败!!", uid);
            return false;
        }
        int score = user["score"].asInt();
        if (score < 2000)
        {
            _q_normal.remove(uid);
        }
        else if (score >= 2000 && score < 3000)
        {
            _q_high.remove(uid);
        }
        else
        {
            _q_super.remove(uid);
        }
        return true;
    }
};
#endif
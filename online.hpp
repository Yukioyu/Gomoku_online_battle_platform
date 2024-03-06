#ifndef __M_ONLINE_H__
#define __M_ONLINE_H__
#include "util.hpp"
#include <mutex>
#include <unordered_map>
#include <utility>
class online_manager;
class online_manager
{
private:
    std::mutex _mutex;
    std::unordered_map<uint64_t, wsserver_t::connection_ptr> _hall_user;
    std::unordered_map<uint64_t, wsserver_t::connection_ptr> _room_user;

public:
    // 1.进入游戏大厅/游戏房间
    void enter_game_hall(uint64_t uid, const wsserver_t::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _hall_user.insert(std::make_pair(uid, conn));
    }

    void enter_game_room(uint64_t uid, const wsserver_t::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _room_user.insert(std::make_pair(uid, conn));
    }
    // 2.退出游戏大厅/游戏房间
    void exit_game_hall(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _hall_user.erase(uid);
    }

    void exit_game_room(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _room_user.erase(uid);
    }
    // 3.判断用户是否在游戏大厅/游戏房间
    bool in_game_hall(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _hall_user.find(uid) == _hall_user.end() ? false : true;
    }

    bool in_game_room(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _room_user.find(uid) == _hall_user.end() ? false : true;
    }
    // 4.获取指定链接
    bool get_conn_from_game_hall(uint64_t uid, wsserver_t::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        std::unordered_map<uint64_t, wsserver_t::connection_ptr>::iterator it = _hall_user.find(uid);
        if (it == _hall_user.end())
            return false;
        conn = it->second;
        return true;
    }

    bool get_conn_from_game_room(uint64_t uid, wsserver_t::connection_ptr &conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        std::unordered_map<uint64_t, wsserver_t::connection_ptr>::iterator it = _room_user.find(uid);
        if (it == _room_user.end())
            return false;
        conn = it->second;
        return true;
    }
};
#endif
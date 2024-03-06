#ifndef __M_ROOM_H__
#define __M_ROOM_H__
#include "util.hpp"
#include "online.hpp"
#include "db.hpp"
#include "logger.hpp"
#include <vector>
typedef enum
{
    GAME_START,
    GAME_OVER
} room_statu;
#define BOARD_ROW 15
#define BOARD_COL 15
#define CHESS_WHITE 1
#define CHESS_BLACK 2
class room
{
private:
    uint64_t _room_id; // 管理模块中生成
    room_statu _statu;
    int _player_count;
    uint64_t _white_id; // 添加用户接口中设置
    uint64_t _black_id;
    user_table *_tb_user;         // 为了修改用户信息
    online_manager *_online_user; // 为了获取用户在线状态以及向同一个房间中的用户推送消息hash(connection)
    std::vector<std::vector<int>> _board;

public:
    room(uint64_t room_id, user_table *tb_user, online_manager *online_user)
        : _room_id(room_id), _statu(GAME_START), _player_count(0),
          _tb_user(tb_user), _online_user(online_user),
          _board(BOARD_ROW, std::vector<int>(BOARD_COL, 0)) { DLOG("%lu 房间创建成功!!", _room_id); }
    ~room() { DLOG("%lu 房间销毁!!", _room_id); }
    uint64_t id() { return _room_id; } // 返回房间id
    room_statu statu() { return _statu; }
    int player_count() { return _player_count; }
    /*收到websocket长连接请求*/
    void add_white_id(uint64_t uid)
    {
        _white_id = uid;
        _player_count++;
    }
    void add_black_id(uint64_t uid)
    {
        _black_id = uid;
        _player_count++;
    }
    uint64_t get_white_id() { return _white_id; }
    uint64_t get_black_id() { return _black_id; }

    bool five(int row, int col, int row_off, int col_off, int color)
    {
        int count = 1;
        int search_row = row + row_off;
        int search_col = col + col_off;
        while (search_row >= 0 && search_row < BOARD_ROW &&
               search_col >= 0 && search_col < BOARD_COL &&
               _board[search_row][search_col] == color)
        {
            count++;
            search_row += row_off;
            search_col += col_off;
        }
        if (count >= 5)
            return true;
        search_row = row - row_off;
        search_col = col - col_off;
        while (search_row >= 0 && search_row < BOARD_ROW &&
               search_col >= 0 && search_col < BOARD_COL &&
               _board[search_row][search_col] == color)
        {
            count++;
            search_row -= row_off;
            search_col -= col_off;
        }
        return (count >= 5);
    }

    uint64_t check_win(int row, int col, int color)
    {
        if (five(row, col, 0, 1, color) ||
            five(row, col, 1, 0, color) ||
            five(row, col, -1, 1, color) ||
            five(row, col, 1, 1, color))
        {
            return color == CHESS_WHITE ? _white_id : _black_id;
        }
        return 0;
    }

    Json::Value handle_chess(Json::Value &req)
    {
        Json::Value json_resp = req;
        // 1.当前请求的房间号是否与当前房间的房间号匹配
        if (req["room_id"].asUInt64() != _room_id)
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = false;
            json_resp["reason"] = "房间号不匹配！";
            return json_resp;
        }
        // 2.判断房间中的两个玩家是否都在线
        if (_online_user->in_game_room(_white_id) == false)
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = true;
            json_resp["reason"] = "运气真好！对方掉线，不战而胜!";
            json_resp["room_id"] = (Json::UInt64)_room_id;
            json_resp["uid"] = req["uid"];
            json_resp["row"] = req["row"];
            json_resp["col"] = req["col"];
            json_resp["winner"] = (Json::UInt64)_black_id;
            return json_resp;
        }
        if (_online_user->in_game_room(_black_id) == false)
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = true;
            json_resp["reason"] = "运气真好！对方掉线，不战而胜!";
            json_resp["room_id"] = (Json::UInt64)_room_id;
            json_resp["uid"] = req["uid"];
            json_resp["row"] = req["row"];
            json_resp["col"] = req["col"];
            json_resp["winner"] = (Json::UInt64)_white_id;
            return json_resp;
        }
        // 3.获取位置，判断是否合理
        int row = req["row"].asInt();
        int col = req["col"].asInt();
        if (_board[row][col] != 0)
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = false;
            json_resp["reason"] = "当前位置已经有了棋子!";
            return json_resp;
        }
        int cur_color = req["uid"].asUInt64() == _white_id ? CHESS_WHITE : CHESS_BLACK;
        _board[row][col] = cur_color;
        std::cout << "in_chess: " << cur_color << std::endl;
        // 4.判断是否有玩家胜利
        uint64_t winner_id = check_win(row, col, cur_color);
        if (winner_id != 0)
        {
            json_resp["reason"] = "五星连珠，战无敌!!";
        }
        json_resp["result"] = true;
        json_resp["winner"] = (Json::UInt64)winner_id;
        return json_resp;
    }

    Json::Value handle_chat(Json::Value &req)
    {
        Json::Value json_resp = req;
        // 1.当前请求的房间号是否与当前房间的房间号匹配
        if (req["room_id"].asUInt64() != _room_id)
        {
            json_resp["result"] = false;
            json_resp["reason"] = "房间号不匹配！";
            return json_resp;
        }
        // 2.检测消息中是否有敏感词
        std::string msg = req["message"].asString();
        size_t pos = msg.find("垃圾");
        if (pos != std::string::npos)
        {
            json_resp["result"] = false;
            json_resp["reason"] = "消息中包含敏感词！不能发送";
            return json_resp;
        }
        // 3.广播消息
        json_resp["result"] = true;
        return json_resp;
    }

    void handle_exit(uint64_t uid)
    {
        // 1.判断退出原因(如果仍然在下棋状态，一方退出，另一方直接胜利)
        //              如果处于结束状态，一方退出，则由handle_chess确定
        Json::Value json_resp;
        if (_statu == GAME_START)
        {
            uint64_t winner_id = (Json::UInt64)(uid == _white_id ? _black_id : _white_id);
            json_resp["optype"] = "put_chess";
            json_resp["result"] = true;
            json_resp["reason"] = "运气真好！对方掉线，不战而胜!";
            json_resp["room_id"] = (Json::UInt64)_room_id;
            json_resp["uid"] = (Json::UInt64)uid;
            json_resp["row"] = -1;
            json_resp["col"] = -1;
            json_resp["winner"] = (Json::UInt64)winner_id;
            // 更新数据库信息
            uint64_t loser_id = winner_id == _white_id ? _black_id : _white_id;
            _tb_user->win(winner_id);
            _tb_user->lose(loser_id);
            // 广播消息
            broadcast(json_resp);
        }
        _player_count--;
        return;
    }

    void handle_request(Json::Value &req)
    {
        Json::Value json_resp = req;
        // 1.当前请求的房间号是否与当前房间的房间号匹配
        if (req["room_id"].asUInt64() != _room_id)
        {
            json_resp["result"] = false;
            json_resp["reason"] = "房间号不匹配！";
            return;
        }
        // 2.根据不同的请求类型匹配不同的处理函数
        if (req["optype"].asString() == "put_chess")
        {
            json_resp = handle_chess(req);
            if (!json_resp["winner"].isNull())
            {
                uint64_t winner_id = json_resp["winner"].asUInt64();
                uint64_t loser_id = winner_id == _white_id ? _black_id : _white_id;
                // 更改数据库
                _tb_user->win(winner_id);
                _tb_user->lose(loser_id);
                _statu = GAME_OVER;
            }
        }
        else if (req["optype"].asString() == "chat")
        {
            json_resp = handle_chat(req);
        }
        else
        {
            json_resp["optype"] = req["optype"].asString();
            json_resp["result"] = false;
            json_resp["reason"] = "未知请求类型";
        }

        std::string body;
        json_util::serialize(json_resp, body);
        DLOG("房间广播动作：%s", body.c_str());
        return broadcast(json_resp);
    }

    void broadcast(Json::Value &rsp)
    {
        // 1.对要响应的信息进行序列化，将Json::Value中的数据序列化成json格式字符串
        std::string body;
        json_util::serialize(rsp, body);
        // 2.获取房间中所有用户的通信连接
        // 3.发送响应信息
        wsserver_t::connection_ptr wconn;
        if (_online_user->get_conn_from_game_room(_white_id, wconn))
        {
            wconn->send(body);
        }
        else
        {
            DLOG("房间—白棋玩家连接获取失败");
        }
        wsserver_t::connection_ptr bconn;
        if (_online_user->get_conn_from_game_room(_black_id, bconn))
        {
            bconn->send(body);
        }
        else
        {
            DLOG("房间—黑棋玩家连接获取失败");
        }
        return;
    }
};

using room_ptr = std::shared_ptr<room>;

class room_manager
{
private:
    uint64_t _next_rid; // 房间计数器
    std::mutex _mutex;

    user_table *_tb_user;
    online_manager *_online_user;

    std::unordered_map<uint64_t, room_ptr> _rooms;
    std::unordered_map<uint64_t, uint64_t> _users;

public:
    /*初始化房间ID计数器*/
    room_manager(user_table *ut, online_manager *om)
        : _tb_user(ut), _online_user(om)
    {
        DLOG("房间管理模块初始化完毕!");
    }
    ~room_manager()
    {
        DLOG("房间管理模块销毁完毕!");
    }
    /*为两个用户创建房间，并返回房间的智能指针管理对象*/
    room_ptr create_room(uint64_t uid1, uint64_t uid2)
    {
        // 两个用户在游戏大厅中进行对战匹配，匹配成功则创建游戏房间
        // 1.校验两个用户是否都还在游戏大厅
        if (!_online_user->in_game_hall(uid1))
        {
            DLOG("用户:%lu 不在大厅中，创建房间失败！", uid1);
            return room_ptr();
        }
        if (!_online_user->in_game_hall(uid2))
        {
            DLOG("用户:%lu 不在大厅中，创建房间失败！", uid2);
            return room_ptr();
        }
        // 2.创建房间，将用户信息添加到游戏房间中
        room_ptr rp(new room(_next_rid, _tb_user, _online_user));
        rp->add_white_id(uid1);
        rp->add_black_id(uid2);
        // 3.将房间信息管理起来
        std::unique_lock<std::mutex> lock(_mutex);
        _rooms.insert(std::make_pair(_next_rid, rp));
        _users.insert(std::make_pair(uid1, _next_rid));
        _users.insert(std::make_pair(uid2, _next_rid));
        _next_rid++;
        // 4.返回房间信息
        return rp;
    }
    /*通过房间ID获取房间信息*/
    room_ptr get_room_by_rid(uint64_t rid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _rooms.find(rid);
        if (it == _rooms.end())
        {
            return room_ptr();
        }
        return it->second;
    }
    /*通过用户ID获取房间信息*/
    room_ptr get_room_by_uid(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        // 1.通过用户ID获取房间ID
        auto it = _users.find(uid);
        std::cout << "get_room_by_uid:" << uid << std::endl;
        if (it == _users.end())
        {
            return room_ptr();
        }
        uint64_t rid = it->second;
        std::cout << "get_room_by_uid-rid:: " << rid << std::endl;
        // 2.通过用户ID获取房间ID
        // return get_room_by_rid(rid);  -->锁冲突
        auto rit = _rooms.find(rid);
        if (rit == _rooms.end())
        {
            return room_ptr();
        }
        return rit->second;
    }
    /*通过房间ID销毁房间*/
    void remove_room(uint64_t rid)
    {
        // 因为房间信息是通过shared_ptr在hash中管理的，只要将shared_ptr从_rooms中移除
        // 则shared_ptr==0 外界没有对房间信息进行操作保存的状况下就会释放掉
        // 1.通过房间ID,获取房间信息
        room_ptr rp = get_room_by_rid(rid);
        if (rp.get() == nullptr)
            return;
        // 2.通过房间信息，获取房间中所有用户的id
        // 3.移除房间管理中的用户信息
        // 4.移除房间管理信息
        uint64_t uid1 = rp->get_white_id();
        uint64_t uid2 = rp->get_black_id();
        std::unique_lock<std::mutex> lock(_mutex);
        _users.erase(uid1);
        _users.erase(uid2);
        _rooms.erase(rid);
    }
    /*删除房间中指定用户，如果房间内没有用户了，则销毁房间，用户断开连接时调用*/
    void remove_user(uint64_t uid)
    {
        room_ptr rp = get_room_by_uid(uid);
        if (rp.get() == nullptr)
            return;
        // 处理房间中玩家退出动作
        rp->handle_exit(uid);
        // 房间中没有玩家了，销毁房间
        if (rp->player_count() == 0)
            remove_room(rp->id());
        return;
    }
};
#endif
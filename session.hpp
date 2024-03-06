#ifndef __M_SS_H__
#define __M_SS_H__
#include "util.hpp"
#include <iostream>
#include <unordered_map>
#include <mutex>
#define SESSION_FOREVER -1
#define SESSION_TIMEOUT 300000
typedef enum
{
    UNLOGIN,
    LOGIN
} ss_stu;
class session
{
private:
    uint64_t _ssid;            // 标识符
    uint64_t _uid;             // session对应的用户ID
    ss_stu _statu;             // 用户状态：未登录，已登录
    wsserver_t::timer_ptr _tp; // session关联的定时器
public:
    session(uint64_t ssid) : _ssid(ssid) { DLOG("SESSION %p 被创建!!", this); }
    ~session() { DLOG("SESSION %p 被释放!!", this); }

    void set_statu(ss_stu statu) { _statu = statu; }
    void set_user(uint64_t uid) { _uid = uid; }
    void set_timer(const wsserver_t::timer_ptr &tp) { _tp = tp; }
    uint64_t ssid() { return _ssid; }
    uint64_t get_user() { return _uid; }
    bool is_login() { return (_statu == LOGIN); }
    wsserver_t::timer_ptr &get_timer() { return _tp; }
};

using session_ptr = std::shared_ptr<session>;
class session_manager
{
private:
    uint64_t _next_ssid;
    std::mutex _mutex;
    std::unordered_map<uint64_t, session_ptr> _session;
    wsserver_t *_server;

public:
    session_manager(wsserver_t *srv) : _next_ssid(1), _server(srv)
    {
        DLOG("session管理器初始化完毕!");
    }

    ~session_manager() { DLOG("session管理器即将销毁!"); }

    session_ptr create_session(uint64_t uid, ss_stu statu)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        session_ptr ssp(new session(_next_ssid));
        ssp->set_statu(statu);
        ssp->set_user(uid);
        _session.insert(std::make_pair(_next_ssid, ssp));
        _next_ssid++;
        return ssp;
    }

    session_ptr get_session_by_ssid(uint64_t ssid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _session.find(ssid);
        if (it == _session.end())
            return session_ptr();
        return it->second;
    }

    void remove_session(uint64_t ssid)
    {
        session_ptr ssp = get_session_by_ssid(ssid);
        std::cout << "remove_session: " << ssid << std::endl;
        std::cout << "before remove,_session: " << std::endl;
        for (auto &e : _session)
        {
            std::cout << e.first << std::endl;
        }
        std::cout << "--------------------------" << std::endl;
        std::unique_lock<std::mutex> lock(_mutex);
        _session.erase(ssid);
        std::cout << "after remove,_session: " << std::endl;
        for (auto &e : _session)
        {
            std::cout << e.first << std::endl;
        }
        std::cout << "--------------------------" << std::endl;
    }

    void append_session(const session_ptr &ssp)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _session.insert(std::make_pair(ssp->ssid(), ssp));
    }

    void set_session_expire_time(uint64_t ssid, int ms)
    {
        std::cout << "set_session_expire_time" << std::endl;
        // 依赖websocketpp的定时器来完成session生命周期的管理
        // 1. 登录后创建session,session需要在指定时间无通信后删除
        // 2. 进入游戏大厅或游戏房间后 s,ession 应该永久存在
        // 3. 等到退出游戏大厅或者游戏房间，session重新设为临时，长时间无通信后删除
        session_ptr ssp = get_session_by_ssid(ssid);
        if (ssp.get() == nullptr)
            return;

        wsserver_t::timer_ptr tp = ssp->get_timer();
        if (tp.get() == nullptr && ms == SESSION_FOREVER)
        {
            // 1. 在session永久存在的情况下，设置永久存在
            return;
        }
        else if (tp.get() == nullptr && ms == SESSION_TIMEOUT)
        {
            // 2.在session永久存在的情况下，设置定时删除
            wsserver_t::timer_ptr tmp_tp = _server->set_timer(ms,
                                                              std::bind(&session_manager::remove_session, this, ssid));
            ssp->set_timer(tmp_tp);
        }
        else if (tp.get() != nullptr && ms == SESSION_FOREVER)
        {
            // 3.在session设置了定时删除的情况下，设置为永久有效
            tp->cancel();                            // 删除同时执行
            ssp->set_timer(wsserver_t::timer_ptr()); // 将定时器置空
            _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp));
        }
        else if (tp.get() != nullptr && ms == SESSION_TIMEOUT)
        {
            // 4.在session设置了定时删除的情况下，重置删除时间
            tp->cancel(); // 取消定时任务不是立刻取消的（删除没有立刻执行）
            // 重新插入session不能立刻执行，而要放到定时器里面（排队）
            ssp->set_timer(wsserver_t::timer_ptr()); // 将定时器置空
            _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp));
            // 给session重新设置计时器
            wsserver_t::timer_ptr tmp_tp = _server->set_timer(ms,
                                                              std::bind(&session_manager::remove_session, this, ssid));
            ssp->set_timer(tmp_tp);
        }
    }
};

#endif
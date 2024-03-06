#ifndef __M_SRV_H__
#define __M_SRV_H__
#include "db.hpp"
#include "matcher.hpp"
#include "online.hpp"
#include "room.hpp"
#include "session.hpp"
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#define WWW_ROOT "./wwwroot"
class gobang_server
{
private:
    std::string _web_root; // 静态资源目录 ./wwroot/ /register.html
    user_table _ut;        // 用户信息操控
    matcher _mm;           // 用户匹配
    online_manager _om;    // 在线用户管理
    room_manager _rm;      // 房间管理模块
    session_manager _sm;   // 会话管理
    wsserver_t _wssrv;

private:
    void file_handler(wsserver_t::connection_ptr &conn)
    {
        // 静态资源请求的处理
        // 1. 获取到请求uri-资源路径，了解客户端请求的页面文件名称
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        // 2. 组合出文件的实际路径 相对根目录+uri
        // 3.如果请求的是个目录，增加一个后缀 login.html  / -> /login.html
        if (uri == "/")
            uri += "login.html";
        std::string pathname = _web_root + uri;
        std::cout << pathname << std::endl;
        // 4.读取文件内容
        std::string body;
        bool ret = file_util::read(pathname, body);
        // 4.1文件不存在，读取内容失败，返回404
        if (ret == false)
        {
            body += "<html>";
            body += "<head>";
            body += "<meta charset='UTF-8'/>";
            body += "</head>";
            body += "<body>";
            body += "<h1> Not Found </h1>";
            body += "</body>";
            conn->set_status(websocketpp::http::status_code::not_found);
            conn->set_body(body);
            return;
        }
        conn->set_status(websocketpp::http::status_code::ok);
        conn->set_body(body);
    }
    void reg(wsserver_t::connection_ptr &conn)
    {
        // 用户注册功能的处理
        websocketpp::http::parser::request req = conn->get_request();
        // 1. 获取到请求正文
        std::string const req_body = conn->get_request_body();
        // 2. 对正文进行json反序列化，得到用户名和密码
        Json::Value login_info;
        bool ret = json_util::unserialize(req_body, login_info);
        if (ret == false)
        {
            DLOG("[reg] 反序列化注册信息失败");
            http_resp(false, websocketpp::http::status_code::bad_request, "请求的正文格式错误", conn);
            return;
        }
        // 3. 进行数据库的用户新增操作
        if (login_info["username"].isNull() || login_info["password"].isNull())
        {
            DLOG("[reg] 用户密码不完整");
            http_resp(false, websocketpp::http::status_code::bad_request, "请输入用户名/密码", conn);
            return;
        }
        ret = _ut.insert(login_info);
        if (ret == false)
        {
            DLOG("[reg] 向数据库插入失败");
            http_resp(false, websocketpp::http::status_code::bad_request, "用户名已经被占用", conn);
            return;
        }
        // 成功，返回200
        http_resp(true, websocketpp::http::status_code::ok, "用户注册成功", conn);
    }

    void http_resp(bool result, websocketpp::http::status_code::value code,
                   const std::string &reason, wsserver_t::connection_ptr &conn)
    {
        Json::Value resp_json; // 响应信息
        resp_json["result"] = result;
        resp_json["reason"] = reason;
        std::string resp_body;
        json_util::serialize(resp_json, resp_body);
        conn->set_status(code);
        conn->set_body(resp_body);
        conn->append_header("Content-Type", "application/json");
        return;
    }
    void login(wsserver_t::connection_ptr &conn)
    {
        // 用户登录请求功能的处理
        websocketpp::http::parser::request req = conn->get_request();
        // 1. 获取请求正文，并进行json反序列化，得到用户名和密码
        std::string const req_body = conn->get_request_body();
        Json::Value login_info;
        bool ret = json_util::unserialize(req_body, login_info);
        if (ret == false)
        {
            DLOG("[log] 反序列化登录信息失败");
            http_resp(false, websocketpp::http::status_code::bad_request, "请求的正文格式错误", conn);
            return;
        }
        // 2. 校验正文完整性，进行数据库的用户信息验证
        // 2.1 验证失败，返回 400
        if (login_info["username"].isNull() || login_info["password"].isNull())
        {
            DLOG("[log] 用户密码不完整");
            http_resp(false, websocketpp::http::status_code::bad_request, "请输入用户名/密码", conn);
            return;
        }
        ret = _ut.login(login_info);
        if (ret == false)
        {
            DLOG("[log] 用户名密码错误");
            http_resp(false, websocketpp::http::status_code::bad_request, "请输入用户名/密码", conn);
            return;
        }
        // 3. 验证成功，创建session
        uint64_t uid = login_info["id"].asUInt64();
        session_ptr ssp = _sm.create_session(uid, LOGIN);
        if (ssp.get() == nullptr)
        {
            DLOG("[log] 创建会话失败");
            http_resp(false, websocketpp::http::status_code::internal_server_error, "请输入用户名/密码", conn);
            return;
        }
        _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
        std::cout << "[login]:"
                  << "session_time" << std::endl; //???
        // 4. 设置响应头部: Set-Cookie,将session 返回客户端
        std::string cookie_ssid = "SSID=" + std::to_string(ssp->ssid());
        conn->append_header("Set-Cookie", cookie_ssid);
        http_resp(true, websocketpp::http::status_code::ok, "登陆成功", conn);
        return;
    }
    bool get_cookie_val(const std::string &cookie_str, const std::string &key, std::string &val)
    {
        // Cookie: SSID=XXX;path=/;
        // 1.以;作为分割，对字符串进行分割，得到各个单个的cookie信息
        std::string sep = ";";
        std::vector<std::string> cookie_arr;
        string_util::split(cookie_str, sep, cookie_arr);
        for (auto &e : cookie_arr)
        {
            // 2.对单个cookie字符串，以 = 为间隔进行分割，得到key和val
            std::vector<std::string> tmp_arr;
            string_util::split(e, "=", tmp_arr);
            if (tmp_arr.size() != 2)
                continue;
            if (tmp_arr[0] == key)
            {
                val = tmp_arr[1];
                return true;
            }
        }
        return false;
    }
    void info(wsserver_t::connection_ptr &conn)
    {
        // 用户信息获取功能的处理
        // 1.获取请求信息中的Cookie
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty())
        {
            // 如果没有cookie，则返回错误，让客户端重新登陆
            return http_resp(false, websocketpp::http::status_code::bad_request, "find no cookie,请重新登录", conn);
        }
        std::cout << "cookie_str" << cookie_str << std::endl;
        // 1.2从cookie中提取ssid
        std::string ssid_str;
        bool ret = get_cookie_val(cookie_str, "SSID", ssid_str);
        if (ret == false)
        {
            // cookie中没有ssid，返回错误;没有ssid信息，让客户端重新登陆
            return http_resp(false, websocketpp::http::status_code::bad_request, "cookie wrong,请重新登录", conn);
        }
        std::cout << "ssid_str" << ssid_str << std::endl;
        // 2.在session管理中查找对应的对话信息
        session_ptr ssp = _sm.get_session_by_ssid(std::stoi(ssid_str));
        if (ssp.get() == nullptr)
        {
            //  没有找到session，则认为登录已经过期，需要重新登陆
            return http_resp(false, websocketpp::http::status_code::bad_request, "session expired,请重新登录", conn);
        }
        // 3.从数据库中取出用户信息，进行序列化发送给客户端
        uint64_t uid = ssp->get_user();
        Json::Value user_info;
        ret = _ut.select_by_id(uid, user_info);
        if (ret == false)
        {
            // 获取用户信息失败，返回错误；找不到用户信息
            return http_resp(false, websocketpp::http::status_code::bad_request, "find no user,请重新登录", conn);
        }
        std::string body;
        json_util::serialize(user_info, body);
        DLOG("info get: %s", user_info);
        conn->set_body(body);
        conn->append_header("Content-Type", "application/json");
        conn->set_status(websocketpp::http::status_code::ok);
        // 4.刷新session的过期时间
        _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
        std::cout << "[info]:"

                  << "session_time" << std::endl;
    }

    void http_callback(websocketpp::connection_hdl hdl)
    {
        wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        // 1. 获取到请求uri-资源路径，了解客户端请求的页面文件名称
        websocketpp::http::parser::request req = conn->get_request();
        std::string method = req.get_method();
        std::string uri = req.get_uri();
        if (method == "POST" && uri == "/reg")
        {
            return reg(conn);
        }
        else if (method == "POST" && uri == "/login")
        {
            return login(conn);
        }
        else if (method == "GET" && uri == "/info")
        {
            return info(conn);
        }
        else
        {
            return file_handler(conn);
        }
    }
    void ws_resp(Json::Value resp, wsserver_t::connection_ptr &conn)
    {
        std::string body;
        json_util::serialize(resp, body);
        conn->send(body);
    }

    session_ptr get_session_by_cookie(wsserver_t::connection_ptr conn)
    {
        // 1. 登录验证:判断当前客户端是否已经成功登录
        Json::Value err_resp;
        // 1.获取请求信息中的Cookie
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty())
        {
            // 如果没有cookie，则返回错误，让客户端重新登陆
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "没有找到cookie信息,需要重新登陆";
            err_resp["result"] = false;
            ws_resp(err_resp, conn);
            return session_ptr();
        }
        // 1.2从cookie中提取ssid
        std::string ssid_str;
        bool ret = get_cookie_val(cookie_str, "SSID", ssid_str);
        if (ret == false)
        {
            // cookie中没有ssid，返回错误;没有ssid信息，让客户端重新登陆
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "没有找到ssid信息,需要重新登陆";
            err_resp["result"] = false;
            ws_resp(err_resp, conn);
            return session_ptr();
        }
        // 2.在session管理中查找对应的对话信息
        session_ptr ssp = _sm.get_session_by_ssid(std::stoi(ssid_str));
        if (ssp.get() == nullptr)
        {
            //  没有找到session，则认为登录已经过期，需要重新登陆
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "没有找到session信息，需要重新登陆";
            err_resp["result"] = false;
            ws_resp(err_resp, conn);
            return session_ptr();
        }
        return ssp;
    }

    void wsopen_game_hall(wsserver_t::connection_ptr conn)
    {
        // 游戏大厅长连接建立成功
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;
        // 2. 判断当前客户端是否是二次重复登录
        Json::Value err_resp;
        if (_om.in_game_hall(ssp->get_user()) || _om.in_game_room(ssp->get_user()))
        {
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "玩家重复登录!";
            err_resp["result"] = false;
            ws_resp(err_resp, conn);
            return;
        }
        // 3. 将当前客户端以及连接加入到游戏大厅enter_game_hall
        _om.enter_game_hall(ssp->get_user(), conn);
        // 4. 给客户端响应游戏大厅连接建立成功
        Json::Value resp_json;
        resp_json["optype"] = "hall_ready";
        resp_json["result"] = true;
        ws_resp(resp_json, conn);
        // 5. 记得将sessin设置为永久存在
        _sm.set_session_expire_time(ssp->ssid(), SESSION_FOREVER);
        std::cout << "[wsopen_game_hall]:"
                  << "session_time" << std::endl;
    }

    void wsopen_game_room(wsserver_t::connection_ptr conn)
    {
        // 1. 获取当前客户端的session
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;
        // 2. 当前用户是否已经在游戏房间(只有连接建立后才有，连接建立前没有)或者游戏大厅中---在线用户管理
        Json::Value err_resp;
        if (_om.in_game_hall(ssp->get_user()) || _om.in_game_room(ssp->get_user()))
        {
            err_resp["optype"] = "room_ready";
            err_resp["reason"] = "玩家重复登录!";
            err_resp["result"] = false;
            ws_resp(err_resp, conn);
            return;
        }
        // 3. 判断当前用户是否已经创建好房间---房间管理
        room_ptr rp = _rm.get_room_by_uid(ssp->get_user());
        std::cout << "server:wsopen_game_room" << rp << std::endl;
        if (rp.get() == nullptr)
        {
            err_resp["optype"] = "room_ready";
            err_resp["reason"] = "没有找到玩家的房间信息";
            err_resp["result"] = false;
            ws_resp(err_resp, conn);
            return;
        }
        // 4. 将当前用户添加到在线用户管理的游戏房间中
        _om.enter_game_room(ssp->get_user(), conn);
        // 5. 将session设置为永久存在
        _sm.set_session_expire_time(ssp->ssid(), SESSION_FOREVER);
        // 6. 回复:房间准备完毕
        Json::Value resp_json;
        resp_json["optype"] = "room_ready";
        resp_json["result"] = true;
        resp_json["room_id"] = (Json::UInt64)rp->id();
        resp_json["uid"] = (Json::UInt64)ssp->get_user();
        resp_json["white_id"] = (Json::UInt64)rp->get_white_id();
        resp_json["black_id"] = (Json::UInt64)rp->get_black_id();
        // std::cout << resp_json << std::endl;
        ws_resp(resp_json, conn);
    }

    void wsopen_callback(websocketpp::connection_hdl hdl)
    {
        // websocket长连接建立成功后的处理函数
        wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        if (uri == "/hall")
        {
            // 建立了游戏大厅的长连接
            return wsopen_game_hall(conn);
        }
        else if (uri == "/room")
        {
            // 建立了游戏房间的长连接
            return wsopen_game_room(conn);
        }
    }

    void wsclose_game_hall(wsserver_t::connection_ptr conn)
    {
        // 游戏大厅长连接断开的处理
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;
        // 1. 将玩家从游戏大厅中移除
        _om.exit_game_hall(ssp->get_user());
        // 2. 将session恢复生命周期的管理，设置定时销毁
        _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
    }

    void wsclose_game_room(wsserver_t::connection_ptr conn)
    {
        // 游戏房间长连接断开的处理
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;
        // 1. 将玩家从游戏房间中移除---在线管理模块
        _om.exit_game_room(ssp->get_user());
        // 2. 将session恢复生命周期的管理，设置定时销毁
        _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
        // 3. 退出游戏房间---房间管理模块
        _rm.remove_user(ssp->get_user());
    }

    void wsclose_callback(websocketpp::connection_hdl hdl)
    {
        // websocket连接断开前的处理
        wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        if (uri == "/hall")
        {
            // 建立了游戏大厅的长连接
            return wsclose_game_hall(conn);
        }
        else if (uri == "/room")
        {
            // 建立了游戏房间的长连接
            return wsclose_game_room(conn);
        }
    }

    void wsmsg_game_hall(wsserver_t::connection_ptr conn, wsserver_t::message_ptr msg)
    {
        // 1.身份验证，当前客户端是哪个玩家
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;
        // 2.获取请求信息
        std::string req_body = msg->get_payload();
        Json::Value req_json;
        bool ret = json_util::unserialize(req_body, req_json);
        if (ret == false)
        {
            req_json["result"] = false;
            req_json["reason"] = "请求信息解析失败";
            return ws_resp(req_json, conn);
        }
        // 3.对于请求进行处理
        Json::Value resp_json;
        //  3.1开始对战匹配：通过匹配模块，将用户添加到匹配队列
        if (!req_json["optype"].isNull() && req_json["optype"].asString() == "match_start")
        {
            _mm.add(ssp->get_user());
            resp_json["optype"] = "match_start";
            resp_json["result"] = true;
            return ws_resp(resp_json, conn);
        }
        // 3.2停止对战匹配 通过匹配模块，将用户从匹配队列中移除
        if (!req_json["optype"].isNull() && req_json["optype"].asString() == "match_stop")
        {
            _mm.del(ssp->get_user());
            resp_json["optype"] = "match_stop";
            resp_json["result"] = true;
            return ws_resp(resp_json, conn);
        }
        // 4.给客户端响应
        resp_json["optype"] = "unkown";
        resp_json["result"] = false;
        return ws_resp(resp_json, conn);
    }

    void wsmsg_game_room(wsserver_t::connection_ptr conn, wsserver_t::message_ptr msg)
    {
        // 1. 获取客户端session,识别客户端身份
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
            return;
        // 2. 获取客户端房间信息
        Json::Value err_resp;
        room_ptr rp = _rm.get_room_by_uid(ssp->get_user());
        if (rp.get() == nullptr)
        {
            err_resp["optype"] = "room_ready";
            err_resp["reason"] = "没有找到玩家的房间信息";
            err_resp["result"] = false;
            ws_resp(err_resp, conn);
            return;
        }
        // 3. 对消息进行反序列化
        std::string req_body = msg->get_payload();
        Json::Value req_json;
        bool ret = json_util::unserialize(req_body, req_json);
        if (ret == false)
        {
            err_resp["optype"] = "unkown";
            err_resp["reason"] = "请求解析失败";
            err_resp["result"] = false;
            ws_resp(err_resp, conn);
            return;
        }
        // 4.通过房间模块进行消息请求处理
        return rp->handle_request(req_json);
    }
    void wsmsg_callback(websocketpp::connection_hdl hdl, wsserver_t::message_ptr msg)
    {
        // websocket长连接通信处理
        wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        if (uri == "/hall")
        {
            // 建立了游戏大厅的长连接
            return wsmsg_game_hall(conn, msg);
        }
        else if (uri == "/room")
        {
            // 建立了游戏房间的长连接
            return wsmsg_game_room(conn, msg);
        }
    }

public:
    /*成员初始化以及回调函数的设置*/
    gobang_server(const std::string &host,
                  const std::string &username,
                  const std::string &psw,
                  const std::string &dbname,
                  uint16_t port = 3306,
                  const std::string &wwwroot = WWW_ROOT)
        : _ut(host, username, psw, dbname, port),
          _rm(&_ut, &_om), _sm(&_wssrv), _mm(&_rm, &_ut, &_om), _web_root(wwwroot)
    {
        _wssrv.set_access_channels(websocketpp::log::alevel::none);
        _wssrv.init_asio();
        _wssrv.set_http_handler(std::bind(&gobang_server::http_callback, this, std::placeholders::_1));
        _wssrv.set_open_handler(std::bind(&gobang_server::wsopen_callback, this, std::placeholders::_1));
        _wssrv.set_close_handler(std::bind(&gobang_server::wsclose_callback, this, std::placeholders::_1));
        _wssrv.set_message_handler(std::bind(&gobang_server::wsmsg_callback, this, std::placeholders::_1, std::placeholders::_2));
    }
    /*启动服务器*/
    void start(uint64_t port)
    {
        _wssrv.listen(port);
        _wssrv.start_accept();
        _wssrv.run();
    }
};

#endif
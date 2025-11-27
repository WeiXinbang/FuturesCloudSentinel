#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

using boost::asio::ip::tcp;
using json = nlohmann::json;

// --- 协议定义 ---
class ChatMessage {
public:
    enum { header_length = 4 };
    enum { max_body_length = 4096 }; // 增加缓冲区大小以适应较大的JSON

    ChatMessage() : body_length_(0) {
    }

    const char* data() const { return data_; }
    char* data() { return data_; }

    std::size_t length() const { return header_length + body_length_; }

    const char* body() const { return data_ + header_length; }
    char* body() { return data_ + header_length; }

    std::size_t body_length() const { return body_length_; }

    void body_length(std::size_t new_length) {
        body_length_ = new_length;
        if (body_length_ > max_body_length)
            body_length_ = max_body_length;
    }

    bool decode_header() {
        char header[header_length + 1] = "";
        std::strncat(header, data_, header_length);
        body_length_ = std::atoi(header);
        if (body_length_ > max_body_length) {
            body_length_ = 0;
            return false;
        }
        return true;
    }

    void encode_header() {
        char header[header_length + 1] = "";
        std::sprintf(header, "%4d", static_cast<int>(body_length_));
        std::memcpy(data_, header, header_length);
    }

private:
    char data_[header_length + max_body_length];
    std::size_t body_length_;
};
// ----------------

typedef std::deque<ChatMessage> chat_message_queue;

// 定义回调函数类型：UI层通过此回调接收服务器消息
typedef std::function<void(const json&)> MessageCallback;

class FuturesClient {
public:
    FuturesClient(boost::asio::io_context& io_context,
               const tcp::resolver::results_type& endpoints)
        : io_context_(io_context),
          socket_(io_context),
          request_id_counter_(0) {
        do_connect(endpoints);
    }

    // --- UI 层注册回调 ---
    void set_message_callback(MessageCallback callback) {
        message_callback_ = callback;
    }

    // --- 业务接口 (供 UI 事件调用) ---

    void register_user(const std::string& username, const std::string& password) {
        json j;
        j["type"] = "register";
        j["request_id"] = generate_request_id();
        j["username"] = username;
        j["password"] = password;
        send_json(j);
    }

    void login(const std::string& username, const std::string& password) {
        json j;
        j["type"] = "login";
        j["request_id"] = generate_request_id();
        j["username"] = username;
        j["password"] = password; 
        send_json(j);
    }

    void set_email(const std::string& email) {
        json j;
        j["type"] = "set_email";
        j["request_id"] = generate_request_id();
        j["email"] = email;
        send_json(j);
    }

    void add_price_warning(const std::string& account, const std::string& symbol, double max_price, double min_price) {
        json j;
        j["type"] = "add_warning";
        j["request_id"] = generate_request_id();
        j["warning_type"] = "price";
        j["account"] = account;
        j["symbol"] = symbol;
        j["max_price"] = max_price;
        j["min_price"] = min_price;
        send_json(j);
    }

    void add_time_warning(const std::string& account, const std::string& symbol, const std::string& trigger_time) {
        json j;
        j["type"] = "add_warning";
        j["request_id"] = generate_request_id();
        j["warning_type"] = "time";
        j["account"] = account;
        j["symbol"] = symbol;
        j["trigger_time"] = trigger_time;
        send_json(j);
    }
    
    void delete_warning(const std::string& order_id) {
        json j;
        j["type"] = "delete_warning";
        j["request_id"] = generate_request_id();
        j["order_id"] = order_id;
        send_json(j);
    }

    void modify_price_warning(const std::string& order_id, double max_price, double min_price) {
        json j;
        j["type"] = "modify_warning";
        j["request_id"] = generate_request_id();
        j["order_id"] = order_id;
        j["warning_type"] = "price";
        j["max_price"] = max_price;
        j["min_price"] = min_price;
        send_json(j);
    }

    void modify_time_warning(const std::string& order_id, const std::string& trigger_time) {
        json j;
        j["type"] = "modify_warning";
        j["request_id"] = generate_request_id();
        j["order_id"] = order_id;
        j["warning_type"] = "time";
        j["trigger_time"] = trigger_time;
        send_json(j);
    }

    void query_warnings(const std::string& status_filter) {
        json j;
        j["type"] = "query_warnings";
        j["request_id"] = generate_request_id();
        j["status_filter"] = status_filter;
        send_json(j);
    }

    void close() {
        boost::asio::post(io_context_, [this]() { socket_.close(); });
    }

private:
    std::string generate_request_id() {
        return "req_" + std::to_string(++request_id_counter_);
    }

    void send_json(const json& j) {
        std::string s = j.dump();
        ChatMessage msg;
        msg.body_length(s.size());
        std::memcpy(msg.body(), s.data(), msg.body_length());
        msg.encode_header();
        write(msg);
    }

    void write(const ChatMessage& msg) {
        boost::asio::post(io_context_,
            [this, msg]() {
                bool write_in_progress = !write_msgs_.empty();
                write_msgs_.push_back(msg);
                if (!write_in_progress) {
                    do_write();
                }
            });
    }

    void do_connect(const tcp::resolver::results_type& endpoints) {
        boost::asio::async_connect(socket_, endpoints,
            [this](boost::system::error_code ec, tcp::endpoint) {
                if (!ec) {
                    std::cout << "[Client] Connected to server." << std::endl;
                    do_read_header();
                } else {
                    std::cerr << "[Client] Connect failed: " << ec.message() << std::endl;
                }
            });
    }

    void do_read_header() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data(), ChatMessage::header_length),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec && read_msg_.decode_header()) {
                    do_read_body();
                } else {
                    socket_.close();
                }
            });
    }

    void do_read_body() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    // 解析收到的消息
                    std::string received_data(read_msg_.body(), read_msg_.body_length());
                    try {
                        json j = json::parse(received_data);
                        handle_message(j);
                    } catch (json::parse_error& e) {
                        std::cerr << "[Client] JSON parse error: " << e.what() << std::endl;
                    }

                    do_read_header();
                } else {
                    socket_.close();
                }
            });
    }

    void handle_message(const json& j) {
        // 1. 协议层逻辑：自动回复 ACK
        if (j.value("type", "") == "alert_triggered" && j.contains("alert_id")) {
            json ack;
            ack["type"] = "alert_ack";
            ack["alert_id"] = j["alert_id"];
            send_json(ack);
            // 注意：这里不return，继续向下传递给UI，因为UI也需要弹窗
        }

        // 2. 通知 UI 层
        if (message_callback_) {
            message_callback_(j);
        } else {
            // 如果没有设置回调，默认打印到控制台方便调试
            std::cout << "[Client] Unhandled Message: " << j.dump(4) << std::endl;
        }
    }

    void do_write() {
        boost::asio::async_write(socket_,
            boost::asio::buffer(write_msgs_.front().data(),
              write_msgs_.front().length()),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    write_msgs_.pop_front();
                    if (!write_msgs_.empty()) {
                        do_write();
                    }
                } else {
                    socket_.close();
                }
            });
    }

private:
    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    ChatMessage read_msg_;
    chat_message_queue write_msgs_;
    int request_id_counter_;
    MessageCallback message_callback_;
};

int main(int argc, char* argv[]) {
    try {
        std::string host = "127.0.0.1";
        std::string port = "8888";

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, port);

        // 实例化客户端
        FuturesClient c(io_context, endpoints);

        // --- 模拟 UI 层集成 ---
        // 在实际 UI 项目中，这里会是 UI 框架的初始化代码
        // UI 层注册回调函数，处理来自服务器的消息
        c.set_message_callback([](const json& j) {
            std::string type = j.value("type", "unknown");
            
            if (type == "alert_triggered") {
                // UI 动作：弹出红色报警窗口，播放声音
                std::cout << "[UI] >>> 收到报警！刷新界面，弹窗提示: " << j.value("message", "") << std::endl;
            } 
            else if (type == "response") {
                // UI 动作：关闭加载动画，提示成功或失败
                bool success = j.value("success", false);
                std::cout << "[UI] >>> 操作结果: " << (success ? "成功" : "失败") << " - " << j.value("message", "") << std::endl;
            }
            else {
                std::cout << "[UI] >>> 收到其他消息: " << j.dump() << std::endl;
            }
        });

        // 启动网络线程
        // 在 UI 程序中，通常会在单独的线程运行 io_context，或者集成到 UI 的事件循环中
        std::thread t([&io_context]() { io_context.run(); });

        // 保持主线程运行 (在实际 UI 程序中，这里是 UI 的主事件循环)
        t.join();

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}

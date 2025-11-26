#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

using boost::asio::ip::tcp;
using json = nlohmann::json;

// --- 协议定义 ---
class ChatMessage {
public:
    enum { header_length = 4 };
    enum { max_body_length = 512 };

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

class ChatClient {
public:
    ChatClient(boost::asio::io_context& io_context,
               const tcp::resolver::results_type& endpoints)
        : io_context_(io_context),
          socket_(io_context) {
        do_connect(endpoints);
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

    void close() {
        boost::asio::post(io_context_, [this]() { socket_.close(); });
    }

private:
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
                    std::cout.write(read_msg_.body(), read_msg_.body_length());
                    std::cout << "\n";
                    do_read_header();
                } else {
                    socket_.close();
                }
            });
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
};

int main(int argc, char* argv[]) {
    try {
        std::string host = "127.0.0.1";
        std::string port = "8888";

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, port);

        ChatClient c(io_context, endpoints);

        std::thread t([&io_context]() { io_context.run(); });

        std::cout << "Enter message: ";
        char line[ChatMessage::max_body_length + 1];
        while (std::cin.getline(line, ChatMessage::max_body_length + 1)) {
            ChatMessage msg;
            msg.body_length(std::strlen(line));
            std::memcpy(msg.body(), line, msg.body_length());
            msg.encode_header();
            c.write(msg);
        }

        c.close();
        t.join();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}

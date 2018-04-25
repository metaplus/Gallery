// server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

const int max_length = 1024;

void session(boost::asio::ip::tcp::socket sock)
{
    try
    {
        fmt::print("sock address {}/{}\n", sock.local_endpoint(), sock.remote_endpoint());
        av::packet pkt{ nullptr };
        av::format_context format{ av::source::path{"C:/Media/H264/MercedesBenzDup.h264"} };
        auto index = 0;
        for (;;)
        {
            [[maybe_unused]] char data[max_length];

            boost::system::error_code error;
            if (error == boost::asio::error::eof)
                break; // Connection closed cleanly by peer.
            if (error)
                throw boost::system::system_error(error); // Some other error.
            while (!(pkt = format.read(av::media::video{})).empty())
            {
                if (pkt->flags)
                {
                    fmt::print("start sending, size: {}\n", pkt.cbuffer_view().size());
                    boost::asio::write(sock, boost::asio::buffer(pkt.cbuffer_view()));
                }
                ++index;
            }
            boost::asio::write(sock, boost::asio::buffer("\r\n123"));
            //boost::asio::write(sock, boost::asio::buffer(data, length));
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception in thread: " << e.what() << "\n";
    }
}

void server(boost::asio::io_context& io_context)
{
    boost::asio::ip::tcp::acceptor a{ io_context,boost::asio::ip::tcp::endpoint{boost::asio::ip::tcp::v4(),0},false };
    fmt::print("listen port {}\n", a.local_endpoint().port());
    for (;;)
    {
        std::thread(session, a.accept()).detach();
    }
}


int main(int argc, char* argv[])
{

    try
    {
        boost::asio::io_context io_context;
        server(io_context);
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
// server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


void session(boost::asio::ip::tcp::socket sock)
{
    try
    {
        fmt::print("sock address {}/{}\n", sock.local_endpoint(), sock.remote_endpoint());
        av::packet packet{ nullptr };
        av::format_context format{ av::source::path{"C:/Media/H264/MercedesBenzDup.h264"} };
        boost::system::error_code error;
        if (error == boost::asio::error::eof)
            return; // Connection closed cleanly by peer.
        if (error)
            throw boost::system::system_error(error); // Some other error.
        const std::string_view delim{ "(delim)" };
        while (!(packet = format.read(av::media::video{})).empty())
        {
            if (packet->flags)
            {
                fmt::print("start sending, size: {}\n", packet.cbuffer_view().size());
                write(sock, boost::asio::buffer(packet.cbuffer_view()));
                write(sock, boost::asio::buffer(delim));
            }
        }
        write(sock, boost::asio::buffer(delim));
        fmt::print("socket shutdown\n");
        sock.shutdown(sock.shutdown_both);
        sock.close();
        fmt::print("socket closed\n");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception in thread: " << e.what() << "\n";
    }
}

void server(boost::asio::io_context& io_context)
{
    boost::asio::ip::tcp::acceptor a{ io_context,boost::asio::ip::tcp::endpoint{boost::asio::ip::tcp::v4(),0},false };
    fmt::print("listen port {}\n", a.local_endpoint().port());
    while (true)
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
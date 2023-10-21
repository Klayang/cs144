#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
    if (message.SYN) {
        reassembler.insert(0, message.payload, message.FIN, inbound_stream);
        zero_point = message.seqno;
    }
    else {
        reassembler.insert(message.seqno.unwrap(zero_point, 0) - 1, message.payload, message.FIN, inbound_stream);
    }
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
    TCPReceiverMessage message = {zero_point + inbound_stream.bytes_pushed() + 1, (uint16_t)inbound_stream.available_capacity()};
    return message;
}

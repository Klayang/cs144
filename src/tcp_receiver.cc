#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
    if (message.SYN) {
        reassembler.insert(0, message.payload, message.FIN, inbound_stream);
        zero_point = message.seqno;
    }
    else if (zero_point.has_value()){
        reassembler.insert(message.seqno.unwrap(zero_point.value(), 0) - 1, message.payload, message.FIN, inbound_stream);
    }
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
    TCPReceiverMessage message;
    if (inbound_stream.available_capacity() <= UINT16_MAX)
        message.window_size = (uint16_t)inbound_stream.available_capacity();
    else message.window_size = UINT16_MAX;

    if (zero_point.has_value()) {
        message.ackno = zero_point.value() + inbound_stream.bytes_pushed() + 1;
        if (inbound_stream.is_closed()) message.ackno = message.ackno.value() + 1;
    }
    else message.ackno = nullopt;
    return message;
}

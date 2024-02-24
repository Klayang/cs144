#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <functional>
#include <algorithm>

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn ):
    isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), left_edge_of_window(0), right_edge_of_window(1),
    initial_RTO_ms_( initial_RTO_ms ), current_RTO_ms(initial_RTO_ms), timer(0), isTimerOn(false),
    num_of_consecutive_retransmissions(0), toBeSentSegments(vector<TCPSenderMessage>()),
    outstandingSegments(vector<TCPSenderMessage>()), SYN(true), FIN(false)
{
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
    uint64_t res = 0;
    for (auto segment: outstandingSegments) {
        res += segment.sequence_length();
    }
    return res;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
    return num_of_consecutive_retransmissions;
}

void TCPSender::handleLookAheadCase(const std::string& data, Reader& outbound_stream) {
    TCPSenderMessage segment;
    // set seqno field of TCPSenderMessage
    segment.seqno = Wrap32::wrap(left_edge_of_window, isn_);
    if (SYN) {
        segment.SYN = true;
        SYN = false;
    }
    else if (!data.empty()) {
        segment.payload = data.substr(0, 1);
    }
    else {
        segment.FIN = true;
        FIN = true;
    }

    outstandingSegments.push_back(segment);
    toBeSentSegments.push_back(segment);
    left_edge_of_window += 1;
    outbound_stream.pop(1);
}

void TCPSender::push( Reader& outbound_stream )
{
    // the reader has been closed
    if (FIN) {
        return;
    }

    // no data in the reader
    if (!SYN && outbound_stream.bytes_buffered() == 0 && !outbound_stream.is_finished()) {
        return;
    }

    // no window size
    if (left_edge_of_window > right_edge_of_window) {
        return;
    }

    // shouldn't send that 1-byte segment since there are to-be-sent segments
    if (left_edge_of_window == right_edge_of_window && !outstandingSegments.empty()) {
        return;
    }

    // no window but has data, and all previous segment being ack-ed => send 1-byte look-ahead
    string data = {outbound_stream.peek().begin(), outbound_stream.peek().end()};
    if (left_edge_of_window == right_edge_of_window && outstandingSegments.empty()) {
        handleLookAheadCase(data, outbound_stream);
        return;
    }

    // from now on, we'll handle the regular case, which is we have both data & window size

    // get if we need to set SYN & FIN, and # bytes in the payload
    uint64_t windowSpace = right_edge_of_window - left_edge_of_window;
    if (SYN) {
        windowSpace -= 1;
    }
    uint64_t dataLength = min(outbound_stream.bytes_buffered(), windowSpace);
    outbound_stream.pop(dataLength);
    if (outbound_stream.is_finished() && dataLength < windowSpace) {
        FIN = true;
    }

    // get # segments that we need to make in this function
    int numOfSegments = dataLength / TCPConfig::MAX_PAYLOAD_SIZE;
    if (dataLength % TCPConfig::MAX_PAYLOAD_SIZE == 0 && numOfSegments > 0) {
        numOfSegments -= 1;
    }

    for (int i = 0; i <= numOfSegments; ++i) {
        TCPSenderMessage segment;

        // set seqno field of TCPSenderMessage
        segment.seqno = Wrap32::wrap(left_edge_of_window, isn_);

        // set SYN field of TCPSenderMessage
        if (SYN) {
            segment.SYN = true;
            SYN = false;
        }

        // set payload field of TCPSenderMessage
        uint16_t  numOfBytes = TCPConfig::MAX_PAYLOAD_SIZE;
        if (i == numOfSegments && dataLength % TCPConfig::MAX_PAYLOAD_SIZE != 0) {
            numOfBytes = dataLength % TCPConfig::MAX_PAYLOAD_SIZE;
        }
        segment.payload = data.substr(i * TCPConfig::MAX_PAYLOAD_SIZE, numOfBytes);

        // set FIN field of TCPSenderMessage
        if (i == numOfSegments && FIN) {
            segment.FIN = true;
        }

        // put the segment in the buffer
        outstandingSegments.push_back(segment);
        toBeSentSegments.push_back(segment);

        // update left edge of window
        left_edge_of_window += segment.sequence_length();
    }
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
    if (toBeSentSegments.empty()) return nullopt;
    if (!isTimerOn) {
        isTimerOn = true;
        timer = 0;
    }
    TCPSenderMessage res = toBeSentSegments[0];
    toBeSentSegments.erase(toBeSentSegments.begin());
    return res;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
    uint64_t ackno = 0;
    if (msg.ackno.has_value()) {
        ackno = msg.ackno.value().unwrap(isn_, left_edge_of_window);
    }
    if (ackno > left_edge_of_window) return;
    uint16_t window = msg.window_size;
    right_edge_of_window = max(right_edge_of_window, ackno + (uint64_t)window);
    UpdateOutstandingSegmentsFunctor functor(*this, ackno);

    // this variable is set to check if any outstanding segments have been acked
    long unsigned int previous_num_of_outstanding_segments = outstandingSegments.size();
    // remove all segments whose seqno are less than the receiver message one, which means, having been acked
    outstandingSegments.erase(remove_if(outstandingSegments.begin(), outstandingSegments.end(), ref(functor)),
                              outstandingSegments.end());
    if (outstandingSegments.size() < previous_num_of_outstanding_segments) {
        current_RTO_ms = initial_RTO_ms_;
        timer = 0;
        num_of_consecutive_retransmissions = 0;
        if (outstandingSegments.empty()) {
            isTimerOn = false;
        }
    }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
    if (!isTimerOn) return;
    timer += ms_since_last_tick;
    if (timer >= current_RTO_ms) {
        if (left_edge_of_window <= right_edge_of_window) {
            current_RTO_ms *= 2;
            num_of_consecutive_retransmissions += 1;
        }
        TCPSenderMessage segment = outstandingSegments[0];
        toBeSentSegments.insert(toBeSentSegments.begin(), segment);
        timer = 0;
    }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
    TCPSenderMessage segment;
    segment.seqno = Wrap32::wrap(left_edge_of_window, isn_);
    return segment;
}

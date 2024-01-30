#pragma once

#include <string>
#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

class TCPSender
{
    Wrap32 isn_;
    uint64_t left_edge_of_window;
    uint64_t right_edge_of_window;
    uint64_t initial_RTO_ms_;
    uint64_t current_RTO_ms;
    uint64_t timer;
    bool isTimerOn;
    uint8_t num_of_consecutive_retransmissions;
    std::vector<TCPSenderMessage> toBeSentSegments;
    std::vector<TCPSenderMessage> outstandingSegments;
    bool SYN;
    bool FIN;
    void handleLookAheadCase(const std::string& data, Reader& outbound_stream);
    class UpdateOutstandingSegmentsFunctor {
        TCPSender& sender_;
        const uint64_t& ackno_; // this is the seqno from the tcp receiver message
    public:
        explicit UpdateOutstandingSegmentsFunctor(TCPSender& sender, const uint64_t& ackno): sender_(sender), ackno_(ackno) {}
        bool operator()(const TCPSenderMessage& segment) {
            uint64_t current_outstanding_seqno = segment.seqno.unwrap(sender_.isn_, sender_.left_edge_of_window)
                    + segment.sequence_length();

            if (current_outstanding_seqno > ackno_) {
                return false;
            }

//            sender_.left_edge_of_window = current_outstanding_seqno;
            return true;
        }
    };
public:
    /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
    TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

    /* Push bytes from the outbound stream */
    void push( Reader& outbound_stream );

    /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
    std::optional<TCPSenderMessage> maybe_send();

    /* Generate an empty TCPSenderMessage */
    TCPSenderMessage send_empty_message() const;

    /* Receive an act on a TCPReceiverMessage from the peer's receiver */
    void receive( const TCPReceiverMessage& msg );

    /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
    void tick( uint64_t ms_since_last_tick );

    /* Accessors for use in testing */
    uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
    uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};

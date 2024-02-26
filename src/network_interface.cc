#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address ),
    mapping_table(unordered_map<uint32_t, EthernetAddress>()),
    buffered_frames(vector<EthernetFrame>()), buffered_datagram_routes(vector<pair<InternetDatagram, uint32_t>>()),
    mapping_timers(unordered_map<uint32_t, size_t>()), arp_timers(unordered_map<uint32_t, size_t>())
{
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop ) {
    if (mapping_table.find(next_hop.ipv4_numeric()) != mapping_table.end()) {
        struct EthernetFrame datagram_frame = make_datagram_frame(ethernet_address_,
                mapping_table[next_hop.ipv4_numeric()], dgram);
        buffered_frames.push_back(datagram_frame);
    } else {
        if (arp_timers.find(next_hop.ipv4_numeric()) == arp_timers.end()) {
            EthernetFrame arp_request_frame = make_arp_frame(ARPMessage::OPCODE_REQUEST,
                ip_address_.ipv4_numeric(), ethernet_address_, next_hop.ipv4_numeric(), ETHERNET_BROADCAST);

            buffered_frames.push_back(arp_request_frame); // this frame is an ARP message for MAC address
            arp_timers[next_hop.ipv4_numeric()] = 0;
        }
        buffered_datagram_routes.push_back(make_pair(dgram, next_hop.ipv4_numeric()));
    }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame ) {
    if (frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_) {
        return nullopt;
    }
    if (frame.header.type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;
        if (!parse<InternetDatagram>(datagram, frame.payload)) {
            return nullopt;
        }
        return datagram;
    } else { // the ARP situation
        ARPMessage arp_message;
        if (!parse<ARPMessage>(arp_message, frame.payload)) {
            return nullopt;
        }

        if (mapping_table.find(arp_message.sender_ip_address) == mapping_table.end()) {
            mapping_table[arp_message.sender_ip_address] = arp_message.sender_ethernet_address;
            mapping_timers[arp_message.sender_ip_address] = 0;
        }

        if (arp_message.target_ip_address != ip_address_.ipv4_numeric()) {
            return nullopt;
        }

        if (arp_message.opcode == ARPMessage::OPCODE_REQUEST) {
            EthernetFrame arp_reply_frame = make_arp_frame(ARPMessage::OPCODE_REPLY, ip_address_.ipv4_numeric(),
                ethernet_address_, arp_message.sender_ip_address, arp_message.sender_ethernet_address);
            buffered_frames.push_back(arp_reply_frame);
        } else {
            // send frames with received MAC-addresses
            for (auto datagram_route: buffered_datagram_routes) {
                if (datagram_route.second == arp_message.sender_ip_address) {
                    EthernetFrame datagram_frame = make_datagram_frame(ethernet_address_,
                        arp_message.sender_ethernet_address, datagram_route.first);
                    buffered_frames.push_back(datagram_frame);
                    arp_timers.erase(arp_message.sender_ip_address);
                }
            }
            // remove entries from the buffered-datagrams
            RemoveBufferedDatagramFunctor functor(arp_message.sender_ip_address);
            buffered_datagram_routes.erase(remove_if(buffered_datagram_routes.begin(), buffered_datagram_routes.end(), ref(functor)),
                                           buffered_datagram_routes.end());
        }
        return nullopt;
    }
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick ) {
    for (auto itr = mapping_timers.begin(); itr != mapping_timers.end();) {
        itr->second += ms_since_last_tick;
        if (itr->second > MAPPING_DURATION) {
            mapping_table.erase(itr->first);
            itr = mapping_timers.erase(itr);
        } else {
            itr++;
        }
    }


    for (auto itr = arp_timers.begin(); itr != arp_timers.end(); ++itr) {
        itr->second += ms_since_last_tick;
        if (itr->second > ARP_RESEND_PERIOD) {
            EthernetFrame arp_request_frame = make_arp_frame(ARPMessage::OPCODE_REQUEST,
                ip_address_.ipv4_numeric(), ethernet_address_, itr->first, ETHERNET_BROADCAST);
            buffered_frames.push_back(arp_request_frame);
            itr->second = 0;
        }
    }

}

optional<EthernetFrame> NetworkInterface::maybe_send() {
    if (buffered_frames.empty()) {
        return nullopt;
    } else {
        EthernetFrame res = buffered_frames[0];
        buffered_frames.erase(buffered_frames.begin());
        return res;
    }
}

EthernetFrame NetworkInterface::make_arp_frame(uint16_t opcode, uint32_t sender_ip_address,
                                               const EthernetAddress& sender_ethernet_address,
                                               uint32_t target_ip_address,
                                               const EthernetAddress& target_ethernet_address) {
    ARPMessage arp_message = {
        .opcode = opcode,
        .sender_ethernet_address = sender_ethernet_address,
        .sender_ip_address = sender_ip_address,
        .target_ip_address = target_ip_address
    };

    if (target_ethernet_address == ETHERNET_BROADCAST) {
        arp_message.target_ethernet_address = {};
    } else {
        arp_message.target_ethernet_address = target_ethernet_address;
    }


    EthernetHeader header = {
        .dst = target_ethernet_address,
        .src = sender_ethernet_address,
        .type = EthernetHeader::TYPE_ARP
    };

    EthernetFrame arp_frame = {
        .header = header,
        .payload = serialize<ARPMessage>(arp_message)
    };

    return arp_frame;
}

EthernetFrame NetworkInterface::make_datagram_frame(const EthernetAddress& sender_ethernet_address,
                                                    const EthernetAddress& target_ethernet_address,
                                                    const InternetDatagram& dgram) {
    struct EthernetHeader header = {
        .dst = target_ethernet_address,
        .src = sender_ethernet_address,
        .type = EthernetHeader::TYPE_IPv4
    };
    struct EthernetFrame datagram_frame = {
        .header = header,
        .payload = serialize<InternetDatagram>(dgram)
    };
    return datagram_frame;
}

bool NetworkInterface::RemoveBufferedDatagramFunctor::operator()(const pair<InternetDatagram, uint32_t>& datagram_route) {
    if (datagram_route.second == sender_ip_address_) {
        return true;
    } else {
        return false;
    }
}

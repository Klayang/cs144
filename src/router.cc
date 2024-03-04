#include "router.hh"

#include <iostream>
#include <limits>
#include <math.h>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( uint32_t route_prefix,
                        uint8_t prefix_length,
                        optional<Address> next_hop,
                        size_t interface_num ) {
    Route route(route_prefix, prefix_length, next_hop, interface_num);
    routing_table_.push_back(route);
}

void Router::route() {
    if (routing_table_.empty()) {
        return;
    }

    for (AsyncNetworkInterface& interface: interfaces_) {
        while (true) {
            auto datagram = interface.maybe_receive();
            if (datagram.has_value()) {
                route_datagrams(datagram.value());
            } else {
                break;
            }
        }
    }
}

void Router::route_datagrams(InternetDatagram &datagram) {
    if (datagram.header.ttl <= 1) {
        return;
    }

    Route target_route = routing_table_[0];
    uint32_t dest_ip = datagram.header.dst;
    bool hasMatches = false;

    for (Route& route: routing_table_) {
        uint64_t power = pow(2, 32 - route.prefix_length_);
        uint32_t ip_prefix = dest_ip / power * power;

        if (ip_prefix != route.route_prefix_) {
            continue;
        } else if (!hasMatches) {
            target_route = route;
            hasMatches = true;
        } else if (route.prefix_length_ > target_route.prefix_length_) {
            target_route = route;
        }
    }

    if (hasMatches) {
        datagram.header.ttl -= 1;
        datagram.header.compute_checksum();
        if (target_route.next_hop_.has_value()) {
            interface(target_route.interface_num_).send_datagram(datagram, target_route.next_hop_.value());
        } else {
            interface(target_route.interface_num_).send_datagram(datagram, Address::from_ipv4_numeric(datagram.header.dst));
        }
    }
}

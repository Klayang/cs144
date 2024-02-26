#pragma once

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"

#include <iostream>
#include <list>
#include <optional>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.

constexpr size_t MAPPING_DURATION = 30000;
constexpr size_t ARP_RESEND_PERIOD = 5000;

class NetworkInterface
{
private:
  // Ethernet (known as hardware, network-access, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as Internet-layer or network-layer) address of the interface
  Address ip_address_;

  // mapping table, used to contain mappings between IP & MAC addresses of other hosts
  std::unordered_map<uint32_t, EthernetAddress> mapping_table;

  // frames made via send_datagram, but not yet send via maybe_send
  std::vector<EthernetFrame> buffered_frames;

  // datagrams queued in send_datagram, for the MAC address is not known
  std::vector<std::pair<InternetDatagram, uint32_t>> buffered_datagram_routes;

  // timer for each IP-to-MAC address mapping (since the mapping should only exist for 30 secs)
  std::unordered_map<uint32_t, size_t> mapping_timers; // entries removed when tick is called

  // timer for sending ARP requests, since a 2nd request can only be sent, 5 secs after the 1st one
  std::unordered_map<uint32_t, size_t> arp_timers; // entries removed when arp (MAC address) received


  // a helper method to create a frame, that contains an ARP request message
  EthernetFrame make_arp_frame(uint16_t opcode, uint32_t sender_ip_address,
                               const EthernetAddress& sender_ethernet_address,
                               uint32_t target_ip_address,
                               const EthernetAddress& target_ethernet_address);

  // a helper method to create a frame, that contains an IPv4 datagram
  EthernetFrame make_datagram_frame(const EthernetAddress& sender_ethernet_address,
                                           const EthernetAddress& target_ethernet_address,
                                           const InternetDatagram& dgram);

  // a functor class used to remake frames after getting the appropriate MAC address from ARP message
  class RemoveBufferedDatagramFunctor {
      uint32_t sender_ip_address_;
  public:
      explicit RemoveBufferedDatagramFunctor(uint32_t sender_ip_address): sender_ip_address_(sender_ip_address) {}
      bool operator()(const std::pair<InternetDatagram, uint32_t>& datagram_route);
  };

public:
  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address );

  // Access queue of Ethernet frames awaiting transmission
  std::optional<EthernetFrame> maybe_send();

  // Sends an IPv4 datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address
  // for the next hop.
  // ("Sending" is accomplished by making sure maybe_send() will release the frame when next called,
  // but please consider the frame sent as soon as it is generated.)
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, returns the datagram.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  std::optional<InternetDatagram> recv_frame( const EthernetFrame& frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );
};

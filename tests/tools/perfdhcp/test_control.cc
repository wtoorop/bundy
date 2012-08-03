// Copyright (C) 2012 Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <exceptions/exceptions.h>
#include <asiolink/io_address.h>
#include <dhcp/libdhcp++.h>
#include <dhcp/iface_mgr.h>
#include <dhcp/dhcp4.h>
#include "test_control.h"
#include "command_options.h"

using namespace std;
using namespace boost;
using namespace boost::posix_time;
using namespace isc;
using namespace isc::dhcp;
using namespace isc::asiolink;

namespace isc {
namespace perfdhcp {

TestControl::TestControlSocket::TestControlSocket(int socket) :
    socket_(socket) {
    initInterface();
}

TestControl::TestControlSocket::~TestControlSocket() {
    IfaceMgr::instance().closeSockets();
}

void
TestControl::TestControlSocket::initInterface() {
    const IfaceMgr::IfaceCollection& ifaces =
        IfaceMgr::instance().getIfaces();
    for (IfaceMgr::IfaceCollection::const_iterator it = ifaces.begin();
         it != ifaces.end();
         ++it) {
        const IfaceMgr::SocketCollection& socket_collection =
            it->getSockets();
        for (IfaceMgr::SocketCollection::const_iterator s =
                 socket_collection.begin();
             s != socket_collection.end();
             ++s) {
            if (s->sockfd_ == socket_) {
                iface_ = it->getName();
                return;
            }
        }
    }
    isc_throw(BadValue, "interface for for specified socket "
              "descriptor not found");
}

TestControl&
TestControl::instance() {
    static TestControl test_control;
    return (test_control);
}

TestControl::TestControl() :
    send_due_(microsec_clock::universal_time()),
    last_sent_(send_due_) {
}

bool
TestControl::checkExitConditions() const {
    CommandOptions& options = CommandOptions::instance();
    if ((options.getNumRequests().size() > 0) &&
        (sent_packets_0_ >= options.getNumRequests()[0])) {
        return(true);
    } else if ((options.getNumRequests().size() == 2) &&
               (sent_packets_1_ >= options.getNumRequests()[1])) {
        return(true);
    }
    return(false);
}

boost::shared_ptr<Pkt4>
TestControl::createDiscoverPkt4(const std::vector<uint8_t>& mac_addr) const {
    const uint32_t transid = static_cast<uint32_t>(random());
    boost::shared_ptr<Pkt4> pkt4(new Pkt4(DHCPDISCOVER, transid));
    if (!pkt4) {
        isc_throw(isc::Unexpected, "failed to create DISCOVER packet");
    }

    if (HW_ETHER_LEN != mac_addr.size()) {
        isc_throw(BadValue, "invalid MAC address size");
    }
    pkt4->setHWAddr(HTYPE_ETHER, HW_ETHER_LEN, mac_addr);

    OptionBuffer buf_msg_type;
    buf_msg_type.push_back(DHCPDISCOVER);
    pkt4->addOption(Option::factory(Option::V4, DHO_DHCP_MESSAGE_TYPE, buf_msg_type));
    pkt4->addOption(Option::factory(Option::V4, DHO_DHCP_PARAMETER_REQUEST_LIST));
    return pkt4;
}

OptionPtr
TestControl::factoryGeneric4(Option::Universe u,
                                 uint16_t type,
                                 const OptionBuffer& buf) {
    OptionPtr opt(new Option(u, type, buf));
    return opt;
}

OptionPtr
TestControl::factoryRequestList4(Option::Universe u,
                                 uint16_t type,
                                 const OptionBuffer& buf) {
    const uint8_t buf_array[] = {
        DHO_SUBNET_MASK,
        DHO_BROADCAST_ADDRESS,
        DHO_TIME_OFFSET,
        DHO_ROUTERS,
        DHO_DOMAIN_NAME,
        DHO_DOMAIN_NAME_SERVERS,
        DHO_HOST_NAME
    };

    OptionBuffer buf_with_options(buf_array, buf_array + sizeof(buf_array));
    OptionPtr opt(new Option(u, type, buf));
    opt->setData(buf_with_options.begin(), buf_with_options.end());
    return opt;
}

const std::vector<uint8_t>&
TestControl::generateMacAddress() {
    CommandOptions& options = CommandOptions::instance();
    uint32_t clients_num = options.getClientsNum();
    if ((clients_num == 0) || (clients_num == 1)) {
        return last_mac_address_;
    }
    for (std::vector<uint8_t>::iterator it = last_mac_address_.end() - 1;
         it >= last_mac_address_.begin();
         --it) {
        if (++(*it) > 0) {
            break;
        }
    }
    return last_mac_address_;
}

uint64_t
TestControl::getNextExchangesNum() const {
    CommandOptions& options = CommandOptions::instance();
    // Reset number of exchanges.
    uint64_t due_exchanges = 0;
    // Get current time.
    ptime now(microsec_clock::universal_time());
    // The due time indicates when we should start sening next chunk
    // of packets. If it is already due time, we should calculate
    // how many packets to send.
    if (now >= send_due_) {
        // If rate is specified from the command line we have to
        // synchornize with it.
        if (options.getRate() != 0) {
            time_period period(send_due_, now);
            // Null condition should not occur because we
            // have checked it in the first if statement but
            // let's keep this check just in case.
            if (period.is_null()) {
                return (0);
            }
            time_duration duration = period.length();
            // due_factor indicates the number of seconds that
            // sending next chunk of packets will take.
            double due_factor = duration.fractional_seconds() /
                time_duration::ticks_per_second();
            due_factor += duration.total_seconds();
            // Multiplying due_factor by expected rate gives the number
            // of exchanges to be initiated.
            due_exchanges = static_cast<uint64_t>(due_factor * options.getRate());
            // We want to make sure that at least one packet goes out.
            due_exchanges += 1;
            // We should not exceed aggressivity as it could have been
            // restricted from command line.
            if (due_exchanges > options.getAggressivity()) {
                due_exchanges = options.getAggressivity();
            }
        } else {
            // Rate is not specified so we rely on aggressivity
            // which is the number of packets to be sent in
            // one chunk.
            due_exchanges = options.getAggressivity();
        }
        return (due_exchanges);
    }
    return (0);
}

int
TestControl::openSocket() const {
    CommandOptions& options = CommandOptions::instance();
    std::string localname = options.getLocalName();
    std::string servername = options.getServerName();
    uint8_t family = AF_INET;
    uint16_t port = 67;
    int sock = 0;
    if (options.getIpVersion() == 6) {
        family = AF_INET6;
        port = 547;
    }
    if (!localname.empty()) {
        bool is_interface = false;;
        try {
            sock = IfaceMgr::instance().openSocketFromIface(localname,
                                                            port,
                                                            family);
            is_interface = true;
        } catch (...) {
            // This is not fatal error. It may be the case that
            // parameter given from command line is not interface
            // name but local IP address.
        }
        if (!is_interface) {
            IOAddress localaddr(localname);
            // We don't catch exception here because parameter given
            // must be either interface name or local address. If
            // both attempts failed, we want exception to be emited.
            sock = IfaceMgr::instance().openSocketFromAddress(localaddr,
                                                              port);
        }
    } else if (!servername.empty()) {
        IOAddress remoteaddr(servername);
        sock = IfaceMgr::instance().openSocketFromRemoteAddress(remoteaddr,
                                                                port);
    }
    if (sock <= 0) {
        isc_throw(BadValue, "unable to open socket to communicate with "
                  "DHCP server");
    }
    return sock;
}

void
TestControl::registerOptionFactories4() const {
    static bool factories_registered = false;
    if (!factories_registered) {
        LibDHCP::OptionFactoryRegister(Option::V4,
                                       DHO_DHCP_MESSAGE_TYPE,
                                       &TestControl::factoryGeneric4);
        LibDHCP::OptionFactoryRegister(Option::V4,
                                       DHO_DHCP_PARAMETER_REQUEST_LIST,
                                       &TestControl::factoryRequestList4);
    }
    factories_registered = true;
}

void
TestControl::registerOptionFactories6() const {
    static bool factories_registered = false;
    if (!factories_registered) {
    }
    factories_registered = true;
}

void
TestControl::registerOptionFactories() const {
    CommandOptions& options = CommandOptions::instance();
    switch(options.getIpVersion()) {
    case 4:
        registerOptionFactories4();
        break;
    case 6:
        registerOptionFactories6();
        break;
    default:
        isc_throw(InvalidOperation, "command line options have to be parsed "
                  "before DHCP option factories can be registered");
    }
}

void
TestControl::resetMacAddress() {
    CommandOptions& options = CommandOptions::instance();
    std::vector<uint8_t> mac_prefix(options.getMacPrefix());
    if (mac_prefix.size() != HW_ETHER_LEN) {
        isc_throw(Unexpected, "MAC address prefix is invalid");
    }
    std::swap(mac_prefix, last_mac_address_);
}

void
TestControl::run() {
    sent_packets_0_ = 0;
    sent_packets_1_ = 0;
    CommandOptions& options = CommandOptions::instance();
    // Ip version is not set ONLY in case the command options
    // were not parsed. This surely means that parse() function
    // was not called prior to starting the test. This is fatal
    // error.
    if (options.getIpVersion() == 0) {
        isc_throw(InvalidOperation,
                  "command options must be parsed before running a test");
    }
    registerOptionFactories();
    TestControlSocket socket(openSocket());
    resetMacAddress();
    uint64_t packets_sent = 0;
    for (;;) {
        updateSendDue();
        if (checkExitConditions()) {
            break;
        }
        uint64_t packets_due = getNextExchangesNum();
        for (uint64_t i = packets_due; i > 0; --i) {
            startExchange(socket);
            ++packets_sent;
            cout << "Packets sent " << packets_sent << endl;
        }
    }
}

void
TestControl::startExchange(const TestControlSocket& socket) {
    ++sent_packets_0_;
    last_sent_ = microsec_clock::universal_time();
    std::vector<uint8_t> mac_address = generateMacAddress();
    boost::shared_ptr<Pkt4> pkt4 = createDiscoverPkt4(mac_address);
    pkt4->setIface(socket.getIface());
    try {
        pkt4->pack();
        IfaceMgr::instance().send(pkt4);
    } catch (const Exception& e) {
        std::cout << e.what() << std::endl;
    }
}

void
TestControl::updateSendDue() {
    // If default constructor was called, this should not happen but
    // if somebody has changed default constructor it is better to
    // keep this check.
    if (last_sent_.is_not_a_date_time()) {
        isc_throw(Unexpected, "time of last sent packet not initialized");
    }
    // Get the expected exchange rate.
    CommandOptions& options = CommandOptions::instance();
    int rate = options.getRate();
    // If rate was not specified we will wait just one clock tick to
    // send next packet. This simulates best effort conditions.
    long duration = 1;
    if (rate != 0) {
        // We use number of ticks instead of nanoseconds because
        // nanosecond resolution may not be available on some
        // machines. Number of ticks guarantees the highest possible
        // timer resolution.
        duration = time_duration::ticks_per_second() / rate;
    }
    // Calculate due time to initate next chunk of exchanges.
    send_due_ = last_sent_ + time_duration(0, 0, 0, duration);
}


} // namespace perfdhcp
} // namespace isc

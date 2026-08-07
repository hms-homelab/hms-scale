#pragma once

#include <string>

namespace hms_colada {

/// Publishes the service on the local network over mDNS/zeroconf, so an ESP
/// scale gateway can be pointed at `hms-scale.local:8889` instead of an IP that
/// only exists on one person's LAN.
///
/// Two records go out, and they do different jobs:
///
///  - A CNAME `<hostname>.local` -> `<this machine>.local`. This is the
///    compatibility record: firmware that hardcodes a name needs the name to
///    resolve. A CNAME is used rather than an A record on purpose, because it
///    follows the host's own address and therefore survives a DHCP lease change
///    without anything having to notice.
///  - A service `_hms-scale._tcp` carrying the port and a TXT `path=`. This is
///    the record a client *should* use: discovery by service type needs no
///    agreed-upon name at all, so it cannot collide and cannot go stale.
///
/// A note on the record that does NOT work, since the mistake is easy to repeat:
/// putting `<host-name>hms-scale.local</host-name>` in an
/// /etc/avahi/services/*.service file does not publish that name. In Avahi that
/// tag means "this service runs on some *other*, already-resolvable host", and
/// since nothing then publishes an address for it, Avahi drops the whole group
/// and you get neither the hostname nor the service.
///
/// Everything here is best-effort. If Avahi is missing, not running, or
/// unreachable (a bridged Docker container cannot do link-local multicast at
/// all), the publisher logs one line and the service carries on — mDNS is a
/// convenience, never a dependency. On platforms without Avahi the class is not
/// compiled in at all; see BUILD_WITH_MDNS in CMakeLists.txt.
class MdnsPublisher {
public:
    /// @param hostname bare name to publish, e.g. "hms-scale" for hms-scale.local
    /// @param port     TCP port the HTTP service listens on
    MdnsPublisher(std::string hostname, int port);
    ~MdnsPublisher();

    MdnsPublisher(const MdnsPublisher&) = delete;
    MdnsPublisher& operator=(const MdnsPublisher&) = delete;

    /// Connects to Avahi and publishes. Returns false if Avahi could not be
    /// reached at all; the caller is expected to shrug and continue.
    ///
    /// Publication is asynchronous: a false return means "definitely not
    /// published", but a true return means "handed to Avahi", with the
    /// confirmation arriving in the log a moment later.
    bool start();

    /// Whether the records are live right now. Flips back to false if Avahi
    /// restarts underneath us, and back to true once it re-registers.
    bool published() const;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace hms_colada

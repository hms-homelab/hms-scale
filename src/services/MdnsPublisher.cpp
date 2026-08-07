#include "services/MdnsPublisher.h"

#include <spdlog/spdlog.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/thread-watch.h>

#include <atomic>
#include <cstdint>
#include <utility>
#include <vector>

namespace hms_colada {

namespace {

constexpr const char* kServiceType = "_hms-scale._tcp";
constexpr const char* kWebhookTxt  = "path=/api/webhook/measurement";
constexpr uint16_t    kCnameTtl    = 60;

/// Encode a dotted name as DNS wire format, which is what the rdata of a CNAME
/// record has to be: each label prefixed with its length, terminated by a zero
/// byte. "maestro.local" becomes "\x07maestro\x05local\x00".
std::vector<uint8_t> encodeDnsName(const std::string& fqdn) {
    std::vector<uint8_t> out;
    size_t start = 0;
    while (start < fqdn.size()) {
        size_t dot = fqdn.find('.', start);
        if (dot == std::string::npos) dot = fqdn.size();
        const size_t len = dot - start;
        // A label longer than 63 bytes cannot be encoded, and an empty one
        // (a doubled dot, or the trailing dot of an absolute name) is just skipped.
        if (len > 0 && len <= 63) {
            out.push_back(static_cast<uint8_t>(len));
            out.insert(out.end(), fqdn.begin() + start, fqdn.begin() + dot);
        }
        start = dot + 1;
    }
    out.push_back(0);
    return out;
}

} // namespace

struct MdnsPublisher::Impl {
    std::string hostname;  // bare name; may gain a suffix if the network disagrees
    std::string service;   // service instance name; likewise
    int port;

    AvahiThreadedPoll* poll   = nullptr;
    AvahiClient*       client = nullptr;
    AvahiEntryGroup*   group  = nullptr;

    std::atomic<bool> published{false};

    Impl(std::string h, int p)
        : hostname(std::move(h)), service(hostname), port(p) {}

    /// Build and commit the entry group. Called on the Avahi poll thread, which
    /// already holds the lock, so it must not take it again.
    void publish() {
        if (!client) return;

        if (!group) {
            group = avahi_entry_group_new(client, &Impl::groupCallback, this);
            if (!group) {
                spdlog::warn("mDNS: could not create entry group: {}",
                             avahi_strerror(avahi_client_errno(client)));
                return;
            }
        }
        if (!avahi_entry_group_is_empty(group)) return;

        // The service record. This is the one clients should discover by type,
        // and it needs no agreed-upon hostname to work.
        int rc = avahi_entry_group_add_service(
            group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, AvahiPublishFlags(0),
            service.c_str(), kServiceType, nullptr, nullptr,
            static_cast<uint16_t>(port), kWebhookTxt, nullptr);

        if (rc == AVAHI_ERR_COLLISION) {
            renameAndRetry("service name");
            return;
        }
        if (rc < 0) {
            spdlog::warn("mDNS: could not add {} service: {}", kServiceType, avahi_strerror(rc));
            return;
        }

        addCnameRecord();

        rc = avahi_entry_group_commit(group);
        if (rc < 0) {
            spdlog::warn("mDNS: could not commit records: {}", avahi_strerror(rc));
        }
    }

    /// Add `<hostname>.local` as a CNAME onto whatever this machine already
    /// calls itself. A CNAME rather than an A record so that the alias tracks
    /// the host's address automatically across DHCP changes.
    void addCnameRecord() {
        const char* host_fqdn = avahi_client_get_host_name_fqdn(client);
        if (!host_fqdn) {
            spdlog::warn("mDNS: no host FQDN available, skipping the {}.local alias", hostname);
            return;
        }

        const std::string alias = hostname + ".local";
        // If the box is already called this, the alias would point at itself.
        if (alias == host_fqdn) {
            spdlog::info("mDNS: host is already {}, no alias needed", host_fqdn);
            return;
        }

        const std::vector<uint8_t> rdata = encodeDnsName(host_fqdn);
        // ALLOW_MULTIPLE is load-bearing, not decoration. Without it a second
        // hms-scale install on the same network makes this record collide,
        // which lands in groupCallback and renames the *service* — the only
        // thing renameAndRetry can rename — leaving the CNAME identical and the
        // group colliding again on every retry. With it the two hosts simply
        // coexist as answers for the same alias (see the README on why running
        // two installs on one network wants distinct MDNS_HOSTNAME values).
        const int rc = avahi_entry_group_add_record(
            group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
            AvahiPublishFlags(AVAHI_PUBLISH_USE_MULTICAST | AVAHI_PUBLISH_ALLOW_MULTIPLE),
            alias.c_str(), AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_CNAME, kCnameTtl,
            rdata.data(), rdata.size());

        if (rc < 0) {
            // Not fatal: the service record above is the better discovery path
            // anyway, so a failed alias degrades rather than breaks.
            spdlog::warn("mDNS: could not add {} -> {} alias: {}",
                         alias, host_fqdn, avahi_strerror(rc));
            return;
        }
        spdlog::debug("mDNS: alias {} -> {} queued", alias, host_fqdn);
    }

    /// Someone else on this network already claims our name. Avahi's convention
    /// is to append a counter and try again, which is what a second hms-scale
    /// install on one LAN will end up doing.
    void renameAndRetry(const char* what) {
        char* alt = avahi_alternative_service_name(service.c_str());
        spdlog::warn("mDNS: {} '{}' is taken on this network, retrying as '{}'",
                     what, service, alt);
        service = alt;
        avahi_free(alt);
        avahi_entry_group_reset(group);
        publish();
    }

    static void groupCallback(AvahiEntryGroup* g, AvahiEntryGroupState state, void* userdata) {
        auto* self = static_cast<Impl*>(userdata);
        switch (state) {
        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            self->published = true;
            spdlog::info("mDNS: published {}.local and {} on port {}",
                         self->hostname, kServiceType, self->port);
            break;
        case AVAHI_ENTRY_GROUP_COLLISION:
            self->published = false;
            self->renameAndRetry("name");
            break;
        case AVAHI_ENTRY_GROUP_FAILURE:
            self->published = false;
            spdlog::warn("mDNS: registration failed: {}",
                         avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));
            break;
        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            break;
        }
    }

    static void clientCallback(AvahiClient* c, AvahiClientState state, void* userdata) {
        auto* self = static_cast<Impl*>(userdata);
        // Avahi hands us the client here before avahi_client_new() has returned,
        // so the member may still be unset on the very first callback.
        self->client = c;

        switch (state) {
        case AVAHI_CLIENT_S_RUNNING:
            self->publish();
            break;
        case AVAHI_CLIENT_S_COLLISION:
        case AVAHI_CLIENT_S_REGISTERING:
            // The daemon is re-establishing its own host records; ours have to
            // be withdrawn and re-added once it settles.
            self->published = false;
            if (self->group) avahi_entry_group_reset(self->group);
            break;
        case AVAHI_CLIENT_FAILURE:
            self->published = false;
            spdlog::warn("mDNS: Avahi connection lost: {}",
                         avahi_strerror(avahi_client_errno(c)));
            break;
        case AVAHI_CLIENT_CONNECTING:
            // avahi-daemon is not up yet. AVAHI_CLIENT_NO_FAIL means we simply
            // wait for it rather than giving up, so a service that starts
            // before avahi-daemon still ends up published.
            spdlog::info("mDNS: waiting for avahi-daemon");
            break;
        }
    }
};

MdnsPublisher::MdnsPublisher(std::string hostname, int port)
    : impl_(new Impl(std::move(hostname), port)) {}

MdnsPublisher::~MdnsPublisher() {
    if (impl_->poll) avahi_threaded_poll_stop(impl_->poll);   // joins the poll thread
    if (impl_->client) avahi_client_free(impl_->client);      // also frees the entry group
    if (impl_->poll) avahi_threaded_poll_free(impl_->poll);
    delete impl_;
}

bool MdnsPublisher::start() {
    impl_->poll = avahi_threaded_poll_new();
    if (!impl_->poll) {
        spdlog::warn("mDNS: could not create the Avahi poll loop, continuing without mDNS");
        return false;
    }

    int error = 0;
    impl_->client = avahi_client_new(avahi_threaded_poll_get(impl_->poll),
                                     AVAHI_CLIENT_NO_FAIL,
                                     &Impl::clientCallback, impl_, &error);
    if (!impl_->client) {
        // Reached when there is no D-Bus at all to talk to, which is the normal
        // case inside a bridged container. Not an error worth alarming about.
        spdlog::info("mDNS: Avahi not available ({}), continuing without mDNS", avahi_strerror(error));
        avahi_threaded_poll_free(impl_->poll);
        impl_->poll = nullptr;
        return false;
    }

    if (avahi_threaded_poll_start(impl_->poll) < 0) {
        spdlog::warn("mDNS: could not start the Avahi poll thread, continuing without mDNS");
        return false;
    }
    return true;
}

bool MdnsPublisher::published() const {
    return impl_->published;
}

} // namespace hms_colada

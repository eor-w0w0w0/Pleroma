#pragma once

// Inferred connectionless LAN packet framing and endpoint types.

namespace darktide {
namespace connectionless {

struct SockAddrIn6Like {
    unsigned short family;
    unsigned short port_be;
    unsigned int flowinfo;
    unsigned char address[16];
    unsigned int scope_id;
};

// This matches the stock 0x1c-byte Windows sockaddr_in6 layout used by Darktide.
// For IPv4 addresses, stock builders encode an IPv4-mapped IPv6 address in
// `address[0..15]` with the IPv4 bytes in the last four positions.

using SendBroadcastFn = int (*)(void *backend, const void *buffer, unsigned int byte_len, unsigned int port);
using SendExplicitFn = int (*)(void *backend, const SockAddrIn6Like *destination, unsigned int flags_or_route, const void *buffer, unsigned int byte_len);
using BuildSockAddrIn6FromTextFn = void *(*)(SockAddrIn6Like *out_address, const char *text);
using BuildIPv4MappedSockAddrIn6Fn = SockAddrIn6Like *(*)(SockAddrIn6Like *out_address, unsigned int ipv4_be, unsigned short port_be);

static constexpr unsigned short kConnectionlessEnvelopeTag = 0x3EE5;
static constexpr unsigned int kConnectionlessReserved24 = 0;
static constexpr unsigned int kDescriptorKeyBits = 0x40;
static constexpr unsigned int kDescriptorKeyBytes = 0x08;
static constexpr unsigned int kBrowserNameFieldSizeBytes = 0x25;
static constexpr unsigned int kBrowserNameTextChars = 0x24;
static constexpr unsigned int kBrowserServiceIdLobbyBrowser = 0;
static constexpr unsigned int kBrowserReserved8 = 0;

// High-level connectionless event ids.
enum ConnectionlessKind {
    ConnectionlessKind_ConnectRequest = 0x10,
    ConnectionlessKind_ConnectReply = 0x11,
    ConnectionlessKind_DiscoverLobby = 0x12,
    ConnectionlessKind_DiscoverLobbyReply = 0x13,
};

constexpr unsigned char EncodeKindToken(unsigned char kind)
{
    return static_cast<unsigned char>((kind << 1) | 1);
}

constexpr unsigned char EncodeBrowserEventToken(unsigned char event_id)
{
    return static_cast<unsigned char>((event_id << 1) | 1);
}

static constexpr unsigned char kConnectRequestToken = EncodeKindToken(ConnectionlessKind_ConnectRequest);
static constexpr unsigned char kDiscoverLobbyToken = EncodeKindToken(ConnectionlessKind_DiscoverLobby);
static constexpr unsigned char kDiscoverLobbyReplyToken = EncodeKindToken(ConnectionlessKind_DiscoverLobbyReply);
static constexpr unsigned char kBrowserEvent11Token = EncodeBrowserEventToken(0x11);
static constexpr unsigned char kBrowserEvent13Token = EncodeBrowserEventToken(0x13);

// Earliest currently identified host-side request-connection receive/parser window.
static constexpr unsigned long kRequestConnectionReceiveParserStartRva = 0x003383A0;
static constexpr unsigned long kRequestConnectionReceiveParserEndRva = 0x00338C22;

// Outer envelope carries a fixed 0x25-byte text field after the 64-bit key.
// Parser treatment indicates it is a fixed 36-byte text string plus NUL and is
// the source of the browser-visible lobby/server name, not a sockaddr payload.
//
// Explicit-destination sender (`0x14056efd0`) current recovered trailer:
//   u4 0, u8 0, u4 1, u1 1, align, byte 0x21
// Discovery sender (`0x14056f2e0`) current recovered trailer uses token 0x25.
// Lower-level browser sub-packet parser expects:
//   u4 service_id
//   u8 zero
//   varlen payload_size
//   payload bytes, where browser payload begins with:
//     1 bit = 1
//     6 bits = browser event id
//     1 bit = 0
// Confirmed browser payload tokens:
//   event 0x11 -> 0x23
//   event 0x13 -> 0x27
// Best current 0x13 payload expectation after the token:
//   u64 lobby_id / descriptor key
// This `u64` is treated as a real lobby/browser entry id and must remain
// consistent through later connect/join handling.

} // namespace connectionless
} // namespace darktide

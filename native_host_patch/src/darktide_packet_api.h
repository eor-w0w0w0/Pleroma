#pragma once

// Inferred packet/bitstream API used by the LAN transport layer.

namespace darktide {
namespace packet {

struct BitCursor {
    void *buffer_start;
    void *cursor;
    unsigned int buffer_limit_or_size;
    unsigned int error;
    unsigned int bit_state;
    unsigned int scratch;
};

constexpr unsigned char EncodeConnectionlessKind(unsigned char kind)
{
    return static_cast<unsigned char>((kind << 1) | 1);
}

// Generic bitstream helpers.
static constexpr unsigned long kWriteBits32Rva = 0x00332E00;
static constexpr unsigned long kWriteBits64Rva = 0x00330DB0;
static constexpr unsigned long kWriteBlobBitsRva = 0x00332EF0;
static constexpr unsigned long kReadBits32Rva = 0x00330EA0;
static constexpr unsigned long kReadBits64Rva = 0x00331020;
static constexpr unsigned long kSkipBitsRva = 0x00330F80;
static constexpr unsigned long kReadBytesRva = 0x00333030;
static constexpr unsigned long kBuildJoinReplyFromTextRva = 0x006C5690;
static constexpr unsigned long kBuildJoinReplyFromIPv4Rva = 0x006C5780;

// Winsock helpers observed in LAN packet send paths.
static constexpr unsigned long kGetSockNameImportRva = 0x00E6AFC8;
static constexpr unsigned long kNtohsImportRva = 0x00E6AEF0;

// Bitstream helper signatures inferred from static analysis.
using WriteBits32Fn = void (*)(BitCursor *cursor, unsigned int value, unsigned int bit_count);
using WriteBits64Fn = void (*)(BitCursor *cursor, unsigned long long value, unsigned int bit_count);
using WriteBlobBitsFn = void (*)(BitCursor *cursor, const void *src, unsigned int bit_count);
using ReadBits32Fn = unsigned int (*)(BitCursor *cursor, unsigned int bit_count);
using ReadBits64Fn = unsigned long long (*)(BitCursor *cursor, unsigned int bit_count);
using SkipBitsFn = void (*)(BitCursor *cursor, unsigned int bit_count);
using ReadBytesFn = void (*)(BitCursor *cursor, void *dst, unsigned int byte_count);
using BuildJoinReplyFromTextFn = void *(*)(void *out_packet, const char *addr_text, unsigned int dest_handle16);
using BuildJoinReplyFromIPv4Fn = void *(*)(void *out_packet, unsigned int ipv4_be, unsigned int dest_handle16);

// Transport send path observations.
// Current best shape for transport vfunc +0x108:
//   send(transport, dst_id_or_remote_conn, system_or_channel, reliable,
//        payload_ptr, payload_bit_count, flags_or_user)
// with:
//   rcx = transport
//   edx = dst/remote connection id
//   r8d = system/channel id
//   r9b = reliable flag
//   [rsp+0x20] = payload pointer
//   [rsp+0x28] = payload bit count
//   [rsp+0x30] = flags/user/callback token

// Packet layout notes recovered from the active LAN cluster.
// Inner join-request send helper (`0x14056c6e0`) writes, in order:
//   - 6 bits  = 0x16
//   - 64 bits = qword from `LanLobby+0x58`
//   - 16 bits = local socket port (`getsockname` + `ntohs`)
// Inner join-reply parse helper (`0x14056ebc6`) reads:
//   - 1 bit   = accepted flag
// Inner reject/control packet (`0x18`) has no payload beyond opcode.
//
// Explicit-destination connectionless send path (`0x14056efd0`) emits:
//   - u16 0x3EE5
//   - u24 0
//   - u64 endpoint_key from obj+0x78
//   - 0x25-byte descriptor from obj+0x80
//   - trailer: u4 0, u8 0, u4 1, u1 1, byte-align, payload byte 0x21
// Total observed size on this path: 54 bytes.
// Confirmed token examples:
//   0x10 -> 0x21
//   0x12 -> 0x25
// Therefore a synthetic 0x13 would most likely use token byte 0x27.

} // namespace packet
} // namespace darktide

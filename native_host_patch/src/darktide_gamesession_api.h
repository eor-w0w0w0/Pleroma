#pragma once

// Inferred GameSession host-related layout recovered from Darktide.exe.

namespace darktide {
namespace gamesession {

static constexpr unsigned long kLocalPeerIdOffset = 0x28;
static constexpr unsigned long kGameSessionHostPeerIdOffset = 0xD0;
static constexpr unsigned long kPeerCountOffset = 0xE0;
static constexpr unsigned long kPeerArrayOffset = 0xE8;
static constexpr unsigned long kPeerEntrySize = 0x28;
static constexpr unsigned long kPeerEntryPeerIdOffset = 0x00;
static constexpr unsigned long kPeerEntryCloseHandleOffset = 0x0C;
static constexpr unsigned long kPeerEntryBackingPointerOffset = 0x18;

static constexpr unsigned long kPeerBackingCountOffset = 0x5F0;
static constexpr unsigned long kPeerBackingArrayOffset = 0x5F8;
static constexpr unsigned long kPeerBackingEntrySize = 0x1460;
static constexpr unsigned long kPeerBackingPeerIdOffset = 0x10;

// Important helpers/consumers.
static constexpr unsigned long kRemovePeerRva = 0x00337010;
static constexpr unsigned long kGetPeerCountRva = 0x003377C0;
static constexpr unsigned long kGetPeerIdByIndexRva = 0x003377D0;
static constexpr unsigned long kGetGameSessionHostRva = 0x00337890;
static constexpr unsigned long kHostPromotionLikePathRva = 0x00335D06;

} // namespace gamesession
} // namespace darktide

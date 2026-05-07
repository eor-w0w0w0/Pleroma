#pragma once

// Inferred transport/callback API from Darktide.exe LAN reversing.
//
// These are not validated SDK declarations; they are a compact reference for
// future native patch work.

namespace darktide {
namespace transport {

static constexpr unsigned long kRegisterOuterCallbackVfuncOffset = 0x38;
static constexpr unsigned long kRegisterChannelCallbackVfuncOffset = 0x50;
static constexpr unsigned long kSendDiscoveryLikeVfuncOffset = 0x20;
static constexpr unsigned long kSendExplicitDestinationVfuncOffset = 0x30;
static constexpr unsigned long kResolveChannelHandleVfuncOffset = 0x60;
static constexpr unsigned long kStartPeerOrChannelVfuncOffset = 0x78;
static constexpr unsigned long kAllocatePeerStateVfuncOffset = 0x88;
static constexpr unsigned long kAttachPeerHelperVfuncOffset = 0xC8;
static constexpr unsigned long kDetachPeerHelperVfuncOffset = 0xD0;
static constexpr unsigned long kAcceptReplyStateVfuncOffset = 0x100;
static constexpr unsigned long kSendPayloadVfuncOffset = 0x108;
static constexpr unsigned long kAbortWithReasonVfuncOffset = 0x158;

// Callback registration appears to use an out-handle plus callback/context.
// The exact ABI is still inferred, but the outer callbacks consistently look
// like this after adaptation:
//   int callback(void *context, unsigned long long peer_id, unsigned int msg,
//                void *packet_reader)
// Current best interpretation:
// - `+0x38` and `+0x50` return token/handle objects, not singleton callback slots
// - registrations are likely multi-entry and removable by returned token

using OuterCallbackFn = int (*)(void *context, unsigned long long peer_id, unsigned int message_id, void *packet_reader);

// Channel callback adapters appear to receive channel/listener id separately,
// then normalize into the same general pattern.
using ChannelCallbackFn = int (*)(void *context, unsigned long long peer_id, unsigned int channel_or_listener_id, unsigned int message_id, void *packet_reader);
using SendInnerPayloadFn = int (*)(void *transport, unsigned int target_handle, unsigned int system_id, unsigned int reliable, const void *payload, unsigned int payload_bits, unsigned int trailing_zero);

// Known registered callbacks.
static constexpr unsigned long kRegisterOuterCallbackImplRva = 0x00351390;
static constexpr unsigned long kLanConnectOuterCallbackRva = 0x0056E550;
static constexpr unsigned long kLanCreateChannelCallbackRva = 0x0056EED0;
static constexpr unsigned long kLanConnectionlessOuterThunkRva = 0x0056F610;
static constexpr unsigned long kLanConnectionlessOuterCallbackRva = 0x0056F630;
static constexpr unsigned long kLanLobbyInnerThunkRva = 0x0056E720;
static constexpr unsigned long kLanLobbyInnerCallbackRva = 0x0056E740;
static constexpr unsigned long kLanChannelEnvelopeThunkRva = 0x0056FA20;
static constexpr unsigned long kLanChannelEnvelopeCallbackRva = 0x0056FA40;

// Token/record lifecycle helpers inferred from cleanup paths.
static constexpr unsigned long kBrowserRecordPushRva = 0x005702F0;
static constexpr unsigned long kBrowserRecordCleanupRva = 0x0056FC00;
static constexpr unsigned long kBrowserRecordVectorResizeRva = 0x00570020;
static constexpr unsigned long kBrowserRecordElementDtorRva = 0x0056F990;
static constexpr unsigned long kTokenMapFindSlotRva = 0x00570BD0;
static constexpr unsigned long kTokenMapGetOrInsertSlotRva = 0x00570C80;
static constexpr unsigned long kTokenMapEraseSlotRva = 0x00570A20;
static constexpr unsigned long kTokenMapGetOrCreateEntryRva = 0x00570990;

// Peer/source-id to sockaddr helpers.
static constexpr unsigned long kPeerAddressUpdateRva = 0x00587380;
static constexpr unsigned long kPeerAddressAddIfMissingRva = 0x00586F30;
static constexpr unsigned long kPeerAddressOverwriteRva = 0x00586F80;
static constexpr unsigned long kResolveCallbackSourceAddressRva = 0x0056E070;
static constexpr unsigned long kPeerAddressCountOffset = 0x80;
static constexpr unsigned long kPeerAddressEntriesOffset = 0x88;
static constexpr unsigned long kPeerAddressEntrySize = 0x30;
static constexpr unsigned long kPeerAddressEntryPeerIdOffset = 0x00;
static constexpr unsigned long kPeerAddressEntrySockAddrOffset = 0x08;

using PeerAddressUpsertFn = void (*)(void *table_owner, unsigned long long peer_id, const void *sockaddr_blob);

// Confirmed unregister methods seen in stock cleanup.
static constexpr unsigned long kUnregisterCallbackTokenVfuncOffset = 0x68;
static constexpr unsigned long kUnregisterChannelOrHandleVfuncOffset = 0xC0;
static constexpr unsigned long kUnregisterCallbackObjectVfuncOffset = 0x98;

} // namespace transport
} // namespace darktide

#pragma once

// Darktide retail build 1.11.0-b704841 native LAN/lobby symbols.
//
// These values are reverse-engineered RVAs from Darktide.exe and are intended
// as a code-side reference for future native host/bootstrap work.

namespace darktide::lan {

// Lua-exposed wrappers.
static constexpr unsigned long kJoinLanLobbyWrapperRva = 0x0048A1F0;
static constexpr unsigned long kLeaveLanLobbyWrapperRva = 0x0048A520;
static constexpr unsigned long kCreateLobbyBrowserWrapperRva = 0x0048AEC0;
static constexpr unsigned long kDestroyLobbyBrowserWrapperRva = 0x0048B070;
static constexpr unsigned long kLanLobbyStateWrapperRva = 0x0048A600;

// Global owners/singletons inferred from wrapper setup.
static constexpr unsigned long kNetworkOwnerGlobalRva = 0x012A4258;
static constexpr unsigned long kLanClientGlobalRva = 0x012A43E0;

// Live LanLobby object.
static constexpr unsigned long kLanLobbyVtableRva = 0x00F44500;
static constexpr unsigned long kLanLobbyBaseVtableRva = 0x00F23F68;
static constexpr unsigned long kLanLobbySize = 0x650;

// LanLobby field offsets.
static constexpr unsigned long kLanLobbyStateOffset = 0x7C;
static constexpr unsigned long kLanLobbyLobbyIdOffset = 0x58;
static constexpr unsigned long kLanLobbyLocalPeerIdOffset = 0x60;
static constexpr unsigned long kLanLobbyGameSessionHostOffset = 0x80;
static constexpr unsigned long kLanLobbyDataSizeOffset = 0x90;
static constexpr unsigned long kLanLobbyDataCapacityOffset = 0x94;
static constexpr unsigned long kLanLobbyDataPointerOffset = 0x98;
static constexpr unsigned long kLanLobbyStagingDataSizeOffset = 0xC0;
static constexpr unsigned long kLanLobbyStagingDataCapacityOffset = 0xC4;
static constexpr unsigned long kLanLobbyStagingDataPointerOffset = 0xC8;
static constexpr unsigned long kLanLobbyTransportOffset = 0x68;
static constexpr unsigned long kLanLobbyHostOrPeerMapOffset = 0x70;
static constexpr unsigned long kLanLobbyPendingConnectionOffset = 0x88;
static constexpr unsigned long kLanLobbyHostAddressOffset = 0x90;
static constexpr unsigned long kLanLobbyMembersCountOffset = 0x590;
static constexpr unsigned long kLanLobbyMembersArrayOffset = 0x598;
static constexpr unsigned long kLanLobbyPendingMembersCountOffset = 0x5A8;
static constexpr unsigned long kLanLobbyPendingMembersArrayOffset = 0x5B0;
static constexpr unsigned long kLanLobbyDirtyMemberDataFlagOffset = 0x648;
static constexpr unsigned long kLanLobbyDirtyLobbyDataFlagOffset = 0x649;
static constexpr unsigned long kLanLobbyHasFreshMemberDataFlagOffset = 0x64A;

// Native helper cluster.
static constexpr unsigned long kLanLobbyJoinBootstrapHelperRva = 0x0056C6E0;
static constexpr unsigned long kLanLobbyMembersSendHelperRva = 0x0056CA10;
static constexpr unsigned long kLanLobbyDataSendHelperRva = 0x0056CE00;
static constexpr unsigned long kLanLobbyDataBuildHelperRva = 0x0056CE60;
static constexpr unsigned long kLanLobbyMemberDataSendHelperRva = 0x0056D1B0;
static constexpr unsigned long kLanLobbyMemberDataBuildHelperRva = 0x0056D200;
static constexpr unsigned long kLanLobbyUpdateRva = 0x0056D7B0;
static constexpr unsigned long kLanLobbyRefreshHostMemberHelperRva = 0x0056E0D0;
static constexpr unsigned long kLanLobbyOuterConnectCallbackRva = 0x0056E4F0;
static constexpr unsigned long kLanLobbyCreateChannelCallbackRva = 0x0056EE70;
static constexpr unsigned long kLanLobbyRemoveMemberHelperRva = 0x0056E430;
static constexpr unsigned long kLanLobbyChannelDispatchRva = 0x0056E6E0;
static constexpr unsigned long kLanLobbyJoinReplyHandlerRva = 0x0056EBC6;
static constexpr unsigned long kLanConnectionlessSendConnectRequestRva = 0x0056EFF0;
static constexpr unsigned long kLanConnectionlessSendDiscoveryRva = 0x0056F2E0;
static constexpr unsigned long kLanConnectionlessParserRva = 0x0056F5D0;
static constexpr unsigned long kLanConnectionlessParserThunkRva = 0x0056F5B0;
static constexpr unsigned long kLanLobbyChannelEnvelopeThunkRva = 0x0056F9C0;
static constexpr unsigned long kLanJoinReplyBuildHelperRva = 0x006C56F0;

// State names table.
static constexpr unsigned long kLanLobbyStateNamesTableRva = 0x00F444D8;
static constexpr unsigned long kLanLobbyStateCreatingNameRva = 0x00F43EE8;
static constexpr unsigned long kLanLobbyStateJoiningNameRva = 0x00F43E88;
static constexpr unsigned long kLanLobbyStateJoinedNameRva = 0x00F43E90;
static constexpr unsigned long kLanLobbyStateFailedNameRva = 0x00F3CF48;

enum LanLobbyState {
    LanLobbyState_Creating = 0,
    LanLobbyState_Joining = 1,
    LanLobbyState_Joined = 2,
    LanLobbyState_Failed = 3,
};

// Recovered packet ids.
enum LanPacketId {
    LanPacket_ConnectionlessConnectRequest = 0x10,
    LanPacket_RequestConnectionReply = 0x11,
    LanPacket_DiscoverLobby = 0x12,
    LanPacket_DiscoverLobbyReply = 0x13,
    LanPacket_LobbyChannelEnvelope = 0x14,
    LanPacket_LobbyData = 0x14,
    LanPacket_LobbyMemberData = 0x15,
    LanPacket_LobbyRequestJoinReply = 0x17,
    LanPacket_LobbyRejectControl = 0x18,
    LanPacket_LobbyMembers = 0x19,
    LanPacket_LobbyRequestJoin = 0x1C,
};

// Outer LAN envelope helpers.
static constexpr unsigned long kLanOuterCreateChannelHandlerRva = 0x0056EE70;
static constexpr unsigned long kLanOuterLobbyChannelHandlerRva = 0x0056F9C0;

// Likely LanClient fields.
static constexpr unsigned long kLanClientBrowserTransportOffset = 0x18;
static constexpr unsigned long kLanClientTransportOffset = 0x68;
static constexpr unsigned long kLanClientActiveLobbiesCountOffset = 0x80;
static constexpr unsigned long kLanClientActiveLobbiesArrayOffset = 0x88;
static constexpr unsigned long kLanClientSelfPeerIdOffset = 0x90;
static constexpr unsigned long kLanClientKnownPeersCountOffset = 0x590;
static constexpr unsigned long kLanClientKnownPeersArrayOffset = 0x598;
static constexpr unsigned long kLanClientKnownPeersOwnerOffset = 0x5A0;

// Known peer/member record layout in the 0x60-byte member array.
static constexpr unsigned long kLanMemberRecordSize = 0x60;
static constexpr unsigned long kLanMemberIdOffset = 0x00;
static constexpr unsigned long kLanMemberAddressStringOffset = 0x14;
static constexpr unsigned long kLanMemberDataSizeOffset = 0x30;
static constexpr unsigned long kLanMemberDataCapacityOffset = 0x34;
static constexpr unsigned long kLanMemberDataPointerOffset = 0x38;
static constexpr unsigned long kLanMemberStagingDataSizeOffset = 0x48;
static constexpr unsigned long kLanMemberStagingDataCapacityOffset = 0x4C;
static constexpr unsigned long kLanMemberStagingDataPointerOffset = 0x50;

// Temporary admission record passed to 0x14056E0D0.
static constexpr unsigned long kLanAdmissionRecordSize = 0x60;
static constexpr unsigned long kLanAdmissionPeerIdOffset = 0x00;
static constexpr unsigned long kLanAdmissionBufferOwnerAOffset = 0x40;
static constexpr unsigned long kLanAdmissionBufferOwnerBOffset = 0x58;

// LanLobbyBrowser object.
static constexpr unsigned long kLanLobbyBrowserVtableRva = 0x00F1F2F8;
static constexpr unsigned long kLanLobbyBrowserSize = 0xB8;
static constexpr unsigned long kLanLobbyBrowserTransportOffset = 0x50;
static constexpr unsigned long kLanLobbyBrowserEntriesCountOffset = 0x58;
static constexpr unsigned long kLanLobbyBrowserEntriesArrayOffset = 0x60;
static constexpr unsigned long kLanLobbyBrowserAllocatorOffset = 0x68;
static constexpr unsigned long kLanLobbyBrowserCallbackHandleOffset = 0x70;
static constexpr unsigned long kLanLobbyBrowserDescriptorPointerOffset = 0x78;
static constexpr unsigned long kLanLobbyBrowserEndpointStringOffset = 0x80;
static constexpr unsigned long kLanLobbyBrowserEndpointStringSize = 0x25;
static constexpr unsigned long kLanLobbyBrowserEntryHashOwnerOffset = 0x48;

// LanLobbyBrowser result entry layout (0x48-byte stride).
static constexpr unsigned long kLanBrowserEntrySize = 0x48;
static constexpr unsigned long kLanBrowserEntryBlobSizeOffset = 0x00;
static constexpr unsigned long kLanBrowserEntryBlobCapacityOffset = 0x04;
static constexpr unsigned long kLanBrowserEntryBlobPointerOffset = 0x08;
static constexpr unsigned long kLanBrowserEntryBlobOwnerOffset = 0x10;
static constexpr unsigned long kLanBrowserEntryKeySizeOffset = 0x18;
static constexpr unsigned long kLanBrowserEntryKeyCapacityOffset = 0x1C;
static constexpr unsigned long kLanBrowserEntryKeyPointerOffset = 0x20;
static constexpr unsigned long kLanBrowserEntryKeyOwnerOffset = 0x28;
static constexpr unsigned long kLanBrowserEntryOpaqueIdOffset = 0x30;
static constexpr unsigned long kLanBrowserEntryMatchKeyOffset = 0x38;
static constexpr unsigned long kLanBrowserEntryOrdinalOffset = 0x3C;
static constexpr unsigned long kLanBrowserEntryUpdatedFlagOffset = 0x40;

} // namespace darktide::lan

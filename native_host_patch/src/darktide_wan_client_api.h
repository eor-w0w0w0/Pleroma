#pragma once

// Inferred script-facing LanClient wrapper and underlying LanTransport pieces.

namespace darktide {
namespace wanclient {

// Script-facing wrapper returned by Network.init_wan_client.
static constexpr unsigned long kInitWanClientWrapperRva = 0x005148D0;
static constexpr unsigned long kWanClientOwnerGlobalRva = 0x012A33F0;
static constexpr unsigned long kWanClientOwnerNestedOffset = 0x418;
static constexpr unsigned long kWanClientOwnerBrowserTransportOffset = 0x68;
static constexpr unsigned long kLanClientWrapperVtableRva = 0x00F41C50;
static constexpr unsigned long kLanClientWrapperBaseVtableRva = 0x00F41C18;
static constexpr unsigned long kLanClientWrapperSize = 0xA0;

static constexpr unsigned long kLanClientWrapperBackendOffset = 0x10;
static constexpr unsigned long kLanClientWrapperManagerOffset = 0x18;
static constexpr unsigned long kLanClientWrapperArrayOffset = 0x28;
static constexpr unsigned long kLanClientWrapperHashOffset = 0x48;
static constexpr unsigned long kLanClientWrapperEndpointOffset = 0x70;

// Underlying backend object family.
static constexpr unsigned long kLanTransportBackendCtorRva = 0x005869B0;
static constexpr unsigned long kLanTransportBackendVtableRva = 0x00F42C90;

// Known wrapper virtuals.
static constexpr unsigned long kLanClientWrapperDtorRva = 0x005706F0;
static constexpr unsigned long kLanClientWrapperUpdateRva = 0x005707D0;

} // namespace wanclient
} // namespace darktide

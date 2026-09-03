// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The codec for every net message, instantiated once per module that sends or receives
// them. Keeping it here is what lets netmessages_schema.h stay free of <meta>, which costs
// roughly 0.6s in every translation unit that parses it.

#include "netmessages_schema.h"
#include "usermessages_schema.h"
#include "proto.h"

#define X( name ) KS_PROTO_INSTANTIATE( ks::net::name )
KS_NET_MESSAGES( X )
KS_USER_MESSAGES( X )
#undef X

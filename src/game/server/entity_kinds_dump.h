// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#pragma once

#ifdef KS_ENTITY_KINDS_DUMP
#define KS_ENTITY_KINDS_DUMP_CLASS( C ) static_assert( false, std::string_view{ std::define_static_string( ks_dump::describe<C>() ) } );
#else
#define KS_ENTITY_KINDS_DUMP_CLASS( C )
#endif

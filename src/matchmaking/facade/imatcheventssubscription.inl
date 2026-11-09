// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMatchEventsSubscription class to CMatchEventsSubscription. Included by the source that defines CMatchEventsSubscription.

void IMatchEventsSubscription::Subscribe( const void *pOwner, MatchEventHandler_t handler )
{ static_cast<CMatchEventsSubscription*>(this)->Subscribe( pOwner, std::move( handler ) ); }
void IMatchEventsSubscription::Unsubscribe( const void *pOwner )
{ static_cast<CMatchEventsSubscription*>(this)->Unsubscribe( pOwner ); }
void IMatchEventsSubscription::BroadcastEvent( KeyValues *pEvent )
{ static_cast<CMatchEventsSubscription*>(this)->BroadcastEvent( pEvent ); }
void IMatchEventsSubscription::RegisterEventData( KeyValues *pEventData )
{ static_cast<CMatchEventsSubscription*>(this)->RegisterEventData( pEventData ); }
KeyValues * IMatchEventsSubscription::GetEventData( char const *szEventDataKey )
{ return static_cast<CMatchEventsSubscription*>(this)->GetEventData( szEventDataKey ); }

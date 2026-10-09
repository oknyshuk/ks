// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

import ks.tier1;
#include "blackbox.h"
#include "cmd.h"
import std;

// development only, off by default for 360
ConVar blackbox( "blackbox", "1" );

constexpr auto MAX_MESSAGE_SIZE = 1024;
constexpr auto DEFAULT_RECORD_LIMIT = 16;

static const char* gTypeMap[] = 
{
	"VCD",
	"WAV",
	"BOT",
	nullptr
};

struct CBlackBoxRecord
{
	CBlackBoxRecord( const char *new_message ) : m_time( Plat_FloatTime() ), m_message( new_message ) {}

	operator const char *() const	
	{
		static CFmtStrN<MAX_MESSAGE_SIZE+16> buf;

		double temp = m_time;
		int hh = int(temp/(60*60));
		int mm = int(temp/60) % 60;
		float ss = temp - ((mm + (hh * 60)) * 60);

		buf.sprintf( "[%02d:%02d:%02.3f]: %s", hh, mm, ss, m_message.c_str() );

		return buf;
	}

	double m_time;
	std::string m_message;
};

class CBlackBox: public IBlackBox
{
public:
	CBlackBox();
	~CBlackBox();

	virtual void Record(int type, const char *msg);
	virtual void SetLimit(int type, unsigned int count);
	virtual const char *Get(int type, unsigned int index);
	virtual int Count(int type);
	virtual void Flush(int type);

	virtual const char *GetTypeName(int type);
	virtual int GetTypeCount();

	enum Types {
		VCD,
		WAV,
		BOT,
		TYPE_COUNT
	};

private:
	bool ValidType(int type) { return (type >= 0 && type < TYPE_COUNT); };

	std::deque< CBlackBoxRecord > m_records[TYPE_COUNT];	// oldest first
	uint m_record_limits[TYPE_COUNT];
};

CBlackBox gCBlackBox;
IBlackBox *gBlackBox = &gCBlackBox;

static InterfaceReg s_Reg_CBlackBox_IBlackBox( []() -> void * { return static_cast<IBlackBox *>( &gCBlackBox ); }, BLACKBOX_INTERFACE_VERSION );;

CBlackBox::CBlackBox()
{
	for ( int i = 0; i < TYPE_COUNT; i++ )
	{
		m_record_limits[i] = DEFAULT_RECORD_LIMIT;
	}
}

CBlackBox::~CBlackBox()
{
	for (int i = 0; i < TYPE_COUNT; i++)
	{
		Flush(i);
	}
}

void CBlackBox::Record( int type, const char *msg )
{
	if ( !blackbox.GetBool() )
		return;

	if ( !ValidType(type) )	
		return;

	m_records[type].emplace_back( msg );
	if ( (uint)std::ssize( m_records[type] ) > m_record_limits[type] )
		m_records[type].pop_front();
}

void CBlackBox::SetLimit(int type, unsigned int count)
{
	if ( !ValidType( type ))
		return;

	m_record_limits[type] = count;
}

const char *CBlackBox::Get( int type, unsigned int index )
{
	if ( !ValidType( type ))
		return nullptr;
	
	return m_records[type][index];
}

const char *CBlackBox::GetTypeName( int type )
{
	if ( !ValidType( type ))
		return nullptr;

	return gTypeMap[type];
}

int CBlackBox::GetTypeCount()
{
	return TYPE_COUNT;
}

int CBlackBox::Count( int type )
{
	if ( !ValidType( type ))
		return -1;

	return std::ssize( m_records[type] );
}

void CBlackBox::Flush( int type )
{
	if ( !ValidType( type ))
		return;

	m_records[type].clear();
}

static void blackbox_record( const CCommand &args );
static ConCommand blackbox_record_command( "blackbox_record", blackbox_record, "Record an entry into the blackbox", FCVAR_DONTRECORD );
static void blackbox_record( const CCommand &args )
{
	if ( !blackbox.GetBool() )
		return;

	if ( args.ArgC() < 2 )
	{
		Msg( "Insufficient arguments to blackbox_record. Usage: blackbox_record <type> <message>\n" );
		return;
	}

	BlackBox_Record( args[1], args[2] );
}

static void blackbox_dump( const CCommand &args );
static ConCommand blackbox_dump_command( "blackbox_dump", blackbox_dump, "Dump the contents of the blackbox", FCVAR_DONTRECORD );
static void blackbox_dump( const CCommand &args )
{

	for ( int type = 0; type < gBlackBox->GetTypeCount(); type++ )
	{
		for ( int i = 0; i < gBlackBox->Count(type); i++ )
		{
			Msg( "%s[%d]: %s\n", gBlackBox->GetTypeName( type ), i+1, gBlackBox->Get( type, i ) );				
		}
	}
}

void BlackBox_Record( const char *type, const char *pFormat, ... )
{
	if ( !blackbox.GetBool() )
		return;

	int type_num;
	for ( type_num = 0; type_num < gBlackBox->GetTypeCount(); type_num++ )
	{
		if ( !V_strcasecmp( gBlackBox->GetTypeName( type_num ), type ) )
			break;
	}

	if ( type_num >= gBlackBox->GetTypeCount() )
	{
		Msg( "Invalid blackbox type: %s\n", type );
		return;
	}

	char szMessage[1024];	
	std::va_list marker;

	__builtin_va_start( marker, pFormat);
	V_vsnprintf( szMessage, sizeof( szMessage ), pFormat, marker);
	__builtin_va_end( marker );	

	//Msg( "Record: %s: %s\n", type, szMessage );
	gBlackBox->Record( type_num, szMessage );
}

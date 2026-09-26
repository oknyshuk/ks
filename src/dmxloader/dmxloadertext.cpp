//====== Copyright (c) 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "dmxloader/dmxelement.h"
#include <ctype.h>
#include "tier1/serialization.h"
#include "tier1/text_reader.h"
#include "tier1/text_writer.h"
#include <limits.h>
#include "dmxserializationdictionary.h"


//-----------------------------------------------------------------------------
// a simple class to keep track of a stack of valid parsed symbols
//-----------------------------------------------------------------------------
class CDmxKeyValues2ErrorStack
{
public:
	CDmxKeyValues2ErrorStack();

	// Sets the filename to report with errors; sets the line number to 0
	void SetFilename( const char *pFilename );

	// Current line control
	void IncrementCurrentLine();
	void SetCurrentLine( int nLine );
	int GetCurrentLine() const;

	// entering a new keyvalues block, save state for errors
	// Not save symbols instead of pointers because the pointers can move!
	int Push( std::string_view symName );

	// exiting block, error isn't in this block, remove.
	void Pop();

	// Allows you to keep the same stack level, but change the name as you parse peers
	void Reset( int stackLevel, std::string_view symName );

	// Hit an error, report it and the parsing stack for context
	void ReportError( const char *pError, ... );

	static ks::intern_pool& GetSymbolTable() { return m_ErrorSymbolTable; }

private:
	enum
	{
		MAX_ERROR_STACK = 64
	};

	std::string_view	m_errorStack[MAX_ERROR_STACK];
	const char *m_pFilename;
	int		m_nFileLine;
	int		m_errorIndex;
	int		m_maxErrorIndex;

	static ks::intern_pool m_ErrorSymbolTable;
};


ks::intern_pool CDmxKeyValues2ErrorStack::m_ErrorSymbolTable;


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CDmxKeyValues2ErrorStack g_KeyValues2ErrorStack;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDmxKeyValues2ErrorStack::CDmxKeyValues2ErrorStack() : 
	m_pFilename("NULL"), m_errorIndex(0), m_maxErrorIndex(0), m_nFileLine(1) 
{
}


//-----------------------------------------------------------------------------
// Sets the filename
//-----------------------------------------------------------------------------
void CDmxKeyValues2ErrorStack::SetFilename( const char *pFilename )
{
	m_pFilename = pFilename;
	m_maxErrorIndex = 0;
	m_nFileLine = 1;
}


//-----------------------------------------------------------------------------
// Current line control
//-----------------------------------------------------------------------------
void CDmxKeyValues2ErrorStack::IncrementCurrentLine()
{
	++m_nFileLine;
}

void CDmxKeyValues2ErrorStack::SetCurrentLine( int nLine )
{
	m_nFileLine = nLine;
}

int CDmxKeyValues2ErrorStack::GetCurrentLine() const
{
	return m_nFileLine;
}


//-----------------------------------------------------------------------------
// entering a new keyvalues block, save state for errors
// Not save symbols instead of pointers because the pointers can move!
//-----------------------------------------------------------------------------
int CDmxKeyValues2ErrorStack::Push( std::string_view symName )
{
	if ( m_errorIndex < MAX_ERROR_STACK )
	{
		m_errorStack[m_errorIndex] = symName;
	}
	m_errorIndex++;
	m_maxErrorIndex = std::max( m_maxErrorIndex, (m_errorIndex-1) );
	return m_errorIndex-1;
}


//-----------------------------------------------------------------------------
// exiting block, error isn't in this block, remove.
//-----------------------------------------------------------------------------
void CDmxKeyValues2ErrorStack::Pop()
{
	m_errorIndex--;
	Assert(m_errorIndex>=0);
}


//-----------------------------------------------------------------------------
// Allows you to keep the same stack level, but change the name as you parse peers
//-----------------------------------------------------------------------------
void CDmxKeyValues2ErrorStack::Reset( int stackLevel, std::string_view symName )
{
	Assert( stackLevel >= 0 && stackLevel < m_errorIndex );
	m_errorStack[stackLevel] = symName;
}


//-----------------------------------------------------------------------------
// Hit an error, report it and the parsing stack for context
//-----------------------------------------------------------------------------
void CDmxKeyValues2ErrorStack::ReportError( const char *pFmt, ... )
{
	char temp[2048];

	va_list args;
	va_start( args, pFmt );
	Q_vsnprintf( temp, sizeof( temp ), pFmt, args );
	va_end( args );

	Warning( "%s(%d) : %s\n", m_pFilename, m_nFileLine, temp );

	for ( int i = 0; i < m_maxErrorIndex; i++ )
	{
		if ( !m_errorStack[i].data() )
			continue;

		if ( i < m_errorIndex )
		{
			Warning( "%s, ", m_errorStack[i].data() );
		}
		else
		{
			Warning( "(*%s*), ", m_errorStack[i].data() );
		}
	}
	Warning( "\n" );
}


//-----------------------------------------------------------------------------
// a simple helper that creates stack entries as it goes in & out of scope
//-----------------------------------------------------------------------------
class CKeyValues2ErrorContext
{
public:
	CKeyValues2ErrorContext( const char *pSymName )
	{
		Init( pSymName ? CDmxKeyValues2ErrorStack::GetSymbolTable().intern( pSymName ) : std::string_view() );
	}

	CKeyValues2ErrorContext( std::string_view symName )
	{
		Init( symName );
	}

	~CKeyValues2ErrorContext()
	{
		g_KeyValues2ErrorStack.Pop();
	}

	void Reset( std::string_view symName )
	{
		g_KeyValues2ErrorStack.Reset( m_stackLevel, symName );
	}

private:
	void Init( std::string_view symName )
	{
		m_stackLevel = g_KeyValues2ErrorStack.Push( symName );
	}

	int m_stackLevel;
};


//-----------------------------------------------------------------------------
// Element dictionary used in unserialization
//-----------------------------------------------------------------------------
typedef int DmxElementDictHandle_t;
enum
{
	ELEMENT_DICT_HANDLE_INVALID = (DmxElementDictHandle_t)~0
};


class CDmxElementDictionary
{
public:
	CDmxElementDictionary();

	DmxElementDictHandle_t InsertElement( CDmxElement *pElement );
	CDmxElement *GetElement( DmxElementDictHandle_t handle );
	void AddAttribute( CDmxAttribute *pAttribute, const DmObjectId_t &pElementId );
	void AddArrayAttribute( CDmxAttribute *pAttribute, DmxElementDictHandle_t hChild );
	void AddArrayAttribute( CDmxAttribute *pAttribute, const DmObjectId_t &pElementId );

	// Finds an element into the table
	DmxElementDictHandle_t FindElement( CDmxElement *pElement );
	DmxElementDictHandle_t FindElement( const DmObjectId_t &objectId );

	// Sets the element id for an element
	void SetElementId( DmxElementDictHandle_t hElement, const DmObjectId_t &objectId );

	// Hook up all element references (which were unserialized as object ids)
	void HookUpElementReferences();

	// Clears the dictionary
	void Clear();

	// iteration through elements
	DmxElementDictHandle_t FirstElement() { return 0; }
	DmxElementDictHandle_t NextElement( DmxElementDictHandle_t h )
	{
		return ks::is_valid_index( m_Dict, h+1 ) ? h+1 : ELEMENT_DICT_HANDLE_INVALID;
	}

private:
	struct DictInfo_t
	{
		CDmxElement *m_pElement;
		DmObjectId_t m_Id;
	};

	struct AttributeInfo_t
	{
		CDmxAttribute *m_pAttribute;
		bool m_bObjectId;
		union
		{
			DmxElementDictHandle_t m_hElement;
			DmObjectId_t m_ObjectId;
		};
	};
	typedef std::vector< AttributeInfo_t > AttributeList_t;

	// Hook up all element references (which were unserialized as object ids)
	void HookUpElementAttributes();
	void HookUpElementArrayAttributes();

	std::vector< DictInfo_t > m_Dict;
	AttributeList_t m_Attributes;
	AttributeList_t m_ArrayAttributes;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDmxElementDictionary::CDmxElementDictionary()
{
}


//-----------------------------------------------------------------------------
// Clears the dictionary
//-----------------------------------------------------------------------------
void CDmxElementDictionary::Clear()
{
	m_Dict.clear(); m_Dict.shrink_to_fit();
	m_Attributes.clear(); m_Attributes.shrink_to_fit();
	m_ArrayAttributes.clear(); m_ArrayAttributes.shrink_to_fit();
}


//-----------------------------------------------------------------------------
// Inserts an element into the table
//-----------------------------------------------------------------------------
DmxElementDictHandle_t CDmxElementDictionary::InsertElement( CDmxElement *pElement )
{
	// Insert it into the reconnection table
	DmxElementDictHandle_t h = std::ssize( m_Dict ); m_Dict.emplace_back();
	m_Dict[h].m_pElement = pElement;
	InvalidateUniqueId( &m_Dict[h].m_Id );
	return h;
}


//-----------------------------------------------------------------------------
// Sets the element id for an element
//-----------------------------------------------------------------------------
void CDmxElementDictionary::SetElementId( DmxElementDictHandle_t hElement, const DmObjectId_t &objectId )
{
	Assert( hElement != ELEMENT_DICT_HANDLE_INVALID );
	CopyUniqueId( objectId, &m_Dict[hElement].m_Id );
}


//-----------------------------------------------------------------------------
// Returns a particular element
//-----------------------------------------------------------------------------
CDmxElement *CDmxElementDictionary::GetElement( DmxElementDictHandle_t handle )
{
	if ( handle == ELEMENT_DICT_HANDLE_INVALID )
		return nullptr;

	return m_Dict[ handle ].m_pElement;
}


//-----------------------------------------------------------------------------
// Adds an attribute to the fixup list
//-----------------------------------------------------------------------------
void CDmxElementDictionary::AddAttribute( CDmxAttribute *pAttribute, const DmObjectId_t &objectId )
{
	int i = std::ssize( m_Attributes ); m_Attributes.emplace_back();
	m_Attributes[i].m_bObjectId = true;
	m_Attributes[i].m_pAttribute = pAttribute;
	CopyUniqueId( objectId, &m_Attributes[i].m_ObjectId );
}


//-----------------------------------------------------------------------------
// Adds an element of an attribute array to the fixup list
//-----------------------------------------------------------------------------
void CDmxElementDictionary::AddArrayAttribute( CDmxAttribute *pAttribute, DmxElementDictHandle_t hElement )
{
	int i = std::ssize( m_ArrayAttributes ); m_ArrayAttributes.emplace_back();
	m_ArrayAttributes[i].m_bObjectId = false;
	m_ArrayAttributes[i].m_pAttribute = pAttribute;
	m_ArrayAttributes[i].m_hElement = hElement;
}

void CDmxElementDictionary::AddArrayAttribute( CDmxAttribute *pAttribute, const DmObjectId_t &objectId )
{
	int i = std::ssize( m_ArrayAttributes ); m_ArrayAttributes.emplace_back();
	m_ArrayAttributes[i].m_bObjectId = true;
	m_ArrayAttributes[i].m_pAttribute = pAttribute;
	CopyUniqueId( objectId, &m_ArrayAttributes[i].m_ObjectId );
}


//-----------------------------------------------------------------------------
// Finds an element into the table
//-----------------------------------------------------------------------------
DmxElementDictHandle_t CDmxElementDictionary::FindElement( CDmxElement *pElement )
{
	int nCount = std::ssize( m_Dict );
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pElement == m_Dict[i].m_pElement )
			return i;
	}
	return ELEMENT_DICT_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Finds an element into the table
//-----------------------------------------------------------------------------
DmxElementDictHandle_t CDmxElementDictionary::FindElement( const DmObjectId_t &objectId )
{
	int nCount = std::ssize( m_Dict );
	for ( int i = 0; i < nCount; ++i )
	{
		if ( IsUniqueIdEqual( objectId, m_Dict[i].m_Id ) )
			return i;
	}
	return ELEMENT_DICT_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Hook up all element references (which were unserialized as object ids)
//-----------------------------------------------------------------------------
void CDmxElementDictionary::HookUpElementAttributes()
{
	int n = std::ssize( m_Attributes );
	for ( int i = 0; i < n; ++i )
	{
		Assert( m_Attributes[i].m_bObjectId );

		DmxElementDictHandle_t hElement = FindElement( m_Attributes[i].m_ObjectId );
		CDmxElement *pElement = GetElement( hElement );
		m_Attributes[i].m_pAttribute->SetValue( pElement );
	}
}


//-----------------------------------------------------------------------------
// Hook up all element array references
//-----------------------------------------------------------------------------
void CDmxElementDictionary::HookUpElementArrayAttributes()
{
	int n = std::ssize( m_ArrayAttributes );
	for ( int i = 0; i < n; ++i )
	{
		std::vector< CDmxElement* > &array = m_ArrayAttributes[i].m_pAttribute->GetArrayForEdit<CDmxElement*>();

		if ( !m_ArrayAttributes[i].m_bObjectId )
		{
			CDmxElement *pElement = GetElement( m_ArrayAttributes[i].m_hElement );
			array.push_back( pElement );
		}
		else
		{
			// search id->handle table (both loaded and unloaded) for id, and if not found, create a new handle, map it to the id and return it
			DmxElementDictHandle_t hElement = FindElement( m_ArrayAttributes[i].m_ObjectId );
			CDmxElement *pElement = GetElement( hElement );
			array.push_back( pElement );
		}
	}
}


//-----------------------------------------------------------------------------
// Hook up all element references (which were unserialized as object ids)
//-----------------------------------------------------------------------------
void CDmxElementDictionary::HookUpElementReferences()
{
	HookUpElementArrayAttributes();
	HookUpElementAttributes();
}



//-----------------------------------------------------------------------------
// Unserialization class for Key Values 2
//-----------------------------------------------------------------------------
class CDmxSerializerKeyValues2
{
public:
	bool Unserialize( const char *pFileName, ks::text_reader &buf, CDmxElement **ppRoot );
	bool Serialize( ks::text_writer &buf, CDmxElement *pRoot, const char *pFileName );

private:
	enum TokenType_t
	{
		TOKEN_INVALID = -1,		// A bogus token
		TOKEN_OPEN_BRACE,		// {
		TOKEN_CLOSE_BRACE,		// }
		TOKEN_OPEN_BRACKET,		// [
		TOKEN_CLOSE_BRACKET,	// ]
		TOKEN_COMMA,			// ,
//		TOKEN_STRING,			// Any non-quoted string
		TOKEN_DELIMITED_STRING,	// Any quoted string
		TOKEN_INCLUDE,			// #include
		TOKEN_EOF,				// End of buffer
	};

	// Methods related to unserialization
	void EatWhitespacesAndComments( ks::text_reader &buf );
	TokenType_t ReadToken( ks::text_reader &buf, std::string_view &token );
	DmxElementDictHandle_t CreateDmxElement( const char *pElementType );
	bool UnserializeAttributeValueFromToken( CDmxAttribute *pAttribute, DmAttributeType_t type, std::string_view token );
	bool UnserializeElementAttribute( ks::text_reader &buf, DmxElementDictHandle_t hElement, const char *pAttributeName, const char *pElementType );
	bool UnserializeElementArrayAttribute( ks::text_reader &buf, DmxElementDictHandle_t hElement, const char *pAttributeName );
	bool UnserializeArrayAttribute( ks::text_reader &buf, DmxElementDictHandle_t hElement, const char *pAttributeName, DmAttributeType_t nAttrType );
	bool UnserializeAttribute( ks::text_reader &buf, DmxElementDictHandle_t hElement, const char *pAttributeName, DmAttributeType_t nAttrType );
	bool UnserializeId( ks::text_reader &buf, DmxElementDictHandle_t hElement );
	bool UnserializeElement( ks::text_reader &buf, const char *pElementType, DmxElementDictHandle_t *pHandle );
	bool UnserializeElement( ks::text_reader &buf, DmxElementDictHandle_t *pHandle );

	// Methods related to serialization
	void SerializeArrayAttribute( ks::text_writer &buf, CDmxAttribute *pAttribute );
	void SerializeElementAttribute( ks::text_writer &buf, CDmxSerializationDictionary &dict, CDmxAttribute *pAttribute );
	void SerializeElementArrayAttribute( ks::text_writer &buf, CDmxSerializationDictionary &dict, CDmxAttribute *pAttribute );
	bool SerializeAttributes( ks::text_writer &buf, CDmxSerializationDictionary &dict, CDmxElement *pElement );
	bool SaveElement( ks::text_writer &buf, CDmxSerializationDictionary &dict, CDmxElement *pElement, bool bWriteDelimiters = true );

	// For unserialization
	CDmxElementDictionary m_ElementDict;
	DmxElementDictHandle_t m_hRoot;
};


//-----------------------------------------------------------------------------
// Serializes a single element attribute
//-----------------------------------------------------------------------------
void CDmxSerializerKeyValues2::SerializeElementAttribute( ks::text_writer &buf, CDmxSerializationDictionary &dict, CDmxAttribute *pAttribute )
{
	CDmxElement *pElement = pAttribute->GetValue< CDmxElement* >();
	if ( dict.ShouldInlineElement( pElement ) )
	{
		buf.printf( "\"%s\"\n{\n", pElement->GetTypeString() );
		if ( pElement )
		{
			SaveElement( buf, dict, pElement, false );
		}
		buf.printf( "}\n" );
	}
	else
	{
		buf.printf( "\"%s\" \"", CDmxAttribute::s_pAttributeTypeName[ AT_ELEMENT ] );
		if ( pElement )
		{
			::Serialize( buf, pElement->GetId() );
		}
		buf.put( '\"' );
	}
}


//-----------------------------------------------------------------------------
// Serializes an array element attribute
//-----------------------------------------------------------------------------
void CDmxSerializerKeyValues2::SerializeElementArrayAttribute( ks::text_writer &buf, CDmxSerializationDictionary &dict, CDmxAttribute *pAttribute )
{
	const std::vector< CDmxElement* > &array = pAttribute->GetArray< CDmxElement* >();

	buf.printf( "\n[\n" );
	buf.push_tab();

	int nCount = std::ssize( array );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pElement = array[i];
		if ( dict.ShouldInlineElement( pElement ) )
		{
			buf.printf( "\"%s\"\n{\n", pElement->GetTypeString() );
			if ( pElement )
			{
				SaveElement( buf, dict, pElement, false );
			}
			buf.put( '}' );
		}
		else
		{
			const char *pAttributeType = CDmxAttribute::s_pAttributeTypeName[ AT_ELEMENT ];
			buf.printf( "\"%s\" \"", pAttributeType );
			if ( pElement )
			{
				::Serialize( buf, pElement->GetId() );
			}
			buf.put( '\"' );
		}

		if ( i != nCount - 1 )
		{
			buf.put( ',' );
		}
		buf.put( '\n' );
	}

	buf.pop_tab();
	buf.printf( "]" );
}


//-----------------------------------------------------------------------------
// Serializes array attributes
//-----------------------------------------------------------------------------
void CDmxSerializerKeyValues2::SerializeArrayAttribute( ks::text_writer &buf, CDmxAttribute *pAttribute )
{
	int nCount = pAttribute->GetArrayCount();

	buf.put( "\n[\n" );
	buf.push_tab();

	for ( int i = 0; i < nCount; ++i )
	{
		if ( pAttribute->GetType() != AT_STRING_ARRAY )
		{
			buf.put( '\"' );
			buf.push_tab();
		}

		pAttribute->SerializeElement( i, buf );

		if ( pAttribute->GetType() != AT_STRING_ARRAY )
		{
			buf.pop_tab();
			buf.put( '\"' );
		}

		if ( i != nCount - 1 )
		{
			buf.put( ',' );
		}
		buf.put( '\n' );
	}
	buf.pop_tab();
	buf.put( ']' );
}


//-----------------------------------------------------------------------------
// Serializes all attributes in an element
//-----------------------------------------------------------------------------
static int SortAttributeByName(const void *p1, const void *p2 )
{
	const CDmxAttribute **ppAtt1 = (const CDmxAttribute**)p1;
	const CDmxAttribute **ppAtt2 = (const CDmxAttribute**)p2;
	const char *pAttName1 = (*ppAtt1)->GetName();
	const char *pAttName2 = (*ppAtt2)->GetName();
	return Q_stricmp( pAttName1, pAttName2 );
}

bool CDmxSerializerKeyValues2::SerializeAttributes( ks::text_writer &buf, CDmxSerializationDictionary &dict, CDmxElement *pElement )
{
	int nCount = pElement->AttributeCount();
	CDmxAttribute **ppAttributes = (CDmxAttribute**)stackalloc( nCount * sizeof(CDmxAttribute*) );
	for ( int i = 0; i < nCount; ++i )
	{
		ppAttributes[i] = pElement->GetAttribute( i );
	}

	// Sort by name
	qsort( ppAttributes, nCount, sizeof(CDmxAttribute*), SortAttributeByName );

	for ( int i = 0; i < nCount; ++i )
	{
		CDmxAttribute *pAttribute = ppAttributes[ i ];

		const char *pName = pAttribute->GetName( );
		DmAttributeType_t nAttrType = pAttribute->GetType();
		if ( nAttrType != AT_ELEMENT )
		{
			buf.printf( "\"%s\" \"%s\" ", pName, CDmxAttribute::s_pAttributeTypeName[ nAttrType ] );
		}
		else
		{
			// Elements either serialize their type name or "element" depending on whether they are inlined
			buf.printf( "\"%s\" ", pName );
		}

		switch( nAttrType )
		{
		default:
			if ( nAttrType >= AT_FIRST_ARRAY_TYPE )
			{
				SerializeArrayAttribute( buf, pAttribute );
			}
			else
			{
				if ( pAttribute->SerializesOnMultipleLines() )
				{
					buf.put( '\n' );
				}

				buf.put( '\"' );
				buf.push_tab();
				pAttribute->Serialize( buf );
				buf.pop_tab();
				buf.put( '\"' );
			}
			break;

		case AT_STRING:
			// Don't explicitly add string delimiters; serialization does that.
			pAttribute->Serialize( buf );
			break;

		case AT_ELEMENT:
			SerializeElementAttribute( buf, dict, pAttribute );
			break;

		case AT_ELEMENT_ARRAY:
			SerializeElementArrayAttribute( buf, dict, pAttribute );
			break;
		}

		buf.put( '\n' );
	}

	return true;
}

bool CDmxSerializerKeyValues2::SaveElement( ks::text_writer &buf, CDmxSerializationDictionary &dict, CDmxElement *pElement, bool bWriteDelimiters )
{
	if ( bWriteDelimiters )
	{
		buf.printf( "\"%s\"\n{\n", pElement->GetTypeString() );
	}
	buf.push_tab();

	// explicitly serialize id, now that it's no longer an attribute
	buf.put( "\"id\" \"elementid\" " );
	buf.put( '\"' );
	::Serialize( buf, pElement->GetId() );
	buf.put( "\"\n" );

	SerializeAttributes( buf, dict, pElement );

	buf.pop_tab();
	if ( bWriteDelimiters )
	{
		buf.printf( "}\n" );
	}
	return true;
}

bool CDmxSerializerKeyValues2::Serialize( ks::text_writer &outBuf, CDmxElement *pRoot, const char *pFormatName )
{
	SetSerializationDelimiter( &ks::c_string_escapes );
	SetSerializationArrayDelimiter( "," );

	bool bFlatMode = !Q_stricmp( pFormatName, "keyvalues2_flat" );

	// Save elements, attribute links
	CDmxSerializationDictionary dict;
	dict.BuildElementList( pRoot, bFlatMode );

	// Save elements to buffer
	DmxSerializationHandle_t i;
	for ( i = dict.FirstRootElement(); i != ELEMENT_DICT_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		SaveElement( outBuf, dict, dict.GetRootElement( i ) );
		outBuf.put( '\n' );
	}

	SetSerializationDelimiter( nullptr );
	SetSerializationArrayDelimiter( nullptr );

	return true;
}


//-----------------------------------------------------------------------------
// Eats whitespaces and c++ style comments
//-----------------------------------------------------------------------------

void CDmxSerializerKeyValues2::EatWhitespacesAndComments( ks::text_reader &buf )
{
	const std::string_view text = buf.rest();
	size_t nOffset = 0;
	while ( nOffset < text.size() )
	{
		// Eat whitespaces, keep track of line count
		while ( nOffset < text.size() && V_isspace( text[nOffset] ) )
		{
			if ( text[nOffset] == '\n' )
			{
				g_KeyValues2ErrorStack.IncrementCurrentLine();
			}
			++nOffset;
		}

		// If we don't have a a c++ style comment next, we're done
		if ( !text.substr( nOffset ).starts_with( "//" ) )
			break;

		// Deal with c++ style comments
		nOffset += 2;

		// read complete line
		while ( nOffset < text.size() && text[nOffset] != '\n' )
		{
			++nOffset;
		}

		g_KeyValues2ErrorStack.IncrementCurrentLine();
	}

	buf.seek( buf.tell() + nOffset );
}


//-----------------------------------------------------------------------------
// Reads a single token, points the token view at it
//-----------------------------------------------------------------------------
CDmxSerializerKeyValues2::TokenType_t CDmxSerializerKeyValues2::ReadToken( ks::text_reader &buf, std::string_view &token )
{
	EatWhitespacesAndComments( buf );

	if ( !buf.ok() || buf.at_end() )
		return TOKEN_EOF;

	// Compute token length and type
	size_t nLength = 0;
	TokenType_t t = TOKEN_INVALID;
	switch( buf.peek_char() )
	{
	case '{':
		nLength = 1;
		t = TOKEN_OPEN_BRACE;
		break;

	case '}':
		nLength = 1;
		t = TOKEN_CLOSE_BRACE;
		break;

	case '[':
		nLength = 1;
		t = TOKEN_OPEN_BRACKET;
		break;

	case ']':
		nLength = 1;
		t = TOKEN_CLOSE_BRACKET;
		break;

	case ',':
		nLength = 1;
		t = TOKEN_COMMA;
		break;

	case '\"':
		// NOTE: The -1 is because peek includes room for the /0
		nLength = buf.peek_quoted_length( ks::c_string_escapes, false ) - 1;
		if ( (nLength <= 1) || ( buf.peek_char( nLength - 1 ) != '\"' ))
		{
			g_KeyValues2ErrorStack.ReportError( "Unexpected EOF in quoted string" );
			t = TOKEN_INVALID;
		}
		else
		{
			t = TOKEN_DELIMITED_STRING;
		}
		break;

	default:
		t = TOKEN_INVALID;
		break;
	}

	// Point the token at the token + update the original buffer get index
	nLength = std::min( nLength, buf.remaining() );
	token = buf.rest().substr( 0, nLength );
	buf.seek( buf.tell() + nLength );

	// Count the number of crs in the token + update the current line
	for ( const char c : token )
	{
		if ( c == '\n' )
		{
			g_KeyValues2ErrorStack.IncrementCurrentLine();
		}
	}

	return t;
}


//-----------------------------------------------------------------------------
// Strips the delimiters from a quoted token and resolves its escapes
//-----------------------------------------------------------------------------
static std::string Unquote( std::string_view token )
{
	ks::text_reader reader( token );
	return reader.read_quoted( ks::c_string_escapes );
}


//-----------------------------------------------------------------------------
// Creates a scene object, adds it to the element dictionary
//-----------------------------------------------------------------------------
DmxElementDictHandle_t CDmxSerializerKeyValues2::CreateDmxElement( const char *pElementType )
{
	// See if we can create an element of that type
	CDmxElement *pElement = new CDmxElement( pElementType );
	return m_ElementDict.InsertElement( pElement );
}


//-----------------------------------------------------------------------------
// Reads an attribute for an element
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeElementAttribute( ks::text_reader &buf, DmxElementDictHandle_t hElement, const char *pAttributeName, const char *pElementType )
{
	CDmxElement *pElement = m_ElementDict.GetElement( hElement );
	if ( pElement->HasAttribute( pAttributeName ) )
	{
		g_KeyValues2ErrorStack.ReportError( "Attribute \"%s\" was defined more than once.\n", pAttributeName );
		return false;
	}
	
	CDmxAttribute *pAttribute;
	{
		CDmxElementModifyScope modify( pElement );
		pAttribute = pElement->AddAttribute( pAttributeName );
	}

	DmxElementDictHandle_t h;
	bool bOk = UnserializeElement( buf, pElementType, &h );
	if ( bOk )
	{
		CDmxElement *pNewElement = m_ElementDict.GetElement( h );
		pAttribute->SetValue( pNewElement );
	}
	return bOk;
}


//-----------------------------------------------------------------------------
// Reads an attribute for an element array
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeElementArrayAttribute( ks::text_reader &buf, DmxElementDictHandle_t hElement, const char *pAttributeName )
{
	CDmxElement *pElement = m_ElementDict.GetElement( hElement );
	if ( pElement->HasAttribute( pAttributeName ) )
	{
		g_KeyValues2ErrorStack.ReportError( "Attribute \"%s\" was defined more than once.\n", pAttributeName );
		return false;
	}

	CDmxAttribute *pAttribute;
	{
		CDmxElementModifyScope modify( pElement );
		pAttribute = pElement->AddAttribute( pAttributeName );

		// NOTE: This allocates an empty array and sets the attribute type appropriately
		// for use when there's an empty array
		pAttribute->GetArrayForEdit<CDmxElement*>();
	}

	// Arrays first must have a '[' specified
	TokenType_t token;
	std::string_view tokenBuf;
	token = ReadToken( buf, tokenBuf );
	if ( token != TOKEN_OPEN_BRACKET )
	{
		g_KeyValues2ErrorStack.ReportError( "Expecting '[', didn't find it!" );
		return false;
	}

	int nElementIndex = 0;

	// Now read a list of array values, separated by commas
	while ( buf.ok() )
	{
		token = ReadToken( buf, tokenBuf );
		if ( token == TOKEN_INVALID || token == TOKEN_EOF )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting ']', didn't find it!" );
			return false;
		}

		// Then, keep reading until we hit a ']'
		if ( token == TOKEN_CLOSE_BRACKET )
			break;

		// If we've already read in an array value, we need to read a comma next
		if ( nElementIndex > 0 )
		{
			if ( token != TOKEN_COMMA )
			{
				g_KeyValues2ErrorStack.ReportError( "Expecting ',', didn't find it!" );
				return false;
			}

			// Read in the next thing, which should be a value
			token = ReadToken( buf, tokenBuf );
		}

		// Ok, we must be reading an array type value
		if ( token != TOKEN_DELIMITED_STRING )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting element type, didn't find it!" );
			return false;
		}

		// Get the element type out
		std::string elementType = Unquote( tokenBuf );

		// Use the element type to figure out if we're using a element reference or an inlined element
		if ( elementType == CDmxAttribute::s_pAttributeTypeName[AT_ELEMENT] )
		{
			token = ReadToken( buf, tokenBuf );

			// Ok, we must be reading an array type value
			if ( token != TOKEN_DELIMITED_STRING )
			{
				g_KeyValues2ErrorStack.ReportError( "Expecting element reference, didn't find it!" );
				return false;
			}

			// Get the element type out
			std::string elementId = Unquote( tokenBuf );

			DmObjectId_t id;
			if ( !UniqueIdFromString( &id, elementId.c_str() ) )
			{
				g_KeyValues2ErrorStack.ReportError( "Encountered invalid element ID data!" );
				return false;
			}

			Assert( IsUniqueIdValid( id ) );
			m_ElementDict.AddArrayAttribute( pAttribute, id );
		}
		else
		{
			DmxElementDictHandle_t hArrayElement;
			bool bOk = UnserializeElement( buf, elementType.c_str(), &hArrayElement );
			if ( !bOk )
				return false;
			m_ElementDict.AddArrayAttribute( pAttribute, hArrayElement );
		}

		// Ok, we've read in another value
		++nElementIndex;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Unserializes an attribute from a token buffer
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeAttributeValueFromToken( CDmxAttribute *pAttribute, DmAttributeType_t type, std::string_view token )
{
	// NOTE: This code is necessary because the attribute code is using Scanf
	// which is not really friendly toward delimiters, so we must pass in
	// non-delimited buffers. Sucky. There must be a better way of doing this
	std::string unquoted;
	std::string_view text = token;

	bool bIsString = ( type == AT_STRING ) || ( type == AT_STRING_ARRAY );
	if ( !bIsString )
	{
		unquoted = Unquote( token );
		text = unquoted;
	}
	else
	{
		SetSerializationDelimiter( &ks::c_string_escapes );
	}

	bool bOk;
	ks::text_reader buf( text );
	if ( type < AT_FIRST_ARRAY_TYPE )
	{
		bOk = pAttribute->Unserialize( type, buf );
	}
	else
	{
		bOk = pAttribute->UnserializeElement( type, buf );
	}

	if ( bIsString )
	{
		SetSerializationDelimiter( nullptr );
	}

	return bOk;
}


//-----------------------------------------------------------------------------
// Reads an attribute for an element array
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeArrayAttribute( ks::text_reader &buf, DmxElementDictHandle_t hElement, const char *pAttributeName, DmAttributeType_t nAttrType )
{
	CDmxElement *pElement = m_ElementDict.GetElement( hElement );
	if ( pElement->HasAttribute( pAttributeName ) )
	{
		g_KeyValues2ErrorStack.ReportError( "Encountered duplicate attribute definition for attribute \"%s\"!", pAttributeName );
		return false;
	}

	CDmxAttribute *pAttribute;
	{
		CDmxElementModifyScope modify( pElement );
		pAttribute = pElement->AddAttribute( pAttributeName );
		}

	// Arrays first must have a '[' specified
	TokenType_t token;
	std::string_view tokenBuf;
	token = ReadToken( buf, tokenBuf );
	if ( token != TOKEN_OPEN_BRACKET )
	{
		g_KeyValues2ErrorStack.ReportError( "Expecting '[', didn't find it!" );
		return false;
	}

	int nElementIndex = 0;

	// Now read a list of array values, separated by commas
	while ( buf.ok() )
	{
		token = ReadToken( buf, tokenBuf );
		if ( token == TOKEN_INVALID || token == TOKEN_EOF )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting ']', didn't find it!" );
			return false;
		}

		// Then, keep reading until we hit a ']'
		if ( token == TOKEN_CLOSE_BRACKET )
			break;

		// If we've already read in an array value, we need to read a comma next
		if ( nElementIndex > 0 )
		{
			if ( token != TOKEN_COMMA )
			{
				g_KeyValues2ErrorStack.ReportError( "Expecting ',', didn't find it!" );
				return false;
			}

			// Read in the next thing, which should be a value
			token = ReadToken( buf, tokenBuf );
		}

		// Ok, we must be reading an attributearray value
		if ( token != TOKEN_DELIMITED_STRING )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting array attribute value, didn't find it!" );
			return false;
		}

		if ( !UnserializeAttributeValueFromToken( pAttribute, nAttrType, tokenBuf ) )
		{
			g_KeyValues2ErrorStack.ReportError("Error reading in array attribute \"%s\" element %d", pAttributeName, nElementIndex );
			return false;
		}

		// Ok, we've read in another value
		++nElementIndex;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Reads an attribute for an element
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeAttribute( ks::text_reader &buf, 
	DmxElementDictHandle_t hElement, const char *pAttributeName, DmAttributeType_t nAttrType )
{
	// Read the attribute value
	std::string_view tokenBuf;
	TokenType_t token = ReadToken( buf, tokenBuf );
	if ( token != TOKEN_DELIMITED_STRING )
	{
		g_KeyValues2ErrorStack.ReportError( "Expecting quoted attribute value for attribute \"%s\", didn't find one!", pAttributeName );
		return false;
	}

	CDmxElement *pElement = m_ElementDict.GetElement( hElement );
	if ( pElement->HasAttribute( pAttributeName ) )
	{
		g_KeyValues2ErrorStack.ReportError( "Encountered duplicate attribute definition for attribute \"%s\"!", pAttributeName );
		return false;
	}

	CDmxAttribute *pAttribute;
	{
		CDmxElementModifyScope modify( pElement );
		pAttribute = pElement->AddAttribute( pAttributeName );
	}

	switch( nAttrType )
	{
	case AT_ELEMENT:
		{
			// Get the attribute value out
			std::string attributeValue = Unquote( tokenBuf );

			// No string? that's ok, it means we have a NULL pointer
			if ( attributeValue.empty() )
				return true;

			DmObjectId_t id;
			if ( !UniqueIdFromString( &id, attributeValue.c_str() ) )
			{
				g_KeyValues2ErrorStack.ReportError("Invalid format for element ID encountered for attribute \"%s\"", pAttributeName );
				return false;
			}

			m_ElementDict.AddAttribute( pAttribute, id );
		}
		return true;

	default:
		if ( UnserializeAttributeValueFromToken( pAttribute, nAttrType, tokenBuf ) )
			return true;

		g_KeyValues2ErrorStack.ReportError("Error reading attribute \"%s\"", pAttributeName );
		return false;
	}
}

bool CDmxSerializerKeyValues2::UnserializeId( ks::text_reader &buf, DmxElementDictHandle_t hElement )
{
	std::string_view tokenBuf;
	TokenType_t token = ReadToken( buf, tokenBuf );
	if ( token != TOKEN_DELIMITED_STRING )
	{
		g_KeyValues2ErrorStack.ReportError( "Expecting quoted value for element ID, didn't find one!" );
		return false;
	}

	std::string elementId = Unquote( tokenBuf );

	DmObjectId_t id;
	if ( !UniqueIdFromString( &id, elementId.c_str() ) )
	{
		g_KeyValues2ErrorStack.ReportError( "Encountered invalid element ID data!" );
		return false;
	}

	CDmxElement *pElement = m_ElementDict.GetElement( hElement );
	m_ElementDict.SetElementId( hElement, id );
	pElement->SetId( id );
	return true;
}


//-----------------------------------------------------------------------------
// Unserializes a single element given the type name
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeElement( ks::text_reader &buf, const char *pElementType, DmxElementDictHandle_t *pHandle )
{
	*pHandle = ELEMENT_DICT_HANDLE_INVALID;

	// Create the element
	DmxElementDictHandle_t hElement = CreateDmxElement( pElementType );

	// Report errors relative to this type name
	CKeyValues2ErrorContext errorReport( pElementType );

	TokenType_t token;
	std::string_view tokenBuf;

	// Then we expect a '{'
	token = ReadToken( buf, tokenBuf );
	if ( token != TOKEN_OPEN_BRACE )
	{
		g_KeyValues2ErrorStack.ReportError( "Expecting '{', didn't find it!" );
		return false;
	}

	while ( buf.ok() )
	{
		token = ReadToken( buf, tokenBuf );
		if ( token == TOKEN_INVALID || token == TOKEN_EOF )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting '}', didn't find it!" );
			return false;
		}

		// Then, keep reading until we hit a '}'
		if ( token == TOKEN_CLOSE_BRACE )
			break;

		// Ok, we must be reading an attribute
		if ( token != TOKEN_DELIMITED_STRING )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting attribute name, didn't find it!" );
			return false;
		}

		// First, read an attribute name
		const std::string attributeNameStr = Unquote( tokenBuf );
		const char *attributeName = attributeNameStr.c_str();

		// Next, read an attribute type
		token = ReadToken( buf, tokenBuf );
		if ( token != TOKEN_DELIMITED_STRING )
		{
			g_KeyValues2ErrorStack.ReportError( "Expecting attribute type for attribute %s, didn't find it!", attributeName );
			return false;
		}

		const std::string attributeTypeStr = Unquote( tokenBuf );
		const char *attributeType = attributeTypeStr.c_str();

		if ( !Q_stricmp( "elementid", attributeType ) )
		{
			if ( Q_stricmp( "id", attributeName ) != 0 )
				return false; // elementid is no longer a valid attribute type - only the id should be of this type
			if ( !UnserializeId( buf, hElement ) )
				return false;
			continue;
		}

		DmAttributeType_t nAttrType = AT_UNKNOWN;
		for ( int i = 0; i < AT_TYPE_COUNT; ++i )
		{
			if ( !Q_stricmp( CDmxAttribute::s_pAttributeTypeName[i], attributeType ) )
			{
				nAttrType = (DmAttributeType_t)i;
				break;
			}
		}

		// Next, read an attribute value
		bool bOk = true;
		switch( nAttrType )
		{
		case AT_UNKNOWN:
			bOk = UnserializeElementAttribute( buf, hElement, attributeName, attributeType );
			break;

		case AT_ELEMENT_ARRAY:
			bOk = UnserializeElementArrayAttribute( buf, hElement, attributeName );
			break;

		default:
			if ( nAttrType >= AT_FIRST_ARRAY_TYPE )
			{
				bOk = UnserializeArrayAttribute( buf, hElement, attributeName, nAttrType );
			}
			else
			{
				bOk = UnserializeAttribute( buf, hElement, attributeName, nAttrType );
			}
			break;
		}

		if ( !bOk )
			return false;
	}

	*pHandle = hElement;
	return true;
}

	
//-----------------------------------------------------------------------------
// Unserializes a single element
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::UnserializeElement( ks::text_reader &buf, DmxElementDictHandle_t *pHandle )
{
	*pHandle = ELEMENT_DICT_HANDLE_INVALID;

	// First, read the type name
	std::string_view tokenBuf;

	TokenType_t token = ReadToken( buf, tokenBuf );
	if ( token == TOKEN_INVALID )
		return false;
	
	if ( token == TOKEN_EOF )
		return true;

	// Get the type name out
	if ( token != TOKEN_DELIMITED_STRING )
	{
		g_KeyValues2ErrorStack.ReportError( "Expecting element type name, didn't find it!" );
		return false;
	}

	const std::string typeName = Unquote( tokenBuf );
	return UnserializeElement( buf, typeName.c_str(), pHandle );
}

			   
//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
bool CDmxSerializerKeyValues2::Unserialize( const char *pFileName, ks::text_reader &buf, CDmxElement **ppRoot )
{
	*ppRoot = nullptr;

	g_KeyValues2ErrorStack.SetFilename( pFileName );
	m_hRoot = ELEMENT_DICT_HANDLE_INVALID;
	m_ElementDict.Clear();

	bool bOk = true;
	while ( buf.ok() )
	{
		DmxElementDictHandle_t h;
		bOk = UnserializeElement( buf, &h );
		if ( !bOk || ( h == ELEMENT_DICT_HANDLE_INVALID ) )
			break;

		if ( m_hRoot == ELEMENT_DICT_HANDLE_INVALID )
		{
			m_hRoot = h;
		}
	}

	// do this *before* getting the root, since the first element might be deleted due to id conflicts
	m_ElementDict.HookUpElementReferences();

	*ppRoot = m_ElementDict.GetElement( m_hRoot );
	m_ElementDict.Clear();

	if ( !bOk )
	{
		CleanupDMX( *ppRoot );
		*ppRoot = nullptr;
	}

	return bOk;
}


//-----------------------------------------------------------------------------
// Unserialization entry point for text files (assumes version has been stripped)
//-----------------------------------------------------------------------------
bool UnserializeTextDMX( const char *pFileName, ks::text_reader &buf, CDmxElement **ppRoot )
{
	CDmxSerializerKeyValues2 dmxUnserializer;
	return dmxUnserializer.Unserialize( pFileName, buf, ppRoot );
}


bool SerializeTextDMX( const char *pFileName, ks::text_writer &buf, CDmxElement *pRoot )
{
	CDmxSerializerKeyValues2 dmxSerializer;
	return dmxSerializer.Serialize( buf, pRoot, pFileName );
}

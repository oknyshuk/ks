// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module;

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <jpeglib.h>

module ks.jpeg;

namespace
{

thread_local char g_error[JMSG_LENGTH_MAX];

struct error_manager
{
	jpeg_error_mgr base;
	jmp_buf recover;
};

void on_error( j_common_ptr info )
{
	auto *manager = reinterpret_cast<error_manager *>( info->err );
	( *info->err->format_message )( info, g_error );
	longjmp( manager->recover, 1 );
}

struct memory_source
{
	jpeg_source_mgr base;
};

void source_noop( j_decompress_ptr ) {}

boolean source_exhausted( j_decompress_ptr info )
{
	// The whole stream is handed over up front, so running dry means the data is truncated.
	static const JOCTET eoi[2] = { 0xFF, JPEG_EOI };
	info->src->next_input_byte = eoi;
	info->src->bytes_in_buffer = 2;
	return TRUE;
}

void source_skip( j_decompress_ptr info, long count )
{
	if ( count <= 0 )
		return;
	if ( static_cast<size_t>( count ) > info->src->bytes_in_buffer )
		count = static_cast<long>( info->src->bytes_in_buffer );
	info->src->next_input_byte += count;
	info->src->bytes_in_buffer -= count;
}

struct destination
{
	jpeg_destination_mgr base;
	ks::jpeg::sink emit;
	void *context;
	JOCTET buffer[4096];
};

void destination_init( j_compress_ptr info )
{
	auto *dest = reinterpret_cast<destination *>( info->dest );
	dest->base.next_output_byte = dest->buffer;
	dest->base.free_in_buffer = sizeof( dest->buffer );
}

boolean destination_flush( j_compress_ptr info )
{
	auto *dest = reinterpret_cast<destination *>( info->dest );
	dest->emit( dest->buffer, sizeof( dest->buffer ), dest->context );
	dest->base.next_output_byte = dest->buffer;
	dest->base.free_in_buffer = sizeof( dest->buffer );
	return TRUE;
}

void destination_finish( j_compress_ptr info )
{
	auto *dest = reinterpret_cast<destination *>( info->dest );
	const size_t pending = sizeof( dest->buffer ) - dest->base.free_in_buffer;
	if ( pending > 0 )
		dest->emit( dest->buffer, pending, dest->context );
}

} // namespace

namespace
{

struct reader
{
	jpeg_decompress_struct info;
	error_manager errors;
	memory_source source;
};

// Returns false (and leaves `r` destroyed) when the stream cannot be opened.
bool open( reader &r, const void *data, unsigned long size )
{
	memset( &r.info, 0, sizeof( r.info ) );
	r.info.err = jpeg_std_error( &r.errors.base );
	r.errors.base.error_exit = on_error;
	jpeg_create_decompress( &r.info );

	if ( setjmp( r.errors.recover ) )
	{
		jpeg_destroy_decompress( &r.info );
		return false;
	}

	memset( &r.source, 0, sizeof( r.source ) );
	r.source.base.init_source = source_noop;
	r.source.base.fill_input_buffer = source_exhausted;
	r.source.base.skip_input_data = source_skip;
	r.source.base.resync_to_restart = jpeg_resync_to_restart;
	r.source.base.term_source = source_noop;
	r.source.base.next_input_byte = static_cast<const JOCTET *>( data );
	r.source.base.bytes_in_buffer = size;
	r.info.src = &r.source.base;

	if ( jpeg_read_header( &r.info, TRUE ) != JPEG_HEADER_OK )
	{
		jpeg_destroy_decompress( &r.info );
		return false;
	}
	return true;
}

} // namespace

bool ks::jpeg::read_header( const void *data, unsigned long size, int *width, int *height ) noexcept
{
	g_error[0] = 0;
	if ( data == nullptr || size == 0 || width == nullptr || height == nullptr )
	{
		snprintf( g_error, sizeof( g_error ), "invalid arguments" );
		return false;
	}

	reader r;
	if ( !open( r, data, size ) )
		return false;

	*width = static_cast<int>( r.info.image_width );
	*height = static_cast<int>( r.info.image_height );
	jpeg_destroy_decompress( &r.info );
	return true;
}

bool ks::jpeg::decode( const void *data, unsigned long size, int channels, unsigned char *pixels, unsigned long capacity ) noexcept
{
	g_error[0] = 0;
	if ( data == nullptr || size == 0 || pixels == nullptr || ( channels != 3 && channels != 4 ) )
	{
		snprintf( g_error, sizeof( g_error ), "invalid arguments" );
		return false;
	}

	reader r;
	if ( !open( r, data, size ) )
		return false;

	if ( setjmp( r.errors.recover ) )
	{
		jpeg_destroy_decompress( &r.info );
		return false;
	}

	r.info.out_color_space = JCS_RGB;
	jpeg_start_decompress( &r.info );

	const unsigned long stride = static_cast<unsigned long>( r.info.output_width ) * channels;
	if ( r.info.output_components != 3 || stride * r.info.output_height > capacity )
	{
		snprintf( g_error, sizeof( g_error ), r.info.output_components != 3 ? "unsupported component count %d" : "buffer too small (%d)", r.info.output_components );
		jpeg_destroy_decompress( &r.info );
		return false;
	}

	while ( r.info.output_scanline < r.info.output_height )
	{
		unsigned char *row = pixels + static_cast<unsigned long>( r.info.output_scanline ) * stride;
		JSAMPROW rows[1] = { row };
		if ( jpeg_read_scanlines( &r.info, rows, 1 ) != 1 )
		{
			snprintf( g_error, sizeof( g_error ), "truncated image" );
			jpeg_destroy_decompress( &r.info );
			return false;
		}

		if ( channels == 4 )
		{
			for ( int x = static_cast<int>( r.info.output_width ) - 1; x >= 0; --x )
			{
				row[x * 4 + 3] = 0xFF;
				row[x * 4 + 2] = row[x * 3 + 2];
				row[x * 4 + 1] = row[x * 3 + 1];
				row[x * 4] = row[x * 3];
			}
		}
	}

	jpeg_finish_decompress( &r.info );
	jpeg_destroy_decompress( &r.info );
	return true;
}

bool ks::jpeg::encode( const unsigned char *rgb, int width, int height, int quality, sink emit, void *context ) noexcept
{
	g_error[0] = 0;
	if ( rgb == nullptr || width <= 0 || height <= 0 || emit == nullptr )
	{
		snprintf( g_error, sizeof( g_error ), "invalid arguments" );
		return false;
	}
	if ( quality < 1 )
		quality = 1;
	if ( quality > 100 )
		quality = 100;

	jpeg_compress_struct info;
	error_manager errors;
	memset( &info, 0, sizeof( info ) );
	info.err = jpeg_std_error( &errors.base );
	errors.base.error_exit = on_error;
	jpeg_create_compress( &info );

	if ( setjmp( errors.recover ) )
	{
		jpeg_destroy_compress( &info );
		return false;
	}

	destination dest{};
	dest.base.init_destination = destination_init;
	dest.base.empty_output_buffer = destination_flush;
	dest.base.term_destination = destination_finish;
	dest.emit = emit;
	dest.context = context;
	info.dest = &dest.base;

	info.image_width = static_cast<JDIMENSION>( width );
	info.image_height = static_cast<JDIMENSION>( height );
	info.input_components = 3;
	info.in_color_space = JCS_RGB;
	jpeg_set_defaults( &info );
	jpeg_set_quality( &info, quality, TRUE );
	jpeg_start_compress( &info, TRUE );

	const unsigned long stride = static_cast<unsigned long>( width ) * 3;
	while ( info.next_scanline < info.image_height )
	{
		JSAMPROW rows[1] = { const_cast<JSAMPROW>( rgb + static_cast<unsigned long>( info.next_scanline ) * stride ) };
		jpeg_write_scanlines( &info, rows, 1 );
	}

	jpeg_finish_compress( &info );
	jpeg_destroy_compress( &info );
	return true;
}

const char *ks::jpeg::last_error() noexcept
{
	return g_error;
}

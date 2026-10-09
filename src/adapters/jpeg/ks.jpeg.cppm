// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.jpeg;

export namespace ks::jpeg
{

using sink = void ( * )( const void *bytes, unsigned long size, void *context );

bool read_header( const void *data, unsigned long size, int *width, int *height ) noexcept;

// Fills `pixels` (width * height * channels bytes, as reported by read_header). `channels` is
// 3 (RGB) or 4 (RGBA with opaque alpha).
bool decode( const void *data, unsigned long size, int channels, unsigned char *pixels, unsigned long capacity ) noexcept;

// Encodes tightly packed RGB8 scanlines, streaming the compressed bytes through `emit`.
bool encode( const unsigned char *rgb, int width, int height, int quality, sink emit, void *context ) noexcept;

const char *last_error() noexcept;

} // namespace ks::jpeg

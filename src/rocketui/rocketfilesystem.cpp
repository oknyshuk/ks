#include "rocketfilesystem.h"

#include "filesystem.h"
#include "interfaces/interfaces.h"

#include "bitmap/imageformat.h"
#include "tier1/utlbuffer.h"
#include "vtf/vtf.h"

RocketFileSystem RocketFileSystem::m_Instance;

RocketFileSystem::RocketFileSystem() {}

bool RocketLoadVTF_RGBA(const char *gamePath, std::vector<unsigned char> &outRGBA,
                        int &outWidth, int &outHeight) {
  outRGBA.clear();
  outWidth = outHeight = 0;

  FileHandle_t f = g_pFullFileSystem->Open(gamePath, "rb", "GAME");
  if (!f)
    return false;

  const int len = g_pFullFileSystem->Size(f);
  if (len <= 0) {
    g_pFullFileSystem->Close(f);
    return false;
  }
  std::vector<unsigned char> raw((size_t)len);
  const int read = g_pFullFileSystem->Read(raw.data(), len, f);
  g_pFullFileSystem->Close(f);
  if (read != len)
    return false;

  CUtlBuffer buf(raw.data(), len, CUtlBuffer::READ_ONLY);
  IVTFTexture *tex = CreateVTFTexture();
  if (!tex)
    return false;

  bool ok = tex->Unserialize(buf);
  if (ok) {
    // Decode to straight RGBA8888 (matches GenerateTexture's expected input).
    tex->ConvertImageFormat(IMAGE_FORMAT_RGBA8888, false);
    outWidth = tex->Width();
    outHeight = tex->Height();
    const unsigned char *pixels = tex->ImageData(0, 0, 0); // frame 0, face 0, mip 0
    if (pixels && outWidth > 0 && outHeight > 0)
      outRGBA.assign(pixels, pixels + (size_t)outWidth * (size_t)outHeight * 4);
    else
      ok = false;

    // The renderer blends premultiplied alpha (as does the TGA path before
    // GenerateTexture), so premultiply here too. No-op for opaque/black sprites.
    for (size_t i = 0; i + 3 < outRGBA.size(); i += 4) {
      const unsigned a = outRGBA[i + 3];
      outRGBA[i + 0] = (unsigned char)((outRGBA[i + 0] * a) / 255);
      outRGBA[i + 1] = (unsigned char)((outRGBA[i + 1] * a) / 255);
      outRGBA[i + 2] = (unsigned char)((outRGBA[i + 2] * a) / 255);
    }
  }

  DestroyVTFTexture(tex);
  return ok && !outRGBA.empty();
}

Rml::FileHandle RocketFileSystem::Open(const Rml::String &path) {
  Rml::String rocketPath = "rocketui/";
  rocketPath += path;
  return (Rml::FileHandle)g_pFullFileSystem->Open(rocketPath.c_str(), "r",
                                                  "GAME");
}

void RocketFileSystem::Close(Rml::FileHandle file) {
  g_pFullFileSystem->Close((FileHandle_t)file);
}

size_t RocketFileSystem::Read(void *buffer, size_t size, Rml::FileHandle file) {
  return g_pFullFileSystem->Read(buffer, size, (FileHandle_t)file);
}

bool RocketFileSystem::Seek(Rml::FileHandle file, long offset, int origin) {
  g_pFullFileSystem->Seek((FileHandle_t)file, offset, (FileSystemSeek_t)origin);
  return true;
}

size_t RocketFileSystem::Tell(Rml::FileHandle file) {
  return g_pFullFileSystem->Tell((FileHandle_t)file);
}

size_t RocketFileSystem::Length(Rml::FileHandle file) {
  return g_pFullFileSystem->Size((FileHandle_t)file);
}

#ifndef ROCKETFILESYSTEM_H
#define ROCKETFILESYSTEM_H

#include <RmlUi/Core/FileInterface.h>
#include <vector>

// Decode a Valve .vtf texture (from the game filesystem, incl. VPKs) to RGBA8888.
// Lets RmlUi reuse the original engine sprites directly (see LoadTexture).
// gamePath is relative to a GAME search path, e.g. "materials/sprites/scope_arc.vtf".
// Returns false if the file is missing or not a valid VTF.
bool RocketLoadVTF_RGBA(const char *gamePath, std::vector<unsigned char> &outRGBA,
                        int &outWidth, int &outHeight);

class RocketFileSystem : public Rml::FileInterface {
public:
  static RocketFileSystem m_Instance;

  RocketFileSystem();

  // Opens a file
  Rml::FileHandle Open(const Rml::String &path) override;

  // Closes a previously opened file
  void Close(Rml::FileHandle file) override;

  // Reads data from a previously opened file
  size_t Read(void *buffer, size_t size, Rml::FileHandle file) override;

  // Seeks to a point in a previously opened file
  bool Seek(Rml::FileHandle file, long offset, int origin) override;

  // Returns the current position of the file pointer
  size_t Tell(Rml::FileHandle file) override;

  // Returns the length of the file
  size_t Length(Rml::FileHandle file) override;
};

#endif // ROCKETFILESYSTEM_H

#ifndef ROCKETFILESYSTEM_H
#define ROCKETFILESYSTEM_H

#include <RmlUi/Core/FileInterface.h>

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

#pragma once
#include <filesystem>
#include <initializer_list>
#include <map>
#include <raylib.h>

// Construct after InitWindow; destroy before CloseWindow.
class UiFonts
{
  std::map<int, Font> fonts;
  void unload();

public:
  UiFonts(const std::filesystem::path& file, std::initializer_list<int> sizes);
  ~UiFonts();
  UiFonts(const UiFonts&) = delete;
  UiFonts& operator=(const UiFonts&) = delete;
  UiFonts(UiFonts&&) = delete;
  UiFonts& operator=(UiFonts&&) = delete;
  Font get(int size) const;
};

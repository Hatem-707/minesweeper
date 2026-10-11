#include "ui_fonts.hpp"
#include <array>
#include <stdexcept>

UiFonts::UiFonts(const std::filesystem::path& file,
                 std::initializer_list<int> sizes)
{
  if (!std::filesystem::is_regular_file(file)) {
    throw std::runtime_error("UI font file is missing: " + file.string());
  }
  std::array<int, 96> codepoints{};
  for (int index = 0; index < 95; ++index) {
    codepoints[index] = 32 + index;
  }
  codepoints.back() = 176; // Degree sign for orientation readings.
  try {
    for (int size : sizes) {
      if (size <= 0) {
        throw std::invalid_argument("UI font size must be positive");
      }
      auto [entry, inserted] = fonts.try_emplace(size, Font{});
      if (!inserted) {
        continue;
      }
      const Font loaded = LoadFontEx(file.string().c_str(),
                                     size,
                                     codepoints.data(),
                                     static_cast<int>(codepoints.size()));
      // Raylib can silently return its shared default font on load failure.
      if (loaded.texture.id == GetFontDefault().texture.id) {
        throw std::runtime_error("Unable to load UI font: " + file.string());
      }
      entry->second = loaded;
      if (!IsFontValid(loaded) || loaded.texture.id == 0) {
        throw std::runtime_error("Unable to load UI font: " + file.string());
      }
      SetTextureFilter(loaded.texture, TEXTURE_FILTER_BILINEAR);
    }
  } catch (...) {
    unload();
    throw;
  }
}

UiFonts::~UiFonts()
{
  unload();
}

void
UiFonts::unload()
{
  for (const auto& [size, font] : fonts) {
    if (font.texture.id != 0 || font.glyphs || font.recs) {
      UnloadFont(font);
    }
  }
  fonts.clear();
}

Font
UiFonts::get(int size) const
{
  return fonts.at(size);
}

#include "ui.hpp"
#include "ui_store.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>

namespace {
class ConfigFile
{
public:
  std::filesystem::path path =
    std::filesystem::temp_directory_path() /
    ("minesweeper-ui-" +
     std::to_string(
       std::chrono::steady_clock::now().time_since_epoch().count()) +
     ".toml");

  explicit ConfigFile(std::string_view contents) { write(contents); }
  ~ConfigFile()
  {
    std::error_code error;
    std::filesystem::remove(path, error);
  }
  void write(std::string_view contents) const
  {
    std::ofstream output(path);
    output << contents;
    if (!output) {
      throw std::runtime_error("Unable to write test UI configuration");
    }
  }
};

TEST(UiStoreTest, ShippedConfigurationLoadsWithoutGraphics)
{
  const UiStore ui(TEST_UI_CONFIG_FILE);
  EXPECT_EQ(ui.screen_width, 1280);
  EXPECT_EQ(ui.screen_height, 800);
  EXPECT_EQ(ui.target_fps, 60);
  EXPECT_EQ(ui.camera_resolution, 720);
  EXPECT_EQ(ui.section_padding, 20);
  EXPECT_FLOAT_EQ(ui.hover_scale, 1.2f);
}

TEST(UiStoreTest, MissingOptionalParametersKeepDefaults)
{
  ConfigFile config("");
  const UiStore ui(config.path);
  EXPECT_EQ(ui.screen_width, 1280);
  EXPECT_EQ(ui.title_font, 24);
  EXPECT_EQ(ui.background.r, 253);
  EXPECT_FLOAT_EQ(ui.controls.bounds.height, 0.45f);
}

TEST(UiStoreTest, LoadsTypedLayoutStyleColorsAndSpringParameters)
{
  ConfigFile config(R"(
[Screen]
width = 1600
height = 1000
fps = 30
[Camera]
resolution = 480
[Style]
app_padding = 10
section_padding = 12
cell_padding = 1
text_padding = 6
title_font = 28
element_font = 18
roundness = 0.1
[Map]
x = 0.4
y = 0
width = 0.6
height = 0.8
color = [10, 20, 30, 40]
[Controls]
table_fraction = 0.6
[Colors]
text = [12, 34, 56, 78]
unchecked = [21, 43, 65]
buried_mine = [100, 101, 102]
[Hover]
scale = 1.4
frequency = 10
damping_ratio = 0.8
)");
  const UiStore ui(config.path);
  EXPECT_EQ(ui.screen_width, 1600);
  EXPECT_EQ(ui.screen_height, 1000);
  EXPECT_EQ(ui.target_fps, 30);
  EXPECT_EQ(ui.camera_resolution, 480);
  EXPECT_EQ(ui.title_font, 28);
  EXPECT_EQ(ui.element_font, 18);
  EXPECT_FLOAT_EQ(ui.roundness, 0.1f);
  EXPECT_EQ(ui.map.color.a, 40);
  EXPECT_EQ(ui.text.r, 12);
  EXPECT_EQ(ui.text.a, 78);
  EXPECT_EQ(ui.unchecked.a, 255);
  EXPECT_FLOAT_EQ(ui.hover_scale, 1.4f);
  EXPECT_FLOAT_EQ(ui.spring_frequency, 10.0f);
  EXPECT_FLOAT_EQ(ui.spring_damping_ratio, 0.8f);

  StateHandler handler;
  Map map(ui, handler);
  const auto rec = map.get_rec();
  EXPECT_NEAR(rec.x, 642.0f, 0.001f);
  EXPECT_NEAR(rec.y, 10.0f, 0.001f);
  EXPECT_NEAR(rec.width, 948.0f, 0.001f);
  EXPECT_NEAR(rec.height, 784.0f, 0.001f);
  const auto last_cell = map.cell_at(19, 19).get_rec();
  EXPECT_NEAR(last_cell.x + last_cell.width, rec.x + rec.width - 12, 0.001f);
  EXPECT_NEAR(last_cell.y + last_cell.height, rec.y + rec.height - 12, 0.001f);

  CellState state;
  EXPECT_EQ(state.get_color(ui).r, 21);
  state.set_mine_state(MineState::buried_mine);
  EXPECT_EQ(state.get_color(ui).r, 100);
}

TEST(UiStoreTest, EditingFileOnlyAffectsNewStoreAndScene)
{
  ConfigFile config("[Screen]\nwidth = 1600\nheight = 1000\n");
  const UiStore original(config.path);
  StateHandler handler;
  Map original_map(original, handler);
  const auto old_rect = original_map.get_rec();
  config.write("[Screen]\nwidth = 2000\nheight = 1200\n");
  EXPECT_EQ(original.screen_width, 1600);
  EXPECT_FLOAT_EQ(original_map.get_rec().width, old_rect.width);
  const UiStore restarted(config.path);
  Map restarted_map(restarted, handler);
  EXPECT_EQ(restarted.screen_width, 2000);
  EXPECT_GT(restarted_map.get_rec().width, old_rect.width);
}

TEST(UiStoreTest, RelativeRectRespectsParentOffsetAndDimensions)
{
  const RelativeRect bounds{ 0.25f, 0.5f, 0.5f, 0.25f };
  const Rectangle rec = bounds.resolve({ 100, 50, 400, 200 });
  EXPECT_FLOAT_EQ(rec.x, 200);
  EXPECT_FLOAT_EQ(rec.y, 150);
  EXPECT_FLOAT_EQ(rec.width, 200);
  EXPECT_FLOAT_EQ(rec.height, 50);
}

TEST(UiStoreTest, MissingFileReportsError)
{
  ConfigFile config("");
  std::filesystem::remove(config.path);
  EXPECT_THROW(UiStore{ config.path }, std::exception);
}

class InvalidUiConfiguration : public testing::TestWithParam<const char*>
{};

TEST_P(InvalidUiConfiguration, RejectsInvalidParameters)
{
  ConfigFile config(GetParam());
  EXPECT_THROW(UiStore{ config.path }, std::exception);
}

INSTANTIATE_TEST_SUITE_P(
  Validation,
  InvalidUiConfiguration,
  testing::Values("invalid = [",
                  "Screen = 3\n",
                  "[Screen]\nwidth = 0\n",
                  "[Screen]\nwidth = 999999999999\n",
                  "[Style]\ntitle_font = 'large'\n",
                  "[Style]\napp_padding = 1000\n",
                  "[Style]\nsection_padding = 1000\n",
                  "[Style]\ncell_padding = 100\n",
                  "[Map]\nwidth = 0\n",
                  "[Map]\nx = 0.8\nwidth = 0.8\n",
                  "[Colors]\ntext = [256, 0, 0]\n",
                  "[Colors]\ntext = [1, 2]\n",
                  "[Hover]\nfrequency = nan\n",
                  "[Hover]\ndamping_ratio = 1.0\n",
                  "[Controls]\ntable_fraction = 0.99\n"));
}

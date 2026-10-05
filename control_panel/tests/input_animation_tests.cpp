#include "state.hpp"
#include "ui.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <unordered_set>

namespace {
struct InputFrame
{
  Vector2 mouse{ 0.0f, 0.0f };
  bool focused = true;
  bool cursor_on_screen = true;
  std::unordered_set<int> pressed;
  std::unordered_set<int> clicked;
};

InputFrame input;
std::vector<Rectangle> drawn_cells;

class InputTest : public testing::Test
{
protected:
  UiStore ui;

  void SetUp() override
  {
    input = {};
    drawn_cells.clear();
  }
};

TEST_F(InputTest, MouseDispatchesInitialMovementExitAndReentry)
{
  input.mouse = { 12.0f, 34.0f };
  StateHandler handler;
  int moves = 0;
  StateHandler::MousePosition last_position;
  auto motion = handler.register_mouse_movement([&](auto position) {
    ++moves;
    last_position = position;
  });

  handler.handle_input();
  EXPECT_EQ(moves, 1);
  EXPECT_EQ(last_position, (StateHandler::MousePosition{ 12, 34 }));
  handler.handle_input();
  EXPECT_EQ(moves, 1) << "Stationary mouse must not repeat motion callbacks";
  input.mouse.x += 1.0f;
  handler.handle_input();
  EXPECT_EQ(moves, 2);

  input.cursor_on_screen = false;
  handler.handle_input();
  EXPECT_EQ(moves, 3);
  EXPECT_FALSE(std::isfinite(last_position.first));
  EXPECT_FALSE(std::isfinite(last_position.second));
  input.cursor_on_screen = true;
  handler.handle_input();
  EXPECT_EQ(moves, 4);
  EXPECT_EQ(last_position, (StateHandler::MousePosition{ 13, 34 }));
}

TEST_F(InputTest, MouseClicksDispatchWithoutMovementForEachRegisteredButton)
{
  input.mouse = { 12, 34 };
  StateHandler handler;
  int left_clicks = 0;
  int right_clicks = 0;
  auto left = handler.register_mouse_click([&](auto position) {
    EXPECT_EQ(position, (StateHandler::MousePosition{ 12, 34 }));
    ++left_clicks;
  });
  auto right = handler.register_mouse_click([&](auto) { ++right_clicks; },
                                            MOUSE_BUTTON_RIGHT);
  handler.handle_input();
  input.clicked = { MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT };
  handler.handle_input();
  EXPECT_EQ(left_clicks, 1);
  EXPECT_EQ(right_clicks, 1);

  input.cursor_on_screen = false;
  handler.handle_input();
  EXPECT_EQ(left_clicks, 1);
  EXPECT_EQ(right_clicks, 1);
  input.cursor_on_screen = true;
  input.focused = false;
  handler.handle_input();
  EXPECT_EQ(left_clicks, 1);
  EXPECT_EQ(right_clicks, 1);
}

TEST_F(InputTest,
       NewMouseListenerInitializesUnderStationaryCursorAndClearsOnFocusLoss)
{
  StateHandler handler;
  handler.handle_input();
  int moves = 0;
  StateHandler::MousePosition last_position;
  auto motion = handler.register_mouse_movement([&](auto position) {
    ++moves;
    last_position = position;
  });
  handler.handle_input();
  EXPECT_EQ(moves, 1);
  EXPECT_EQ(last_position, (StateHandler::MousePosition{ 0, 0 }));
  input.focused = false;
  handler.handle_input();
  EXPECT_EQ(moves, 2);
  EXPECT_FALSE(std::isfinite(last_position.first));
}

TEST_F(InputTest, KeyPressDispatchesOnceToMultipleListeners)
{
  StateHandler handler;
  int presses = 0;
  int second_presses = 0;
  auto press = handler.register_key_press(KEY_F5, [&] { ++presses; });
  auto second_press =
    handler.register_key_press(KEY_F5, [&] { ++second_presses; });
  ASSERT_TRUE(press);

  input.pressed.insert(KEY_F5);
  handler.handle_input();
  EXPECT_EQ(presses, 1);
  EXPECT_EQ(second_presses, 1);
  input.pressed.clear();
  handler.handle_input();
  EXPECT_EQ(presses, 1);
  EXPECT_EQ(second_presses, 1);
  handler.handle_input();
  EXPECT_EQ(presses, 1);
  input.pressed.insert(KEY_F5);
  handler.handle_input();
  EXPECT_EQ(presses, 2);
  EXPECT_EQ(second_presses, 2);
}

TEST_F(InputTest, KeyPressIsIgnoredWhileUnfocused)
{
  StateHandler handler;
  int presses = 0;
  auto press = handler.register_key_press(KEY_UP, [&] { ++presses; });
  input.pressed.insert(KEY_UP);
  input.focused = false;
  handler.handle_input();
  EXPECT_EQ(presses, 0);
  input.pressed.clear();
  input.focused = true;
  handler.handle_input();
  EXPECT_EQ(presses, 0);
  input.pressed.insert(KEY_UP);
  handler.handle_input();
  EXPECT_EQ(presses, 1);
}

TEST_F(InputTest, InvalidRegistrationsReturnDisconnectedConnections)
{
  StateHandler handler;
  EXPECT_FALSE(handler.register_key_press(KEY_NULL, [] {}));
  EXPECT_FALSE(handler.register_key_press(-1, [] {}));
  EXPECT_FALSE(handler.register_key_press(KEY_KB_MENU + 1, [] {}));
  EXPECT_FALSE(handler.register_key_press(KEY_A, {}));
  EXPECT_FALSE(handler.register_mouse_movement({}));
  EXPECT_FALSE(handler.register_mouse_click([](auto) {}, -1));
  EXPECT_FALSE(
    handler.register_mouse_click([](auto) {}, MOUSE_BUTTON_BACK + 1));
}

TEST_F(InputTest, DisconnectingListenerDuringDispatchSkipsItsPendingCallback)
{
  StateHandler handler;
  int removed_calls = 0;
  InputConnection removed;
  auto remover =
    handler.register_mouse_movement([&](auto) { removed.disconnect(); });
  removed = handler.register_mouse_movement([&](auto) { ++removed_calls; });
  handler.handle_input();
  EXPECT_EQ(removed_calls, 0);
}

TEST_F(InputTest, RegistrationDuringDispatchWaitsUntilNextFrame)
{
  StateHandler handler;
  int added_calls = 0;
  InputConnection added;
  auto adder = handler.register_mouse_movement([&](auto) {
    if (!added) {
      added = handler.register_mouse_movement([&](auto) { ++added_calls; });
    }
  });
  handler.handle_input();
  EXPECT_EQ(added_calls, 0);
  handler.handle_input();
  EXPECT_EQ(added_calls, 1);
}

TEST_F(InputTest, ConnectionsUnregisterOnDestructionAndSupportSelfDisconnection)
{
  StateHandler handler;
  int temporary_calls = 0;
  {
    auto temporary =
      handler.register_mouse_click([&](auto) { ++temporary_calls; });
    input.clicked.insert(MOUSE_BUTTON_LEFT);
    handler.handle_input();
  }
  handler.handle_input();
  EXPECT_EQ(temporary_calls, 1);

  int self_calls = 0;
  InputConnection self;
  self = handler.register_mouse_movement([&](auto) {
    ++self_calls;
    self.disconnect();
  });
  handler.handle_input();
  input.mouse.x += 1.0f;
  handler.handle_input();
  EXPECT_EQ(self_calls, 1);
}

TEST_F(InputTest, ConnectionCanOutliveItsHandler)
{
  InputConnection survivor;
  {
    StateHandler handler;
    survivor = handler.register_key_press(KEY_A, [] {});
  }
  EXPECT_TRUE(survivor);
  survivor.disconnect();
  EXPECT_FALSE(survivor);
}

TEST_F(InputTest, SpringIsFrameRateIndependentAndHasGentleOvershoot)
{
  CellState thirty;
  CellState sixty;
  thirty.set_target_scale(1.2f);
  sixty.set_target_scale(1.2f);
  EXPECT_FLOAT_EQ(thirty.get_scale(), 1.0f);
  for (int frame = 0; frame < 6; ++frame) {
    thirty.update_state(1.0f / 30.0f, ui);
  }
  for (int frame = 0; frame < 12; ++frame) {
    sixty.update_state(1.0f / 60.0f, ui);
  }
  EXPECT_NEAR(thirty.get_scale(), sixty.get_scale(), 0.0001f);
  EXPECT_GT(sixty.get_scale(), 1.2f);
}

TEST_F(InputTest, SpringTargetReversalPreservesScaleAndMomentum)
{
  CellState state;
  state.set_target_scale(1.2f);
  state.update_state(0.05f, ui);
  const float moving_scale = state.get_scale();
  state.set_target_scale(1.0f);
  EXPECT_FLOAT_EQ(state.get_scale(), moving_scale);
  state.update_state(0.0005f, ui);
  EXPECT_GT(state.get_scale(), moving_scale);
  state.update_state(5.0f, ui);
  EXPECT_FLOAT_EQ(state.get_scale(), 1.0f);
}

TEST_F(InputTest, InvalidSpringInputsDoNotPoisonAnimation)
{
  CellState state;
  state.set_target_scale(std::numeric_limits<float>::infinity());
  state.set_target_scale(-1.0f);
  state.update_state(std::numeric_limits<float>::quiet_NaN(), ui);
  state.update_state(-1.0f, ui);
  EXPECT_FLOAT_EQ(state.get_scale(), 1.0f);
}

TEST_F(InputTest, MapIndexesMatchCoordinatesAndHoverUsesOriginalBounds)
{
  StateHandler handler;
  {
    Map map(ui, handler);
    const Rectangle first = map.cell_at(0, 0).get_rec();
    const Rectangle right = map.cell_at(1, 0).get_rec();
    const Rectangle below = map.cell_at(0, 1).get_rec();
    EXPECT_GT(right.x, first.x);
    EXPECT_FLOAT_EQ(right.y, first.y);
    EXPECT_GT(below.y, first.y);
    EXPECT_FLOAT_EQ(below.x, first.x);

    auto& cell = map.cell_at(3, 7);
    const Rectangle logical = cell.get_rec(ui.cell_padding);
    input.mouse = { logical.x + logical.width / 2,
                    logical.y + logical.height / 2 };
    handler.handle_input();
    EXPECT_TRUE(cell.is_hovered());
    EXPECT_FLOAT_EQ(cell.get_draw_rec().width, logical.width);
    map.draw(0.05f);
    const Rectangle animated = cell.get_draw_rec();
    EXPECT_GT(animated.width, logical.width);
    EXPECT_NEAR(animated.x + animated.width / 2, input.mouse.x, 0.0001f);
    EXPECT_NEAR(animated.y + animated.height / 2, input.mouse.y, 0.0001f);
    ASSERT_EQ(drawn_cells.size(), UiStore::grid_size * UiStore::grid_size);
    EXPECT_NEAR(drawn_cells.back().x, animated.x, 0.0001f);
    EXPECT_NEAR(drawn_cells.back().y, animated.y, 0.0001f);

    input.mouse.x = logical.x - 0.25f;
    handler.handle_input();
    EXPECT_FALSE(cell.is_hovered()) << "Animated bounds must not steal hover";
    map.draw(2.0f);
    EXPECT_FLOAT_EQ(cell.get_draw_rec().width, logical.width);
    input.mouse = { logical.x + 1, logical.y + 1 };
    handler.handle_input();
    input.cursor_on_screen = false;
    handler.handle_input();
    EXPECT_FALSE(cell.is_hovered());
    input.cursor_on_screen = true;
    handler.handle_input();
    EXPECT_TRUE(cell.is_hovered());
  }
  input.mouse.x += 1.0f;
  handler.handle_input();
}

TEST_F(InputTest, HoveredCellUpdatesExactlyOnceInCombinedWalk)
{
  StateHandler handler;
  Map map(ui, handler);
  auto& cell = map.cell_at(3, 7);
  const Rectangle logical = cell.get_rec(ui.cell_padding);
  input.mouse = { logical.x + 1, logical.y + 1 };
  handler.handle_input();
  ASSERT_TRUE(cell.is_hovered());
  CellState expected;
  expected.set_target_scale(ui.hover_scale);
  expected.update_state(0.05f, ui);
  map.draw(0.05f);
  EXPECT_NEAR(
    cell.get_draw_rec().width / logical.width, expected.get_scale(), 0.0001f);
}

class ProbeDrawable : public Drawable
{
public:
  int updates = 0;
  int draws = 0;
  float elapsed = 0;
  explicit ProbeDrawable(const UiStore& ui)
    : Drawable(ui, { 0, 0, 10, 10 })
  {
  }
  void self_update(float dt) override
  {
    ++updates;
    elapsed += dt;
  }
  void self_draw() override
  {
    ++draws;
    EXPECT_EQ(updates, draws) << "Each draw must be preceded by one update";
  }
};

TEST_F(InputTest, CombinedWalkUpdatesAndDrawsEveryNodeOnce)
{
  ProbeDrawable root(ui);
  auto child = std::make_unique<ProbeDrawable>(ui);
  auto grandchild = std::make_unique<ProbeDrawable>(ui);
  auto* child_ptr = child.get();
  auto* grandchild_ptr = grandchild.get();
  child->children.push_back(std::move(grandchild));
  root.children.push_back(std::move(child));
  root.draw(0.05f);
  root.draw(0.05f);
  for (const auto* node : { &root, child_ptr, grandchild_ptr }) {
    EXPECT_EQ(node->updates, 2);
    EXPECT_EQ(node->draws, 2);
    EXPECT_FLOAT_EQ(node->elapsed, 0.1f);
  }
}
}

// Replace only input and map-drawing calls. No window, camera or GPU is needed.
extern "C"
{
  bool IsWindowFocused()
  {
    return input.focused;
  }
  bool IsCursorOnScreen()
  {
    return input.cursor_on_screen;
  }
  Vector2 GetMousePosition()
  {
    return input.mouse;
  }
  bool IsKeyPressed(int key)
  {
    return input.pressed.contains(key);
  }
  bool IsMouseButtonPressed(int button)
  {
    return input.clicked.contains(button);
  }
  void DrawRectangleRounded(Rectangle rec, float, int, Color)
  {
    drawn_cells.push_back(rec);
  }
  void DrawRectangleRoundedLinesEx(Rectangle, float, int, float, Color) {}
  void DrawRectangleRec(Rectangle, Color) {}
  void DrawText(const char*, int, int, int, Color) {}
  int MeasureText(const char*, int)
  {
    return 0;
  }
}

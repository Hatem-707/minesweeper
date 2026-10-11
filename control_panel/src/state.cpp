#include "state.hpp"
#include <algorithm>
#include <limits>

namespace {
bool
is_valid_key(int key)
{
  return key > KEY_NULL && key <= KEY_KB_MENU;
}

template<typename Callback>
void
prune(std::vector<std::weak_ptr<Callback>>& events)
{
  std::erase_if(events,
                [](const auto& callback) { return callback.expired(); });
}

template<typename Callback, typename... Args>
void
dispatch(const std::vector<std::weak_ptr<Callback>>& events, Args... args)
{
  for (const auto& registration : events) {
    if (auto callback = registration.lock()) {
      (*callback)(args...);
    }
  }
}
}

void
StateHandler::handle_input()
{
  const bool focused = IsWindowFocused();
  window_focused = focused;
  for (auto& [key, events] : key_pressed_events) {
    prune(events);
  }
  std::erase_if(key_pressed_events,
                [](const auto& entry) { return entry.second.empty(); });
  for (auto& [key, events] : key_down_events) {
    prune(events);
  }
  std::erase_if(key_down_events,
                [](const auto& entry) { return entry.second.empty(); });
  prune(mouse_motion_events);
  for (auto& [button, events] : mouse_click_events) {
    prune(events);
  }
  std::erase_if(mouse_click_events,
                [](const auto& entry) { return entry.second.empty(); });
  for (auto& [button, events] : mouse_down_events) {
    prune(events);
  }
  std::erase_if(mouse_down_events,
                [](const auto& entry) { return entry.second.empty(); });

  const auto position = GetMousePosition();
  const MousePosition mouse_position{ position.x, position.y };
  const bool cursor_available = focused && IsCursorOnScreen();
  const bool mouse_changed = !mouse_initialized || mouse_registration_changed ||
                             mouse_position != previous_mouse_position ||
                             cursor_available != previous_cursor_available;
  previous_mouse_position = mouse_position;
  previous_cursor_available = cursor_available;
  mouse_initialized = true;
  mouse_registration_changed = false;

  // Snapshot registrations before invoking callbacks: callbacks may register
  // new listeners or destroy other listeners during this frame.
  const auto keys = key_pressed_events;
  const auto down_events = key_down_events;
  const auto motions = mouse_motion_events;
  const auto clicks = mouse_click_events;
  const auto mouse_down = mouse_down_events;
  std::vector<int> pressed_keys;
  std::vector<std::pair<int, bool>> down_keys;
  for (const auto& [key, events] : down_events) {
    down_keys.emplace_back(key, focused && IsKeyDown(key));
  }
  if (focused) {
    for (const auto& [key, events] : keys) {
      if (IsKeyPressed(key)) {
        pressed_keys.push_back(key);
      }
    }
  }
  std::vector<int> clicked_buttons;
  std::vector<std::pair<int, bool>> held_buttons;
  for (const auto& [button, events] : mouse_down) {
    held_buttons.emplace_back(button,
                              cursor_available && IsMouseButtonDown(button));
  }
  if (cursor_available) {
    for (const auto& [button, events] : clicks) {
      if (IsMouseButtonPressed(button)) {
        clicked_buttons.push_back(button);
      }
    }
  }

  for (int key : pressed_keys) {
    dispatch(keys.at(key));
  }
  for (const auto& [key, down] : down_keys) {
    dispatch(down_events.at(key), down);
  }
  if (mouse_changed) {
    // An unavailable cursor must miss every element, even if raylib retains
    // its last in-window position after the pointer leaves or focus is lost.
    const float outside = -std::numeric_limits<float>::infinity();
    dispatch(motions,
             cursor_available ? mouse_position
                              : MousePosition{ outside, outside });
  }
  for (int button : clicked_buttons) {
    dispatch(clicks.at(button), mouse_position);
  }
  for (const auto& [button, down] : held_buttons) {
    dispatch(mouse_down.at(button), mouse_position, down);
  }
}

InputConnection
StateHandler::register_key_press(int key, KeyCallback callback)
{
  return is_valid_key(key)
           ? connect(key_pressed_events[key], std::move(callback))
           : InputConnection{};
}

InputConnection
StateHandler::register_key_down(int key, KeyDownCallback callback)
{
  return is_valid_key(key) ? connect(key_down_events[key], std::move(callback))
                           : InputConnection{};
}

InputConnection
StateHandler::register_mouse_movement(MouseCallback callback)
{
  auto connection = connect(mouse_motion_events, std::move(callback));
  mouse_registration_changed = mouse_registration_changed || bool(connection);
  return connection;
}

InputConnection
StateHandler::register_mouse_click(MouseCallback callback, int button)
{
  if (button < MOUSE_BUTTON_LEFT || button > MOUSE_BUTTON_BACK) {
    return {};
  }
  return connect(mouse_click_events[button], std::move(callback));
}

InputConnection
StateHandler::register_mouse_down(MouseDownCallback callback, int button)
{
  if (button < MOUSE_BUTTON_LEFT || button > MOUSE_BUTTON_BACK) {
    return {};
  }
  return connect(mouse_down_events[button], std::move(callback));
}

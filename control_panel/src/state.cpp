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
  for (auto& [key, events] : key_pressed_events) {
    prune(events);
  }
  std::erase_if(key_pressed_events,
                [](const auto& entry) { return entry.second.empty(); });
  prune(mouse_motion_events);
  for (auto& [button, events] : mouse_click_events) {
    prune(events);
  }
  std::erase_if(mouse_click_events,
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
  const auto motions = mouse_motion_events;
  const auto clicks = mouse_click_events;
  std::vector<int> pressed_keys;
  if (focused) {
    for (const auto& [key, events] : keys) {
      if (IsKeyPressed(key)) {
        pressed_keys.push_back(key);
      }
    }
  }
  std::vector<int> clicked_buttons;
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
}

InputConnection
StateHandler::register_key_press(int key, KeyCallback callback)
{
  return is_valid_key(key)
           ? connect(key_pressed_events[key], std::move(callback))
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

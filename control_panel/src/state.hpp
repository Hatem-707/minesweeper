#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <raylib.h>
#include <unordered_map>
#include <utility>
#include <vector>
// Keep the connection alive for as long as its callback should be registered.
class InputConnection
{
  friend class StateHandler;
  std::shared_ptr<void> callback;
  explicit InputConnection(std::shared_ptr<void> callback)
    : callback(std::move(callback))
  {
  }

public:
  InputConnection() = default;
  InputConnection(const InputConnection&) = delete;
  InputConnection& operator=(const InputConnection&) = delete;
  InputConnection(InputConnection&&) noexcept = default;
  InputConnection& operator=(InputConnection&&) noexcept = default;
  void disconnect() { callback.reset(); }
  explicit operator bool() const { return callback != nullptr; }
};

class StateHandler
{
public:
  using MousePosition = std::pair<float, float>;
  using KeyCallback = std::function<void()>;
  using MouseCallback = std::function<void(MousePosition)>;

private:
  template<typename Callback>
  using Callbacks = std::vector<std::weak_ptr<Callback>>;

  std::unordered_map<int, Callbacks<KeyCallback>> key_pressed_events;
  Callbacks<MouseCallback> mouse_motion_events;
  std::unordered_map<int, Callbacks<MouseCallback>> mouse_click_events;
  MousePosition previous_mouse_position{};
  bool mouse_initialized = false;
  bool previous_cursor_available = false;
  bool mouse_registration_changed = false;

  template<typename Callback>
  InputConnection connect(Callbacks<Callback>& events, Callback callback)
  {
    if (!callback) {
      return {};
    }
    auto registered = std::make_shared<Callback>(std::move(callback));
    events.push_back(registered);
    return InputConnection(std::move(registered));
  }

public:
  StateHandler() = default;
  ~StateHandler() = default;

  void handle_input();

  StateHandler(const StateHandler&) = delete;
  StateHandler& operator=(const StateHandler&) = delete;

  [[nodiscard]] InputConnection register_key_press(int key,
                                                   KeyCallback callback);
  // The initial position is dispatched once; (-inf, -inf) clears hover when
  // the cursor leaves the window or the window loses focus.
  [[nodiscard]] InputConnection register_mouse_movement(MouseCallback callback);
  [[nodiscard]] InputConnection register_mouse_click(
    MouseCallback callback,
    int button = MOUSE_BUTTON_LEFT);
};

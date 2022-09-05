#pragma once

#include <functional>
#include <Arduino.h>

template<const uint8_t MAX_ACTIONS = 10>
class ActionQueue {
public:
  typedef std::function<uint32_t()> action_t;

  ActionQueue() : _count(0) {}

  uint8_t count() const {
    return _count;
  }
  void clear();
  bool add(action_t action);
  void remove(action_t action);

  void loop();

protected:
  struct action_item_t {
    action_t action;
    uint32_t next;
  };

  int8_t find(action_t action);

  action_item_t _actions[MAX_ACTIONS];
  uint8_t _count;
};

template<const uint8_t MAX_ACTIONS>
inline void ActionQueue<MAX_ACTIONS>::clear() {
  _count = 0;
}

template<const uint8_t MAX_ACTIONS>
bool ActionQueue<MAX_ACTIONS>::add(action_t action) {
  if (_count < MAX_ACTIONS) {
    _actions[_count].action = action;
    _actions[_count].next = 0;
    ++_count;
  }
  return false;
}

template<const uint8_t MAX_ACTIONS>
void ActionQueue<MAX_ACTIONS>::remove(action_t action) {
  int8_t index = find(action);

  if (index >= 0) {
    if (index < _count - 1) {
      memmove(&_actions[index], &_actions[index + 1], sizeof(action_item_t) * (_count - index - 1));
    }
    --_count;
  }
}

template<const uint8_t MAX_ACTIONS>
void ActionQueue<MAX_ACTIONS>::loop() {
  for (uint8_t i = 0; i < _count; ++i) {
    if ((int32_t)(_actions[i].next - millis()) <= 0) {
      uint32_t period;

      period = _actions[i].action();
      if (period)
        _actions[i].next = millis() + period;
      else { // Remove action
        if (i < _count - 1)
          memmove(&_actions[i], &_actions[i + 1], sizeof(action_item_t) * (_count - i - 1));
        --_count;
        --i; // for() step workaround
      }
    }
  }
}

template<const uint8_t MAX_ACTIONS>
int8_t ActionQueue<MAX_ACTIONS>::find(action_t action) {
  for (uint8_t i = 0; i < _count; ++i) {
    if (_actions[i].action == action)
      return i;
  }
  return -1;
}

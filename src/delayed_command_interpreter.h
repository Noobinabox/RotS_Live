#pragma once

struct char_data;

namespace game_types {
class delayed_command_interpreter {
public:
    delayed_command_interpreter(char_data* character);

    void run();

private:
    char_data* m_character;
};
}

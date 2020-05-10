#pragma once

#include "singleton.h"
#include <vector>

struct char_data;

namespace game_timer {
    class skill_timer : public world_singleton<skill_timer> {
        public:
            // Called to add player and skill to cooldown list.
            void add_skill_timer(const char_data& ch, const int skill_id, const int counter);
            // Returns true if the player and skill aren't on cooldown.
            bool is_skill_allowed(const char_data& ch, const int skill_id);
            // Updates the skill timer list and removes any that have expired.
            void update_skill_timer();
        private:
            friend class world_singleton<skill_timer>;
            struct skill_data {
                int player_id;
                int skill_id;
                int counter;

                skill_data()
                    : player_id(0)
                    , skill_id(0)
                    , counter(0) {};
            };
            std::vector<skill_data> m_skill_timer;
            skill_timer(const weather_data* weather, const room_data* world)
                : world_singleton<skill_timer>(weather, world) { };
    };
}
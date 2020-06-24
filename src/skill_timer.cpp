#include "skill_timer.h"
#include <vector>
#include "structs.h"
#include "char_utils.h"
#include "utils.h"

template<>
game_timer::skill_timer* world_singleton<game_timer::skill_timer>::m_pInstance(0);

template<>
bool world_singleton<game_timer::skill_timer>::m_bDestroyed(false);

namespace game_timer {
    void skill_timer::add_skill_timer(const char_data& ch, const int skill_id, const int counter) {
        if (utils::is_npc(ch) || !is_skill_allowed(ch, skill_id)) {
            return;
        }

        int player_id = utils::get_idnum(ch);
        skill_data data;
        data.player_id = player_id;
        data.skill_id = skill_id;
        data.counter = counter;
        vmudlog(CMP, "Skill cooldown set: char:[%s] skill:[%d] counter:[%d]", utils::get_name(ch), skill_id, counter);
        m_skill_timer.push_back(data);
    }

    void skill_timer::update_skill_timer() {
        for (int i = 0; i < m_skill_timer.size(); ++i) {
            auto &data =  m_skill_timer[i];
            if (data.counter > 0) {
                data.counter -= 1;
            }
            else {
                vmudlog(CMP, "Skill cooldown expired char:[%d] skill:[%d]", data.player_id, data.skill_id);
                m_skill_timer.erase(m_skill_timer.begin() + i);
            }
        }
    }

    bool skill_timer::is_skill_allowed(const char_data& ch, const int skill_id) {
        if (utils::is_npc(ch)) {
            return true;
        }

        int player_id = utils::get_idnum(ch);

        for(int i = 0; i < m_skill_timer.size(); ++i) {
            auto data = m_skill_timer[i];
            if (data.player_id == player_id && data.skill_id == skill_id) {
                return false;
            }
        }

        return true;
    }
}
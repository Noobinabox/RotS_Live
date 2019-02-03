#pragma once

struct char_data;

namespace magic {
    class spell_engine {
    private:
        int get_mage_caster_level(const char_data* caster);
        int get_mystic_caster_level(const char_data* caster);
        int get_magic_power(const char_data* caster);
        bool should_apply_spell_peneration(const char_data* caster);
        double get_spell_pen_value(const char_data* caster);
        double get_victim_saving_throw(const char_data* caster, const char_data* victim);
        const int MAX_SPELL_AFFECTS = 1;

    public:
        int mag_damaga(int level, struct char_data* caster, struct char_data* victim,
            int spellnum, int savetype);
        void mag_affects(int level, struct char_data* caster, struct char_data* victim, 
            int spellnum, int savetype);
        void perform_mag_groups(int level, struct char_data* caster, struct char_data *tch,
            int spellnum, int savetype);        
        void mag_groups(int level, struct char_data* caster, int spellnum, int savetype);
        void mag_masses(int level, struct char_data* caster, int spellnum, int savetype);

    };
}


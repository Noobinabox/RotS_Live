#pragma once

#include "interpre.h"
#include "structs.h"

class mob_csv_extract {
public:
    std::string file_location = "../mobs/mob.csv";
    FILE* file;

    static std::string generate_npc_stat(char_data* ch);
    void create_file(char_data* ch);
    void create_header(char_data* ch) const;
    void close_file(char_data* ch) const;
    void write_to_file(char_data* ch, const std::string& mob_data) const;
};

ACMD(do_mob_csv_extract);

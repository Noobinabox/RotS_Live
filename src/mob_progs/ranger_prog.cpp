#include "ranger_prog.h"
#include "shopkeeper.h"

#include "../comm.h"
#include "../db.h"
#include "../interpre.h"
#include "../mob_options.h"
#include "../structs.h"
#include "../utils.h"

#include <cstdio>
#include <unordered_map>

extern struct char_data* mob_proto;
extern struct index_data* mob_index;
extern int top_of_mobt;
extern int no_specials;

SPECIAL(mob_ranger_new);

using mob_options_detail::split_lines;
using mob_options_detail::trim;

namespace {

std::unordered_map<int, ranger_config> g_ranger_configs; /* by mob rnum */

bool is_ranger_proto(int rnum) { return is_ranger_candidate(&mob_proto[rnum], rnum); }

/* Every ranger setting: its key and where it lands in ranger_config. */
struct ranger_key {
    const char* key;
    bool ranger_config::*setting;
};
constexpr ranger_key ranger_keys[] = {
    { "hide", &ranger_config::hide },
    { "ambush", &ranger_config::ambush },
    { "track", &ranger_config::track },
    { "riposte", &ranger_config::riposte },
};
constexpr int ranger_key_count = sizeof(ranger_keys) / sizeof(ranger_keys[0]);

int find_ranger_key(const std::string& key)
{
    for (int i = 0; i < ranger_key_count; ++i)
        if (key == ranger_keys[i].key)
            return i;
    return -1;
}

} // namespace

ranger_config parse_ranger_options(const char* text, std::vector<vendor_problem>* problems)
{
    ranger_config config;
    bool seen[ranger_key_count] = {};
    auto problem = [&](int line, const std::string& what) {
        if (problems)
            problems->push_back({ line, what });
    };

    int line_no = 0;
    for (const std::string& raw : split_lines(text ? text : "")) {
        ++line_no;
        std::string line = trim(raw);
        if (line.empty() || line.compare(0, 2, "//") == 0)
            continue;
        size_t eq = line.find('=');
        std::string key = trim(eq == std::string::npos ? line : line.substr(0, eq));
        std::string value = eq == std::string::npos ? "" : trim(line.substr(eq + 1));
        const int index = find_ranger_key(key);
        if (index < 0 && mob_option_is_general(key))
            continue; /* another feature's line (mob_general_options) */
        if (index < 0 || eq == std::string::npos) {
            problem(line_no, "unknown setting - line ignored");
            continue;
        }
        if (seen[index]) {
            problem(line_no, "duplicate " + key + " - line ignored");
            continue;
        }
        seen[index] = true;
        if (value == "yes" || value == "no")
            config.*ranger_keys[index].setting = value == "yes";
        else
            problem(line_no, "bad " + key + " - line ignored");
    }
    return config;
}

bool is_ranger_candidate(const struct char_data* proto, int rnum)
{
    if (no_specials)
        return false;
    if (!IS_SET(proto->specials2.act, MOB_SPEC) || proto->specials.store_prog_number != PROG_RANGER)
        return false;
    return rnum < 0 || !mob_index[rnum].func || mob_index[rnum].func == (special_func)mob_ranger_new;
}

void ranger_config_check(const struct char_data* proto, int mob_vnum, struct char_data* builder)
{
    std::vector<vendor_problem> problems;
    parse_ranger_options(proto->specials.mob_options, &problems);
    for (const vendor_problem& problem : problems)
        vendor_send(vendor_problem_line(mob_vnum, problem), builder);
}

std::vector<std::string> ranger_flag_warnings(const struct char_data* proto, int mob_vnum)
{
    static const std::pair<long, const char*> unsupported[] = { { MOB_SCAVENGER, "scavenger" },
        { MOB_BODYGUARD, "bodyguard" }, { MOB_ASSISTANT, "assistant" } };
    std::vector<std::string> lines;
    char buf[128];
    for (const auto& flag : unsupported) {
        if (!IS_SET(proto->specials2.act, flag.first))
            continue;
        snprintf(buf, sizeof(buf), "MOB WARNING: mobile #%d: %s has no effect with program %d", mob_vnum,
            flag.second, PROG_RANGER);
        lines.push_back(buf);
    }
    return lines;
}

/* Soft checks, on /imp only, to the builder alone (not at boot, not logged). */
void ranger_implement_check(int mob_rnum, struct char_data* builder)
{
    if (!builder || mob_rnum < 0 || mob_rnum > top_of_mobt || !is_ranger_proto(mob_rnum))
        return;
    for (const std::string& line : ranger_flag_warnings(&mob_proto[mob_rnum], mob_index[mob_rnum].virt)) {
        send_to_char(line.c_str(), builder);
        send_to_char("\n\r", builder);
    }
}

void ranger_config_rebuild(int mob_rnum, struct char_data* builder, bool report)
{
    if (mob_rnum < 0 || mob_rnum > top_of_mobt || !is_ranger_proto(mob_rnum)) {
        g_ranger_configs.erase(mob_rnum);
        return;
    }
    std::vector<vendor_problem> problems;
    g_ranger_configs[mob_rnum] = parse_ranger_options(mob_proto[mob_rnum].specials.mob_options, &problems);
    if (!report)
        return;
    for (const vendor_problem& problem : problems)
        vendor_send(vendor_problem_line(mob_index[mob_rnum].virt, problem), builder);
}

const ranger_config* ranger_config_for(int mob_rnum)
{
    auto it = g_ranger_configs.find(mob_rnum);
    return it == g_ranger_configs.end() ? nullptr : &it->second;
}

const ranger_config& ranger_settings(const struct char_data* mob)
{
    static const ranger_config none;
    if (!mob || !IS_NPC(mob) || mob->specials.store_prog_number != PROG_RANGER)
        return none;
    const ranger_config* config = ranger_config_for(mob->nr);
    return config ? *config : none;
}

void ranger_config_boot()
{
    g_ranger_configs.clear();
    for (int rnum = 0; rnum <= top_of_mobt; ++rnum)
        if (is_ranger_proto(rnum))
            ranger_config_rebuild(rnum, nullptr);
}

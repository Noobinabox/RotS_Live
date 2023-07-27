#include "CharPlayerDataBuilder.h"

namespace builders {
    CharPlayerDataBuilder &CharPlayerDataBuilder::setName(char *value) {
        data.name = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setShortDescription(char* value) {
        data.short_descr = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setLongDescription(char* value) {
        data.long_descr = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setDescription(char* value) {
        data.description = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setTitle(char* value) {
        data.title = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setDeathCry(char* value) {
        data.death_cry = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setDeathCry2(char* value) {
        data.death_cry2 = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setCorpseNum(const int value) {
        data.corpse_num = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setRace(const int value) {
        data.race = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setSex(const byte value) {
        data.sex = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setBodyType(const byte value) {
        data.bodytype = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setProf(const byte value) {
        data.prof = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setLevel(const int value) {
        data.level = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setLanguage(const byte value) {
        data.language = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setHometown(const int value) {
        data.hometown = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setTalks(const byte value) {
        data.talks[1] = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setTime(const time_data value) {
        data.time = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setWeight(const int value) {
        data.weight = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setHeight(const int value) {
        data.height = value;
        return *this;
    }

    CharPlayerDataBuilder &CharPlayerDataBuilder::setRanking(const int value) {
        data.ranking = value;
        return *this;
    }

} // builders
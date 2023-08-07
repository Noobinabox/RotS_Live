#ifndef CHAR_PLAYER_DATA_BUILDER_H_
#define CHAR_PLAYER_DATA_BUILDER_H_

#include "../../structs.h"
#include <strings.h>

namespace builders {

    class CharPlayerDataBuilder {
    private:
        char_player_data data;

    public:
        CharPlayerDataBuilder() {

        }

        CharPlayerDataBuilder &setName(char* value);

        CharPlayerDataBuilder &setShortDescription(char* value);

        CharPlayerDataBuilder &setLongDescription(char* value);

        CharPlayerDataBuilder &setDescription(char* value);

        CharPlayerDataBuilder &setTitle(char* value);

        CharPlayerDataBuilder &setDeathCry(char* value);

        CharPlayerDataBuilder &setDeathCry2(char* value);

        CharPlayerDataBuilder &setCorpseNum(const int value);

        CharPlayerDataBuilder &setRace(const int value);

        CharPlayerDataBuilder &setSex(const byte value);

        CharPlayerDataBuilder &setBodyType(const byte value);

        CharPlayerDataBuilder &setProf(const byte value);

        CharPlayerDataBuilder &setLevel(const int value);

        CharPlayerDataBuilder &setLanguage(const byte value);

        CharPlayerDataBuilder &setHometown(const int value);

        CharPlayerDataBuilder &setTalks(const byte value);

        CharPlayerDataBuilder &setTime(const time_data value);

        CharPlayerDataBuilder &setWeight(const int value);

        CharPlayerDataBuilder &setHeight(const int value);

        CharPlayerDataBuilder &setRanking(const int value);
    };

} // builders

#endif //CHAR_PLAYER_DATA_BUILDER_H_

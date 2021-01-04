#include "platdef.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comm.h"
#include "db.h"
#include "handler.h"
#include "interpre.h"
#include "mudlle.h"
#include "protos.h"
#include "structs.h"
#include "utils.h"

ACMD(do_move);
ACMD(do_say);
ACMD(do_act);
ACMD(do_cast);
extern struct room_data world;
extern char** mobile_program;
extern int* mobile_program_zone;
extern int num_of_programs;
extern struct char_data* waiting_list;
extern struct char_data* character_list;

void TO_STACK(struct char_data* host, long x)
{
    int tmp;

    SPECIAL_STACK(host)
    [SPECIAL_STACKPOINT(host)] = x;

    if (SPECIAL_STACKPOINT(host) == SPECIAL_STACKLEN) {
        for (tmp = 0; tmp < SPECIAL_STACKLEN - 1; tmp++)
            SPECIAL_STACK(host)
        [tmp] = SPECIAL_STACK(host)[tmp + 1];
        SPECIAL_STACK(host)
        [SPECIAL_STACKPOINT(host)] = x;
    } else {
        SPECIAL_STACK(host)
        [SPECIAL_STACKPOINT(host)] = x;
        SPECIAL_STACKPOINT(host)
        ++;
    }
}

long STACK_VALUE(struct char_data* host)
{
    int tmp;
    if (SPECIAL_STACKPOINT(host) < 0)
        tmp = 0;
    else
        tmp = SPECIAL_STACK(host)[SPECIAL_STACKPOINT(host) - 1];

    return tmp;
}

long FROM_STACK(struct char_data* host)
{
    int tmp;

    if (SPECIAL_STACKPOINT(host) <= 0)
        tmp = 0;
    else
        tmp = SPECIAL_STACK(host)[--(SPECIAL_STACKPOINT(host))];

    return tmp;
}

void DOWN_STACK(struct char_data* host)
{
    int tmp;

    tmp = SPECIAL_STACK(host)[0];
    memmove(SPECIAL_STACK(host), SPECIAL_STACK(host) + sizeof(long),
        sizeof(long) * (SPECIAL_STACKLEN - 1));
    SPECIAL_STACK(host)
    [SPECIAL_STACKPOINT(host) - 1] = tmp;
}

void UP_STACK(struct char_data* host)
{
    int tmp;
    tmp = STACK_VALUE(host);
    memmove(SPECIAL_STACK(host) + sizeof(long), SPECIAL_STACK(host),
        sizeof(long) * (SPECIAL_STACKLEN - 1));
    SPECIAL_STACK(host)
    [0] = tmp;
}

void TO_LIST(struct char_data* host, void* entr, int type)
{
    char tmp, tmp2, overfl;

    /*
   * if(type == TARGET_TEXT) 
   *  entr = get_from_txt_block_pool((char *) entr);
   */

    if (!entr)
        type = TARGET_NONE;

    overfl = 0;
    for (tmp = 0; tmp < SPECIAL_STACKLEN; tmp++)
        if (SPECIAL_LIST_AREA(host)->field[(int)tmp].type == SPECIAL_VOID)
            break;

    if (tmp == SPECIAL_STACKLEN) {
        for (tmp2 = 0, tmp = SPECIAL_LIST_HEAD(host);
             (tmp2 < SPECIAL_STACKLEN) && (SPECIAL_LIST_AREA(host)->next[(int)tmp] >= 0);
             tmp2++, tmp = SPECIAL_LIST_AREA(host)->next[(int)tmp])
            ;
    }
    SPECIAL_LIST_AREA(host)
        ->next[(int)tmp]
        = SPECIAL_LIST_HEAD(host);
    SPECIAL_LIST_HEAD(host) = tmp;
    SPECIAL_LIST_TYPE(host) = type;
    SPECIAL_LIST(host)
        .ptr.other
        = entr;

    if (type == TARGET_CHAR)
        SPECIAL_LIST_REFS(host) = SPECIAL_LIST(host).ptr.ch->abs_number;
    else
        SPECIAL_LIST_REFS(host) = -1;
    /*    
   *  printf("to_list, type=%d, head=%d\n",type,SPECIAL_LIST_HEAD(host));
   *  if(type == SPECIAL_STR)
   *    printf("to_list str:%s.\n",SPECIAL_LIST(host).str);
   *  else printf("to_list - not str\n");
   */
}

void TO_LIST(struct char_data* host, target_data* newtarg)
{
    char tmp, tmp2, overfl;

    /*
   * if(type == TARGET_TEXT)
   *   entr = get_from_txt_block_pool((char *) entr);
   */

    if (!newtarg) {
        TO_LIST(host, 0, TARGET_NONE);
        return;
    }

    overfl = 0;
    for (tmp = 0; tmp < SPECIAL_STACKLEN; tmp++)
        if (SPECIAL_LIST_AREA(host)->field[(int)tmp].type == SPECIAL_VOID)
            break;

    if (tmp == SPECIAL_STACKLEN) {
        for (tmp2 = 0, tmp = SPECIAL_LIST_HEAD(host);
             (tmp2 < SPECIAL_STACKLEN) && (SPECIAL_LIST_AREA(host)->next[(int)tmp] >= 0);
             tmp2++, tmp = SPECIAL_LIST_AREA(host)->next[(int)tmp])
            ;
    }
    SPECIAL_LIST_AREA(host)
        ->next[(int)tmp]
        = SPECIAL_LIST_HEAD(host);
    SPECIAL_LIST_HEAD(host) = tmp;
    SPECIAL_LIST_AREA(host)
        ->field[(int)tmp]
        = *newtarg;
}

void UP_LIST(struct char_data* host);

void REMOVE_LIST(struct char_data* host)
{
    int tmp;

    if (SPECIAL_LIST_TYPE(host) == SPECIAL_MARK) {
        UP_LIST(host);
        return;
    }

    if (SPECIAL_LIST_TYPE(host) == TARGET_TEXT)
        put_to_txt_block_pool(SPECIAL_LIST(host).ptr.text);

    SPECIAL_LIST(host)
        .ptr.other
        = 0;
    tmp = SPECIAL_LIST_NEXT(host);
    SPECIAL_LIST_NEXT(host) = -1;
    SPECIAL_LIST_TYPE(host) = SPECIAL_VOID;
    SPECIAL_LIST_HEAD(host) = tmp;
}

void UP_LIST(struct char_data* host)
{
    char tmp, tmp2;

    if (SPECIAL_LIST_NEXT(host) < 0) {
        do_say(host, "My list is less than two elements, I can't move in it.",
            0, 0, 0);
        return;
    }

    for (tmp = SPECIAL_LIST_HEAD(host), tmp2 = 0;
         (SPECIAL_LIST_AREA(host)->next[(int)tmp] >= 0) && (tmp2 < SPECIAL_STACKLEN - 1);
         tmp = SPECIAL_LIST_AREA(host)->next[(int)tmp], tmp2++)
        ;
    SPECIAL_LIST_AREA(host)
        ->next[(int)tmp]
        = SPECIAL_LIST_HEAD(host);
    SPECIAL_LIST_NEXT(host) = -1;
    SPECIAL_LIST_HEAD(host) = SPECIAL_LIST_NEXT(host);

    if (SPECIAL_LIST_TYPE(host) == TARGET_CHAR)
        if (!char_exists(SPECIAL_LIST_REFS(host))) {
            SPECIAL_LIST_TYPE(host) = TARGET_NONE;
            SPECIAL_LIST(host)
                .ptr.other
                = 0;
        }
}

void DOWN_LIST(struct char_data* host)
{
    char tmp, tmp2, tmp3;

    if (SPECIAL_LIST_NEXT(host) < 0) {
        do_say(host, "My list is less than two elements, I can't move in it.",
            0, 0, 0);
        return;
    }

    for (tmp = SPECIAL_LIST_HEAD(host), tmp2 = 0, tmp3 = 0;
         (SPECIAL_LIST_AREA(host)->next[(int)tmp] >= 0) && (tmp2 < SPECIAL_STACKLEN - 1);
         tmp = SPECIAL_LIST_AREA(host)->next[(int)tmp], tmp2++)
        tmp3 = tmp;
    SPECIAL_LIST_AREA(host)
        ->next[(int)tmp3]
        = -1;
    SPECIAL_LIST_AREA(host)
        ->next[(int)tmp]
        = SPECIAL_LIST_HEAD(host);
    SPECIAL_LIST_HEAD(host) = tmp;

    if (SPECIAL_LIST_TYPE(host) == TARGET_CHAR)
        if (!char_exists(SPECIAL_LIST_REFS(host))) {
            SPECIAL_LIST_TYPE(host) = TARGET_NONE;
            SPECIAL_LIST(host)
                .ptr.other
                = 0;
        }
}

int CHECK_LIST(struct char_data* host, char val)
{
    if (SPECIAL_LIST_AREA(host)->field[(int)val].type == TARGET_CHAR)
        if (!char_exists(SPECIAL_LIST_AREA(host)->field[(int)val].ch_num)) {
            SPECIAL_LIST_AREA(host)
                ->field[(int)val]
                .type
                = TARGET_NONE;
            SPECIAL_LIST_AREA(host)
                ->field[(int)val]
                .ptr.other
                = 0;
            return 0;
        }

    return 1;
}

void TEXT_LIST(struct char_data* host, char* targ)
{
    static char void_line[] = "Lost_char";

    switch (SPECIAL_LIST_TYPE(host)) {
    case TARGET_TEXT:
        strcpy(targ, SPECIAL_LIST(host).ptr.text->text);
        targ[strlen(SPECIAL_LIST(host).ptr.text->text)] = 0;
        break;

    case TARGET_CHAR:
        if (!char_exists(SPECIAL_LIST_REFS(host))) {
            strcpy(targ, void_line);
            targ[strlen(void_line)] = 0;
        } else
            strcpy(targ, PERS(SPECIAL_LIST(host).ptr.ch, host, FALSE, FALSE));
        break;

    case TARGET_OBJ:
        sscanf(SPECIAL_LIST(host).ptr.obj->name, "%s", targ);
        break;

    case TARGET_ROOM:
        strcpy(targ, SPECIAL_LIST(host).ptr.room->name);
        targ[strlen(SPECIAL_LIST(host).ptr.room->name)] = 0;
        break;

    case SPECIAL_MARK:
        strcpy(targ, "Mark_item");
        break;

    default:
        strcpy(targ, void_line);
        targ[strlen(void_line)] = 0;
        break;
    }
}

int compare_list(struct char_data* host, struct target_data* it1,
    struct target_data* it2)
{
    char typ1, typ2;
    int tmp;
    char str1[255], str2[255];

    if (SPECIAL_LIST_HEAD(host) < 0)
        return 0;
    if (SPECIAL_LIST_NEXT(host) < 0)
        return 0;

    typ1 = it1->type;
    typ2 = it2->type;

    //  printf("comparing items, types=%d, %d\n",typ1, typ2);
    if ((typ1 == typ2) && (it1 == it2))
        return 1;

    switch (typ1) {
    case TARGET_TEXT:
        break;

    case TARGET_OBJ:
        tmp = strlen(it1->ptr.obj->name);
        if (tmp > 250)
            tmp = 250;
        strncpy(str1, it1->ptr.obj->name, tmp);
        str1[tmp] = 0;
        break;

    case TARGET_CHAR:
        tmp = strlen(it1->ptr.ch->player.name);
        if (tmp > 250)
            tmp = 250;
        strncpy(str1, it1->ptr.ch->player.name, tmp);
        str1[tmp] = 0;
        break;

    case TARGET_ROOM:
        tmp = strlen(it1->ptr.room->name);
        if (tmp > 250)
            tmp = 250;
        strncpy(str1, it1->ptr.room->name, tmp);
        str1[tmp] = 0;
        break;

    default:
        str1[0] = 0;
        break;
    }

    switch (typ2) {
    case TARGET_TEXT:
        break;

    case TARGET_OBJ:
        tmp = strlen(it2->ptr.obj->name);
        if (tmp > 250)
            tmp = 250;
        strncpy(str2, it2->ptr.obj->name, tmp);
        str2[tmp] = 0;
        break;

    case TARGET_CHAR:
        tmp = strlen(it2->ptr.ch->player.name);
        if (tmp > 250)
            tmp = 250;
        strncpy(str2, it2->ptr.ch->player.name, tmp);
        str2[tmp] = 0;
        break;

    case TARGET_ROOM:
        tmp = strlen(it2->ptr.room->name);
        if (tmp > 250)
            tmp = 250;
        strncpy(str2, it2->ptr.room->name, tmp);
        str2[tmp] = 0;
        break;

    default:
        str2[0] = 0;
        break;
    }

    if ((typ1 == TARGET_TEXT) && (typ2 == TARGET_TEXT)) {
        tmp = strcmp(it1->ptr.text->text, it2->ptr.text->text);
        // printf("going to compare %s, %s, res=%d.\n",it1->str, it2->str, tmp);
        return (!tmp);
    }
    if (typ1 == TARGET_TEXT) {
        if (isname(it1->ptr.text->text, str2, TRUE))
            return 1;
        else
            return 0;
    }
    if (typ2 == TARGET_TEXT) {
        if (isname(it2->ptr.text->text, str1, TRUE))
            return 1;
        else
            return 0;
    }

    if (typ1 != typ2)
        return 0;

    switch (typ1) {
    case TARGET_TEXT:
        return 0;

    case TARGET_OBJ:
        if ((it1->ptr.obj->item_number >= 0) && (it1->ptr.obj->item_number == it2->ptr.obj->item_number))
            return 1;
        return 0;

    case TARGET_CHAR:
        if (IS_NPC(it1->ptr.ch) && IS_NPC(it2->ptr.ch) && (it1->ptr.ch->nr == it1->ptr.ch->nr))
            return 1;
        return 0;

    case TARGET_ROOM:
        if (it1->ptr.room->number == it2->ptr.room->number)
            return 1;
        return 0;
    };

    return 0;
}

void int_itemtostring(struct char_data* host)
{
    switch (SPECIAL_LIST_TYPE(host)) {
    case TARGET_NONE:
        return;

    case TARGET_TEXT:
        return;

    case TARGET_CHAR:
        SPECIAL_LIST_TYPE(host) = TARGET_TEXT;
        SPECIAL_LIST(host)
            .ptr.text
            = get_from_txt_block_pool(GET_NAME(SPECIAL_LIST(host).ptr.ch));
        break;

    case TARGET_OBJ:
        SPECIAL_LIST_TYPE(host) = TARGET_TEXT;
        SPECIAL_LIST(host)
            .ptr.text
            = get_from_txt_block_pool(SPECIAL_LIST(host).ptr.obj->short_description);
        break;

    case TARGET_ROOM:
        SPECIAL_LIST_TYPE(host) = TARGET_TEXT;
        SPECIAL_LIST(host)
            .ptr.text
            = get_from_txt_block_pool(SPECIAL_LIST(host).ptr.room->name);
        break;

    default:
        SPECIAL_LIST_TYPE(host) = TARGET_TEXT;
        SPECIAL_LIST(host)
            .ptr.text
            = get_from_txt_block_pool("Weird item");
        break;
    }

    return;
}

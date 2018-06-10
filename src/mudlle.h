/* mudlle.h */

#ifndef MUDLLE_H
#define MUDLLE_H

#include "structs.h" /* For the txt_block structure */

/* the following are the types of special_list entries */
#define SPECIAL_MARK -2
#define SPECIAL_VOID -1
#define SPECIAL_NULL TARGET_NONE
#define SPECIAL_CHAR TARGET_CHAR
#define SPECIAL_OBJ TARGET_OBJ
#define SPECIAL_ROOM TARGET_ROOM
#define SPECIAL_TEXT TARGET_TEXT
#define SPECIAL_FILE 17 // naturally has no analog in mud targets

#define SPECIAL_CALLLIST 10
#define SPECIAL_STACKLEN 50
#define SPECIAL_STACK(ch) ((long*)((ch)->specials.poofIn))
#define PROG_POINT(ch) (ch)->specials.union2.prog_point[(ch)->specials.tactics]
#define PROG_NUMBER(ch) (ch)->specials.union1.prog_number[(ch)->specials.tactics]
#define SPECIAL_STACKPOINT(ch) ((ch)->specials.invis_level)
#define SPECIAL_PROGRAM(ch) (mobile_program[PROG_NUMBER(ch)])

union list_field {
    struct char_data* chr;
    struct obj_data* obj;
    struct room_data* room;
    txt_block* text;
    void* other;
    void operator=(list_field x) { other = x.other; }
};

struct special_list {
    signed char head;
    //  char               type[SPECIAL_STACKLEN];
    //  union list_field   field[SPECIAL_STACKLEN];
    //  long               refs[SPECIAL_STACKLEN]; // safety numbers for chars
    //                                             // in the list.
    signed char next[SPECIAL_STACKLEN];
    struct target_data field[SPECIAL_STACKLEN];
};

#define SPECIAL_LIST_AREA(ch) \
    ((struct special_list*)((ch)->specials.poofOut))

#define SPECIAL_LIST_HEAD(ch) \
    (SPECIAL_LIST_AREA(ch)->head)

#define SPECIAL_LIST(ch) \
    (SPECIAL_LIST_AREA(ch)->field[SPECIAL_LIST_HEAD(ch)])

#define SPECIAL_LIST_NEXT(ch) \
    (SPECIAL_LIST_AREA(ch)->next[SPECIAL_LIST_HEAD(ch)])

#define SPECIAL_LIST_TYPE(ch) \
    (SPECIAL_LIST_AREA(ch)->field[SPECIAL_LIST_HEAD(ch)].type)

#define SPECIAL_LIST_REFS(ch) \
    (SPECIAL_LIST_AREA(ch)->field[SPECIAL_LIST_HEAD(ch)].ch_num)

#define SPECIAL_LIST_RAW(ch) (ch)->specials.poofOut

#define MAX_MUDLLE_BUFFER 10000

SPECIAL(intelligent);

#endif /* MUDLLE_H */

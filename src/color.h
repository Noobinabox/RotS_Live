/* color.h */

#ifndef COLOR_H
#define COLOR_H

/* Number big enough to hold the fields below */
#define MAX_COLOR_FIELDS 16

#define COLOR_NARR 0
#define COLOR_CHAT 1
#define COLOR_YELL 2
#define COLOR_TELL 3
#define COLOR_SAY 4
#define COLOR_ROOM 5
#define COLOR_HIT 6
#define COLOR_DAMG 7
#define COLOR_CHAR 8
#define COLOR_OBJ 9
#define COLOR_ENMY 10
#define COLOR_DESC 11
#define COLOR_GTELL 12

#define CNRM 0
#define CRED 1
#define CGRN 2
#define CYEL 3
#define CBLU 4
#define CMAG 5
#define CCYN 6
#define CWHT 7
#define CBRED 8
#define CBGRN 9
#define CBYEL 10
#define CBBLU 11
#define CBMAG 12
#define CBCYN 13
#define CBWHT 14

/* TRUE if 'ch' has colors turned on */
#define clr(ch) (PRF_FLAGGED((ch), PRF_COLOR) ? 1 : 0)

#define CC_USE(ch, col) \
    (clr(ch) ? (get_colornum(ch, col) != CNRM ? color_sequence[(int)get_colornum(ch, col)] : "") : "")
#define CC_NORM(ch) \
    (clr((ch)) ? color_sequence[0] : "")
#define CC_FIX(ch, col) \
    (clr((ch)) ? color_sequence[col] : "")

extern const char* color_color[];
extern char* color_sequence[];

void set_colors_default(struct char_data*);

#endif /* COLOR_H */

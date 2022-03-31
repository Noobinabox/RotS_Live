/* obj2html.cc */

#include <sys/stat.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "comm.h" /* For vsend_to_char() */
#include "db.h" /* For REAL */
#include "interpre.h" /* For ACMD() */
#include "spells.h" /* For TYPE_xxx */
#include "structs.h" /* For ITEM_xxx */
#include "utils.h" /* For vmudlog() */

extern struct index_data* obj_index;
extern struct obj_data* obj_proto;
extern int top_of_objt;
extern char* object_materials[];
extern int num_of_object_materials;

int tables; /* The number of tables written so far */
int entries; /* The number of entries in the current table so far*/
int total_entries; /* The number of entries for all tables so far */
int dump_all; /* If asserted, then we dump /all objects/ */

/*
 * The type structure.
 *   - The 'title' field is displayed as the title of the table
 *     corresponding to this type;
 *   - 'keywords' is a NULL-terminated list of strings that
 *     associate input from the user with this type.
 *   - The 'typemask' stores which values from get_typemask()
 *     correspond to this type.
 *   - get_typemask() returns a bitvector of type-dependent data
 *     when given an object.
 *   - 'subtypes' is the list of subtypes of this type.
 *   - 'to_output' is for RUNTIME use!  During the declarations
 *     here, it should always be NULL.  It is used to hold all
 *     items which have been approved and are to be output as an
 *     item of this type.
 *   - 'num_to_output' is for RUNTIME use!  During these declarations,
 *     this value should be zero.  'num_to_output' is the number of
 *     objects stored in the to_output list.
 *   - 'valid_types' is for RUNTIME use!  During these declarations,
 *     it should always be set to zero.  It contains a bitvector that
 *     is set depending on what arguments the user supplies.  If the
 *     user requested to see a certain type, it will be set in valid_
 *     types.
 */
/*
 * XXX: rewrite: get_typemask should be compare, anything that uses
 * get_typemask should instead check to see if compare returns 0 to
 * determine whether two items are of the same type mask.  No, this
 * is a different thing completely.  Compare function should be diff-
 * erent.
 */
struct obj2html_type {
    char* title; /* Title for a table of this type */
    char** keywords;
    unsigned long typemask;
    unsigned long (*get_typemask)(struct obj_data*);
    struct obj2html_type* subtypes;
    struct obj_data** to_output; /* For runtime use */
    int num_to_output; /* For runtime use */
    unsigned long valid_types; /* For runtime use */
};

/*
 * Weapon subtypes: currently, for every weapon type we only consider
 * an object a subtype depending on how many hands are required to
 * wield it.  This depends on bulk; the formula used in do_wield to
 * cause an automatic two-hand wield is: wield two-handed if bulk is
 * greater than 4.  In order to allow our user to specify multiple 
 * bulks, we have to shift this into a bitvector.
 */
char* handed_keywords[][3] = {
    { "2h", "two-handed", NULL },
    { "1h", "one-handed", NULL }
};

/* Two handed weapons have bulk of 5-10, one handed have 0-4 */
unsigned long thmask = 1 << 5 | 1 << 6 | 1 << 7 | 1 << 8 | 1 << 9 | 1 << 10;
unsigned long ohmask = 1 << 0 | 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4;

unsigned long
handed_typemask(struct obj_data* o)
{
    return 1 << o->obj_flags.value[2];
}

struct obj2html_type whips_flails_subtypes[] = {
    { "Two-handed Whips and Flails", handed_keywords[0],
        thmask, handed_typemask, NULL,
        NULL, 0, 0 },
    { "One-handed Whips and Flails", handed_keywords[1],
        ohmask, handed_typemask, NULL,
        NULL, 0, 0 },
    { NULL, NULL, 0, NULL, NULL, NULL, 0, 0 }
};

struct obj2html_type slashing_subtypes[] = {
    { "Two-handed Slashing Weapons", handed_keywords[0],
        thmask, handed_typemask, NULL,
        NULL, 0, 0 },
    { "One-handed Slashing Weapons", handed_keywords[1],
        ohmask, handed_typemask, NULL,
        NULL, 0, 0 },
    { NULL, NULL, 0, NULL, NULL, NULL, 0, 0 }
};

struct obj2html_type piercing_subtypes[] = {
    { "Two-handed Piercing Weapons", handed_keywords[0],
        thmask, handed_typemask, NULL,
        NULL, 0, 0 },
    { "One-handed Piercing Weapons", handed_keywords[1],
        ohmask, handed_typemask, NULL,
        NULL, 0, 0 },
    { NULL, NULL, 0, NULL, NULL, NULL, 0, 0 }
};

struct obj2html_type concussion_subtypes[] = {
    { "Two-handed Concussion Weapons", handed_keywords[0],
        thmask, handed_typemask, NULL,
        NULL, 0, 0 },
    { "One-handed Concussion Weapons", handed_keywords[1],
        ohmask, handed_typemask, NULL,
        NULL, 0, 0 },
    { NULL, NULL, 0, NULL, NULL, NULL, 0, 0 }
};

struct obj2html_type axe_subtypes[] = {
    { "Two-handed Axes", handed_keywords[0],
        thmask, handed_typemask, NULL,
        NULL, 0, 0 },
    { "One-handed Axes", handed_keywords[1],
        ohmask, handed_typemask, NULL,
        NULL, 0, 0 },
    { NULL, NULL, 0, NULL, NULL, NULL, 0, 0 }
};

struct obj2html_type spear_subtypes[] = {
    { "Two-handed Spears", handed_keywords[0],
        thmask, handed_typemask, NULL,
        NULL, 0, 0 },
    { "One-handed Spears", handed_keywords[1],
        ohmask, handed_typemask, NULL,
        NULL, 0, 0 },
    { NULL, NULL, 0, NULL, NULL, NULL, 0, 0 }
};

/*
 * The weapon type.  Different weapons are classified according to
 * the skill required to use them; this data is currently held in
 * values[3].  However, this data is held in terms of the TYPE_*
 * macros in order to allow our damage() function to generate damage
 * messages for any type of damage, including spells.  Thus these
 * TYPE_* macros are generally quite large (in the range of 130+ for
 * weapons).  In order to allow the user to specify multiple types
 * in his argument list, we again wish to shift these TYPE_*'s into
 * a bitvector; thus we subtract the lowest type (TYPE_HIT) from
 * each type value before shifting; that way we end up with a range
 * of values between 0-31, enough to fit in a 32 bit vector.
 */
char* weapon_keywords[][4] = {
    { "whip", "flail", NULL },
    { "slash", NULL },
    { "pierce", NULL },
    { "crush", "pound", "smite", NULL },
    { "cleave", NULL },
    { "stab", NULL }
};

unsigned long
weapon_typemask(struct obj_data* o)
{
    extern int weapon_hit_type(int);
    return 1 << (weapon_hit_type(o->obj_flags.value[3]) - TYPE_HIT);
}

struct obj2html_type weapon_subtypes[] = {
    { "Whips and Flails", weapon_keywords[0],
        (1 << (TYPE_WHIP - TYPE_HIT)) | (1 << (TYPE_FLAIL - TYPE_HIT)),
        weapon_typemask, whips_flails_subtypes,
        NULL, 0, 0 },
    { "Slashing Weapons", weapon_keywords[1],
        1 << (TYPE_SLASH - TYPE_HIT),
        weapon_typemask, slashing_subtypes,
        NULL, 0, 0 },
    { "Piercing Weapons", weapon_keywords[2],
        1 << (TYPE_PIERCE - TYPE_HIT),
        weapon_typemask, piercing_subtypes,
        NULL, 0, 0 },
    { "Concussion Weapons", weapon_keywords[3],
        (1 << (TYPE_CRUSH - TYPE_HIT)) | (1 << (TYPE_BLUDGEON - TYPE_HIT)) | (1 << (TYPE_SMITE - TYPE_HIT)),
        weapon_typemask, concussion_subtypes,
        NULL, 0, 0 },
    { "Axes", weapon_keywords[4],
        1 << (TYPE_CLEAVE - TYPE_HIT),
        weapon_typemask, axe_subtypes,
        NULL, 0, 0 },
    { "Spears", weapon_keywords[5],
        1 << (TYPE_SPEARS - TYPE_HIT),
        weapon_typemask, spear_subtypes,
        NULL, 0, 0 },
    { NULL, NULL, 0, NULL, NULL, NULL, 0, 0 },
};

/*
 * The armor type.  Different types of armor are differentiated
 * by wear they can be worn.  This data is already bitvectorized,
 * so there's no need to shift it into a bitvector.  The bitvector
 * in question is stored in (object)->obj_flags.wear_flags.
 */
char* armor_keywords[][2] = {
    { "head", NULL },
    { "neck", NULL },
    { "about", NULL },
    { "body", NULL },
    { "waist", NULL },
    { "arms", NULL },
    { "wrist", NULL },
    { "hands", NULL },
    { "finger", NULL },
    { "legs", NULL },
    { "feet", NULL }
};

unsigned long
armor_typemask(struct obj_data* o)
{
    return o->obj_flags.wear_flags;
}

/* Some (Armor) titles to show that these types are from ITEM_ARMOR */
struct obj2html_type armor_subtypes[] = {
    { "Worn on Head", armor_keywords[0],
        ITEM_WEAR_HEAD, armor_typemask, NULL,
        NULL, 0, 0 },
    { "Worn on Neck", armor_keywords[1],
        ITEM_WEAR_NECK, armor_typemask, NULL,
        NULL, 0, 0 },
    { "Cloaks", armor_keywords[2],
        ITEM_WEAR_ABOUT, armor_typemask, NULL,
        NULL, 0, 0 },
    { "Worn on Body", armor_keywords[3],
        ITEM_WEAR_BODY, armor_typemask, NULL,
        NULL, 0, 0 },
    { "Belts", armor_keywords[4],
        ITEM_WEAR_WAISTE, armor_typemask, NULL,
        NULL, 0, 0 },
    { "Worn on Arms", armor_keywords[5],
        ITEM_WEAR_ARMS, armor_typemask, NULL,
        NULL, 0, 0 },
    { "Worn on Wrist (Armor)", armor_keywords[6],
        ITEM_WEAR_WRIST, armor_typemask, NULL,
        NULL, 0, 0 },
    { "Worn on Hands", armor_keywords[7],
        ITEM_WEAR_HANDS, armor_typemask, NULL,
        NULL, 0, 0 },
    { "Rings (Armor)", armor_keywords[8],
        ITEM_WEAR_FINGER, armor_typemask, NULL,
        NULL, 0, 0 },
    { "Worn on Legs", armor_keywords[9],
        ITEM_WEAR_LEGS, armor_typemask, NULL,
        NULL, 0, 0 },
    { "Worn on Feet", armor_keywords[10],
        ITEM_WEAR_FEET, armor_typemask, NULL,
        NULL, 0, 0 },
    { NULL, NULL, 0, NULL, NULL, NULL, 0, 0 }
};

/*
 * The top-level item "type".  This is the highest level of item
 * classification, and is defined directly by ITEM_* macros.  The
 * item type can be fetched by the macro GET_ITEM_TYPE(object).
 * These types are not shifted into bitvectors, so to allow the
 * user to specify multiple items, we shift the GET_ITEM_TYPE values
 * into a bit vector.  Note that there is no item of type 0.
 */
char* type_keywords[][2] = {
    { NULL },
    { "light", NULL },
    { "scroll", NULL },
    { "wand", NULL },
    { "staff", NULL },
    { "weapon", NULL },
    { "fireweapon", NULL },
    { "missile", NULL },
    { "treasure", NULL },
    { "armor", NULL },
    { "potion", NULL },
    { "worn", NULL },
    { "other", NULL },
    { "trash", NULL },
    { "trap", NULL },
    { "container", NULL },
    { "note", NULL },
    { "drinkcon", NULL },
    { "key", NULL },
    { "food", NULL },
    { "money", NULL },
    { "pen", NULL },
    { "boat", NULL },
    { "fountain", NULL },
    { "shield", NULL },
    { "lever", NULL }
};

unsigned long
item_typemask(struct obj_data* o)
{
    return 1 << GET_ITEM_TYPE(o);
}

/*
 * This array needs to define its members so that the ITEM_LIGHT . .
 * ITEM_MAX #defines in structs.h work appropriately as indeces.
 */
struct obj2html_type obj2html_types[] = {
    { "Light Sources", type_keywords[ITEM_LIGHT],
        1 << ITEM_LIGHT, item_typemask, NULL,
        NULL, 0, 0 },
    { "Scrolls", type_keywords[ITEM_SCROLL],
        1 << ITEM_SCROLL, item_typemask, NULL,
        NULL, 0, 0 },
    { "Wands", type_keywords[ITEM_WAND],
        1 << ITEM_WAND, item_typemask, NULL,
        NULL, 0, 0 },
    { "Staves", type_keywords[ITEM_STAFF],
        1 << ITEM_STAFF, item_typemask, NULL,
        NULL, 0, 0 },
    { "Weapons", type_keywords[ITEM_WEAPON],
        1 << ITEM_WEAPON, item_typemask, weapon_subtypes,
        NULL, 0, 0 },
    { "Fire Weapons", type_keywords[ITEM_FIREWEAPON],
        1 << ITEM_FIREWEAPON, item_typemask, NULL,
        NULL, 0, 0 },
    { "Missile Weapons", type_keywords[ITEM_MISSILE],
        1 << ITEM_MISSILE, item_typemask, NULL,
        NULL, 0, 0 },
    { "Treasure", type_keywords[ITEM_TREASURE],
        1 << ITEM_TREASURE, item_typemask, NULL,
        NULL, 0, 0 },
    { "Armor", type_keywords[ITEM_ARMOR],
        1 << ITEM_ARMOR, item_typemask, armor_subtypes,
        NULL, 0, 0 },
    { "Potions", type_keywords[ITEM_POTION],
        1 << ITEM_POTION, item_typemask, NULL,
        NULL, 0, 0 },
    { "Worn", type_keywords[ITEM_WORN],
        1 << ITEM_WORN, item_typemask, NULL,
        NULL, 0, 0 },
    { "Other", type_keywords[ITEM_OTHER],
        1 << ITEM_OTHER, item_typemask, NULL,
        NULL, 0, 0 },
    { "Trash", type_keywords[ITEM_TRASH],
        1 << ITEM_TRASH, item_typemask, NULL,
        NULL, 0, 0 },
    { "Traps", type_keywords[ITEM_TRAP],
        1 << ITEM_TRAP, item_typemask, NULL,
        NULL, 0, 0 },
    { "Containers", type_keywords[ITEM_CONTAINER],
        1 << ITEM_CONTAINER, item_typemask, NULL,
        NULL, 0, 0 },
    { "Notes", type_keywords[ITEM_NOTE],
        1 << ITEM_NOTE, item_typemask, NULL,
        NULL, 0, 0 },
    { "Drink Containers", type_keywords[ITEM_DRINKCON],
        1 << ITEM_DRINKCON, item_typemask, NULL,
        NULL, 0, 0 },
    { "Keys", type_keywords[ITEM_KEY],
        1 << ITEM_KEY, item_typemask, NULL,
        NULL, 0, 0 },
    { "Food Sources", type_keywords[ITEM_FOOD],
        1 << ITEM_FOOD, item_typemask, NULL,
        NULL, 0, 0 },
    { "Money Objects", type_keywords[ITEM_MONEY],
        1 << ITEM_MONEY, item_typemask, NULL,
        NULL, 0, 0 },
    { "Writing Instruments", type_keywords[ITEM_PEN],
        1 << ITEM_PEN, item_typemask, NULL,
        NULL, 0, 0 },
    { "Boats", type_keywords[ITEM_BOAT],
        1 << ITEM_BOAT, item_typemask, NULL,
        NULL, 0, 0 },
    { "Fountains", type_keywords[ITEM_FOUNTAIN],
        1 << ITEM_FOUNTAIN, item_typemask, NULL,
        NULL, 0, 0 },
    { "Shields", type_keywords[ITEM_SHIELD],
        1 << ITEM_SHIELD, item_typemask, NULL,
        NULL, 0, 0 },
    { "Lever Objects", type_keywords[ITEM_LEVER],
        1 << ITEM_LEVER, item_typemask, NULL,
        NULL, 0, 0 },
    { NULL, NULL, 0, NULL, NULL, NULL, 0, 0 }
};

/*
 * Begins a new HTML table; increments the number of tables
 * created and resets the number of entries for this table to
 * zero.  Also important: writes a new header dependent on
 * object type.
 *
 * NOTE: It is absolutely imperative that the headers in this
 * function for an item of type ITEM_xxx match the format of
 * data printed in obj2html_xxx().
 */
void obj2html_newtable(struct char_data* ch, FILE* f,
    struct obj2html_type* t, int type)
{
    vsend_to_char(ch, "Constructing object table '%s':\n",
        t->title);
    fprintf(f, "\n"
               "<BR> <BR>\n"
               "<CENTER>\n"
               "<H2> %s </H2>\n"
               "<TABLE BORDER=0 CELLPADDING=4 CELLSPACING=1>\n"
               "     <TR ALIGN=RIGHT>\n"
               "          <TH> Vnum </TH>\n"
               "          <TH ALIGN=LEFT> Item Name </TH>\n",
        t->title);

    /* Write the type dependent header data */
    switch (type) {
    case ITEM_LIGHT:
        fprintf(f, "          <TH> Hours </TH>\n");
        break;
    case ITEM_WEAPON:
        fprintf(f,
            "          <TH> OB </TH>\n"
            "          <TH> PB </TH>\n"
            "          <TH> Bulk </TH>\n"
            "          <TH> Type </TH>\n"
            "          <TH> Damage </TH>\n");
        break;
    case ITEM_ARMOR:
        fprintf(f,
            "          <TH> Absorb %% </TH>\n"
            "          <TH> Min Abs. </TH>\n"
            "          <TH> Skill Enc. </TH>\n"
            "          <TH> DB </TH>\n");
        break;
    case ITEM_CONTAINER:
        fprintf(f,
            "          <TH> Can Carry </TH>\n"
            "          <TH> Key to Open</TH>\n");
        break;
    case ITEM_NOTE:
        fprintf(f, "          <TH> Language </TH>\n");
        break;
    case ITEM_DRINKCON:
        fprintf(f, "          <TH> Drink Units </TH>\n");
        break;
    case ITEM_FOOD:
        fprintf(f,
            "          <TH> Hours Filled </TH>\n"
            "          <TH ALIGN=LEFT> Notes </TH>\n");
        break;
    case 1 << ITEM_FOUNTAIN:
        fprintf(f,
            "          <TH> Drink Units </TH>\n"
            "          <TH ALIGN=LEFT> Notes </TH>\n");
        break;
    case ITEM_SHIELD:
        fprintf(f,
            "          <TH> DB </TH>\n"
            "          <TH> PB </TH>\n"
            "          <TH> Skill Enc. </TH>\n"
            "          <TH> Block % </TH>\n");
        break;
    case ITEM_LEVER:
        fprintf(f,
            "          <TH> Room </TH>\n"
            "          <TH> Direction </TH>\n");
        break;
    }

    fprintf(f,
        "          <TH> Weight </TH>\n"
        "          <TH> Value </TH>\n"
        "          <TH> Level </TH>\n"
        "          <TH> Material </TH>\n"
        "          <TH ALIGN=LEFT> Affections </TH>\n"
        "     </TR>\n");

    ++tables;
    entries = 0;
}

/*
 * Finishes the writing of an HTML table; increments the total
 * number of entries by the entries in this table and sends output.
 */
void obj2html_endtable(struct char_data* ch, FILE* f,
    struct obj2html_type* t)
{
    vsend_to_char(ch, "\r\nTable '%s' created with %d entries.\r\n",
        t->title, entries);
    fprintf(f, "</TABLE>\n"
               "</CENTER>\n");

    total_entries += entries;
    entries = 0;
}

int obj2html(FILE* f, struct obj_data* o)
{
    int i, found;
    extern char* apply_types[];

    /* Header */
    /* We highlight it purple if it's magical already */
    fprintf(f, "     <TR ALIGN=RIGHT%s>\n",
        IS_SET(o->obj_flags.extra_flags, ITEM_MAGIC) ? " BGCOLOR=33033" : "");
    fprintf(f, "         <TD> %d </TD>\n",
        obj_index[o->item_number].virt);
    fprintf(f, "         <TD ALIGN=LEFT> %s </TD>\n",
        o->short_description);

    /* Type dependent data */
    switch (GET_ITEM_TYPE(o)) {
    case ITEM_LIGHT:
        if (o->obj_flags.value[2] == -1)
            fprintf(f, "         <TD> Infinite </TD>");
        else
            fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[2]);
        break;
    case ITEM_WEAPON:
        int weapon_hit_type(int);
        extern struct attack_hit_type attack_hit_text[];
        extern int get_weapon_damage(struct obj_data*);

        fprintf(f, "          <TD> %d </TD>\n", o->obj_flags.value[0]);
        fprintf(f, "          <TD> %d </TD>\n", o->obj_flags.value[1]);
        fprintf(f, "          <TD> %d </TD>\n", o->obj_flags.value[2]);
        fprintf(f, "          <TD> %s </TD>\n",
            attack_hit_text[weapon_hit_type(o->obj_flags.value[3]) - TYPE_HIT]);
        fprintf(f, "          <TD> %.1f </TD>\n", get_weapon_damage(o) / 10.0);
        break;
    case ITEM_ARMOR:
        fprintf(f, "         <TD> %d </TD>\n", armor_absorb(o));
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[1]);
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[2]);
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[3]);
        break;
    case ITEM_CONTAINER:
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[0]);
        if (o->obj_flags.value[2] == -1 || o->obj_flags.value[2] == 0)
            fprintf(f, "         <TD> None </TD>\n");
        else
            fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[2]);
        break;
    case ITEM_NOTE:
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[0]);
        break;
    case ITEM_DRINKCON:
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[0]);
        break;
    case ITEM_FOOD:
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[0]);
        if (o->obj_flags.value[3])
            fprintf(f, "         <TD> Poisoned </TD>\n");
        else
            fprintf(f, "         <TD> </TD>\n");
        break;
    case ITEM_FOUNTAIN:
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[0]);
        if (o->obj_flags.value[3])
            fprintf(f, "         <TD> Poisoned </TD>\n");
        else
            fprintf(f, "         <TD> </TD>\n");
        break;
    case ITEM_SHIELD:
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[0]);
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[1]);
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[2]);
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[3]);
        break;
    case ITEM_LEVER:
        extern char* dirs[];
        fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.value[0]);
        fprintf(f, "         <TD> %s </TD>\n", dirs[o->obj_flags.value[1]]);
        break;
    }

    /* Footer */
    fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.weight);
    fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.cost);
    fprintf(f, "         <TD> %d </TD>\n", o->obj_flags.level);
    if (o->obj_flags.material >= 0 && o->obj_flags.material < num_of_object_materials)
        fprintf(f, "         <TD> %s </TD>\n",
            object_materials[o->obj_flags.material]);

    /* Affection list */
    fprintf(f, "          <TD ALIGN=LEFT>");
    for (found = 0, i = 0; i < MAX_OBJ_AFFECT; i++) {
        if (o->affected[i].modifier) {
            if (strcmp(apply_types[o->affected[i].location], "\n")) {
                fprintf(f, "%s %+d %s",
                    found ? "," : "",
                    o->affected[i].modifier,
                    apply_types[o->affected[i].location]);
                ++found;
            }
        }
    }
    fprintf(f, " </TD>\n");

    /* Done */
    fprintf(f, "     </TR>\n");
    ++entries;

    return obj_index[o->item_number].virt;
}

/*
 * Sets ALL matching typemasks; thus if this type 't' has a type
 * matching 'arg', it will be set; and if this 't' additionally
 * has some subtype that matches this arg, that subtype's
 * typemask will also be set.  Returns the number of types and
 * subtypes with matching keywords.
 */
int check_keywords(struct obj2html_type* list, char* arg)
{
    int i, j;
    int types_selected;
    char* c;

    types_selected = 0;

    /* Make sure to check all types in this list */
    for (i = 0; list[i].title != NULL; ++i) {
        /* Now check all keywords for this type */
        for (j = 0; list[i].keywords[j] != NULL; ++j)
            if (!strcmp(arg, list[i].keywords[j])) {
                ++types_selected;
                SET_BIT(list[i].valid_types, list[i].typemask);

                /* now we need the next keyword */
                /* If this type matched, then its subtypes may match */
                if (list[i].subtypes)
                    types_selected += check_keywords(list[i].subtypes, arg);
            }
    }

    return types_selected;
}

void build_output(struct obj2html_type* list, struct obj_data* o)
{
    int i, j;

    /* Alright, check every type in this list of types */
    for (i = 0; list[i].title != NULL; ++i) {
        /* Was this item in the bitvector types selected by the user? */
        if (IS_SET(list[i].valid_types, list[i].get_typemask(o)) || (dump_all && IS_SET(list[i].typemask, list[i].get_typemask(o)))) {

            /*
       * If there are subtypes, then we have to continue recursing until
       * we hit a leaf.  If we don't, then if someone types "weapon pierce",
       * a spear will match at "weapon" and get included in the tables even
       * though it doesn't match the subtype of the weapon.  Otherwise, if
       * there are no subtypes, then there is no way to invalidate this item
       * and it is good for outputting.
       */
            if (list[i].subtypes)
                build_output(list[i].subtypes, o);
            else {
                if (!strcmp(o->short_description, "gizmo"))
                    return;

                /* First, make the tables bigger so that they can hold it */
                RECREATE(list[i].to_output, struct obj_data*,
                    list[i].num_to_output + 1, list[i].num_to_output);

                /* Now insert the item into the table in a type-dependent order */
                /* XXX: Right now, we're just sorting by level.  Once we make
	 * item-dependent comparison functions, we'll be able to sort
	 * in item-dependent fashion.
	 */
                /* Find where to insert to preserve sorted order */
                for (j = 0; j < list[i].num_to_output; ++j)
                    if (o->obj_flags.level > list[i].to_output[j]->obj_flags.level)
                        break;

                /* Insert the item */
                bcopy(list[i].to_output + j, list[i].to_output + j + 1,
                    sizeof(struct obj_data*) * (list[i].num_to_output - j));
                list[i].to_output[j] = o;
                list[i].num_to_output++;
            }
        }
    }
}

/*
 * XXX: The only reason we have a character argument here is to allow
 * per-vnum output.  This can probably be removed once the the whole
 * system is known to be working.
 */
void dump_output(struct char_data* ch, FILE* f, struct obj2html_type* list)
{
    int i, j, vnum;

    for (i = 0; list[i].title != NULL; ++i) {
        if (list[i].subtypes)
            dump_output(ch, f, list[i].subtypes);
        else {
            /*
       * XXX: The num_to_output check is only here to protect the
       * GET_ITEM_TYPE call from operating on a null to_output list.
       * Once we have per-type headers, we won't need this at all.
       */
            if (list[i].num_to_output) {
                obj2html_newtable(ch, f, &list[i],
                    GET_ITEM_TYPE(list[i].to_output[0]));
                for (j = 0; j < list[i].num_to_output; ++j) {
                    vnum = obj2html(f, list[i].to_output[j]);
                    vsend_to_char(ch, " %5d", vnum);

                    if (!(entries % 10))
                        vsend_to_char(ch, "\r\n");
                }
                vsend_to_char(ch, "\r\n");
                obj2html_endtable(ch, f, &list[i]);
                RELEASE(list[i].to_output);
            }
        }
    }
}

/*
 * Recursively clear all RUNTIME data in our type lists
 */
void obj2html_clear(struct obj2html_type* list)
{
    int i;

    for (i = 0; list[i].title != NULL; ++i) {
        list[i].valid_types = 0;
        list[i].to_output = NULL;
        list[i].num_to_output = 0;
        if (list[i].subtypes)
            obj2html_clear(list[i].subtypes);
    }
}

/*
 * Create an html file (named object_dump.html for the moment) and
 * create a nice, tabularly structured output for easy viewing of
 * object data.  Now we expect to be one directory below the rots/
 * directory, where another directory "www/" exists.  We put the
 * object dump here.
 *
 * NOTE: Uses the global 'arg' buffer to chop up command arguments.
 */
ACMD(do_obj2html)
{
    FILE* f;
    char* c;
    int i, selected;
    FILE* obj2html_start(void);
    FILE* obj2html_finish(struct char_data*, FILE * f);

    /* Clear all output lists and valid type masks */
    obj2html_clear(obj2html_types);

    /* Parse arguments.  NOTE: one_argument() guarantees lower case */
    selected = dump_all = 0;
    for (c = argument;;) {
        c = one_argument(c, arg);
        if (*arg == '\0')
            break;
        if (!strcmp(arg, "all")) {
            dump_all = 1;
            break;
        }

        selected += check_keywords(obj2html_types, arg);
    }

    /* Make sure they at least asked for something */
    if (!selected && !dump_all) {
        vsend_to_char(ch, "Usage: obj2html [ <types> ] "
                          "[ <subtypes> ] | all\r\n");
        return;
    } else
        vsend_to_char(ch, "Found %d matching types and subtypes.\r\n",
            selected);

    /* Make the tables */
    for (i = 0; i < top_of_objt; ++i)
        build_output(obj2html_types, &obj_proto[i]);

    /* Write the tables */
    f = obj2html_start();
    if (f == NULL) {
        vsend_to_char(ch, "Failed to start the HTML dump.\r\n");
        return;
    }

    dump_output(ch, f, obj2html_types);

    f = obj2html_finish(ch, f);
}

/*
 * Begins a new HTML page; opens the file, writes the header
 * and begins the body, and initializes the number of tables,
 * entries for this table (even though one hasn't started yet)
 * and total entries for all tables in this file to zero.
 */
FILE* obj2html_start(void)
{
    FILE* f;

    f = fopen("../www/objects.html", "w");
    if (f == NULL) {
        vmudlog(NRM, "Couldn't open the object dump file: %s.",
            strerror(errno));
        return NULL;
    }

    /* Shouldn't there be a way to do this on open()? */
    chmod("../www/objects.html", S_IRWXU | S_IRWXG | S_IRWXO);

    fprintf(f, "<HTML>"
               "\n"
               "<HEAD>\n"
               "<STYLE>\n"
               "     body {background-color: black}\n"
               "     body {color: white}\n"
               "     td {font-size: small}\n"
               "     th {font-size: x-small}\n"
               "</STYLE>\n"
               "</HEAD>\n"
               "\n"
               "<BODY>\n");

    tables = 0;
    entries = 0;
    total_entries = 0;

    return f;
}

/*
 * Finishes the entire HTML page.  Closes the file and makes
 * a usage announcement.
 */
FILE* obj2html_finish(struct char_data* ch, FILE* f)
{
    fprintf(f, "</BODY>\n"
               "</HTML>\n");
    fclose(f);
    vsend_to_char(ch, "Total output: %d tables and %d entries.\r\n",
        tables, total_entries);

    tables = 0;
    entries = 0;
    total_entries = 0;

    return f;
}

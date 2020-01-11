/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik Strfeldt, Tom Madsen, and Katja Nyboe.    *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
 *  ROM 2.4 is copyright 1993-1998 Russ Taylor                             *
 *  ROM has been brought to you by the ROM consortium                      *
 *      Russ Taylor (rtaylor@hypercube.org)                                *
 *      Gabrielle Taylor (gtaylor@hypercube.org)                           *
 *      Brian Moore (zump@rom.org)                                         *
 *  By using this code, you have agreed to follow the terms of the         *
 *  ROM license, in the file Rom24/doc/rom.license                         *
 ***************************************************************************/

#include <string.h>

#include "chars.h"
#include "lookup.h"
#include "objs.h"
#include "affects.h"
#include "memory.h"
#include "utils.h"
#include "globals.h"
#include "comm.h"
#include "save.h"

#include "players.h"

bool player_has_clan (const CHAR_T *ch) {
    if (IS_NPC (ch))
        return FALSE;
    return (ch->clan > 0) ? TRUE : FALSE;
}

bool player_is_independent (const CHAR_T *ch) {
    const CLAN_T *clan;
    if (IS_NPC (ch))
        return TRUE;
    if ((clan = clan_get (ch->clan)) == NULL)
        return TRUE;
    return clan->independent ? TRUE : FALSE;
}

bool player_in_same_clan (const CHAR_T *ch, const CHAR_T *victim) {
    const CLAN_T *clan;
    if (IS_NPC (ch) || IS_NPC (victim))
        return FALSE;
    if ((clan = clan_get (ch->clan)) == NULL)
        return FALSE;
    if (clan->independent)
        return FALSE;
    return (ch->clan == victim->clan);
}

/* used to de-screw characters */
void player_reset (CHAR_T *ch) {
    int loc, mod, stat;
    OBJ_T *obj;
    AFFECT_T *af;
    int i;

    if (IS_NPC (ch))
        return;

    if (ch->pcdata->perm_hit   == 0 ||
        ch->pcdata->perm_mana  == 0 ||
        ch->pcdata->perm_move  == 0 ||
        ch->pcdata->last_level == 0)
    {
        /* do a FULL reset */
        for (loc = 0; loc < WEAR_LOC_MAX; loc++) {
            obj = char_get_eq_by_wear_loc (ch, loc);
            if (obj == NULL)
                continue;
            if (!obj->enchanted) {
                for (af = obj->index_data->affected; af != NULL;
                     af = af->next)
                {
                    mod = af->modifier;
                    switch (af->apply) {
                        case APPLY_SEX:
                            ch->sex -= mod;
                            if (ch->sex < 0 || ch->sex > 2)
                                ch->sex = IS_NPC (ch) ? 0 : ch->pcdata->true_sex;
                            break;
                        case APPLY_MANA: ch->max_mana -= mod; break;
                        case APPLY_HIT:  ch->max_hit  -= mod; break;
                        case APPLY_MOVE: ch->max_move -= mod; break;
                    }
                }
            }

            for (af = obj->affected; af != NULL; af = af->next) {
                mod = af->modifier;
                switch (af->apply) {
                    case APPLY_SEX:  ch->sex      -= mod; break;
                    case APPLY_MANA: ch->max_mana -= mod; break;
                    case APPLY_HIT:  ch->max_hit  -= mod; break;
                    case APPLY_MOVE: ch->max_move -= mod; break;
                }
            }
        }

        /* now reset the permanent stats */
        ch->pcdata->perm_hit = ch->max_hit;
        ch->pcdata->perm_mana = ch->max_mana;
        ch->pcdata->perm_move = ch->max_move;
        ch->pcdata->last_level = ch->played / 3600;

        if (ch->pcdata->true_sex < 0 || ch->pcdata->true_sex > 2) {
            if (ch->sex > 0 && ch->sex < 3)
                ch->pcdata->true_sex = ch->sex;
            else
                ch->pcdata->true_sex = 0;
        }
    }

    /* now restore the character to his/her true condition */
    for (stat = 0; stat < STAT_MAX; stat++)
        ch->mod_stat[stat] = 0;

    if (ch->pcdata->true_sex < 0 || ch->pcdata->true_sex > 2)
        ch->pcdata->true_sex = 0;
    ch->sex      = ch->pcdata->true_sex;
    ch->max_hit  = ch->pcdata->perm_hit;
    ch->max_mana = ch->pcdata->perm_mana;
    ch->max_move = ch->pcdata->perm_move;

    for (i = 0; i < 4; i++)
        ch->armor[i] = 100;

    ch->hitroll = 0;
    ch->damroll = 0;
    ch->saving_throw = 0;

    /* now start adding back the effects */
    for (loc = 0; loc < WEAR_LOC_MAX; loc++) {
        obj = char_get_eq_by_wear_loc (ch, loc);
        if (obj == NULL)
            continue;
        for (i = 0; i < 4; i++)
            ch->armor[i] -= obj_get_ac_type (obj, loc, i);
        if (obj->enchanted)
            continue;
        for (af = obj->index_data->affected; af != NULL; af = af->next)
            affect_modify_apply (ch, af, TRUE);
        for (af = obj->affected; af != NULL; af = af->next)
            affect_modify_apply (ch, af, TRUE);
    }

    /* now add back spell effects */
    for (af = ch->affected; af != NULL; af = af->next)
        affect_modify_apply (ch, af, TRUE);

    /* make sure sex is RIGHT!!!! */
    if (ch->sex < 0 || ch->sex > 2)
        ch->sex = ch->pcdata->true_sex;
}

/* Config Colour stuff */
void player_reset_colour (CHAR_T *ch) {
    int i;

    if (ch == NULL)
        return;
    ch = REAL_CH (ch);

    if (ch->pcdata == NULL)
        return;

    for (i = 0; i < COLOUR_MAX; i++)
        ch->pcdata->colour[i] = colour_setting_table[i].default_colour;
}

void player_set_title (CHAR_T *ch, char *title) {
    char buf[MAX_STRING_LENGTH];

    BAIL_IF_BUG (IS_NPC (ch),
        "player_set_title: NPC.", 0);

    if (title[0] != '.' && title[0] != ',' && title[0] != '!' && title[0] != '?') {
        buf[0] = ' ';
        strcpy (buf + 1, title);
    }
    else
        strcpy (buf, title);

    str_replace_dup (&(ch->pcdata->title), buf);
}

/* Advancement stuff. */
void player_advance_level (CHAR_T *ch, bool hide) {
    char buf[MAX_STRING_LENGTH];
    int add_hp, add_mana, add_move, add_prac;

    if (IS_NPC (ch))
        return;

    ch->pcdata->last_level =
        (ch->played + (int) (current_time - ch->logon)) / 3600;

    sprintf (buf, "the %s",
        title_table[ch->class][ch->level][ch->sex == SEX_FEMALE ? 1 : 0]);
    player_set_title (ch, buf);

    add_hp = char_con_level_hp (ch) +
        number_range (class_table[ch->class].hp_min,
                      class_table[ch->class].hp_max);
    add_mana = number_range (2, (2 * char_get_curr_stat (ch, STAT_INT)
                                 + char_get_curr_stat (ch, STAT_WIS)) / 5);
    if (!class_table[ch->class].gains_mana)
        add_mana /= 2;
    add_move = number_range (1, (char_get_curr_stat (ch, STAT_CON)
                                 + char_get_curr_stat (ch, STAT_DEX)) / 6);
    add_prac = char_wis_level_practices (ch);

    add_hp   = add_hp   * 9 / 10;
    add_mana = add_mana * 9 / 10;
    add_move = add_move * 9 / 10;

    add_hp   = UMAX (2, add_hp);
    add_mana = UMAX (2, add_mana);
    add_move = UMAX (6, add_move);

    ch->max_hit  += add_hp;
    ch->max_mana += add_mana;
    ch->max_move += add_move;
    ch->practice += add_prac;
    ch->train += 1;

    ch->pcdata->perm_hit += add_hp;
    ch->pcdata->perm_mana += add_mana;
    ch->pcdata->perm_move += add_move;

    if (!hide) {
        printf_to_char (ch, "You gain %d hit point%s, %d mana, %d move, and "
            "%d practice%s.\n\r", add_hp, add_hp == 1 ? "" : "s", add_mana,
            add_move, add_prac, add_prac == 1 ? "" : "s");
    }
}

void player_gain_exp (CHAR_T *ch, int gain) {
    if (IS_NPC (ch) || ch->level >= LEVEL_HERO)
        return;

    ch->exp = UMAX (player_get_exp_per_level (ch), ch->exp + gain);
    while (ch->level < LEVEL_HERO && ch->exp >=
           player_get_exp_per_level (ch) * (ch->level + 1))
    {
        send_to_char ("{GYou raise a level!!  {x", ch);
        ch->level += 1;
        log_f ("%s gained level %d", ch->name, ch->level);
        wiznetf (ch, NULL, WIZ_LEVELS, 0, 0,
            "$N has attained level %d!", ch->level);
        player_advance_level (ch, FALSE);
        save_char_obj (ch);
    }
}

int player_get_exp_to_next_level (const CHAR_T *ch) {
    if (IS_NPC (ch) || ch->level >= LEVEL_HERO)
        return 1000;
    return (ch->level + 1) * player_get_exp_per_level (ch) - ch->exp;
}

int player_get_exp_per_level (const CHAR_T *ch) {
    return player_get_exp_per_level_with_points (
        ch, ch->pcdata->creation_points);
}

int player_get_exp_per_level_with_points (const CHAR_T *ch, int points) {
    const PC_RACE_T *race;
    int expl, inc, mult;

    if (IS_NPC (ch))
        return 1000;

    expl = 1000;
    inc = 500;

    race = pc_race_get_by_race (ch->race);
    mult = race->class_mult[ch->class];

    if (points < 40)
        return 1000 * (mult ? mult / 100 : 1);

    /* processing */
    points -= 40;
    while (points > 9) {
        expl += inc;
        points -= 10;
        if (points > 9) {
            expl += inc;
            inc *= 2;
            points -= 10;
        }
    }

    expl += points * inc / 10;
    return expl * mult / 100;
}
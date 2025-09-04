#ifndef _H_RULES_H_
#define _H_RULES_H_

typedef struct base_attributes {
    unsigned char strength, dexterity, constitution, intelligence, wisdom, charisma;
    unsigned char level;
    unsigned char armor_class;
} base_attributes;

typedef struct game_attributes {
    int hitpoints;
} game_attributes;

int rules_d4(int n);
int rules_d6(int n);
int rules_d8(int n);
int rules_d10(int n);
int rules_d12(int n);
int rules_d20(int n);
int rules_d100(int n);

#endif

#include <string.h>

const char *
locale_get(const char *key)
{
    if(key == NULL)
        return "";
    if(strcmp(key, "habit_default_name_format") == 0)
        return "Habit %d";
    if(strcmp(key, "habit_default_meditation_name") == 0)
        return "Meditation";
    if(strcmp(key, "habit_default_meditation_description") == 0)
        return "Breathing and meditation sessions.";
    if(strcmp(key, "habit_default_sun_salutation_name") == 0)
        return "Sun Salutation";
    if(strcmp(key, "habit_default_sun_salutation_description") == 0)
        return "Sun Salutation practice sessions.";
    if(strcmp(key, "habit_new_default_name") == 0)
        return "New Habit";
    return key;
}

/* The kitchen's recipes: one entry per way of making a dish, and the lists of
 * quality bonuses they share. The cooking menu walks `recipes` in order for the
 * first entry the player's pick fits, so the order is the priority; the final
 * entry, the failed dish, always fits. */

#include "types.h"
#include "farm_tables.h"
#include "house.h"
#include "items.h"
#include "text_bank.h"

/* The dish's name: its offset in the messages (Recipe.nameOffset). */
#define DISH_NAME(name) offsetof(TextMessages, dishes.name)
/* The quality of the failed dish, the one that always fits. */
#define COOKING_FIASCO_QUALITY 246

/* Per ingredient, utensil or seasoning: how much it adds to a dish's quality. */
static RecipeBonus mayonnaise_s_bonuses[] = {
    {0, 0},
};

static RecipeBonus mayonnaise_m_bonuses[] = {
    {ITEM_NORMAL_EGG, 0}, {0, 0},
};

static RecipeBonus mayonnaise_l_bonuses[] = {
    {ITEM_NORMAL_EGG, 0}, {ITEM_GOOD_EGG, 0}, {0, 0},
};

static RecipeBonus mayonnaise_g_bonuses[] = {
    {ITEM_NORMAL_EGG, 0}, {ITEM_GOOD_EGG, 0}, {ITEM_EXCELLENT_EGG, 0}, {0, 0},
};

static RecipeBonus pickles_bonuses[] = {
    {ITEM_KNIFE, 3}, {0, 0},
};

static RecipeBonus salad_bonuses[] = {
    {ITEM_CABBAGE, 3}, {ITEM_CUCUMBER, 7}, {ITEM_TOMATO, 6}, {ITEM_CARROT, 5}, {ITEM_ONION, 5},
    {ITEM_SPINACH, 3}, {ITEM_GREEN_PEPPER, 4}, {ITEM_POTATO, 7}, {ITEM_CORN, 7},
    {ITEM_TURNIP, 2}, {ITEM_ANY_CHEESE, 3}, {ITEM_STRAWBERRY, 2}, {ITEM_PINEAPPLE, 4},
    {ITEM_APPLE, 5}, {ITEM_SUGDW_APPLE, 5}, {ITEM_MUSHROOM, 2}, {ITEM_TRUFFLE, 1},
    {ITEM_BOILED_EGG, 7}, {ITEM_OIL, 5}, {ITEM_ANY_MAYONNAISE, 5}, {ITEM_SALT, 5},
    {ITEM_VINEGAR, 5}, {ITEM_SOY_SAUCE, 3}, {0, 0},
};

static RecipeBonus curry_bonuses[] = {
    {ITEM_TURNIP, 2}, {ITEM_POTATO, 7}, {ITEM_TOMATO, 5}, {ITEM_CORN, 7}, {ITEM_ONION, 6},
    {ITEM_PUMPKIN, 7}, {ITEM_EGGPLANT, 6}, {ITEM_CARROT, 7}, {ITEM_SWEET_POTATO, 5},
    {ITEM_SPINACH, 2}, {ITEM_GREEN_PEPPER, 3}, {ITEM_ANY_EGG, 6}, {ITEM_BOILED_EGG, 4},
    {ITEM_SPA_BOILED_EGG, 5}, {ITEM_ANY_MILK, 5}, {ITEM_ANY_CHEESE, 6}, {ITEM_APPLE, 7},
    {ITEM_SUGDW_APPLE, 7}, {ITEM_HONEY, 7}, {ITEM_BAMBOO_SHOOT, 5}, {ITEM_WILD_GRAPE, 3},
    {ITEM_MUSHROOM, 5}, {ITEM_TRUFFLE, 4}, {ITEM_SMALL_FISH, 5}, {ITEM_MEDIUM_FISH, 5},
    {ITEM_LARGE_FISH, 6}, {ITEM_WINE, 2}, {ITEM_GRAPE_JUICE, 2}, {ITEM_CHOCOLATE, 2},
    {ITEM_OIL, 0}, {ITEM_VEGETABLE_JUICE, 2}, {ITEM_SUGAR, 3}, {ITEM_SALT, 3},
    {ITEM_VINEGAR, 1}, {ITEM_SOY_SAUCE, 3}, {ITEM_KNIFE, 8}, {ITEM_PINEAPPLE, 7}, {0, 0},
};

static RecipeBonus stew_bonuses[] = {
    {ITEM_MILK_S, 5}, {ITEM_MILK_M, 5}, {ITEM_MILK_L, 5}, {ITEM_MILK_G, 5}, {ITEM_TURNIP, 5},
    {ITEM_POTATO, 7}, {ITEM_TOMATO, 5}, {ITEM_CORN, 7}, {ITEM_ONION, 6}, {ITEM_PUMPKIN, 4},
    {ITEM_PINEAPPLE, 2}, {ITEM_EGGPLANT, 6}, {ITEM_CARROT, 7}, {ITEM_SWEET_POTATO, 5},
    {ITEM_SPINACH, 2}, {ITEM_GREEN_PEPPER, 3}, {ITEM_ANY_EGG, 6}, {ITEM_BOILED_EGG, 4},
    {ITEM_SPA_BOILED_EGG, 5}, {ITEM_ANY_CHEESE, 6}, {ITEM_APPLE, 5}, {ITEM_SUGDW_APPLE, 5},
    {ITEM_HONEY, 3}, {ITEM_BAMBOO_SHOOT, 5}, {ITEM_WILD_GRAPE, 3}, {ITEM_MUSHROOM, 5},
    {ITEM_TRUFFLE, 5}, {ITEM_SMALL_FISH, 5}, {ITEM_MEDIUM_FISH, 5}, {ITEM_LARGE_FISH, 6},
    {ITEM_WINE, 2}, {ITEM_GRAPE_JUICE, 2}, {ITEM_CHOCOLATE, 2}, {ITEM_OIL, 0},
    {ITEM_VEGETABLE_JUICE, 2}, {ITEM_KNIFE, 8}, {0, 0},
};

static RecipeBonus miso_soup_bonuses[] = {
    {ITEM_TURNIP, 5}, {ITEM_POTATO, 3}, {ITEM_CABBAGE, 3}, {ITEM_TOMATO, 3}, {ITEM_CORN, 4},
    {ITEM_ONION, 5}, {ITEM_PUMPKIN, 4}, {ITEM_EGGPLANT, 4}, {ITEM_CARROT, 4},
    {ITEM_SWEET_POTATO, 3}, {ITEM_SPINACH, 3}, {ITEM_GREEN_PEPPER, 3}, {ITEM_ANY_EGG, 4},
    {ITEM_BAMBOO_SHOOT, 5}, {ITEM_MUSHROOM, 5}, {ITEM_TRUFFLE, 8}, {ITEM_SMALL_FISH, 5},
    {ITEM_KNIFE, 8}, {ITEM_SALT, 7}, {ITEM_SOY_SAUCE, 5}, {0, 0},
};

static RecipeBonus stir_fry_bonuses[] = {
    {ITEM_TURNIP, 3}, {ITEM_POTATO, 1}, {ITEM_CUCUMBER, 2}, {ITEM_TOMATO, 1}, {ITEM_CORN, 5},
    {ITEM_ONION, 7}, {ITEM_PUMPKIN, 2}, {ITEM_EGGPLANT, 6}, {ITEM_CARROT, 7},
    {ITEM_SWEET_POTATO, 2}, {ITEM_SPINACH, 3}, {ITEM_GREEN_PEPPER, 7}, {ITEM_BAMBOO_SHOOT, 5},
    {ITEM_MUSHROOM, 5}, {ITEM_TRUFFLE, 9}, {ITEM_SALT, 7}, {ITEM_VINEGAR, 2},
    {ITEM_MISO, 3}, {0, 0},
};

static RecipeBonus fried_rice_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_CUCUMBER, 1}, {ITEM_CABBAGE, 5}, {ITEM_CORN, 5}, {ITEM_ONION, 3}, {ITEM_EGGPLANT, -1},
    {ITEM_CARROT, 5}, {ITEM_SPINACH, 1}, {ITEM_GREEN_PEPPER, 3}, {ITEM_SPA_BOILED_EGG, 3},
    {ITEM_ANY_CHEESE, -1}, {ITEM_BAMBOO_SHOOT, 1}, {ITEM_MUSHROOM, 1}, {ITEM_TRUFFLE, 5},
    {ITEM_WINE, 1}, {ITEM_LARGE_FISH, 3}, {ITEM_MEDIUM_FISH, 3}, {ITEM_SMALL_FISH, 3},
    {ITEM_FRIED_RICE, -1}, {ITEM_STIR_FRY, 1}, {ITEM_SCRAMBLED_EGGS, 1}, {ITEM_BOILED_EGG, 1},
    {ITEM_SASHIMI, 3}, {ITEM_GRILLED_FISH, 2}, {ITEM_SUGAR, -1}, {ITEM_SALT, 5},
    {ITEM_SOY_SAUCE, 5}, {ITEM_KNIFE, 3}, {0, 0},
};

static RecipeBonus veggie_pancake_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_TURNIP, 2}, {ITEM_POTATO, 1}, {ITEM_CUCUMBER, 1}, {ITEM_CORN, 4}, {ITEM_ONION, 5},
    {ITEM_PUMPKIN, 2}, {ITEM_EGGPLANT, 3}, {ITEM_CARROT, 4}, {ITEM_SPINACH, 1},
    {ITEM_ANY_MAYONNAISE, 7}, {ITEM_ANY_MILK, 1}, {ITEM_ANY_CHEESE, 3},
    {ITEM_BAMBOO_SHOOT, 5}, {ITEM_MUSHROOM, 5}, {ITEM_TRUFFLE, 7}, {ITEM_SMALL_FISH, 7},
    {ITEM_MEDIUM_FISH, 7}, {ITEM_LARGE_FISH, 7}, {ITEM_SALT, 3}, {ITEM_SOY_SAUCE, 5},
    {0, 0},
};

static RecipeBonus sandwich_bonuses[] = {
    {ITEM_CUCUMBER, 5}, {ITEM_TOMATO, 6}, {ITEM_BOILED_EGG, 7}, {ITEM_BUTTER, 7},
    {ITEM_POTATO, 7}, {ITEM_STRAWBERRY, 5}, {ITEM_CABBAGE, 3}, {ITEM_CORN, 5}, {ITEM_ONION, 4},
    {ITEM_PINEAPPLE, 5}, {ITEM_CARROT, 4}, {ITEM_SWEET_POTATO, 4}, {ITEM_ANY_MAYONNAISE, 5},
    {ITEM_ANY_CHEESE, 5}, {ITEM_APPLE, 4}, {ITEM_SUGDW_APPLE, 4}, {ITEM_WILD_GRAPE, 4},
    {ITEM_MUSHROOM, 4}, {ITEM_TRUFFLE, 2}, {ITEM_SMALL_FISH, 3}, {ITEM_MEDIUM_FISH, 5},
    {ITEM_LARGE_FISH, 4}, {ITEM_HONEY, 3}, {ITEM_WINE, 3}, {ITEM_SALT, 5}, {0, 0},
};

static RecipeBonus strawberry_milk_bonuses[] = {
    {ITEM_MILK_S, 5}, {ITEM_MILK_M, 5}, {ITEM_MILK_L, 5}, {ITEM_MILK_G, 5}, {ITEM_HONEY, 3},
    {ITEM_SUGAR, 3}, {ITEM_SALT, 0}, {0, 0},
};

static RecipeBonus fruit_juice_bonuses[] = {
    {ITEM_SUGDW_APPLE, 3}, {ITEM_APPLE, 3}, {ITEM_STRAWBERRY, 3}, {ITEM_PINEAPPLE, 3},
    {ITEM_WILD_GRAPE, 3}, {ITEM_GRAPE_JUICE, 3}, {ITEM_HONEY, 1}, {ITEM_KNIFE, 1},
    {ITEM_SUGAR, 2}, {0, 0},
};

static RecipeBonus fruit_latte_bonuses[] = {
    {ITEM_MILK_S, 5}, {ITEM_MILK_M, 5}, {ITEM_MILK_L, 5}, {ITEM_MILK_G, 5},
    {ITEM_SUGDW_APPLE, 3}, {ITEM_APPLE, 3}, {ITEM_STRAWBERRY, 3}, {ITEM_PINEAPPLE, 3},
    {ITEM_WILD_GRAPE, 3}, {ITEM_GRAPE_JUICE, 3}, {ITEM_HONEY, 1}, {ITEM_KNIFE, 1},
    {ITEM_SUGAR, 2}, {0, 0},
};

static RecipeBonus vegetable_juice_bonuses[] = {
    {ITEM_CUCUMBER, 5}, {ITEM_CABBAGE, 4}, {ITEM_CARROT, 4}, {ITEM_TOMATO, 5},
    {ITEM_BAMBOO_SHOOT, 3}, {ITEM_MUSHROOM, 3}, {ITEM_TRUFFLE, 1}, {ITEM_TURNIP, 3},
    {ITEM_CORN, 2}, {ITEM_ONION, 4}, {ITEM_EGGPLANT, 3}, {ITEM_SPINACH, 7},
    {ITEM_GREEN_PEPPER, 5}, {ITEM_KNIFE, 1}, {ITEM_SALT, 2}, {0, 0},
};

static RecipeBonus veggie_latte_bonuses[] = {
    {ITEM_MILK_S, 5}, {ITEM_MILK_M, 5}, {ITEM_MILK_L, 5}, {ITEM_MILK_G, 5}, {ITEM_CUCUMBER, 5},
    {ITEM_CABBAGE, 4}, {ITEM_CARROT, 4}, {ITEM_TOMATO, 5}, {ITEM_BAMBOO_SHOOT, 3},
    {ITEM_MUSHROOM, 3}, {ITEM_TRUFFLE, 1}, {ITEM_TURNIP, 3}, {ITEM_CORN, 2}, {ITEM_ONION, 4},
    {ITEM_EGGPLANT, 3}, {ITEM_SPINACH, 7}, {ITEM_GREEN_PEPPER, 5}, {ITEM_KNIFE, 1},
    {ITEM_SALT, 2}, {0, 0},
};

static RecipeBonus mixed_juice_bonuses[] = {
    {ITEM_SUGDW_APPLE, 3}, {ITEM_APPLE, 3}, {ITEM_STRAWBERRY, 3}, {ITEM_PINEAPPLE, 3},
    {ITEM_WILD_GRAPE, 3}, {ITEM_CUCUMBER, 5}, {ITEM_CABBAGE, 4}, {ITEM_CARROT, 4},
    {ITEM_GRAPE_JUICE, 3}, {ITEM_HONEY, 1}, {ITEM_TOMATO, 5}, {ITEM_BAMBOO_SHOOT, 3},
    {ITEM_MUSHROOM, 3}, {ITEM_TRUFFLE, 1}, {ITEM_TURNIP, 3}, {ITEM_CORN, 2}, {ITEM_ONION, 4},
    {ITEM_EGGPLANT, 3}, {ITEM_SPINACH, 7}, {ITEM_GREEN_PEPPER, 5}, {ITEM_KNIFE, 1},
    {ITEM_SUGAR, 2}, {ITEM_SALT, 2}, {0, 0},
};

static RecipeBonus mixed_latte_bonuses[] = {
    {ITEM_MILK_S, 5}, {ITEM_MILK_M, 5}, {ITEM_MILK_L, 5}, {ITEM_MILK_G, 5}, {ITEM_KNIFE, 1},
    {ITEM_SUGAR, 2}, {ITEM_SALT, 2}, {0, 0},
};

static RecipeBonus mixed_latte_2_bonuses[] = {
    {ITEM_MILK_S, 5}, {ITEM_MILK_M, 5}, {ITEM_MILK_L, 5}, {ITEM_MILK_G, 5},
    {ITEM_SUGDW_APPLE, 3}, {ITEM_APPLE, 3}, {ITEM_STRAWBERRY, 3}, {ITEM_PINEAPPLE, 3},
    {ITEM_WILD_GRAPE, 3}, {ITEM_CUCUMBER, 5}, {ITEM_CABBAGE, 4}, {ITEM_CARROT, 4},
    {ITEM_TOMATO, 5}, {ITEM_GRAPE_JUICE, 3}, {ITEM_HONEY, 1}, {ITEM_BAMBOO_SHOOT, 3},
    {ITEM_MUSHROOM, 3}, {ITEM_TRUFFLE, 1}, {ITEM_TURNIP, 3}, {ITEM_CORN, 2}, {ITEM_ONION, 4},
    {ITEM_EGGPLANT, 3}, {ITEM_SPINACH, 7}, {ITEM_GREEN_PEPPER, 5}, {ITEM_FRUIT_JUICE, 2},
    {ITEM_VEGETABLE_JUICE, 2}, {ITEM_FRUIT_LATTE, 2}, {ITEM_VEGGIE_LATTE, 2}, {ITEM_KNIFE, 1},
    {ITEM_SUGAR, 2}, {ITEM_SALT, 2}, {0, 0},
};

static RecipeBonus pickled_turnips_bonuses[] = {
    {ITEM_SUGAR, 2}, {ITEM_SALT, 2}, {ITEM_SOY_SAUCE, 1}, {0, 0},
};

static RecipeBonus fries_bonuses[] = {
    {ITEM_SALT, 5}, {ITEM_KETCHUP, 5}, {0, 2},
};

static RecipeBonus strawberry_jam_bonuses[] = {
    {ITEM_SALT, 1}, {0, 0},
};

static RecipeBonus tomato_juice_bonuses[] = {
    {ITEM_SALT, 5}, {0, 0},
};

static RecipeBonus ketchup_bonuses[] = {
    {0, 0},
};

static RecipeBonus popcorn_bonuses[] = {
    {ITEM_BUTTER, 5}, {ITEM_SALT, 5}, {0, 0},
};

static RecipeBonus pumpkin_pudding_bonuses[] = {
    {ITEM_MILK_S, 5}, {ITEM_MILK_M, 5}, {ITEM_MILK_L, 5}, {ITEM_MILK_G, 5},
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_HONEY, 1}, {ITEM_WINE, 1}, {0, 0},
};

static RecipeBonus happy_eggplant_bonuses[] = {
    {0, 0},
};

static RecipeBonus sweet_potato_dish_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {0, 0},
};

static RecipeBonus roasted_potatoes_bonuses[] = {
    {ITEM_SUGAR, 1}, {ITEM_SALT, 1}, {0, 0},
};

static RecipeBonus greens_bonuses[] = {
    {0, 0},
};

static RecipeBonus scrambled_eggs_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_BUTTER, 3}, {ITEM_ANY_MAYONNAISE, 3}, {ITEM_WHISK, 3}, {ITEM_SUGAR, 3},
    {ITEM_SALT, 3}, {ITEM_SOY_SAUCE, 3}, {0, 0},
};

static RecipeBonus omelet_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_POTATO, 7}, {ITEM_CABBAGE, 3}, {ITEM_CORN, 7}, {ITEM_ONION, 6}, {ITEM_PUMPKIN, 3},
    {ITEM_EGGPLANT, 3}, {ITEM_CARROT, 4}, {ITEM_SWEET_POTATO, 4}, {ITEM_SPINACH, 5},
    {ITEM_GREEN_PEPPER, 4}, {ITEM_ANY_MAYONNAISE, 4}, {ITEM_ANY_CHEESE, 6},
    {ITEM_BAMBOO_SHOOT, 3}, {ITEM_MUSHROOM, 4}, {ITEM_TRUFFLE, 2}, {ITEM_SMALL_FISH, 5},
    {ITEM_MEDIUM_FISH, 7}, {ITEM_LARGE_FISH, 5}, {ITEM_KETCHUP, 5}, {ITEM_KNIFE, 2},
    {ITEM_WHISK, 3}, {ITEM_SUGAR, 3}, {ITEM_SALT, 3}, {ITEM_SOY_SAUCE, 3}, {0, 0},
};

static RecipeBonus rice_omelet_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_POTATO, 7}, {ITEM_CABBAGE, 3}, {ITEM_CORN, 7}, {ITEM_ONION, 6}, {ITEM_PUMPKIN, 3},
    {ITEM_EGGPLANT, 3}, {ITEM_CARROT, 4}, {ITEM_SWEET_POTATO, 4}, {ITEM_SPINACH, 5},
    {ITEM_GREEN_PEPPER, 4}, {ITEM_ANY_MAYONNAISE, 4}, {ITEM_ANY_CHEESE, 6},
    {ITEM_BAMBOO_SHOOT, 3}, {ITEM_MUSHROOM, 4}, {ITEM_TRUFFLE, 3}, {ITEM_SMALL_FISH, 5},
    {ITEM_MEDIUM_FISH, 7}, {ITEM_LARGE_FISH, 5}, {ITEM_KETCHUP, 5}, {ITEM_KNIFE, 2},
    {ITEM_WHISK, 3}, {ITEM_SUGAR, 3}, {ITEM_SALT, 3}, {ITEM_SOY_SAUCE, 3}, {0, 0},
};

static RecipeBonus boiled_egg_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_SALT, 3}, {0, 0},
};

static RecipeBonus hot_milk_bonuses[] = {
    {ITEM_MILK_S, 5}, {ITEM_MILK_M, 5}, {ITEM_MILK_L, 5}, {ITEM_MILK_G, 5}, {ITEM_SUGAR, 4},
    {0, 0},
};

static RecipeBonus butter_bonuses[] = {
    {ITEM_MILK_S, 5}, {ITEM_MILK_M, 5}, {ITEM_MILK_L, 5}, {ITEM_MILK_G, 5}, {0, 0},
};

static RecipeBonus cheesecake_bonuses[] = {
    {ITEM_MILK_S, 5}, {ITEM_MILK_M, 5}, {ITEM_MILK_L, 5}, {ITEM_MILK_G, 5}, {ITEM_CHEESE_S, 5},
    {ITEM_CHEESE_M, 5}, {ITEM_CHEESE_L, 5}, {ITEM_CHEESE_G, 5}, {ITEM_NORMAL_EGG, 5},
    {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5}, {ITEM_HONEY, 2}, {0, 0},
};

static RecipeBonus cheese_fondue_bonuses[] = {
    {ITEM_CHEESE_S, 5}, {ITEM_CHEESE_M, 5}, {ITEM_CHEESE_L, 5}, {ITEM_CHEESE_G, 5},
    {ITEM_WINE, 5}, {ITEM_KNIFE, 3}, {ITEM_SALT, 5}, {0, 0},
};

static RecipeBonus apple_pie_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_SUGDW_APPLE, 3}, {ITEM_APPLE, 3}, {ITEM_HONEY, 3}, {ITEM_WINE, 1}, {0, 0},
};

static RecipeBonus apple_jam_bonuses[] = {
    {ITEM_SUGDW_APPLE, 3}, {ITEM_APPLE, 3}, {ITEM_HONEY, 3}, {ITEM_WINE, 1}, {0, 0},
};

static RecipeBonus bamboo_rice_bonuses[] = {
    {ITEM_KNIFE, 3}, {ITEM_SALT, 1}, {ITEM_SOY_SAUCE, 1}, {0, 0},
};

static RecipeBonus grape_jam_bonuses[] = {
    {ITEM_HONEY, 3}, {ITEM_WINE, 1}, {0, 0},
};

static RecipeBonus mushroom_rice_bonuses[] = {
    {ITEM_KNIFE, 3}, {ITEM_SALT, 1}, {ITEM_SOY_SAUCE, 1}, {0, 0},
};

static RecipeBonus truffle_rice_bonuses[] = {
    {ITEM_KNIFE, 3}, {ITEM_SALT, 1}, {ITEM_SOY_SAUCE, 1}, {0, 0},
};

static RecipeBonus sushi_bonuses[] = {
    {ITEM_SOY_SAUCE, 1}, {0, 0},
};

static RecipeBonus jam_bun_bonuses[] = {
    {ITEM_HONEY, 2}, {ITEM_APPLE_JAM, 5}, {ITEM_STRAWBERRY_JAM, 5}, {ITEM_GRAPE_JAM, 5}, {0, 0},
};

static RecipeBonus dinner_roll_bonuses[] = {
    {ITEM_HONEY, 2}, {0, 0},
};

static RecipeBonus raisin_bread_bonuses[] = {
    {ITEM_HONEY, 2}, {0, 0},
};

static RecipeBonus sashimi_bonuses[] = {
    {ITEM_MEDIUM_FISH, 5}, {ITEM_LARGE_FISH, 5}, {ITEM_SOY_SAUCE, 3}, {0, 0},
};

static RecipeBonus grilled_fish_bonuses[] = {
    {ITEM_SALT, 3}, {ITEM_SOY_SAUCE, 3}, {0, 0},
};

static RecipeBonus chirashi_sushi_bonuses[] = {
    {ITEM_CUCUMBER, 2}, {ITEM_SCRAMBLED_EGGS, 3}, {ITEM_SOY_SAUCE, 3}, {0, 0},
};

static RecipeBonus pizza_bonuses[] = {
    {ITEM_CHEESE_S, 5}, {ITEM_CHEESE_M, 5}, {ITEM_CHEESE_L, 5}, {ITEM_CHEESE_G, 5},
    {ITEM_POTATO, 7}, {ITEM_TOMATO, 8}, {ITEM_CORN, 7}, {ITEM_ONION, 6}, {ITEM_PUMPKIN, 2},
    {ITEM_PINEAPPLE, 3}, {ITEM_EGGPLANT, 3}, {ITEM_CARROT, 3}, {ITEM_SWEET_POTATO, 2},
    {ITEM_GREEN_PEPPER, 6}, {ITEM_ANY_EGG, 4}, {ITEM_ANY_MAYONNAISE, 3},
    {ITEM_BAMBOO_SHOOT, 2}, {ITEM_MUSHROOM, 5}, {ITEM_TRUFFLE, 3}, {ITEM_SMALL_FISH, 4},
    {ITEM_MEDIUM_FISH, 5}, {ITEM_LARGE_FISH, 4}, {ITEM_KNIFE, 7}, {ITEM_SUGAR, 1},
    {ITEM_SALT, 3}, {0, 0},
};

static RecipeBonus noodles_bonuses[] = {
    {ITEM_TURNIP, 1}, {ITEM_CABBAGE, 1}, {ITEM_ONION, 3}, {ITEM_EGGPLANT, 3}, {ITEM_CARROT, 3},
    {ITEM_SWEET_POTATO, 2}, {ITEM_GREEN_PEPPER, 3}, {ITEM_ANY_EGG, 8},
    {ITEM_BAMBOO_SHOOT, 3}, {ITEM_MUSHROOM, 3}, {ITEM_TRUFFLE, 3}, {ITEM_SMALL_FISH, 6},
    {ITEM_MEDIUM_FISH, 5}, {ITEM_LARGE_FISH, 4}, {ITEM_SUGAR, 3}, {ITEM_SALT, 3},
    {ITEM_SOY_SAUCE, 7}, {ITEM_MISO, 5}, {0, 0},
};

static RecipeBonus curry_noodles_bonuses[] = {
    {ITEM_TURNIP, -1}, {ITEM_CABBAGE, 1}, {ITEM_ONION, 3}, {ITEM_EGGPLANT, 3}, {ITEM_CARROT, 3},
    {ITEM_SWEET_POTATO, 2}, {ITEM_GREEN_PEPPER, 3}, {ITEM_ANY_EGG, 2},
    {ITEM_BAMBOO_SHOOT, 3}, {ITEM_MUSHROOM, 3}, {ITEM_TRUFFLE, 3}, {ITEM_SMALL_FISH, 6},
    {ITEM_MEDIUM_FISH, 5}, {ITEM_LARGE_FISH, 4}, {ITEM_SUGAR, 1}, {ITEM_SALT, 3},
    {ITEM_SOY_SAUCE, 3}, {ITEM_MISO, 2}, {0, 0},
};

static RecipeBonus tempura_noodles_bonuses[] = {
    {ITEM_TURNIP, 1}, {ITEM_CABBAGE, 1}, {ITEM_ONION, 3}, {ITEM_EGGPLANT, 3}, {ITEM_CARROT, 3},
    {ITEM_SWEET_POTATO, 2}, {ITEM_GREEN_PEPPER, 3}, {ITEM_ANY_EGG, 3},
    {ITEM_BAMBOO_SHOOT, 3}, {ITEM_MUSHROOM, 3}, {ITEM_TRUFFLE, 3}, {ITEM_SMALL_FISH, 6},
    {ITEM_MEDIUM_FISH, 5}, {ITEM_LARGE_FISH, 4}, {ITEM_SUGAR, 3}, {ITEM_SALT, 3},
    {ITEM_SOY_SAUCE, 7}, {ITEM_MISO, 5}, {0, 0},
};

static RecipeBonus fried_noodles_bonuses[] = {
    {ITEM_TURNIP, -2}, {ITEM_CABBAGE, 1}, {ITEM_ONION, 3}, {ITEM_EGGPLANT, 3}, {ITEM_CARROT, 3},
    {ITEM_SWEET_POTATO, 2}, {ITEM_GREEN_PEPPER, 3}, {ITEM_ANY_EGG, 8},
    {ITEM_BAMBOO_SHOOT, 3}, {ITEM_MUSHROOM, 3}, {ITEM_TRUFFLE, 3}, {ITEM_SMALL_FISH, 6},
    {ITEM_MEDIUM_FISH, 5}, {ITEM_LARGE_FISH, 4}, {ITEM_SUGAR, 1}, {ITEM_SALT, 3},
    {ITEM_SOY_SAUCE, 7}, {ITEM_MISO, 5}, {0, 0},
};

static RecipeBonus cookie_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_HONEY, 2}, {0, 0},
};

static RecipeBonus chocolate_cookies_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_HONEY, 2}, {0, 0},
};

static RecipeBonus tempura_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_TURNIP, 3}, {ITEM_POTATO, 6}, {ITEM_CUCUMBER, 3}, {ITEM_CABBAGE, 4}, {ITEM_CORN, 5},
    {ITEM_ONION, 6}, {ITEM_PUMPKIN, 5}, {ITEM_PINEAPPLE, 5}, {ITEM_EGGPLANT, 7},
    {ITEM_CARROT, 5}, {ITEM_SWEET_POTATO, 6}, {ITEM_SPINACH, 5}, {ITEM_GREEN_PEPPER, 6},
    {ITEM_ANY_EGG, 4}, {ITEM_SPA_BOILED_EGG, 4}, {ITEM_BOILED_EGG, 5}, {ITEM_MUSHROOM, 6},
    {ITEM_POISONOUS_MUSHROOM, -60}, {ITEM_TRUFFLE, 2}, {ITEM_SMALL_FISH, 5},
    {ITEM_MEDIUM_FISH, 7}, {ITEM_LARGE_FISH, 6}, {ITEM_CHOCOLATE, -2}, {ITEM_WHISK, 1}, {0, 0},
};

static RecipeBonus ice_cream_bonuses[] = {
    {ITEM_MILK_S, 5}, {ITEM_MILK_M, 5}, {ITEM_MILK_L, 5}, {ITEM_MILK_G, 5},
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_STRAWBERRY, 5}, {ITEM_PINEAPPLE, 5}, {ITEM_APPLE, 5}, {ITEM_SUGDW_APPLE, 5},
    {ITEM_WILD_GRAPE, 5}, {ITEM_HONEY, 2}, {ITEM_KNIFE, 1}, {0, 0},
};

static RecipeBonus cake_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_STRAWBERRY, 5}, {ITEM_PINEAPPLE, 5}, {ITEM_APPLE, 5}, {ITEM_SUGDW_APPLE, 5},
    {ITEM_WILD_GRAPE, 5}, {ITEM_HONEY, 2}, {ITEM_KNIFE, 1}, {0, 0},
};

static RecipeBonus chocolate_cake_bonuses[] = {
    {ITEM_NORMAL_EGG, 5}, {ITEM_GOOD_EGG, 5}, {ITEM_EXCELLENT_EGG, 5}, {ITEM_GOLDEN_EGG, 5},
    {ITEM_STRAWBERRY, 5}, {ITEM_PINEAPPLE, 5}, {ITEM_APPLE, 5}, {ITEM_SUGDW_APPLE, 5},
    {ITEM_WILD_GRAPE, 5}, {ITEM_HONEY, 2}, {ITEM_KNIFE, 1}, {0, 0},
};

static RecipeBonus relaxation_tea_bonuses[] = {
    {ITEM_STRAWBERRY, 3}, {ITEM_PINEAPPLE, 3}, {ITEM_WILD_GRAPE, 3}, {ITEM_APPLE, 5},
    {ITEM_SUGDW_APPLE, 5}, {ITEM_HONEY, 5}, {ITEM_WINE, 5}, {ITEM_ANY_MILK, 5},
    {ITEM_ORANGE_CUP_FRUIT, 10}, {ITEM_BLUE_GRASS, 10}, {ITEM_GREEN_GRASS, 20},
    {ITEM_RED_GRASS, -10}, {ITEM_KNIFE, 3}, {ITEM_SUGAR, 3}, {0, 0},
};

static RecipeBonus food_fiasco_bonuses[] = {
    {0, 0},
};

Recipe recipes[RECIPE_COUNT] = {
    /* Mayonnaise S */
    {DISH_NAME(mayonnaise_s), ITEM_MAYONNAISE_S, .quality = 1, .tools = UTENSIL_FLAG_WHISK | SEASONING_FLAG_VINEGAR,
     .ingredients = {ITEM_OIL, ITEM_NORMAL_EGG}, .bonuses = mayonnaise_s_bonuses},
    /* Mayonnaise M */
    {DISH_NAME(mayonnaise_m), ITEM_MAYONNAISE_M, .quality = 2, .tools = UTENSIL_FLAG_WHISK | SEASONING_FLAG_VINEGAR,
     .ingredients = {ITEM_OIL, ITEM_GOOD_EGG}, .bonuses = mayonnaise_m_bonuses},
    /* Mayonnaise L */
    {DISH_NAME(mayonnaise_l), ITEM_MAYONNAISE_L, .quality = 3, .tools = UTENSIL_FLAG_WHISK | SEASONING_FLAG_VINEGAR,
     .ingredients = {ITEM_OIL, ITEM_EXCELLENT_EGG}, .bonuses = mayonnaise_l_bonuses},
    /* Mayonnaise XL */
    {DISH_NAME(mayonnaise_xl), ITEM_MAYONNAISE_G, .quality = 4, .tools = UTENSIL_FLAG_WHISK | SEASONING_FLAG_VINEGAR,
     .ingredients = {ITEM_OIL, ITEM_GOLDEN_EGG}, .bonuses = mayonnaise_g_bonuses},
    /* Pickles */
    {DISH_NAME(pickles), ITEM_PICKLES, .quality = 20, .tools = SEASONING_FLAG_SALT,
     .ingredients = {ITEM_CUCUMBER}, .bonuses = pickles_bonuses},
    /* Salad */
    {DISH_NAME(salad), ITEM_SALAD, .quality = 20, .tools = UTENSIL_FLAG_KNIFE,
     .ingredients = {ITEM_CUCUMBER}, .bonuses = salad_bonuses},
    {DISH_NAME(salad), ITEM_SALAD, .quality = 20, .tools = UTENSIL_FLAG_KNIFE,
     .ingredients = {ITEM_TOMATO}, .bonuses = salad_bonuses},
    {DISH_NAME(salad), ITEM_SALAD, .quality = 20, .tools = UTENSIL_FLAG_KNIFE,
     .ingredients = {ITEM_CARROT}, .bonuses = salad_bonuses},
    {DISH_NAME(salad), ITEM_SALAD, .quality = 20, .tools = UTENSIL_FLAG_KNIFE,
     .ingredients = {ITEM_CABBAGE}, .bonuses = salad_bonuses},
    /* Curry */
    {DISH_NAME(curry), ITEM_CURRY, .quality = 30, .tools = UTENSIL_FLAG_POT,
     .ingredients = {ITEM_CURRY_POWDER, ITEM_RICE_BALL}, .bonuses = curry_bonuses},
    /* Stew */
    {DISH_NAME(stew), ITEM_STEW, .quality = 30, .tools = UTENSIL_FLAG_POT | SEASONING_FLAG_SALT,
     .ingredients = {ITEM_ANY_MILK, ITEM_FLOUR}, .bonuses = stew_bonuses},
    /* Miso soup */
    {DISH_NAME(miso_soup), ITEM_MISO_SOUP, .quality = 5, .tools = UTENSIL_FLAG_POT | SEASONING_FLAG_MISO,
     .ingredients = {}, .bonuses = miso_soup_bonuses},
    /* Stir fry */
    {DISH_NAME(stir_fry), ITEM_STIR_FRY, .quality = 40, .tools = UTENSIL_FLAG_KNIFE | UTENSIL_FLAG_FRYING_PAN | SEASONING_FLAG_SOY_SAUCE,
     .ingredients = {ITEM_OIL, ITEM_CABBAGE}, .bonuses = stir_fry_bonuses},
    /* Fried rice */
    {DISH_NAME(fried_rice), ITEM_FRIED_RICE, .quality = 40, .tools = UTENSIL_FLAG_FRYING_PAN,
     .ingredients = {ITEM_RICE_BALL, ITEM_ANY_EGG, ITEM_OIL}, .bonuses = fried_rice_bonuses},
    /* Veggie pancake */
    {DISH_NAME(veggie_pancake), ITEM_VEGGIE_PANCAKE, .quality = 50, .tools = UTENSIL_FLAG_KNIFE | UTENSIL_FLAG_FRYING_PAN,
     .ingredients = {ITEM_FLOUR, ITEM_ANY_EGG, ITEM_CABBAGE, ITEM_OIL}, .bonuses = veggie_pancake_bonuses},
    /* Sandwich */
    {DISH_NAME(sandwich), ITEM_SANDWICH, .quality = 40, .tools = UTENSIL_FLAG_KNIFE,
     .ingredients = {ITEM_BREAD, ITEM_CUCUMBER}, .bonuses = sandwich_bonuses},
    {DISH_NAME(sandwich), ITEM_SANDWICH, .quality = 40, .tools = UTENSIL_FLAG_KNIFE,
     .ingredients = {ITEM_BREAD, ITEM_TOMATO}, .bonuses = sandwich_bonuses},
    {DISH_NAME(sandwich), ITEM_SANDWICH, .quality = 40, .tools = UTENSIL_FLAG_KNIFE,
     .ingredients = {ITEM_BREAD, ITEM_BOILED_EGG}, .bonuses = sandwich_bonuses},
    /* Strawberry milk */
    {DISH_NAME(strawberry_milk), ITEM_STRAWBERRY_MILK, .quality = 30, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_STRAWBERRY, ITEM_ANY_MILK}, .bonuses = strawberry_milk_bonuses},
    /* Fruit juice */
    {DISH_NAME(fruit_juice), ITEM_FRUIT_JUICE, .quality = 20, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_STRAWBERRY}, .bonuses = fruit_juice_bonuses},
    {DISH_NAME(fruit_juice), ITEM_FRUIT_JUICE, .quality = 20, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_PINEAPPLE}, .bonuses = fruit_juice_bonuses},
    {DISH_NAME(fruit_juice), ITEM_FRUIT_JUICE, .quality = 20, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_APPLE}, .bonuses = fruit_juice_bonuses},
    {DISH_NAME(fruit_juice), ITEM_FRUIT_JUICE, .quality = 20, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_SUGDW_APPLE}, .bonuses = fruit_juice_bonuses},
    {DISH_NAME(fruit_juice), ITEM_FRUIT_JUICE, .quality = 20, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_WILD_GRAPE}, .bonuses = fruit_juice_bonuses},
    /* Fruit latte */
    {DISH_NAME(fruit_latte), ITEM_FRUIT_LATTE, .quality = 30, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_PINEAPPLE, ITEM_ANY_MILK}, .bonuses = fruit_latte_bonuses},
    {DISH_NAME(fruit_latte), ITEM_FRUIT_LATTE, .quality = 30, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_APPLE, ITEM_ANY_MILK}, .bonuses = fruit_latte_bonuses},
    {DISH_NAME(fruit_latte), ITEM_FRUIT_LATTE, .quality = 30, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_SUGDW_APPLE, ITEM_ANY_MILK}, .bonuses = fruit_latte_bonuses},
    {DISH_NAME(fruit_latte), ITEM_FRUIT_LATTE, .quality = 30, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_WILD_GRAPE, ITEM_ANY_MILK}, .bonuses = fruit_latte_bonuses},
    {DISH_NAME(fruit_latte), ITEM_FRUIT_LATTE, .quality = 30, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_FRUIT_JUICE, ITEM_ANY_MILK}, .bonuses = fruit_latte_bonuses},
    /* Vegetable juice */
    {DISH_NAME(vegetable_juice), ITEM_VEGETABLE_JUICE, .quality = 20, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_CUCUMBER}, .bonuses = vegetable_juice_bonuses},
    {DISH_NAME(vegetable_juice), ITEM_VEGETABLE_JUICE, .quality = 20, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_CABBAGE}, .bonuses = vegetable_juice_bonuses},
    {DISH_NAME(vegetable_juice), ITEM_VEGETABLE_JUICE, .quality = 20, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_CARROT}, .bonuses = vegetable_juice_bonuses},
    /* Veggie latte */
    {DISH_NAME(veggie_latte), ITEM_VEGGIE_LATTE, .quality = 30, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_CUCUMBER, ITEM_ANY_MILK}, .bonuses = veggie_latte_bonuses},
    {DISH_NAME(veggie_latte), ITEM_VEGGIE_LATTE, .quality = 30, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_CABBAGE, ITEM_ANY_MILK}, .bonuses = veggie_latte_bonuses},
    {DISH_NAME(veggie_latte), ITEM_VEGGIE_LATTE, .quality = 30, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_CARROT, ITEM_ANY_MILK}, .bonuses = veggie_latte_bonuses},
    {DISH_NAME(veggie_latte), ITEM_VEGGIE_LATTE, .quality = 30, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_VEGETABLE_JUICE, ITEM_ANY_MILK}, .bonuses = veggie_latte_bonuses},
    /* Mixed juice */
    {DISH_NAME(mixed_juice), ITEM_MIXED_JUICE, .quality = 50, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_STRAWBERRY, ITEM_PINEAPPLE, ITEM_APPLE, ITEM_WILD_GRAPE, ITEM_CUCUMBER, ITEM_CABBAGE, ITEM_CARROT}, .bonuses = mixed_juice_bonuses},
    {DISH_NAME(mixed_juice), ITEM_MIXED_JUICE, .quality = 50, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_STRAWBERRY, ITEM_PINEAPPLE, ITEM_SUGDW_APPLE, ITEM_WILD_GRAPE, ITEM_CUCUMBER, ITEM_CABBAGE, ITEM_CARROT}, .bonuses = mixed_juice_bonuses},
    {DISH_NAME(mixed_juice), ITEM_MIXED_JUICE, .quality = 50, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_FRUIT_JUICE, ITEM_VEGETABLE_JUICE}, .bonuses = mixed_juice_bonuses},
    /* Mixed latte */
    {DISH_NAME(mixed_latte), ITEM_MIXED_LATTE, .quality = 60, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_STRAWBERRY, ITEM_PINEAPPLE, ITEM_APPLE, ITEM_WILD_GRAPE, ITEM_CUCUMBER, ITEM_CABBAGE, ITEM_CARROT, ITEM_ANY_MILK}, .bonuses = mixed_latte_bonuses},
    {DISH_NAME(mixed_latte), ITEM_MIXED_LATTE, .quality = 60, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_STRAWBERRY, ITEM_PINEAPPLE, ITEM_SUGDW_APPLE, ITEM_WILD_GRAPE, ITEM_CUCUMBER, ITEM_CABBAGE, ITEM_CARROT, ITEM_ANY_MILK}, .bonuses = mixed_latte_bonuses},
    {DISH_NAME(mixed_latte), ITEM_MIXED_LATTE, .quality = 60, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_FRUIT_JUICE, ITEM_VEGETABLE_JUICE, ITEM_ANY_MILK}, .bonuses = mixed_latte_2_bonuses},
    {DISH_NAME(mixed_latte), ITEM_MIXED_LATTE, .quality = 60, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_MIXED_JUICE, ITEM_ANY_MILK}, .bonuses = mixed_latte_2_bonuses},
    /* Pickled turnips */
    {DISH_NAME(pickled_turnips), ITEM_PICKLED_TURNIPS, .quality = 20, .tools = UTENSIL_FLAG_KNIFE | SEASONING_FLAG_VINEGAR,
     .ingredients = {ITEM_TURNIP}, .bonuses = pickled_turnips_bonuses},
    /* Fries */
    {DISH_NAME(fries), ITEM_FRIES, .quality = 30, .tools = UTENSIL_FLAG_KNIFE | UTENSIL_FLAG_FRYING_PAN,
     .ingredients = {ITEM_POTATO, ITEM_OIL}, .bonuses = fries_bonuses},
    /* Strawberry jam */
    {DISH_NAME(strawberry_jam), ITEM_STRAWBERRY_JAM, .quality = 5, .tools = UTENSIL_FLAG_POT | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_STRAWBERRY}, .bonuses = strawberry_jam_bonuses},
    /* Tomato juice */
    {DISH_NAME(tomato_juice), ITEM_TOMATO_JUICE, .quality = 20, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_TOMATO}, .bonuses = tomato_juice_bonuses},
    /* Ketchup */
    {DISH_NAME(ketchup), ITEM_KETCHUP, .quality = 1, .tools = UTENSIL_FLAG_MIXER | SEASONING_FLAG_SUGAR | SEASONING_FLAG_SALT | SEASONING_FLAG_VINEGAR,
     .ingredients = {ITEM_TOMATO, ITEM_ONION}, .bonuses = ketchup_bonuses},
    /* Popcorn */
    {DISH_NAME(popcorn), ITEM_POPCORN, .quality = 30, .tools = UTENSIL_FLAG_FRYING_PAN,
     .ingredients = {ITEM_CORN}, .bonuses = popcorn_bonuses},
    /* Pumpkin pudding */
    {DISH_NAME(pumpkin_pudding), ITEM_PUMPKIN_PUDDING, .quality = 50, .tools = UTENSIL_FLAG_POT | UTENSIL_FLAG_OVEN | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_PUMPKIN, ITEM_ANY_MILK, ITEM_ANY_EGG}, .bonuses = pumpkin_pudding_bonuses},
    /* Happy eggplant */
    {DISH_NAME(happy_eggplant), ITEM_HAPPY_EGGPLANT, .quality = 30, .tools = UTENSIL_FLAG_FRYING_PAN | SEASONING_FLAG_SUGAR | SEASONING_FLAG_SOY_SAUCE | SEASONING_FLAG_MISO,
     .ingredients = {ITEM_EGGPLANT}, .bonuses = happy_eggplant_bonuses},
    /* Sweet potato */
    {DISH_NAME(sweet_potato), ITEM_SWEET_POTATO_DISH, .quality = 40, .tools = UTENSIL_FLAG_POT | UTENSIL_FLAG_OVEN | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_SWEET_POTATO, ITEM_ANY_EGG, ITEM_BUTTER}, .bonuses = sweet_potato_dish_bonuses},
    /* Roasted potatoes */
    {DISH_NAME(roasted_potatoes), ITEM_ROASTED_POTATOES, .quality = 40, .tools = UTENSIL_FLAG_OVEN,
     .ingredients = {ITEM_SWEET_POTATO, ITEM_STONE}, .bonuses = roasted_potatoes_bonuses},
    /* Greens */
    {DISH_NAME(greens), ITEM_GREENS, .quality = 20, .tools = UTENSIL_FLAG_POT | SEASONING_FLAG_SOY_SAUCE,
     .ingredients = {ITEM_SPINACH}, .bonuses = greens_bonuses},
    /* Scrambled eggs */
    {DISH_NAME(scrambled_eggs), ITEM_SCRAMBLED_EGGS, .quality = 40, .tools = UTENSIL_FLAG_FRYING_PAN,
     .ingredients = {ITEM_ANY_EGG, ITEM_OIL}, .bonuses = scrambled_eggs_bonuses},
    /* Omelet */
    {DISH_NAME(omelet), ITEM_OMELET, .quality = 50, .tools = UTENSIL_FLAG_FRYING_PAN,
     .ingredients = {ITEM_ANY_EGG, ITEM_ANY_MILK, ITEM_OIL}, .bonuses = omelet_bonuses},
    /* Rice omelet */
    {DISH_NAME(rice_omelet), ITEM_RICE_OMELET, .quality = 60, .tools = UTENSIL_FLAG_FRYING_PAN,
     .ingredients = {ITEM_ANY_EGG, ITEM_ANY_MILK, ITEM_RICE_BALL, ITEM_OIL}, .bonuses = rice_omelet_bonuses},
    /* Boiled egg */
    {DISH_NAME(boiled_egg), ITEM_BOILED_EGG, .quality = 20, .tools = UTENSIL_FLAG_POT,
     .ingredients = {ITEM_ANY_EGG}, .bonuses = boiled_egg_bonuses},
    /* Hot milk */
    {DISH_NAME(hot_milk), ITEM_HOT_MILK, .quality = 20, .tools = UTENSIL_FLAG_POT,
     .ingredients = {ITEM_ANY_MILK}, .bonuses = hot_milk_bonuses},
    /* Butter */
    {DISH_NAME(butter), ITEM_BUTTER, .quality = 1, .tools = UTENSIL_FLAG_MIXER,
     .ingredients = {ITEM_ANY_MILK}, .bonuses = butter_bonuses},
    /* Cheesecake */
    {DISH_NAME(cheesecake), ITEM_CHEESECAKE, .quality = 50, .tools = UTENSIL_FLAG_POT | UTENSIL_FLAG_WHISK | UTENSIL_FLAG_OVEN | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_ANY_CHEESE, ITEM_ANY_EGG, ITEM_ANY_MILK}, .bonuses = cheesecake_bonuses},
    /* Cheese fondue */
    {DISH_NAME(cheese_fondue), ITEM_CHEESE_FONDUE, .quality = 40, .tools = UTENSIL_FLAG_POT,
     .ingredients = {ITEM_ANY_CHEESE, ITEM_BREAD}, .bonuses = cheese_fondue_bonuses},
    /* Apple pie */
    {DISH_NAME(apple_pie), ITEM_APPLE_PIE, .quality = 50, .tools = UTENSIL_FLAG_KNIFE | UTENSIL_FLAG_POT | UTENSIL_FLAG_ROLLING_PIN | UTENSIL_FLAG_OVEN | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_APPLE, ITEM_BUTTER, ITEM_ANY_EGG, ITEM_FLOUR}, .bonuses = apple_pie_bonuses},
    {DISH_NAME(apple_pie), ITEM_APPLE_PIE, .quality = 50, .tools = UTENSIL_FLAG_KNIFE | UTENSIL_FLAG_POT | UTENSIL_FLAG_ROLLING_PIN | UTENSIL_FLAG_OVEN | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_SUGDW_APPLE, ITEM_BUTTER, ITEM_ANY_EGG, ITEM_FLOUR}, .bonuses = apple_pie_bonuses},
    /* Apple jam */
    {DISH_NAME(apple_jam), ITEM_APPLE_JAM, .quality = 5, .tools = UTENSIL_FLAG_POT | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_APPLE}, .bonuses = apple_jam_bonuses},
    {DISH_NAME(apple_jam), ITEM_APPLE_JAM, .quality = 5, .tools = UTENSIL_FLAG_POT | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_SUGDW_APPLE}, .bonuses = apple_jam_bonuses},
    /* Bamboo rice */
    {DISH_NAME(bamboo_rice), ITEM_BAMBOO_RICE, .quality = 30, .tools = 0,
     .ingredients = {ITEM_BAMBOO_SHOOT, ITEM_RICE_BALL}, .bonuses = bamboo_rice_bonuses},
    /* Grape jam */
    {DISH_NAME(grape_jam), ITEM_GRAPE_JAM, .quality = 5, .tools = UTENSIL_FLAG_POT | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_WILD_GRAPE}, .bonuses = grape_jam_bonuses},
    /* Mushroom rice */
    {DISH_NAME(mushroom_rice), ITEM_MUSHROOM_RICE, .quality = 30, .tools = 0,
     .ingredients = {ITEM_MUSHROOM, ITEM_RICE_BALL}, .bonuses = mushroom_rice_bonuses},
    /* Truffle rice */
    {DISH_NAME(truffle_rice), ITEM_TRUFFLE_RICE, .quality = 60, .tools = 0,
     .ingredients = {ITEM_TRUFFLE, ITEM_RICE_BALL}, .bonuses = truffle_rice_bonuses},
    /* Sushi */
    {DISH_NAME(sushi), ITEM_SUSHI, .quality = 40, .tools = SEASONING_FLAG_VINEGAR,
     .ingredients = {ITEM_RICE_BALL, ITEM_SASHIMI}, .bonuses = sushi_bonuses},
    /* Jam bun */
    {DISH_NAME(jam_bun), ITEM_JAM_BUN, .quality = 40, .tools = 0,
     .ingredients = {ITEM_BREAD, ITEM_APPLE_JAM}, .bonuses = jam_bun_bonuses},
    {DISH_NAME(jam_bun), ITEM_JAM_BUN, .quality = 40, .tools = 0,
     .ingredients = {ITEM_BREAD, ITEM_GRAPE_JAM}, .bonuses = jam_bun_bonuses},
    {DISH_NAME(jam_bun), ITEM_JAM_BUN, .quality = 40, .tools = 0,
     .ingredients = {ITEM_BREAD, ITEM_STRAWBERRY_JAM}, .bonuses = jam_bun_bonuses},
    /* Dinner roll */
    {DISH_NAME(dinner_roll), ITEM_DINNER_ROLL, .quality = 30, .tools = 0,
     .ingredients = {ITEM_BREAD, ITEM_BUTTER}, .bonuses = dinner_roll_bonuses},
    /* Raisin bread */
    {DISH_NAME(raisin_bread), ITEM_RAISIN_BREAD, .quality = 30, .tools = 0,
     .ingredients = {ITEM_BREAD, ITEM_WILD_GRAPE}, .bonuses = raisin_bread_bonuses},
    /* Sashimi */
    {DISH_NAME(sashimi), ITEM_SASHIMI, .quality = 30, .tools = UTENSIL_FLAG_KNIFE,
     .ingredients = {ITEM_MEDIUM_FISH}, .bonuses = sashimi_bonuses},
    {DISH_NAME(sashimi), ITEM_SASHIMI, .quality = 30, .tools = UTENSIL_FLAG_KNIFE,
     .ingredients = {ITEM_LARGE_FISH}, .bonuses = sashimi_bonuses},
    /* Grilled fish */
    {DISH_NAME(grilled_fish), ITEM_GRILLED_FISH, .quality = 30, .tools = UTENSIL_FLAG_FRYING_PAN,
     .ingredients = {ITEM_MEDIUM_FISH}, .bonuses = grilled_fish_bonuses},
    /* Chirashi sushi */
    {DISH_NAME(chirashi_sushi), ITEM_CHIRASHI_SUSHI, .quality = 50, .tools = UTENSIL_FLAG_KNIFE | SEASONING_FLAG_VINEGAR,
     .ingredients = {ITEM_RICE_BALL, ITEM_SCRAMBLED_EGGS, ITEM_SASHIMI}, .bonuses = chirashi_sushi_bonuses},
    /* Pizza */
    {DISH_NAME(pizza), ITEM_PIZZA, .quality = 40, .tools = UTENSIL_FLAG_ROLLING_PIN | UTENSIL_FLAG_OVEN,
     .ingredients = {ITEM_FLOUR, ITEM_ANY_CHEESE, ITEM_KETCHUP}, .bonuses = pizza_bonuses},
    /* Noodles */
    {DISH_NAME(noodles), ITEM_NOODLES, .quality = 40, .tools = UTENSIL_FLAG_KNIFE | UTENSIL_FLAG_POT | UTENSIL_FLAG_ROLLING_PIN,
     .ingredients = {ITEM_FLOUR}, .bonuses = noodles_bonuses},
    /* Curry noodles */
    {DISH_NAME(curry_noodles), ITEM_CURRY_NOODLES, .quality = 60, .tools = UTENSIL_FLAG_POT,
     .ingredients = {ITEM_NOODLES, ITEM_CURRY_POWDER}, .bonuses = curry_noodles_bonuses},
    {DISH_NAME(curry_noodles), ITEM_CURRY_NOODLES, .quality = 60, .tools = UTENSIL_FLAG_KNIFE | UTENSIL_FLAG_POT | UTENSIL_FLAG_ROLLING_PIN,
     .ingredients = {ITEM_FLOUR, ITEM_CURRY_POWDER}, .bonuses = curry_noodles_bonuses},
    /* Tempura noodles */
    {DISH_NAME(tempura_noodles), ITEM_TEMPURA_NOODLES, .quality = 60, .tools = UTENSIL_FLAG_KNIFE | UTENSIL_FLAG_POT | UTENSIL_FLAG_ROLLING_PIN,
     .ingredients = {ITEM_FLOUR, ITEM_TEMPURA}, .bonuses = tempura_noodles_bonuses},
    {DISH_NAME(tempura_noodles), ITEM_TEMPURA_NOODLES, .quality = 60, .tools = UTENSIL_FLAG_POT,
     .ingredients = {ITEM_NOODLES, ITEM_TEMPURA}, .bonuses = tempura_noodles_bonuses},
    /* Fried noodles */
    {DISH_NAME(fried_noodles), ITEM_FRIED_NOODLES, .quality = 60, .tools = UTENSIL_FLAG_FRYING_PAN,
     .ingredients = {ITEM_NOODLES, ITEM_OIL}, .bonuses = fried_noodles_bonuses},
    /* Cookie */
    {DISH_NAME(cookie), ITEM_COOKIE, .quality = 15, .tools = UTENSIL_FLAG_ROLLING_PIN | UTENSIL_FLAG_OVEN | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_FLOUR, ITEM_BUTTER, ITEM_ANY_EGG}, .bonuses = cookie_bonuses},
    /* Chocolate cookie */
    {DISH_NAME(chocolate_cookie), ITEM_CHOCOLATE_COOKIES, .quality = 30, .tools = UTENSIL_FLAG_ROLLING_PIN | UTENSIL_FLAG_OVEN | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_FLOUR, ITEM_BUTTER, ITEM_ANY_EGG, ITEM_CHOCOLATE}, .bonuses = chocolate_cookies_bonuses},
    {DISH_NAME(chocolate_cookie), ITEM_CHOCOLATE_COOKIES, .quality = 30, .tools = 0,
     .ingredients = {ITEM_CHOCOLATE, ITEM_COOKIE}, .bonuses = chocolate_cookies_bonuses},
    /* Tempura */
    {DISH_NAME(tempura), ITEM_TEMPURA, .quality = 20, .tools = UTENSIL_FLAG_FRYING_PAN,
     .ingredients = {ITEM_FLOUR, ITEM_ANY_EGG, ITEM_OIL}, .bonuses = tempura_bonuses},
    /* Ice cream */
    {DISH_NAME(ice_cream), ITEM_ICE_CREAM, .quality = 30, .tools = UTENSIL_FLAG_POT | UTENSIL_FLAG_WHISK | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_ANY_MILK, ITEM_ANY_EGG}, .bonuses = ice_cream_bonuses},
    /* Cake */
    {DISH_NAME(cake), ITEM_CAKE, .quality = 40, .tools = UTENSIL_FLAG_WHISK | UTENSIL_FLAG_OVEN | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_FLOUR, ITEM_BUTTER, ITEM_ANY_EGG}, .bonuses = cake_bonuses},
    /* Chocolate cake */
    {DISH_NAME(chocolate_cake), ITEM_CHOCOLATE_CAKE, .quality = 70, .tools = UTENSIL_FLAG_WHISK | UTENSIL_FLAG_OVEN | SEASONING_FLAG_SUGAR,
     .ingredients = {ITEM_FLOUR, ITEM_BUTTER, ITEM_ANY_EGG, ITEM_CHOCOLATE}, .bonuses = chocolate_cake_bonuses},
    /* Relaxation tea */
    {DISH_NAME(relaxation_tea), ITEM_RELAXATION_TEA, .quality = 70, .tools = UTENSIL_FLAG_POT,
     .ingredients = {ITEM_RELAXATION_TEA_LEAVES}, .bonuses = relaxation_tea_bonuses},
    /* Food fiasco */
    {DISH_NAME(food_fiasco), ITEM_FOOD_FIASCO, .quality = COOKING_FIASCO_QUALITY, .tools = 0,
     .ingredients = {}, .bonuses = food_fiasco_bonuses},
};

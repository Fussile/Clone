#pragma once
#include "CoreMinimal.h"

struct FSurvivalGearDefinition
{
    const TCHAR* Id;
    const TCHAR* Label;
    const TCHAR* Slot;
    int32 LootValue;
    const TCHAR* MeshOverride = nullptr;
};

inline constexpr FSurvivalGearDefinition SurvivalGear[] = {
    {TEXT("caraqule_military_hat_ushanka"), TEXT("Ushanka"), TEXT("Helmet"), 22, TEXT("russian_soviet_helmet")},
    {TEXT("fur_hooded_parka_coat"), TEXT("Fur-lined parka"), TEXT("Torso"), 36, TEXT("bomber_jacket")},
    {TEXT("tactical_vest_pack"), TEXT("Tactical vest"), TEXT("Vest"), 60, TEXT("modern_tactical_vest_plate_carrier")},
    {TEXT("black_jeans"), TEXT("Black jeans"), TEXT("Legs"), 16},
    {TEXT("body_armor_flak_jacket_remaster_pack"), TEXT("Flak jacket"), TEXT("Vest"), 54},
    {TEXT("bomber_jacket"), TEXT("Bomber jacket"), TEXT("Torso"), 28},
    {TEXT("clothes_jackets_pack"), TEXT("Field jacket"), TEXT("Torso"), 25},
    {TEXT("clothing_and_character_kit_1_0_cc0"), TEXT("Field outfit"), TEXT("Body"), 42},
    {TEXT("fleece_jacket"), TEXT("Fleece jacket"), TEXT("Torso"), 21},
    {TEXT("free_shirt_sleeved"), TEXT("Long-sleeve shirt"), TEXT("Torso"), 14},
    {TEXT("game_res_us_army_jacket"), TEXT("Army jacket"), TEXT("Torso"), 31},
    {TEXT("gas_mask_and_helmet_easterneuropean"), TEXT("Helmet with gas mask"), TEXT("Helmet"), 65},
    {TEXT("gas_mask_scary"), TEXT("Full-face gas mask"), TEXT("Face"), 52},
    {TEXT("gas_maskrespirator"), TEXT("Respirator"), TEXT("Face"), 35},
    {TEXT("gp_5_soviet_russian_gas_mask"), TEXT("GP-5 gas mask"), TEXT("Face"), 45},
    {TEXT("hard_hat"), TEXT("Hard hat"), TEXT("Helmet"), 17},
    {TEXT("ir_binoculars"), TEXT("Infrared binoculars"), TEXT("Eyes"), 65},
    {TEXT("male_jeans"), TEXT("Blue jeans"), TEXT("Legs"), 17},
    {TEXT("mich_helmet_v1_full_setup_black"), TEXT("MICH helmet"), TEXT("Helmet"), 62},
    {TEXT("miners_helmetsafety_helmet_with_light"), TEXT("Miner’s helmet"), TEXT("Helmet"), 33},
    {TEXT("modern_tactical_vest_plate_carrier"), TEXT("Plate carrier"), TEXT("Vest"), 68},
    {TEXT("motorcycle_helmet_full_face"), TEXT("Motorcycle helmet"), TEXT("Helmet"), 45},
    {TEXT("nvg_phosphor"), TEXT("Night-vision goggles"), TEXT("Eyes"), 95},
    {TEXT("police_sam_browne_gun_belt"), TEXT("Duty belt"), TEXT("Belt"), 26},
    {TEXT("polo_shirt"), TEXT("Polo shirt"), TEXT("Torso"), 12},
    {TEXT("poncho"), TEXT("Poncho"), TEXT("Torso"), 25},
    {TEXT("prom_suit"), TEXT("Formal suit"), TEXT("Body"), 27, TEXT("ww2_soviet_uniform")},
    {TEXT("quad_night_vision_goggle"), TEXT("Quad night-vision goggles"), TEXT("Eyes"), 110},
    {TEXT("russian_soviet_helmet"), TEXT("Soviet helmet"), TEXT("Helmet"), 37},
    {TEXT("tactical_clothing_asset_pack"), TEXT("Tactical uniform"), TEXT("Body"), 92},
    {TEXT("tactical_gloves_3d_model"), TEXT("Tactical gloves"), TEXT("Hands"), 31},
    {TEXT("tactical_modern_helmet_w_nvgs"), TEXT("Helmet with night vision"), TEXT("Helmet"), 117},
    {TEXT("team_wendy_exfil_helmet_camo"), TEXT("EXFIL camouflage helmet"), TEXT("Helmet"), 83},
    {TEXT("vetements_cargo_jeans_wide_leg_track_pants"), TEXT("Wide-leg cargo trousers"), TEXT("Legs"), 29},
    {TEXT("ww2_soviet_uniform"), TEXT("WWII uniform"), TEXT("Body"), 48},
};

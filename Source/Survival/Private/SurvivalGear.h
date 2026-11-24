#pragma once
#include "CoreMinimal.h"

struct FSurvivalGearDefinition
{
    const TCHAR* Id;
    const TCHAR* Label;
    const TCHAR* Slot;
    const TCHAR* MeshOverride = nullptr;
};

inline constexpr FSurvivalGearDefinition SurvivalGear[] = {
    {TEXT("caraqule_military_hat_ushanka"), TEXT("Ushanka"), TEXT("Helmet"), TEXT("russian_soviet_helmet")},
    {TEXT("fur_hooded_parka_coat"), TEXT("Fur-lined parka"), TEXT("Torso"), TEXT("bomber_jacket")},
    {TEXT("tactical_vest_pack"), TEXT("Tactical vest"), TEXT("Vest"), TEXT("modern_tactical_vest_plate_carrier")},
    {TEXT("black_jeans"), TEXT("Black jeans"), TEXT("Legs")},
    {TEXT("body_armor_flak_jacket_remaster_pack"), TEXT("Flak jacket"), TEXT("Vest")},
    {TEXT("bomber_jacket"), TEXT("Bomber jacket"), TEXT("Torso")},
    {TEXT("clothes_jackets_pack"), TEXT("Field jacket"), TEXT("Torso")},
    {TEXT("clothing_and_character_kit_1_0_cc0"), TEXT("Field outfit"), TEXT("Body")},
    {TEXT("fleece_jacket"), TEXT("Fleece jacket"), TEXT("Torso")},
    {TEXT("free_shirt_sleeved"), TEXT("Long-sleeve shirt"), TEXT("Torso")},
    {TEXT("game_res_us_army_jacket"), TEXT("Army jacket"), TEXT("Torso")},
    {TEXT("gas_mask_and_helmet_easterneuropean"), TEXT("Helmet with gas mask"), TEXT("Helmet")},
    {TEXT("gas_mask_scary"), TEXT("Full-face gas mask"), TEXT("Face")},
    {TEXT("gas_maskrespirator"), TEXT("Respirator"), TEXT("Face")},
    {TEXT("gp_5_soviet_russian_gas_mask"), TEXT("GP-5 gas mask"), TEXT("Face")},
    {TEXT("hard_hat"), TEXT("Hard hat"), TEXT("Helmet")},
    {TEXT("ir_binoculars"), TEXT("Infrared binoculars"), TEXT("Eyes")},
    {TEXT("male_jeans"), TEXT("Blue jeans"), TEXT("Legs")},
    {TEXT("mich_helmet_v1_full_setup_black"), TEXT("MICH helmet"), TEXT("Helmet")},
    {TEXT("miners_helmetsafety_helmet_with_light"), TEXT("Miner’s helmet"), TEXT("Helmet")},
    {TEXT("modern_tactical_vest_plate_carrier"), TEXT("Plate carrier"), TEXT("Vest")},
    {TEXT("motorcycle_helmet_full_face"), TEXT("Motorcycle helmet"), TEXT("Helmet")},
    {TEXT("nvg_phosphor"), TEXT("Night-vision goggles"), TEXT("Eyes")},
    {TEXT("police_sam_browne_gun_belt"), TEXT("Duty belt"), TEXT("Belt")},
    {TEXT("polo_shirt"), TEXT("Polo shirt"), TEXT("Torso")},
    {TEXT("poncho"), TEXT("Poncho"), TEXT("Torso")},
    {TEXT("prom_suit"), TEXT("Formal suit"), TEXT("Body"), TEXT("ww2_soviet_uniform")},
    {TEXT("quad_night_vision_goggle"), TEXT("Quad night-vision goggles"), TEXT("Eyes")},
    {TEXT("russian_soviet_helmet"), TEXT("Soviet helmet"), TEXT("Helmet")},
    {TEXT("tactical_clothing_asset_pack"), TEXT("Tactical uniform"), TEXT("Body")},
    {TEXT("tactical_gloves_3d_model"), TEXT("Tactical gloves"), TEXT("Hands")},
    {TEXT("tactical_modern_helmet_w_nvgs"), TEXT("Helmet with night vision"), TEXT("Helmet")},
    {TEXT("team_wendy_exfil_helmet_camo"), TEXT("EXFIL camouflage helmet"), TEXT("Helmet")},
    {TEXT("vetements_cargo_jeans_wide_leg_track_pants"), TEXT("Wide-leg cargo trousers"), TEXT("Legs")},
    {TEXT("ww2_soviet_uniform"), TEXT("WWII uniform"), TEXT("Body")},
};

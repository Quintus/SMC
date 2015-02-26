/***************************************************************************
 * savegame.cpp  -  Savegame handler
 *
 * Copyright © 2003 - 2011 Florian Richter
 * Copyright © 2013 - 2014 The TSC Contributors
 ***************************************************************************/
/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "savegame.hpp"
#include "savegame_loader.hpp"
#include "preferences.hpp"
#include "../core/game_core.hpp"
#include "../core/obj_manager.hpp"
#include "../core/errors.hpp"
#include "../level/level.hpp"
#include "../overworld/world_manager.hpp"
#include "../level/level_player.hpp"
#include "../overworld/overworld.hpp"
#include "../core/i18n.hpp"
#include "../core/filesystem/filesystem.hpp"
#include "../core/filesystem/resource_manager.hpp"
#include "../core/filesystem/package_manager.hpp"
#include "../scripting/events/level_load_event.hpp"
#include "../scripting/events/level_save_event.hpp"
#include "../core/global_basic.hpp"
#include "../audio/audio.hpp"
#include "../enemies/army.hpp"

using namespace std;


namespace fs = boost::filesystem;

namespace TSC {

/* *** *** *** *** *** *** *** cSave_Overworld_Waypoint *** *** *** *** *** *** *** *** *** *** */

cSave_Overworld_Waypoint::cSave_Overworld_Waypoint(void)
{
    m_access = 0;
}

cSave_Overworld_Waypoint::~cSave_Overworld_Waypoint(void)
{

}

/* *** *** *** *** *** *** *** cSave_Overworld *** *** *** *** *** *** *** *** *** *** */

cSave_Overworld::cSave_Overworld(void)
{

}

cSave_Overworld::~cSave_Overworld(void)
{
    for (unsigned int i = 0; i < m_waypoints.size(); i++) {
        delete m_waypoints[i];
    }

    m_waypoints.clear();
}

/* *** *** *** *** *** *** *** cSave_Level_Object *** *** *** *** *** *** *** *** *** *** */

cSave_Level_Object_Property::cSave_Level_Object_Property(const std::string& new_name /* = "" */, const std::string& new_value /* = "" */)
{
    m_name = new_name;
    m_value = new_value;
}

/* *** *** *** *** *** *** *** cSave_Level_Object *** *** *** *** *** *** *** *** *** *** */

cSave_Level_Object::cSave_Level_Object(void)
{
    m_type = TYPE_UNDEFINED;
}

cSave_Level_Object::~cSave_Level_Object(void)
{
    m_properties.clear();
}

bool cSave_Level_Object::exists(const std::string& val_name)
{
    for (Save_Level_Object_ProprtyList::iterator itr = m_properties.begin(); itr != m_properties.end(); ++itr) {
        cSave_Level_Object_Property obj = (*itr);

        if (obj.m_name.compare(val_name) == 0) {
            // found
            return 1;
        }
    }

    // not found
    return 0;
}

std::string cSave_Level_Object::Get_Value(const std::string& val_name)
{
    for (Save_Level_Object_ProprtyList::iterator itr = m_properties.begin(); itr != m_properties.end(); ++itr) {
        cSave_Level_Object_Property obj = (*itr);

        if (obj.m_name.compare(val_name) == 0) {
            // found
            return obj.m_value;
        }
    }

    // not found
    return "";
}

/* *** *** *** *** *** *** *** cSave_Level *** *** *** *** *** *** *** *** *** *** */

cSave_Level::cSave_Level(void)
{
    // level
    m_active = false;
    m_level_pos_x = 0.0f;
    m_level_pos_y = 0.0f;
}

cSave_Level::~cSave_Level(void)
{
    for (cSprite_List::iterator itr = m_regular_objects.begin(); itr != m_regular_objects.end(); ++itr) {
        delete *itr;
    }

    m_regular_objects.clear();

    for (cSprite_List::iterator itr = m_spawned_objects.begin(); itr != m_spawned_objects.end(); ++itr) {
        delete *itr;
    }

    m_spawned_objects.clear();
}

cSave_Level::Save_To_Node(xmlpp::Element* p_parent_node)
{
    cSprite_List::const_iterator iter;

    // <level>
    xmlpp::Element* p_node = p_parent_node->add_child("level");
    Add_Property(p_node, "level_name", m_name);

    // Player position. Only save that for the active level.
    if (m_active) {
        Add_Property(p_node, "player_posx", p_level->m_level_pos_x);
        Add_Property(p_node, "player_posy", p_level->m_level_pos_y);
    }

    /* Custom data a script writer wants to store; empty if the
     * script writer didn’t hook into the on_load and on_save
     * events. */
    if (!m_mruby_data.empty())
        Add_Property(p_node, "mruby_data", p_level->m_mruby_data);

    // The regular objects.
    // <object_data>
    xmlpp::Element* p_objects_data_node->add_child("objects_data");
    for(iter=m_regular_objects.begin(); iter != m_regular_objects.end(); iter++) {
        cSprite* p_sprite = (*iter);

        /* TODO: Have the sprite itself decide whether it wants to be
         * saved or not by not adding anything to the node and
         * returning false from Save_To_Savegame_XML_Node(). For now,
         * just assume TYPE_UNDEFINED sprites, i.e. all static
         * nonmoving sprites, do not need to be saved. */
        if (p_sprite->m_type != TYPE_UNDEFINED) {
            xmlpp::Element* p_object_node = p_objects_data_node->add_child("object");
            p_sprite->Save_To_Savegame_XML_Node(p_object_node);
        }
    }

    // The spawned objects. These have always to be saved.
    // <spawned_objects>
    xmlpp::Element* p_spawned_node = p_node->add_child("spawned_objects");
    for(iter=m_spawned_objects.begin(); iter != m_spawned_objects.end(); iter++) {
        cSprite* p_sprite = (*iter);
        p_sprite->Save_To_XML_Node(p_spawned_node);
    }
    // </spawned_objects>

    //</level>
}

/* *** *** *** *** *** cSave_Player_Return_Entry *** *** *** *** *** *** *** *** */
cSave_Player_Return_Entry::cSave_Player_Return_Entry(const std::string& level, const std::string& entry) :
    m_level(level), m_entry(entry)
{
}

/* *** *** *** *** *** *** *** cSave *** *** *** *** *** *** *** *** *** *** */

cSave::cSave(void)
{
    Init();
}

cSave::~cSave(void)
{
    for (Save_LevelList::iterator itr = m_levels.begin(); itr != m_levels.end(); ++itr) {
        delete *itr;
    }

    m_levels.clear();

    for (Save_OverworldList::iterator itr = m_overworlds.begin(); itr != m_overworlds.end(); ++itr) {
        delete *itr;
    }

    m_overworlds.clear();
}

void cSave::Init(void)
{
    // save
    m_save_time = 0;
    m_version = 0;

    // player
    m_lives = 0;
    m_points = 0;
    m_goldpieces = 0;
    m_player_type = 0;
    m_player_state = 0;
    m_itembox_item = 0;
    m_invincible = 0;
    m_invincible_star = 0;
    m_ghost_time = 0;
    m_ghost_time_mod = 0;

    //Player state (ie power ups)
    m_player_type = 0;
    m_player_type_temp_power = 0;

    // level
    m_level_time = 0;
    // set earliest available version
    m_level_engine_version = 39;

    // overworld
    m_overworld_current_waypoint = 0;
}

cSave* cSave::Load_From_File(fs::path filepath)
{
    debug_print("Loading savegame file '%s'\n", path_to_utf8(filepath).c_str());

    cSavegameLoader loader;
    loader.parse_file(filepath);
    return loader.Get_Save();
}

std::string cSave::Get_Active_Level(void)
{
    if (m_levels.empty()) {
        return "";
    }

    for (Save_LevelList::iterator itr = m_levels.begin(); itr != m_levels.end(); ++itr) {
        cSave_Level* save_level = (*itr);

        if (pLevel_Manager->Get_Path(save_level->m_name).empty()) {
            continue;
        }

        // if active level
        if (!Is_Float_Equal(save_level->m_level_pos_x, 0.0f) && !Is_Float_Equal(save_level->m_level_pos_y, 0.0f)) {
            return save_level->m_name;
        }
    }

    return "";
}

void cSave::Write_To_File(fs::path filepath)
{
    xmlpp::Document doc;
    xmlpp::Element* p_root = doc.create_root_node("savegame");
    xmlpp::Element* p_node = NULL;

    // <information>
    p_node = p_root->add_child("information");
    Add_Property(p_node, "version", m_version);
    Add_Property(p_node, "level_engine_version", m_level_engine_version);
    Add_Property(p_node, "save_time", static_cast<Uint64>(m_save_time));
    Add_Property(p_node, "description", m_description);
    // </information>

    // <player>
    p_node = p_root->add_child("player");
    Add_Property(p_node, "lives", m_lives);
    Add_Property(p_node, "points", m_points);
    Add_Property(p_node, "goldpieces", m_goldpieces);
    Add_Property(p_node, "type", m_player_type);
    Add_Property(p_node, "type_temp_power", m_player_type_temp_power);
    Add_Property(p_node, "invincible_star", m_invincible_star);
    Add_Property(p_node, "invincible", m_invincible);
    Add_Property(p_node, "ghost_time", m_ghost_time);
    Add_Property(p_node, "ghost_time_mod", m_ghost_time_mod);

    Add_Property(p_node, "state", m_player_state);
    Add_Property(p_node, "itembox_item", m_itembox_item);
    // if a level is available
    if (!m_levels.empty())
        Add_Property(p_node, "level_time", m_level_time);
    Add_Property(p_node, "overworld_active", m_overworld_active);
    Add_Property(p_node, "overworld_current_waypoint", m_overworld_current_waypoint);
    // </player>

    // player return stack
    std::vector<cSave_Player_Return_Entry>::const_iterator return_iter;
    for (return_iter = m_return_entries.begin(); return_iter != m_return_entries.end(); return_iter++) {
        cSave_Player_Return_Entry item = *return_iter;

        p_node = p_root->add_child("return");
        if (!item.m_level.empty())
            Add_Property(p_node, "level", item.m_level);
        if (!item.m_entry.empty())
            Add_Property(p_node, "entry", item.m_entry);
    }

    // levels
    Save_LevelList::const_iterator iter;
    for (iter=m_levels.begin(); iter != m_levels.end(); iter++) {
        cSave_Level* p_level = *iter;
        p_level->Save_To_Node(p_root);
    }

    // Overworlds
    Save_OverworldList::const_iterator oiter;
    for (oiter=m_overworlds.begin(); oiter != m_overworlds.end(); oiter++) {
        cSave_Overworld* p_overworld = *oiter;

        // <overworld>
        p_node = p_root->add_child("overworld");
        Add_Property(p_node, "name", p_overworld->m_name);

        Save_Overworld_WaypointList::const_iterator wpiter;
        for (wpiter=p_overworld->m_waypoints.begin(); wpiter != p_overworld->m_waypoints.end(); wpiter++) {
            cSave_Overworld_Waypoint* p_wp = *wpiter;

            // skip empty waypoints
            if (p_wp->m_destination.empty())
                continue;

            // <waypoint>
            xmlpp::Element* p_waypoint_node = p_node->add_child("waypoint");
            Add_Property(p_waypoint_node, "destination", p_wp->m_destination);
            Add_Property(p_waypoint_node, "access", p_wp->m_access);
            // </waypoint>
        }

        // </overworld>
    }

    // Write to file (raises xmlpp::exception on error)
    doc.write_to_file_formatted(Glib::filename_from_utf8(path_to_utf8(filepath)));
    debug_print("Wrote savegame file '%s'.\n", path_to_utf8(filepath).c_str());
}

/* *** *** *** *** *** *** *** cSavegame *** *** *** *** *** *** *** *** *** *** */

cSavegame::cSavegame(void)
{
    m_savegame_dir = pResource_Manager->Get_User_Savegame_Directory();
}

cSavegame::~cSavegame(void)
{
    //
}

int cSavegame::Load_Game(unsigned int save_slot)
{
    cSave* savegame = Load(save_slot);

    if (!savegame) {
        return 0;
    }

    // check if unsupported save
    if (savegame->m_version <= SAVEGAME_VERSION_UNSUPPORTED) {
        cerr << "Warning : Savegame " << save_slot << " : Versions " << SAVEGAME_VERSION_UNSUPPORTED << " and below are unsupported" << endl;
    }

    // reset custom level mode type
    if (Game_Mode_Type == MODE_TYPE_LEVEL_CUSTOM) {
        Game_Mode_Type = MODE_TYPE_DEFAULT;
    }

    // #### Overworld ####

    // set overworld progress
    if (!savegame->m_overworlds.empty()) {
        for (Save_OverworldList::iterator itr = savegame->m_overworlds.begin(); itr != savegame->m_overworlds.end(); ++itr) {
            // get savegame overworld pointer
            cSave_Overworld* save_overworld = (*itr);

            // get overworld
            cOverworld* overworld = pOverworld_Manager->Get_from_Name(save_overworld->m_name);

            if (!overworld) {
                cerr << "Warning : Savegame " << save_slot << " : Overworld " << save_overworld->m_name << " not found" << endl;
                continue;
            }

            for (Save_Overworld_WaypointList::iterator wp_itr = save_overworld->m_waypoints.begin(); wp_itr != save_overworld->m_waypoints.end(); ++wp_itr) {
                // get savegame waypoint pointer
                cSave_Overworld_Waypoint* save_waypoint = (*wp_itr);

                // get overworld waypoint
                cWaypoint* waypoint = overworld->Get_Waypoint(overworld->Get_Waypoint_Num(save_waypoint->m_destination));

                // not found
                if (!waypoint) {
                    cerr << "Warning : Savegame " << save_slot << " : Overworld " << save_overworld->m_name << " Waypoint " << save_waypoint->m_destination << " not found" << endl;
                    continue;
                }

                // set access
                waypoint->Set_Access(save_waypoint->m_access);
            }
        }
    }

    // if an overworld is active
    if (!savegame->m_overworld_active.empty()) {
        // Set active overworld
        if (!pOverworld_Manager->Set_Active(savegame->m_overworld_active)) {
            cerr << "Warning : Savegame " << save_slot << " : Couldn't set Overworld active " << savegame->m_overworld_active << endl;
        }

        // Current waypoint
        if (!pOverworld_Player->Set_Waypoint(savegame->m_overworld_current_waypoint)) {
            cerr << "Warning : Savegame " << save_slot << " : Overworld Current Waypoint " << savegame->m_overworld_current_waypoint << " is invalid" << endl;
        }
    }
    // overworld is not active
    else {
        // Set custom level mode
        Game_Mode_Type = MODE_TYPE_LEVEL_CUSTOM;
    }

    // #### Level ####

    // default is world savegame
    unsigned int save_type = 2;

    // load levels
    if (!savegame->m_levels.empty()) {
        for (Save_LevelList::iterator itr = savegame->m_levels.begin(); itr != savegame->m_levels.end(); ++itr) {
            cSave_Level* save_level = (*itr);
            cLevel* level = pLevel_Manager->Load(save_level->m_name);

            if (!level) {
                cerr << "Warning : Couldn't load Savegame Level " << save_level->m_name << endl;
                continue;
            }

            // active level
            if (!Is_Float_Equal(save_level->m_level_pos_x, 0.0f) && !Is_Float_Equal(save_level->m_level_pos_y, 0.0f)) {
                pLevel_Manager->Set_Active(level);
                level->Init();

                // if below version 9 : move y coordinate bottom to 0 and remove screen height adjustment
                if (savegame->m_version < 9) {
                    save_level->m_level_pos_y -= 1200.0f;
                }

                // time
                pHud_Time->Set_Time(savegame->m_level_time);
                
                // position
                pLevel_Player->Set_Pos(save_level->m_level_pos_x, save_level->m_level_pos_y);           

                // level savegame
                save_type = 1;
            }

            // spawned objects
            for (cSprite_List::iterator itr = save_level->m_spawned_objects.begin(); itr != save_level->m_spawned_objects.end(); ++itr) {
                cSprite* sprite = (*itr);

                sprite->Set_Sprite_Manager(level->m_sprite_manager);
                sprite->Set_Spawned(1);
                level->m_sprite_manager->Add(sprite);
            }

            save_level->m_spawned_objects.clear();

            // objects data
            for (Save_Level_ObjectList::iterator itr = save_level->m_level_objects.begin(); itr != save_level->m_level_objects.end(); ++itr) {
                cSave_Level_Object* save_object = (*itr);

                // get position
                int posx = string_to_int(save_object->Get_Value("posx"));
                int posy = string_to_int(save_object->Get_Value("posy"));

                // get level object
                int checkPosition = 1;

                /*The Get_from_Position method below searches for the saved object in the level definition using its original position information.
                Loose shells will slightly have their current position offset based on their image during the initialization process during level loading
                Only require the original position field be checked for them.*/
                if (save_object->m_type == TYPE_SHELL ) {
                    checkPosition = 0;
                }

                cSprite* level_object = level->m_sprite_manager->Get_from_Position(posx, posy, save_object->m_type, checkPosition);

                // if not anymore available
                if (!level_object) {
                    cerr << "Warning : Savegame object type " << save_object->m_type << " on x " << posx << ", y " << posy << " not available" << endl;
                    continue;
                }

                level_object->Load_From_Savegame(save_object);
                //If the currently loaded object is a shell (loose or with army in it) and if it was linked, call the Get_Item
                //method to properly set it up with the player
                if (save_object ->m_type == TYPE_SHELL  || save_object -> m_type == TYPE_ARMY) {
                    cArmy* army = static_cast<cArmy*>(level_object);
                    if (army ->m_state == STA_OBJ_LINKED) {
                        pLevel_Player ->Get_Item(army ->m_type, false, army);
                    }
                }
            }

            // Feed the data stored by the save event back
            // to the load event. pSavegame holds the event
            // table required for this.
            Scripting::cLevel_Load_Event evt(save_level->m_mruby_data);
            evt.Fire(level->m_mruby, pSavegame);
        }
    }

    // #### Player ####
    //Note: Some of these steps (ie invincibility) have to be done after level -> Init() is called above.
    //Otherwise, it will wipe them out.

    // below version 8 the state was the type
    if (savegame->m_version < 8) {
        pLevel_Player->Set_Type(static_cast<Alex_type>(savegame->m_player_state), 0, 0);
    }
    else {
        /*Set the player's power up type.
        For ghost alex, we first set the ghost power up as the type and then set the other power up as the type,
        marking a flag for the temporary ghost power up.  Logic for both fields is not included above in the version
        8 code because version 8 save files did not have the temporary power up field saved*/
        if (savegame -> m_player_type == ALEX_GHOST) {
            pLevel_Player->Set_Type(static_cast<Alex_type>(savegame->m_player_type), false, false);
            pLevel_Player->Set_Type(static_cast<Alex_type>(savegame->m_player_type_temp_power), false, false, true);
        }
        else {
            pLevel_Player->Set_Type(static_cast<Alex_type>(savegame->m_player_type), false, false);
        }
        pLevel_Player -> Set_Moving_State(static_cast<Moving_state>(savegame->m_player_state));
    }

    //Set invincibility time and star time for player
    pLevel_Player -> m_invincible = savegame->m_invincible;
    pLevel_Player -> m_invincible_star = savegame->m_invincible_star;

    //If the player had less than a second of invincibility or was not invincible (value of 0), give them a
    //second of invincibility.  This is only applicable if a level save state was loaded (not a world map save state).
    if (save_type == 1 && pLevel_Player->m_invincible < speedfactor_fps) {
        pLevel_Player->m_invincible = speedfactor_fps;
    }

    pLevel_Player -> m_ghost_time = savegame->m_ghost_time;
    pLevel_Player -> m_ghost_time_mod = savegame->m_ghost_time_mod;

    //Play the appropriate music
    if (!Is_Float_Equal(pLevel_Player -> m_invincible_star, 0.0f)) { //Player Invincible - Invincibility music
        pAudio->Play_Music("game/star.ogg", 0, 1, 500);
        pAudio->Play_Music(pActive_Level->m_musicfile, -1, 0);
    }
    else if (save_type == 1) { //Level loaded - level music
        pAudio->Play_Music(pActive_Level->m_musicfile, -1, 1, 1000);
    }
    else { //World map loaded -- world map music
        pAudio->Play_Music(pActive_Overworld->m_musicfile, -1, 1, 1000);
    }

    pHud_Points->Set_Points(savegame->m_points);
    pHud_Goldpieces->Set_Gold(savegame->m_goldpieces);
    pHud_Lives->Set_Lives(savegame->m_lives);
    pHud_Itembox->Set_Item(static_cast<SpriteType>(savegame->m_itembox_item), 0);
    pHud_Debug->Set_Text(_("Savegame ") + int_to_string(save_slot) + _(" loaded"));
    pHud_Manager->Update();

    // #### Return stack ####

    pLevel_Player->Clear_Return();
    if (!savegame->m_return_entries.empty()) {
        std::vector<cSave_Player_Return_Entry>::const_iterator return_iter;
        for (return_iter = savegame->m_return_entries.begin(); return_iter != savegame->m_return_entries.end(); return_iter++) {
            pLevel_Player->Push_Return(return_iter->m_level, return_iter->m_entry);
        }
    }

    delete savegame;
    return save_type;
}

bool cSavegame::Save_Game(unsigned int save_slot, std::string description)
{
    if (pLevel_Player->m_alex_type == ALEX_DEAD || pLevel_Player->m_lives < 0) {
        cerr << "Error : Couldn't save savegame " << description << " because of invalid game state" << endl;
        return 0;
    }

    cSave* savegame = new cSave();

    // General stuff
    savegame->m_version = SAVEGAME_VERSION;
    savegame->m_level_engine_version = level_engine_version;
    savegame->m_save_time = time(NULL);
    savegame->m_description = description;
    savegame->m_goldpieces = pLevel_Player->m_goldpieces;

    savegame->m_lives = pLevel_Player->m_lives;
    savegame->m_points = pLevel_Player->m_points;
    savegame->m_player_type = pLevel_Player->m_alex_type;
    savegame->m_player_type_temp_power = pLevel_Player->m_alex_type_temp_power;
    savegame->m_invincible = pLevel_Player->m_invincible;
    savegame->m_invincible_star = pLevel_Player->m_invincible_star;
    savegame->m_ghost_time = pLevel_Player -> m_ghost_time;
    savegame->m_ghost_time_mod = pLevel_Player -> m_ghost_time_mod;

    savegame->m_player_state = pLevel_Player->m_state;
    savegame->m_itembox_item = pHud_Itembox->m_item_id;

    // player return stack
    for (std::vector<cLevel_Player_Return_Entry>::const_iterator itr = pLevel_Player->m_return_stack.begin(); itr != pLevel_Player->m_return_stack.end(); itr++) {
        savegame->m_return_entries.push_back(cSave_Player_Return_Entry(itr->level, itr->entry));
    }

    // if in a level
    if (pActive_Level->Is_Loaded()) {
        // General info relating to all loaded levels
        savegame->m_level_time = pHud_Time->m_milliseconds;

        // Create save data for all loaded levels. Note that if a user entered
        // a sublevel, multiple levels can be loaded.
        for (vector<cLevel*>::iterator itr = pLevel_Manager->objects.begin(); itr != pLevel_Manager->objects.end(); ++itr) {
            cLevel* level = (*itr);

            if (!level->Is_Loaded()) {
                continue;
            }

            cSave_Level* save_level = new cSave_Level();

            // General info about the level
            save_level->m_name = path_to_utf8(Trim_Filename(level->m_level_filename, false, false));

            // Special treatment of the active level
            if (pActive_Level == level) {
                save_level->m_active = true;
                // Position.
                save_level->m_level_pos_x = pLevel_Player->m_pos_x;
                save_level->m_level_pos_y = pLevel_Player->m_pos_y - 5.0f;

                // Custom data a script writer wants to store in the
                // savegame (pSavegame holds the event table for the
                // level saving events).
                // TODO: Why not have mruby saving in sublevels?
                mrb_state* p_state = pActive_Level->m_mruby->Get_MRuby_State();
                mrb_value storage_hash = mrb_hash_new(p_state);
                mrb_int key = pActive_Level->m_mruby->Protect_From_GC(storage_hash);

                Scripting::cLevel_Save_Event evt(storage_hash);
                evt.Fire(pActive_Level->m_mruby, pSavegame);

                // We use JSON to store the data for now, as mruby doesn’t have Marshal, sadly.
                mrb_value mod_json = mrb_const_get(p_state, mrb_obj_value(p_state->object_class), mrb_intern_cstr(p_state, "JSON"));
                mrb_value result = mrb_funcall(p_state, mod_json, "stringify", 1, storage_hash);
                save_level->m_mruby_data = std::string(mrb_string_value_ptr(p_state, result));

                pActive_Level->m_mruby->Unprotect_From_GC(key); // GC can collect it now
            }

            // All the sprites in the level.
            for (cSprite_List::iterator itr = level->m_sprite_manager->objects.begin(); itr != level->m_sprite_manager->objects.end(); ++itr) {
                cSprite* p_obj = (*itr);

                /* Spawned objects. These cannot be in the level XML
                 * by definition, e.g. they can be generated by level
                 * script or C++ code. */
                if (p_obj->m_spawned) {
                    if (!p_obj->m_auto_destroy) {
                        save_level->m_spawned_objects.push_back(obj->Copy());
                    }
                }
                else {
                    // All other objects.
                    save_level->m_regular_objects.push_back(obj->Copy());
                }
            }

            savegame->m_levels.push_back(save_level);
        }
    }

    // save overworld progress
    for (vector<cOverworld*>::iterator itr = pOverworld_Manager->objects.begin(); itr != pOverworld_Manager->objects.end(); ++itr) {
        // Get Overworld
        cOverworld* overworld = (*itr);

        // create Overworld
        cSave_Overworld* save_overworld = new cSave_Overworld();
        save_overworld->m_name = overworld->m_description->m_name;

        // Waypoints
        for (cSprite_List::iterator wp_itr = overworld->m_sprite_manager->objects.begin(); wp_itr != overworld->m_sprite_manager->objects.end(); ++wp_itr) {
            // get waypoint
            cSprite* obj = static_cast<cSprite*>(*wp_itr);

            if (obj->m_type != TYPE_OW_WAYPOINT) {
                continue;
            }

            // get waypoint
            cWaypoint* waypoint = static_cast<cWaypoint*>(obj);

            // create savegame waypoint
            cSave_Overworld_Waypoint* save_waypoint = new cSave_Overworld_Waypoint();

            // destination
            save_waypoint->m_destination = waypoint->Get_Destination();
            // set access
            save_waypoint->m_access = waypoint->m_access;
            // save
            save_overworld->m_waypoints.push_back(save_waypoint);
        }

        savegame->m_overworlds.push_back(save_overworld);
    }

    // if an overworld is active and not custom level mode
    if (pActive_Overworld && Game_Mode_Type != MODE_TYPE_LEVEL_CUSTOM) {
        // set overworld name
        savegame->m_overworld_active = pActive_Overworld->m_description->m_name;

        // if valid waypoint
        if (pOverworld_Player->m_current_waypoint >= 0) {
            // set current waypoint
            savegame->m_overworld_current_waypoint = pOverworld_Player->m_current_waypoint;
        }
    }

    fs::path save_dir = pPackage_Manager->Get_User_Savegame_Path();
    fs::path filename = save_dir / utf8_to_path(int_to_string(save_slot) + ".tscsav");
    // remove old format savegame files
    fs::remove(save_dir / utf8_to_path(int_to_string(save_slot) + ".save"));
    fs::remove(save_dir / utf8_to_path(int_to_string(save_slot) + ".smcsav"));

    try {
        savegame->Write_To_File(filename);
    }
    catch (xmlpp::exception& e) {
        cerr << "Failed to save savegame '" << filename << "': " << e.what() << endl
             << "Is the file read-only?" << endl;
        pHud_Debug->Set_Text(_("Couldn't save savegame ") + path_to_utf8(filename), speedfactor_fps * 5.0f);
    }

    if (pHud_Debug) {
        pHud_Debug->Set_Text(_("Saved to Slot ") + int_to_string(save_slot));
    }

    delete savegame;

    return 1;
}

cSave* cSavegame::Load(unsigned int save_slot)
{
    fs::path save_dir = pPackage_Manager->Get_User_Savegame_Path();
    fs::path filename = save_dir / utf8_to_path(int_to_string(save_slot) + ".tscsav");

    cSave* savegame = NULL; //The save game object read from the save state file

    //First try to load the game using the newer format
    if (File_Exists(filename)) {
        savegame = cSave::Load_From_File(filename);
    }
    else {
        //If it is not in the newer tscsav format, try the older .smcsav format
        fs::path filename_smc = save_dir / utf8_to_path(int_to_string(save_slot) + ".smcsav");
        if (File_Exists(filename_smc)) {
            savegame = cSave::Load_From_File(filename_smc);
        }
        else {
            //If it is not in the other two formats, try the very old .save format
            fs::path filename_old = m_savegame_dir / utf8_to_path(int_to_string(save_slot) + ".save");
            if (File_Exists(filename_old)) {
                savegame = cSave::Load_From_File(filename_old);
            }
            else {
                //There is not a file in any useful format -- throw an exception
                std::stringstream ss;
                ss << "No savegame found at slot " << save_slot << " (filename '" << filename << "' or '" << filename_old << "')!";
                throw(InvalidSavegameError(save_slot, ss.str()));
            }
        }
    }

    //Now check to make sure each level referenced in the save file exists
    if (!savegame->m_levels.empty()) {
        for (Save_LevelList::iterator itr = savegame->m_levels.begin(); itr != savegame->m_levels.end(); ++itr) {
            cSave_Level* save_level = (*itr);
            fs::path filename = pLevel_Manager->Get_Path(save_level->m_name);
            if (filename.empty()) {
                throw(InvalidLevelError("Empty level filename!"));
            }
            if (!File_Exists(filename)) {
                std::string msg = "Level file not found: " + path_to_utf8(filename);
                throw (InvalidLevelError(msg));
            }
        }

    }

    return savegame;
}

std::string cSavegame::Get_Description(unsigned int save_slot, bool only_description /* = 0 */)
{
    std::string str_description;

    if (!Is_Valid(save_slot)) {
        char str[255];

        // TRANS: %u is replaced by the number of the save slot, starting with 1.
        int count = sprintf(str, _("%u. Free Save"), save_slot);
        return std::string(str, count);
    }

    // Raises exceptions if fails; caller must take care of them.
    cSave* savegame = Load(save_slot);

    // complete description
    if (!only_description) {
        str_description = int_to_string(save_slot) + ". " + savegame->m_description;

        if (savegame->m_levels.empty()) {
            str_description += " - " + savegame->m_overworld_active;
        }
        else {
            bool found_active = 0;

            for (Save_LevelList::iterator itr = savegame->m_levels.begin(); itr != savegame->m_levels.end(); ++itr) {
                cSave_Level* level = (*itr);

                // if active level
                if (!Is_Float_Equal(level->m_level_pos_x, 0.0f) && !Is_Float_Equal(level->m_level_pos_y, 0.0f)) {
                    str_description += _(" -  Level ") + level->m_name;
                    found_active = 1;
                    break;
                }
            }

            if (!found_active) {
                str_description += _(" -  Unknown");
            }
        }

        str_description += _(" - Date ") + Time_to_String(savegame->m_save_time, "%Y-%m-%d  %H:%M:%S");
    }
    // only the user description
    else {
        str_description = savegame->m_description;
    }

    delete savegame;
    return str_description;
}

bool cSavegame::Is_Valid(unsigned int save_slot) const
{
    fs::path save_dir = pPackage_Manager->Get_User_Savegame_Path();
    return (File_Exists(save_dir / utf8_to_path(int_to_string(save_slot) + ".tscsav")) || File_Exists(save_dir / utf8_to_path(int_to_string(save_slot) + ".smcsav")) ||
            File_Exists(save_dir / utf8_to_path(int_to_string(save_slot) + ".save")));
}

cSavegame* pSavegame = NULL;

/* *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** */

} // namespace TSC

/***************************************************************************
 * img_set.cpp  -  Image set container
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

#include "../core/game_core.hpp"
#include "../core/framerate.hpp"
#include "../video/gl_surface.hpp"
#include "../video/renderer.hpp"
#include "../core/i18n.hpp"
#include "../core/file_parser.hpp"
#include "../core/filesystem/resource_manager.hpp"
#include "../core/filesystem/package_manager.hpp"
#include "../core/global_basic.hpp"

using namespace std;

namespace fs = boost::filesystem;

namespace TSC {


/* *** *** *** *** *** *** *** cImageSet_Parser *** *** *** *** *** *** *** *** *** *** */
cImageSet_Parser::cImageSet_Parser(Uint32 time) : m_time(time)
{
}

bool cImageSet_Parser::HandleMessage(const std::string* parts, unsigned int count, unsigned int line)
{
    if(count == 1) {
        // only a filename
        m_images.push_back(Entry_Type(parts[0], m_time));
    }
    else if(count == 2) {
        if(parts[0] == "time") {
            // Setting default time for images
            m_time = string_to_int(parts[1]);
        }
        else {
            // filename and a time for that specific image
            m_images.push_back(Entry_Type(parts[0], string_to_int(parts[1])));
        }
    }

    return 1;
}

/* *** *** *** *** *** *** *** cImageSet_Surface *** *** *** *** *** *** *** *** *** *** */

cImageSet_Surface::cImageSet_Surface(void)
{
    m_image = NULL;
    m_time = 0;
}

cImageSet_Surface::~cImageSet_Surface(void)
{
    //
}


/* *** *** *** *** *** *** *** cImageSet *** *** *** *** *** *** *** *** *** *** */

cImageSet::cImageSet()
{
    // animation data
    m_curr_img = -1;
    m_anim_enabled = 0;
    m_anim_img_start = 0;
    m_anim_img_end = 0;
    m_anim_time_default = 1000;
    m_anim_counter = 0;
    m_anim_last_ticks = pFramerate->m_last_ticks - 1;
    m_anim_mod = 1.0f;
}

cImageSet::~cImageSet()
{
}

void cImageSet::Add_Image(cGL_Surface* image, Uint32 time /* = 0 */)
{
    // set to default time
    if (time == 0) {
        time = m_anim_time_default;
    }

    cImageSet_Surface obj;
    obj.m_image = image;
    obj.m_time = time;

    m_images.push_back(obj);
}

bool cImageSet::Add_Image_Set(const std::string& name, boost::filesystem::path path, Uint32 time /* = 0 */, int* start_num /* = NULL */, int* end_num /* = NULL */)
{
    int start, end;
    boost::filesystem::path filename;

    // Set to -1 until added
    if(start_num)
        *start_num = -1;

    if(end_num)
        *end_num = -1;

    start = m_images.size();
    if(path.extension() == utf8_to_path(".png")) {
        // Adding a single image
        Add_Image(pVideo->Get_Package_Surface(path), time);
        filename = path;
    }
    else {
        // Parse the animation file
        filename = pPackage_Manager->Get_Pixmap_Reading_Path(path_to_utf8(path));
        if(filename == boost::filesystem::path()) {
            cerr << "Warning: Unable to load image set: " << name << Get_Identity() << endl;
            return false;
        }

        cImageSet_Parser parser(time);
        if(!parser.Parse(path_to_utf8(filename))) {
            cerr << "Warning: Unable to parse image set: " << filename << endl;
            return false;
        }

        if(parser.m_images.size() == 0) {
            cerr << "Warning: Empty image set: " << filename << endl;
            return false;
        }

        // Add images
        for(cImageSet_Parser::List_Type::iterator itr = parser.m_images.begin(); itr != parser.m_images.end(); ++itr) {
            Add_Image(pVideo->Get_Package_Surface(path.parent_path() / utf8_to_path(itr->first)), itr->second);
        }
    }
    end = m_images.size() - 1;

    // Add the item
    if(end >= start) {
        m_named_ranges[name] = std::pair<int, int>(start, end);

        if(start_num)
            *start_num = start;

        if(end_num)
            *end_num = end;

        return true;
    }
    else {
        cerr << "Warning: No image set added: " << filename << endl;
        return false;
    }
}

bool cImageSet::Set_Image_Set(const std::string& name, bool new_startimage /* =0 */)
{
    cImageSet_Name_Map::iterator it = m_named_ranges.find(name);

    if(it == m_named_ranges.end()) {
        cerr << "Warning: Named image set not found: " << name << Get_Identity() << endl;
        Set_Image_Num(-1, new_startimage);
        Set_Animation(0);
        return false;
    }
    else {
        int start = it->second.first;
        int end = it->second.second;

        Set_Image_Num(start, new_startimage);
        Set_Animation_Image_Range(start, end);
        Set_Animation((end > start)); // True if more than one image
        Reset_Animation();

        return true;
    }
}

void cImageSet::Set_Image_Num(const int num, bool new_startimage /* = 0 */)
{
    if (m_curr_img == num) {
        return;
    }

    m_curr_img = num;

    if (m_curr_img < 0) {
        // BUG: Setting m_image to NULL may crash if code direclty access m_image such as m_image->m_w
        // This may happen if a loaded image set does not exist but the code still calls Set_Image_Set
        // then tries to acces m_image. Perhaps this should set some type of blank image instead.
        Set_Image_Set_Image(NULL, new_startimage);
    }
    else if (m_curr_img < static_cast<int>(m_images.size())) {
        Set_Image_Set_Image(m_images[m_curr_img].m_image, new_startimage);
    }
    else {
        debug_print("Warning : Object image number %d bigger as the array size %u%s", m_curr_img, static_cast<unsigned int>(m_images.size()), Get_Identity().c_str());
    }
}

cGL_Surface* cImageSet::Get_Image(const unsigned int num) const
{
    if (num >= m_images.size()) {
        return NULL;
    }

    return m_images[num].m_image;
}

void cImageSet::Clear_Images(void)
{
    m_curr_img = -1;
    m_images.clear();
    m_named_ranges.clear();
}

void cImageSet::Update_Animation(void)
{
    // prevent calling twice within the same update cycle
    if (m_anim_last_ticks == pFramerate->m_last_ticks) {
        return;
    }
    m_anim_last_ticks = pFramerate->m_last_ticks;

    // if not valid
    if (!m_anim_enabled || m_anim_img_end == 0) {
        return;
    }

    m_anim_counter += pFramerate->m_elapsed_ticks;

    // out of range
    if (m_curr_img < 0 || m_curr_img >= static_cast<int>(m_images.size())) {
        cerr << "Warning: Animation image " << m_curr_img << " for " << Get_Identity() << " out of range (max " << (m_images.size() - 1) << "). Forcing start image." << endl;
        Set_Image_Num(m_anim_img_start);
        return;
    }

    cImageSet_Surface image = m_images[m_curr_img];

    if (static_cast<Uint32>(m_anim_counter * m_anim_mod) >= image.m_time) {
        if (m_curr_img >= m_anim_img_end) {
            Set_Image_Num(m_anim_img_start);
        }
        else {
            Set_Image_Num(m_curr_img + 1);
        }

        m_anim_counter = static_cast<Uint32>(m_anim_counter * m_anim_mod) - image.m_time;
    }

    return;
}

void cImageSet::Set_Time_All(const Uint32 time, const bool default_time /* = 0 */)
{
    for (cImageSet_Surface_List::iterator itr = m_images.begin(); itr != m_images.end(); ++itr) {
        cImageSet_Surface& obj = (*itr);
        obj.m_time = time;
    }

    if (default_time) {
        Set_Default_Time(time);
    }
}

/* *** *** *** *** *** *** cSimpleImageSet *** *** *** *** *** *** *** *** *** */
cSimpleImageSet::cSimpleImageSet()
    : m_image(NULL)
{
}

cSimpleImageSet::~cSimpleImageSet()
{
}

std::string cSimpleImageSet::Get_Identity(void)
{
    return m_identity;
}

void cSimpleImageSet::Set_Image_Set_Image(cGL_Surface* new_image, bool new_startimage /*=0*/)
{
    m_image = new_image;
}

/* *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** */

} // namespace TSC

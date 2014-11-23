/***************************************************************************
 * overworld_layer_loader.cpp - Loading overworld layer XML
 *
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

#include "overworld_layer_loader.hpp"
#include "world_layer.hpp"
#include "overworld.hpp"

namespace fs = boost::filesystem;
using namespace TSC;

cOverworldLayerLoader::cOverworldLayerLoader(cOverworld* p_overworld)
    : xmlpp::SaxParser()
{
    mp_layer = NULL;
    mp_overworld = p_overworld;
}

cOverworldLayerLoader::~cOverworldLayerLoader()
{
    // Do not delete the cLayer instance — it
    // is used by the caller and deleted by him.
    mp_layer = NULL;
    mp_overworld = NULL; // This is managed by the caller anyways
}

cLayer* cOverworldLayerLoader::Get_Layer()
{
    return mp_layer;
}

cOverworld* cOverworldLayerLoader::Get_Overworld()
{
    return mp_overworld;
}

/***************************************
 * SAX parser callbacks
 ***************************************/

void cOverworldLayerLoader::parse_file(fs::path filename)
{
    m_layerfile = filename;
    xmlpp::SaxParser::parse_file(path_to_utf8(filename));
}

void cOverworldLayerLoader::on_start_document()
{
    if (mp_layer)
        throw("Restarted XML parser after already starting it."); // FIXME: proper exception

    mp_layer = new cLayer(mp_overworld);
}

void cOverworldLayerLoader::on_end_document()
{
    //
}

void cOverworldLayerLoader::on_start_element(const Glib::ustring& name, const xmlpp::SaxParser::AttributeList& properties)
{
    if (name == "property" || name == "Property") {
        std::string key;
        std::string value;

        /* Collect all the <property> elements for the surrounding
         * mayor element (like <settings> or <sprite>). When the
         * surrounding element is closed, the results are handled
         * in on_end_element(). */
        for (xmlpp::SaxParser::AttributeList::const_iterator iter = properties.begin(); iter != properties.end(); iter++) {
            xmlpp::SaxParser::Attribute attr = *iter;

            if (attr.name == "name" || attr.name == "Name")
                key = attr.value;
            else if (attr.name == "value" || attr.name == "Value")
                value = attr.value;
        }

        m_current_properties[key] = value;
    }
}

void cOverworldLayerLoader::on_end_element(const Glib::ustring& name)
{
    // Already handled
    if (name == "property" || name == "Property")
        return;

    // Ignore root tag
    if (name == "layer")
        return;

    if (name == "line")
        handle_line();
    else
        std::cerr << "Warning: Unknown overworld layer element '" << name << "'" << std::endl;

    // Everything handled, next one
    m_current_properties.clear();
}

/***************************************
 * Helpers
 ***************************************/

void cOverworldLayerLoader::handle_line()
{
    // if V.1.9 and lower: move y coordinate bottom to 0
    if (mp_overworld->m_engine_version < 2) {
        if (m_current_properties.exists("Y1"))
            m_current_properties["Y1"] = float_to_string(m_current_properties.retrieve<float>("Y1") - 600.0f);

        if (m_current_properties.exists("Y2"))
            m_current_properties["Y2"] = float_to_string(m_current_properties.retrieve<float>("Y2") - 600.0f);
    }

    // Add layer line
    mp_layer->Add(new cLayer_Line_Point_Start(m_current_properties, mp_overworld->m_sprite_manager, mp_overworld));
}

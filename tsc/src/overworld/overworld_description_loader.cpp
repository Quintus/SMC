/***************************************************************************
 * overworld_description_loader.cpp - Loading overworld description XML
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

#include "overworld_description_loader.hpp"
#include "overworld.hpp"

namespace fs = boost::filesystem;
using namespace TSC;

cOverworldDescriptionLoader::cOverworldDescriptionLoader()
    : xmlpp::SaxParser()
{
    mp_description = NULL;
}

cOverworldDescriptionLoader::~cOverworldDescriptionLoader()
{
    // Do not delete the cOverworld_description instance — it
    // is used by the caller and deleted by him.
    mp_description = NULL;
}

cOverworld_description* cOverworldDescriptionLoader::Get_Overworld_Description()
{
    return mp_description;
}

/***************************************
 * SAX parser callbacks
 ***************************************/

void cOverworldDescriptionLoader::parse_file(fs::path filename)
{
    m_descfile = filename;
    xmlpp::SaxParser::parse_file(path_to_utf8(filename));
}

void cOverworldDescriptionLoader::on_start_document()
{
    if (mp_description)
        throw("Restarted XML parser after already starting it."); // FIXME: proper exception

    mp_description = new cOverworld_description();
}

void cOverworldDescriptionLoader::on_end_document()
{

}

void cOverworldDescriptionLoader::on_start_element(const Glib::ustring& name, const xmlpp::SaxParser::AttributeList& properties)
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

void cOverworldDescriptionLoader::on_end_element(const Glib::ustring& name)
{
    // Already handled
    if (name == "property" || name == "Property")
        return;

    if (name == "world" || name == "World")
        handle_world();
    else if (name == "description" || name == "Description") {
        /* Ignore */
    }
    else
        std::cerr << "Warning: Unknown World Description element '" << name << "'." << std::endl;

    m_current_properties.clear();
}

void cOverworldDescriptionLoader::handle_world()
{
    mp_description->m_name = m_current_properties.fetch("name", mp_description->m_name);
    mp_description->m_visible = m_current_properties.fetch<bool>("visible", true);
}

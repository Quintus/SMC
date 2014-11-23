/***************************************************************************
 * activate_event.hpp
 *
 * Copyright © 2013-2014 The TSC Contributors
 ***************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TSC_SCRIPTING_ACTIVATE_EVENT_HPP
#define TSC_SCRIPTING_ACTIVATE_EVENT_HPP
#include "../scripting.hpp"
#include "event.hpp"

namespace TSC {
    namespace Scripting {
        class cActivate_Event: public cEvent {
        public:
            virtual std::string Event_Name()
            {
                return "activate";
            }
        };
    }
}
#endif

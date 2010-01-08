/*
 *   
 *   Copyright (c) 2007 David Moore, All Rights Reserved.
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *   
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *   
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *   MA 02111-1307 USA
 *   
*/
#ifndef __LADSPA_HOST_H__
#define __LADSPA_HOST_H__

#include <ladspa.h>

/// the host will instanciate plugins by name
/// it stores some info about the plugins
class LadspaHost{
	typedef std::vector<void*> LADSPALibraryHandleList;
	LADSPALibraryHandleList m_libraries;
	typedef std::map<std::string, const LADSPA_Descriptor*> LADSPADescriptorMap;
	LADSPADescriptorMap m_descriptors;
public:
	LadspaHost();
	~LadspaHost(); 

	/// load a ladspa library storing all descriptors found
	void load_library(const char* path);

	/// get a descriptor by name
	const LADSPA_Descriptor* instantiate(const std::string& name);
};

#endif


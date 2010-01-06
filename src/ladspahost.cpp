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
#include <vector>
#include <map>
#include <string>
#include "resoundnv/ladspahost.hpp"
#include <dlfcn.h>
#include <iostream>
#include <dirent.h>

#include "resoundnv/resound_exception.hpp"

LadspaHost::LadspaHost(){

      DIR *dp;
       struct dirent *ep;
     
       dp = opendir ("/usr/lib/ladspa/");
       if (dp != NULL)
         {
           while (ep = readdir (dp))
             load_library((std::string("/usr/lib/ladspa/") + std::string(ep->d_name)).c_str());
           (void) closedir (dp);
         }
}
LadspaHost::~LadspaHost(){
	for(int n = 0; n < m_libraries.size(); n++){
		dlclose(m_libraries[n]);
	}
	m_libraries.clear();
}

//typedef const LADSPA_Descriptor* (*LadspaDescriptorFunction)(unsigned long); // i did it like this but its already got one

void LadspaHost::load_library(const char* path){
	//const LADSPA_Descriptor * ladspa_descriptor(unsigned long Index);
	
	void* hndl = dlopen(path, RTLD_NOW);
	if(hndl != 0){
		m_libraries.push_back(hndl);
		int n = 0;
		LADSPA_Descriptor_Function ladspa_descriptor = (LADSPA_Descriptor_Function)dlsym(hndl, "ladspa_descriptor");
		const LADSPA_Descriptor* d = ladspa_descriptor(n++);
		while(d != 0){	
			std::cout << "Found LADSPA plugin " << d->Name << std::endl;
			LADSPADescriptorMap::iterator it = m_descriptors.find(d->Name);
			if(it == m_descriptors.end()){
				m_descriptors[d->Name] = d;
			} else {
				throw Exception("A descriptor with this name is already loaded.");
			}
			
			d=ladspa_descriptor(n++);
		}

	} else {
		std::cout << "LADSPA Host: failed to load " << path << std::endl;
	}
}

const LADSPA_Descriptor* LadspaHost::instantiate(const char* name){
	LADSPADescriptorMap::iterator it = m_descriptors.find(name);
	if(it != m_descriptors.end()){
		return it->second;
	} else {
		throw Exception("A descriptor with this name could not be found.");
	}
}

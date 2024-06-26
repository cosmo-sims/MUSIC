// This file is part of MUSIC
// A software package to generate ICs for cosmological simulations
// Copyright (C) 2010-2024 by Oliver Hahn
// 
// monofonIC is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// monofonIC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef __REGION_GENERATOR_HH
#define __REGION_GENERATOR_HH

#include <vector>
#include <config_file.hh>

#include <array>
using vec3_t = std::array<double,3>;
using index3_t = std::array<ptrdiff_t,3>;

//! Abstract base class for region generators
/*!
 This class implements a purely virtual interface that can be
 used to derive instances implementing various region generators.
 */
class region_generator_plugin{
public:
    config_file *pcf_;
    unsigned levelmin_, levelmax_;
    
public:
    region_generator_plugin( config_file& cf )
    : pcf_( &cf )
    {
        levelmin_ = cf.get_value<int>("setup","levelmin");
        levelmax_ = cf.get_value<int>("setup","levelmax");
    }
    
    //! destructor
    virtual ~region_generator_plugin() { };
    
    //! compute the bounding box of the region
    virtual void get_AABB( vec3_t& left, vec3_t& right, unsigned level) const = 0;
    
    //! query whether a point intersects the region
    virtual bool query_point( const vec3_t& x, int level ) const = 0;
    
    //! query whether the region generator explicitly forces the grid dimensions
    virtual bool is_grid_dim_forced( index3_t& ndims ) const = 0;
    
    //! get the center of the region
    virtual void get_center( vec3_t& xc ) const = 0;

    //! get the center of the region with a possible re-centering unapplied
    virtual void get_center_unshifted( vec3_t& xc ) const = 0;
  
    //! update the highres bounding box to what the grid generator actually uses
    virtual void update_AABB( vec3_t& left, vec3_t& right ) = 0;
};

//! Implements abstract factory design pattern for region generator plug-ins
struct region_generator_plugin_creator
{
	//! create an instance of a transfer function plug-in
	virtual region_generator_plugin * create( config_file& cf ) const = 0;
	
	//! destroy an instance of a plug-in
	virtual ~region_generator_plugin_creator() { }
};

//! Write names of registered region generator plug-ins to stdout
std::map< std::string, region_generator_plugin_creator *>& get_region_generator_plugin_map();
void print_region_generator_plugins( void );

//! Concrete factory pattern for region generator plug-ins
template< class Derived >
struct region_generator_plugin_creator_concrete : public region_generator_plugin_creator
{
	//! register the plug-in by its name
	region_generator_plugin_creator_concrete( const std::string& plugin_name )
	{
		get_region_generator_plugin_map()[ plugin_name ] = this;
	}
	
	//! create an instance of the plug-in
	region_generator_plugin * create( config_file& cf ) const
	{
		return new Derived( cf );
	}
};

typedef region_generator_plugin region_generator;

region_generator_plugin *select_region_generator_plugin( config_file& cf );

extern region_generator_plugin *the_region_generator;

#endif

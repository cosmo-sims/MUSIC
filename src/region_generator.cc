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


#include <algorithm>
#include "region_generator.hh"

std::map<std::string, region_generator_plugin_creator *> &
get_region_generator_plugin_map()
{
    static std::map<std::string, region_generator_plugin_creator *> region_generator_plugin_map;
    return region_generator_plugin_map;
}

void print_region_generator_plugins()
{
    std::map<std::string, region_generator_plugin_creator *> &m = get_region_generator_plugin_map();
    std::map<std::string, region_generator_plugin_creator *>::iterator it;
    it = m.begin();
    std::cout << " - Available region generator plug-ins:\n";
    while (it != m.end())
    {
        if ((*it).second)
            std::cout << "\t\'" << (*it).first << "\'\n";
        ++it;
    }
}

region_generator_plugin *select_region_generator_plugin(config_file &cf)
{
    std::string rgname = cf.get_value_safe<std::string>("setup", "region", "box");

    region_generator_plugin_creator *the_region_generator_plugin_creator = get_region_generator_plugin_map()[rgname];

    if (!the_region_generator_plugin_creator)
    {
        std::cerr << " - Error: region generator plug-in \'" << rgname << "\' not found." << std::endl;
        music::elog.Print("Invalid/Unregistered region generator plug-in encountered : %s", rgname.c_str());
        print_region_generator_plugins();
        throw std::runtime_error("Unknown region generator plug-in");
    }
    else
    {
        std::cout << " - Selecting region generator plug-in \'" << rgname << "\'..." << std::endl;
        music::ulog.Print("Selecting region generator plug-in  : %s", rgname.c_str());
    }

    region_generator_plugin *the_region_generator_plugin = the_region_generator_plugin_creator->create(cf);

    return the_region_generator_plugin;
}

/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/

#include <cmath>

class region_box_plugin : public region_generator_plugin
{
private:
    double
        x0ref_[3], //!< coordinates of refinement region origin (in [0..1[)
        lxref_[3], //!< extent of refinement region (int [0..1[)
        xcref_[3];
    size_t lnref_[3];
    bool bhave_nref_;
    unsigned levelmin_, levelmax_;
    bool do_extra_padding_;
    int padding_;
    double padding_fine_;

public:
    region_box_plugin(config_file &cf)
        : region_generator_plugin(cf)
    {
        levelmin_ = pcf_->get_value<unsigned>("setup", "levelmin");
        levelmax_ = pcf_->get_value<unsigned>("setup", "levelmax");

        if (levelmin_ != levelmax_)
        {
            padding_ = cf.get_value<int>("setup", "padding");

            std::string temp;

            if (!pcf_->contains_key("setup", "ref_offset") && !pcf_->contains_key("setup", "ref_center"))
            {
                music::elog.Print("Found levelmin!=levelmax but neither ref_offset nor ref_center was specified.");
                throw std::runtime_error("Found levelmin!=levelmax but neither ref_offset nor ref_center was specified.");
            }
            if (!pcf_->contains_key("setup", "ref_extent") && !pcf_->contains_key("setup", "ref_dims"))
            {
                music::elog.Print("Found levelmin!=levelmax but neither ref_extent nor ref_dims was specified.");
                throw std::runtime_error("Found levelmin!=levelmax but neither ref_extent nor ref_dims was specified.");
            }
            if (pcf_->contains_key("setup", "ref_extent"))
            {
                temp = pcf_->get_value<std::string>("setup", "ref_extent");
                std::remove_if(temp.begin(), temp.end(), isspace);
                if (sscanf(temp.c_str(), "%lf,%lf,%lf", &lxref_[0], &lxref_[1], &lxref_[2]) != 3)
                {
                    music::elog.Print("Error parsing triple for ref_extent");
                    throw std::runtime_error("Error parsing triple for ref_extent");
                }
                bhave_nref_ = false;
            }
            else if (pcf_->contains_key("setup", "ref_dims"))
            {
                temp = pcf_->get_value<std::string>("setup", "ref_dims");
                std::remove_if(temp.begin(), temp.end(), isspace);
                if (sscanf(temp.c_str(), "%lu,%lu,%lu", &lnref_[0], &lnref_[1], &lnref_[2]) != 3)
                {
                    music::elog.Print("Error parsing triple for ref_dims");
                    throw std::runtime_error("Error parsing triple for ref_dims");
                }
                bhave_nref_ = true;

                lxref_[0] = lnref_[0] * 1.0 / (double)(1 << levelmax_);
                lxref_[1] = lnref_[1] * 1.0 / (double)(1 << levelmax_);
                lxref_[2] = lnref_[2] * 1.0 / (double)(1 << levelmax_);
            }

            if (pcf_->contains_key("setup", "ref_center"))
            {
                temp = pcf_->get_value<std::string>("setup", "ref_center");
                std::remove_if(temp.begin(), temp.end(), isspace);
                if (sscanf(temp.c_str(), "%lf,%lf,%lf", &xcref_[0], &xcref_[1], &xcref_[2]) != 3)
                {
                    music::elog.Print("Error parsing triple for ref_center");
                    throw std::runtime_error("Error parsing triple for ref_center");
                }
                x0ref_[0] = fmod(xcref_[0] - 0.5 * lxref_[0] + 1.0, 1.0);
                x0ref_[1] = fmod(xcref_[1] - 0.5 * lxref_[1] + 1.0, 1.0);
                x0ref_[2] = fmod(xcref_[2] - 0.5 * lxref_[2] + 1.0, 1.0);
            }
            else if (pcf_->contains_key("setup", "ref_offset"))
            {
                temp = pcf_->get_value<std::string>("setup", "ref_offset");
                std::remove_if(temp.begin(), temp.end(), isspace);
                if (sscanf(temp.c_str(), "%lf,%lf,%lf", &x0ref_[0], &x0ref_[1], &x0ref_[2]) != 3)
                {
                    music::elog.Print("Error parsing triple for ref_offset");
                    throw std::runtime_error("Error parsing triple for ref_offset");
                }
                xcref_[0] = fmod(x0ref_[0] + 0.5 * lxref_[0], 1.0);
                xcref_[1] = fmod(x0ref_[1] + 0.5 * lxref_[1], 1.0);
                xcref_[2] = fmod(x0ref_[2] + 0.5 * lxref_[2], 1.0);
            }

            // conditions should be added here
            {
                do_extra_padding_ = false;
                std::string output_plugin = cf.get_value<std::string>("output", "format");
                if (output_plugin == std::string("grafic2"))
                    do_extra_padding_ = true;
                padding_fine_ = 0.0;
                if (do_extra_padding_)
                    padding_fine_ = (double)(padding_ + 1) * 1.0 / (1ul << levelmax_);
            }
        }
        else
        {
            x0ref_[0] = x0ref_[1] = x0ref_[2] = 0.0;
            lxref_[0] = lxref_[1] = lxref_[2] = 1.0;
            xcref_[0] = xcref_[1] = xcref_[2] = 0.5;
        }
    }

    void get_AABB(vec3_t &left, vec3_t &right, unsigned level) const
    {
        double dx = 1.0 / (1ul << level);
        double pad = (double)(padding_ + 1) * dx;

        if (!do_extra_padding_)
            pad = 0.0;

        for (int i = 0; i < 3; ++i)
        {
            left[i] = x0ref_[i] - pad;
            right[i] = x0ref_[i] + lxref_[i] + pad;
        }
    }

    void update_AABB(vec3_t &left, vec3_t &right)
    {
        for (int i = 0; i < 3; ++i)
        {
            double dx = right[i] - left[i];
            if (dx < -0.5)
                dx += 1.0;
            else if (dx > 0.5)
                dx -= 1.0;
            x0ref_[i] = left[i];
            lxref_[i] = dx;
            xcref_[i] = left[i] + 0.5 * dx;
        }
        // fprintf(stderr,"left = %f,%f,%f - right = %f,%f,%f\n",left[0],left[1],left[2],right[0],right[1],right[2]);
    }

    bool query_point( const vec3_t &x, int ilevel) const
    {
        if (!do_extra_padding_)
            return true;

        bool check = true;
        double dx;
        for (int i = 0; i < 3; ++i)
        {
            dx = x[i] - x0ref_[i];
            if (dx < -0.5)
                dx += 1.0;
            else if (dx > 0.5)
                dx -= 1.0;

            check &= ((dx >= padding_fine_) & (dx <= lxref_[i] - padding_fine_));
        }
        return check;
    }

    bool is_grid_dim_forced(index3_t& ndims) const
    {
        for (int i = 0; i < 3; ++i)
            ndims[i] = lnref_[i];
        return bhave_nref_;
    }

    void get_center(vec3_t &xc) const
    {
        xc[0] = xcref_[0];
        xc[1] = xcref_[1];
        xc[2] = xcref_[2];
    }

    void get_center_unshifted(vec3_t &xc) const
    {
        get_center(xc);
    }
};

namespace
{
    region_generator_plugin_creator_concrete<region_box_plugin> creator("box");
}

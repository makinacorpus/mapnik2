/*****************************************************************************
 * 
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2006 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

//$Id$

// mapnik
#include <mapnik/proj_transform.hpp>
#include <mapnik/coord.hpp> 
#include <mapnik/utils.hpp>

// proj4
#include <proj_api.h>

// stl
#include <vector>

namespace mapnik {
    
proj_transform::proj_transform(projection const& source, 
                               projection const& dest)
    : source_(source),
      dest_(dest) 
{
    is_source_longlat_ = source_.is_geographic();
    is_dest_longlat_ = dest_.is_geographic();
    is_source_equal_dest_ = (source_ == dest_);
}

bool proj_transform::equal() const
{
    return is_source_equal_dest_;
}

bool proj_transform::forward (double & x, double & y , double & z) const
{
    if (is_source_equal_dest_)
        return true;

    if (is_source_longlat_)
    {
        x *= DEG_TO_RAD;
        y *= DEG_TO_RAD;
    }

#if defined(MAPNIK_THREADSAFE) && PJ_VERSION < 480
    mutex::scoped_lock lock(projection::mutex_);
#endif

    if (pj_transform( source_.proj_, dest_.proj_, 1, 
                      0, &x,&y,&z) != 0)
    {
        return false;
    }

    if (is_dest_longlat_)
    {
        x *= RAD_TO_DEG;
        y *= RAD_TO_DEG;
    }

    return true;
} 
        
bool proj_transform::backward (double & x, double & y , double & z) const
{
    if (is_source_equal_dest_)
        return true;

    if (is_dest_longlat_)
    {
        x *= DEG_TO_RAD;
        y *= DEG_TO_RAD;
    }

#if defined(MAPNIK_THREADSAFE) && PJ_VERSION < 480
    mutex::scoped_lock lock(projection::mutex_);
#endif

    if (pj_transform( dest_.proj_, source_.proj_, 1, 
                      0, &x,&y,&z) != 0)
    {
        return false;
    }

    if (is_source_longlat_)
    {
        x *= RAD_TO_DEG;
        y *= RAD_TO_DEG;
    }

    return true;
}

bool proj_transform::forward (box2d<double> & box) const
{
    if (is_source_equal_dest_)
        return true;

    double minx = box.minx();
    double miny = box.miny();
    double maxx = box.maxx();
    double maxy = box.maxy();
    double z = 0.0;
    if (!forward(minx,miny,z))
        return false;
    if (!forward(maxx,maxy,z))
        return false;
    box.init(minx,miny,maxx,maxy);
    return true;
} 

bool proj_transform::backward (box2d<double> & box) const
{
    if (is_source_equal_dest_)
        return true;

    double minx = box.minx();
    double miny = box.miny();
    double maxx = box.maxx();
    double maxy = box.maxy();
    double z = 0.0;
    if (!backward(minx,miny,z))
        return false;
    if (!backward(maxx,maxy,z))
        return false;
    box.init(minx,miny,maxx,maxy);
    return true;
}

void envelope_points(std::vector< coord<double,2> > & coords, box2d<double>& env, int points)
{
    double width = env.width();
    double height = env.height();
    
    int steps;
    
    if (points <= 4) {
        steps = 0;
    } else {
        steps = static_cast<int>(ceil((points - 4) / 4.0));
    }
    
    steps += 1;
    double xstep = width / steps;
    double ystep = height / steps;
    
    for (int i=0; i<=steps; i++) {
        coords.push_back(coord<double,2>(env.minx() + i * xstep, env.miny()));
        coords.push_back(coord<double,2>(env.minx() + i * xstep, env.maxy()));

    }
    for (int i=1; i<steps; i++) {
        coords.push_back(coord<double,2>(env.minx(), env.miny() + i * ystep));
        coords.push_back(coord<double,2>(env.maxx(), env.miny() + i * ystep));
    }
}

box2d<double> calculate_bbox(std::vector<coord<double,2> > & points) {
    std::vector<coord<double,2> >::iterator it = points.begin();
    std::vector<coord<double,2> >::iterator it_end = points.end();
    
    box2d<double> env(*it, *(++it));
    for (; it!=it_end; ++it) {
        env.expand_to_include(*it);
    }
    return env;
}


/* More robust, but expensive, bbox transform
 * in the face of proj4 out of bounds conditions.
 * Can result in 20 -> 10 r/s performance hit.
 * Alternative is to provide proper clipping box
 * in the target srs by setting map 'maximum-extent' 
 */
bool proj_transform::backward(box2d<double>& env, int points) const
{
    if (is_source_equal_dest_)
        return true;
    
    std::vector<coord<double,2> > coords;
    envelope_points(coords, env, points);
    
    double z;
    for (std::vector<coord<double,2> >::iterator it = coords.begin(); it!=coords.end(); ++it) {
        z = 0;
        if (!backward(it->x, it->y, z)) {
            return false;
        }
    }

    box2d<double> result = calculate_bbox(coords);
    
    env.re_center(result.center().x, result.center().y);
    env.height(result.height());
    env.width(result.width());

    return true;
}

bool proj_transform::forward(box2d<double>& env, int points) const
{
    if (is_source_equal_dest_)
        return true;
    
    std::vector<coord<double,2> > coords;
    envelope_points(coords, env, points);
    
    double z;
    for (std::vector<coord<double,2> >::iterator it = coords.begin(); it!=coords.end(); ++it) {
        z = 0;
        if (!forward(it->x, it->y, z)) {
            return false;
        }
    }

    box2d<double> result = calculate_bbox(coords);
    
    env.re_center(result.center().x, result.center().y);
    env.height(result.height());
    env.width(result.width());

    return true;
}

mapnik::projection const& proj_transform::source() const
{
    return source_;
}
mapnik::projection const& proj_transform::dest() const
{
    return dest_;
}
 
}

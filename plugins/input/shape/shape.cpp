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

#include <iostream>
#include <fstream>
#include <stdexcept>

#include <mapnik/geom_util.hpp>


#include "shape_featureset.hpp"
#include "shape_index_featureset.hpp"

#include "shape.hpp"

DATASOURCE_PLUGIN(shape_datasource)

   shape_datasource::shape_datasource(const parameters &params)
      : datasource (params) ,
        shape_name_(params.get("file")),
        type_(datasource::Vector),
        file_length_(0),
        indexed_(false),
        desc_(params.get("name"),"latin1")
{
   try
   {
      shape_io shape(shape_name_);
      init(shape);
      for (int i=0;i<shape.dbf().num_fields();++i)
      {
         field_descriptor const& fd=shape.dbf().descriptor(i);
         std::string fld_name=fd.name_;
         switch (fd.type_)
         {
            case 'C':
            case 'D':
            case 'M':
            case 'L':		
               desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::String));
               break;
            case 'N':
            case 'F':
            {
               if (fd.dec_>0)
               {   
                  desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::Double,false,8));
               }
               else
               {
                  desc_.add_descriptor(attribute_descriptor(fld_name,mapnik::Integer,false,4));
               }
               break;
            }
            default:
#ifdef MAPNIK_DEBUG                
               std::clog << "unknown type "<<fd.type_<<"\n";
#endif 
               break;
         }
      }
   }
   catch  (datasource_exception& ex)
   {
      std::clog<<ex.what()<<std::endl;
      throw;
   }
}


shape_datasource::~shape_datasource()
{
}

const std::string shape_datasource::name_="shape";

void  shape_datasource::init(shape_io& shape)
{
   //first read header from *.shp
   int file_code=shape.shp().read_xdr_integer();
   if (file_code!=9994)
   {
      //invalid
      throw datasource_exception("wrong file code");
   }
   shape.shp().skip(5*4);
   file_length_=shape.shp().read_xdr_integer();
   int version=shape.shp().read_ndr_integer();
   if (version!=1000)
   {
      //invalid version number
      throw datasource_exception("invalid version number");
   }
   int shape_type=shape.shp().read_ndr_integer();
   shape.shp().read_envelope(extent_);
   shape.shp().skip(4*8);

   // check if we have an index file around
   std::string index_name(shape_name_+".index");
   std::ifstream file(index_name.c_str(),std::ios::in | std::ios::binary);
   if (file)
   {
      indexed_=true;
      file.close();
   }

#ifdef MAPNIK_DEBUG
   std::clog << extent_ << std::endl;
   std::clog << "file_length=" << file_length_ << std::endl;
   std::clog << "shape_type=" << shape_type << std::endl;
#endif

}

int shape_datasource::type() const
{
   return type_;
}

layer_descriptor shape_datasource::get_descriptor() const
{
   return desc_;
}

std::string shape_datasource::name()
{
   return name_;
}

featureset_ptr shape_datasource::features(const query& q) const
{
   filter_in_box filter(q.get_bbox());
   if (indexed_)
   {
      return featureset_ptr
         (new shape_index_featureset<filter_in_box>(filter,shape_name_,q.property_names()));
   }
   else
   {
      return featureset_ptr
         (new shape_featureset<filter_in_box>(filter,shape_name_,q.property_names(),file_length_));
   }
}

featureset_ptr shape_datasource::features_at_point(coord2d const& pt) const
{
   filter_at_point filter(pt);
   // collect all attribute names
   std::vector<attribute_descriptor> const& desc_vector = desc_.get_descriptors();
   std::vector<attribute_descriptor>::const_iterator itr = desc_vector.begin();
   std::vector<attribute_descriptor>::const_iterator end = desc_vector.end();
   std::set<std::string> names;
    
   while (itr != end)
   {    
      names.insert(itr->get_name());
      ++itr;
   }
    
   if (indexed_)
   {
      return featureset_ptr
         (new shape_index_featureset<filter_at_point>(filter,shape_name_,names));
   }
   else
   {
      return featureset_ptr
         (new shape_featureset<filter_at_point>(filter,shape_name_,names,file_length_));
   }
}

Envelope<double> shape_datasource::envelope() const
{
   return extent_;
}

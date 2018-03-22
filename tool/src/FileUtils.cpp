// ----------------------------------------------------------------------------
// Copyright (c) 2013-2014 by Graz University of Technology and
//                            Johannes Kepler University Linz
//
// This is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, see
// <http://www.gnu.org/licenses/>.
//
// For more information about this software see
//   <http://www.iaik.tugraz.at/content/research/design_verification/demiurge/>
// or email the authors directly.
//
// ----------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------
/// @file FileUtils.cpp
/// @brief Contains the definition of the class FileUtils.
// -------------------------------------------------------------------------------------------

#include "FileUtils.h"

// -------------------------------------------------------------------------------------------
bool FileUtils::fileExists(const string &file_name)
{
  ifstream inp(file_name.c_str());
  bool exists = !inp.fail();
  inp.close();
  return exists;
}

// -------------------------------------------------------------------------------------------
bool FileUtils::readFile(const string &file_name, string &file_content)
{
  ifstream is(file_name.c_str());
  if(is.fail())
    return false;
  is.seekg(0, std::ios::end);
  if(is.fail() || is.bad())
  {
    is.close();
    return false;
  }
  size_t length = is.tellg();
  is.seekg(0, std::ios::beg);
  if(is.fail() || is.bad())
  {
    is.close();
    return false;
  }
  char *buffer = new char[length];
  MASSERT(buffer != NULL, "Out of memory.");
  is.read(buffer, length);
  is.close();
  file_content.append(buffer, length);
  delete[] buffer;
  buffer = NULL;
  return true;
}

// -------------------------------------------------------------------------------------------
bool FileUtils::writeFile(const string &file_name, const string &file_content)
{
  ofstream out(file_name.c_str(), std::ios::out | std::ios::trunc);
  if(out.fail())
    return false;
  out << file_content;
  out.close();
  if(out.fail() || out.bad())
    return false;
  return true;
}


// -------------------------------------------------------------------------------------------
FileUtils::~FileUtils()
{
  // nothing to do
}


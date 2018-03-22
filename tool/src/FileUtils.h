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
/// @file FileUtils.h
/// @brief Contains the declaration of the class FileUtils.
// -------------------------------------------------------------------------------------------

#ifndef FileUtils_H__
#define FileUtils_H__

#include "defines.h"

// -------------------------------------------------------------------------------------------
///
/// @class FileUtils
/// @brief Contains utility functions for access to files.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.0.0
class FileUtils
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if a file exists.
///
/// @param file_name The name of the file to check.
/// @return True if the file exists (and could be opened), false otherwise.
  static bool fileExists(const string &file_name);

// -------------------------------------------------------------------------------------------
///
/// @brief Reads the content of a file into a string.
///
/// @param file_name The name of the file to read.
/// @param file_content The read file content is appended to this string.
/// @return True if file reading was successful, false otherwise (e.g. because the file does
///         not exist, could not be opened, etc.)
  static bool readFile(const string &file_name, string &file_content);

// -------------------------------------------------------------------------------------------
///
/// @brief Writes the content of a string into a file.
///
/// If the file already exists, it is overwritten. The directory into which the file should
/// be written must exist.
///
/// @param file_name The name of the file to write.
/// @param file_content The content to write into this file.
/// @return True if writing was successful, false otherwise.
  static bool writeFile(const string &file_name, const string &file_content);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~FileUtils();

protected:

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// The constructor is private and not implemented. Use the static methods.
  FileUtils();

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  FileUtils(const FileUtils &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  FileUtils& operator=(const FileUtils &other);

};

#endif // FileUtils_H__

#ifndef OPENSIM_COMMON_DLL_H_
#define OPENSIM_COMMON_DLL_H_
/* -------------------------------------------------------------------------- *
 *                         OpenSim:  osimCommonDLL.h                          *
 * -------------------------------------------------------------------------- *
 * The OpenSim API is a toolkit for musculoskeletal modeling and simulation.  *
 * See http://opensim.stanford.edu and the NOTICE file for more information.  *
 * OpenSim is developed at Stanford University and supported by the US        *
 * National Institutes of Health (U54 GM072970, R24 HD065690) and by DARPA    *
 * through the Warrior Web program.                                           *
 *                                                                            *
 * Copyright (c) 2005-2017 Stanford University and the Authors                *
 * Author(s): Frank C. Anderson                                               *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.         *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */

// IMPORT AND EXPORT
// UNIX
#ifndef _WIN32
    #define OSIMCOMMON_API

// WINDOWS
#else

    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    #if defined(OPENSIM_USE_STATIC_LIBRARIES)
        #define OSIMCOMMON_API
    #elif defined(OSIMCOMMON_EXPORTS)
        #define OSIMCOMMON_API __declspec(dllexport)
    #else
        #define OSIMCOMMON_API __declspec(dllimport)
    #endif

    #pragma warning(disable:4251) /*no DLL interface for type of member of exported class*/
    #pragma warning(disable:4275) /*no DLL interface for base class of exported class*/
    #pragma warning(disable:4661) /*instantiating incomplete template class*/

#endif

#endif // OPENSIM_COMMON_DLL_H_

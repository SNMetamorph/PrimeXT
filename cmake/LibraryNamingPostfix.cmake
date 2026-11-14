#
# LibraryNamingPostfix.cmake - computes the library name postfix
#
# This is a free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# The naming scheme is described in
# https://github.com/FWGS/xash3d-fwgs/blob/master/Documentation/extensions/library-naming.md
#
#   client.$ext             - Win/Lin/Mac with x86
#   client_$arch.$ext       - Win/Lin/Mac with non-x86
#   client_$os_$arch.$ext   - everything else (Android, consoles, ...)
#
# The postfix is computed in a separate module so that it can be checked by a
# unit test (tests/cmake/library_naming_test.cmake) without having to configure
# the whole project.
#

# xash_set_build_postfix( <os> <arch> <result_var> )
#
# Sets <result_var> to the library name postfix for the given operating system
# and architecture.
function( xash_set_build_postfix os arch result_var )
	if( os STREQUAL "android" )
		# force disable for Android, as Android ports aren't distributed in normal
		# way and doesn't follow library naming
		set( ${result_var} "" PARENT_SCOPE )
	elseif( os AND arch )
		set( ${result_var} "_${os}_${arch}" PARENT_SCOPE )
	elseif( arch )
		set( ${result_var} "_${arch}" PARENT_SCOPE )
	else()
		set( ${result_var} "" PARENT_SCOPE )
	endif()
endfunction()

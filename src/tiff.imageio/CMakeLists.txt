# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

# REDSHIFT PATCH BEGIN:
#   link liblzma
if (NOT MSVC)
	set(LZMA_STATIC_LIB /usr/local/lib/liblzma.a)
endif (NOT MSVC)

add_oiio_plugin (tiffinput.cpp tiffoutput.cpp
                 INCLUDE_DIRS ${TIFF_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS}
                 LINK_LIBRARIES ${TIFF_LIBRARIES} ${JPEG_LIBRARIES}
                                ${ZLIB_LIBRARIES} ${LZMA_STATIC_LIB})

#add_oiio_plugin (tiffinput.cpp tiffoutput.cpp
#                 INCLUDE_DIRS ${TIFF_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS}
#                 LINK_LIBRARIES ${TIFF_LIBRARIES} ${JPEG_LIBRARIES}
#                                ${ZLIB_LIBRARIES})
# REDSHIFT PATCH END

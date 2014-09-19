/*
OpenImageIO and all code, documentation, and other materials contained
therein are:

Copyright 2010 Larry Gritz and the other authors and contributors.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of the software's owners nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

(This is the Modified BSD License)
*/

#include "softimage_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace softimage_pvt;


class SoftimageOutput : public ImageOutput
{
public:
	SoftimageOutput () { init(); }
	virtual ~SoftimageOutput () { close(); }
	virtual const char *format_name (void) const { return "softimage"; }
	virtual bool open (const std::string &name, const ImageSpec &newspec, OpenMode mode=Create);
	virtual bool close();
	virtual bool write_scanline (int y, int z, TypeDesc format, const void *data, stride_t xstride);

private:
	void init ();
	bool writeU8 (uint8_t v);
	bool writeU16 (uint16_t v);
	bool writePixel (uint8_t pixel[3]);
		
	FILE *m_fd;
	std::string m_filename;
	std::vector<unsigned char> m_scratch;
};

// symbols required for OpenImageIO plugin
OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT ImageOutput *softimage_output_imageio_create() {
        return new SoftimageOutput;
    }
    OIIO_EXPORT const char *softimage_output_extensions[] = {
        "pic", NULL
    };

OIIO_PLUGIN_EXPORTS_END

//////////////////////////////////////////////////////////////////////////
bool
SoftimageOutput::open (const std::string &name, const ImageSpec &userspec, OpenMode mode)
{
	using softimage_pvt::PicFileHeader;
	using softimage_pvt::ChannelPacket;

	if (mode != Create) {
		error ("%s does not support subimages or MIP levels", format_name());
		return false;
	}

	close ();  // Close any already-opened file
	m_spec = userspec;  // Stash the spec
	m_filename = name;	// Stash the filename

	// Check for things this format doesn't support
	if (m_spec.width < 1 || m_spec.height < 1) {
		error ("Image resolution must be at least 1x1, you asked for %d x %d", m_spec.width, m_spec.height);
		return false;
	}
	if (m_spec.width > std::numeric_limits<uint16_t>::max() || m_spec.height > std::numeric_limits<uint16_t>::max()) {
		error ("Image resolution %d x %d too large for %s (maxiumum %d x %d)", 
			m_spec.width, m_spec.height, format_name(), (int)std::numeric_limits<uint16_t>::max(), (int)std::numeric_limits<uint16_t>::max());
		return false;
	}
	if (m_spec.depth < 1)
		m_spec.depth = 1;
	if (m_spec.depth > 1) {
		error ("%s does not support volume images (depth > 1)", format_name());
		return false;
	}

	//if(m_spec.nchannels != 4) {
	//	error ("%s only supports 4 channels (RGBA), not %d", format_name(), m_spec.nchannels);
	//	return false;
	//}

	// Open file
	m_fd = Filesystem::fopen (m_filename, "wb");
	if (! m_fd) {
		error ("Could not open file \"%s\"", m_filename.c_str());
		return false;
	}

	// Write header
	PicFileHeader picHeader;
	memset(&picHeader, 0, sizeof(PicFileHeader));
	picHeader.magic = PicFileHeader::kSoftimageMagicNumber;
	picHeader.version = 3.5f;
	//picHeader.comment;
	picHeader.id[0] = 'P'; 
	picHeader.id[1] = 'I';
	picHeader.id[2] = 'C';
	picHeader.id[3] = 'T';
	picHeader.width = (short) m_spec.width;
	picHeader.height = (short) m_spec.height;
	picHeader.ratio = 1.0f;	//the pixel ratio (not the aspect ratio)
	picHeader.fields = 3;	//FULL_FRAME
	//picHeader.padding;

	if (! picHeader.write_header(m_fd)) {
		error ("\"%s\": failed to write header", m_filename.c_str());
		close();
		return false;
	}

	// Write channel packets
	ChannelPacket channel;

	//RGB
	channel.chained = 1;
	channel.size = 8;
	channel.type = 2;
	channel.channelCode = 0xE0;
	if(sizeof(channel) != fwrite(&channel, 1, sizeof(channel), m_fd)) {
		error ("\"%s\": failed to write channel packet", m_filename.c_str());
		close();
		return false;
	}

	//Alpha
	channel.chained = 0;
	channel.size = 8;
	channel.type = 2;
	channel.channelCode = 0x10;
	if(sizeof(channel) != fwrite(&channel, 1, sizeof(channel), m_fd)) {
		error ("\"%s\": failed to write channel packet", m_filename.c_str());
		close();
		return false;
	}
	
	return true;
}

bool
SoftimageOutput::close ()
{
	if (m_fd) {
		fclose (m_fd);
		m_fd = NULL;
	}

	init ();
	return true;
}

inline void 
SoftimageOutput::init ()
{
	m_fd = NULL;
	m_filename.clear();
}

inline bool 
SoftimageOutput::writeU8 (uint8_t v)
{
	return sizeof(v) == fwrite(&v, 1, sizeof(v), m_fd);
}

inline bool
SoftimageOutput::writeU16 (uint16_t v)
{
	uint8_t temp = ((v >> 8) & 0xFF);
	if( writeU8(temp) ) {
		temp = (v & 0xFF);
		return writeU8(temp);
	}
	return false;
}

inline bool 
SoftimageOutput::writePixel (uint8_t pixel[3])
{
	return writeU8(pixel[0]) && writeU8(pixel[1]) && writeU8(pixel[2]);
}

bool 
SoftimageOutput::write_scanline (int y, int z, TypeDesc format, const void *data, stride_t xstride)
{
	int			same, seqSame, count;
	int			i, k;
	uint8_t		pixel[128][3], col[3];
	uint32_t*	line = (uint32_t*)to_native_scanline(format, data, xstride, m_scratch);

	count = 0;
	for(k = 0; k < m_spec.width; k++) {
		col[0] = (line[k]) & 0xFF;
		col[1] = (line[k] >> 8) & 0xFF;
		col[2] = (line[k] >> 16) & 0xFF;

		if(count == 0) {
			pixel[0][0] = col[0];
			pixel[0][1] = col[1];
			pixel[0][2] = col[2];
			count++;
		} else
			if(count == 1) {
				seqSame  = (col[0] == pixel[0][0]);
				seqSame &= (col[1] == pixel[0][1]);
				seqSame &= (col[2] == pixel[0][2]);

				if(!seqSame) {
					pixel[count][0] = col[0];
					pixel[count][1] = col[1];
					pixel[count][2] = col[2];
				}
				count++;
			} else
				if(count > 1) { 
					if(seqSame) {
						same  = (col[0] == pixel[0][0]);
						same &= (col[1] == pixel[0][1]);
						same &= (col[2] == pixel[0][2]);
					} else {
						same  = (col[0] == pixel[count - 1][0]);
						same &= (col[1] == pixel[count - 1][1]);
						same &= (col[2] == pixel[count - 1][2]);
					}

					if(same ^ seqSame) {
						if(!seqSame) {
							writeU8(uint8_t(count - 2));
							for(i = 0; i < count - 1; i++) {
								writePixel(pixel[i]);
							}
							pixel[0][0] = pixel[1][0] = col[0];
							pixel[0][1] = pixel[1][1] = col[1];
							pixel[0][2] = pixel[1][2] = col[2];
							count = 2;
							seqSame = 1;
						} else {
							if(count < 128) {
								writeU8(uint8_t(count + 127));
							}
							else {
								writeU8(128);
								writeU16(uint16_t(count));
							}
							writePixel(pixel[0]);
							pixel[0][0] = col[0];
							pixel[0][1] = col[1];
							pixel[0][2] = col[2];
							count = 1;
						}
					} else {
						if(!same) {
							pixel[count][0] = col[0];
							pixel[count][1] = col[1];
							pixel[count][2] = col[2];
						}
						count++;
						if((count == 128) && !seqSame) {
							writeU8(127);
							for(i = 0; i < count; i++) {
								writePixel(pixel[i]);
							}
							count = 0;
						}
						if((count == 65536) && seqSame) {
							writeU8(128);
							writeU16(uint16_t(count));
							writePixel(pixel[0]);
							count = 0;
						}
					}
				}
				if(ferror(m_fd))
					return false;
	}
	if(count) {
		if((count == 1) || (!seqSame)) {
			writeU8(uint8_t(count-1));
			for(i = 0; i < count; i++) {
				writePixel(pixel[i]);
			}
		} else {
			if(count < 128) {
				writeU8(uint8_t(count+127));
			}
			else {
				writeU8(128);
				writeU16(uint16_t(count));
			}
			writePixel(pixel[0]);
		}
		if(ferror(m_fd))
			return false;
	}

	count = 0;
	for(k = 0; k < m_spec.width; k++) {
		col[0] = (line[k] >> 24) & 0xFF;

		if(count == 0) {
			pixel[0][0] = col[0];
			count++;
		} else
			if(count == 1) {
				seqSame  = (col[0] == pixel[0][0]);

				if(!seqSame) {
					pixel[count][0] = col[0];
				}
				count++;
			} else
				if(count > 1) { 
					if(seqSame) {
						same  = (col[0] == pixel[0][0]);
					} else {
						same  = (col[0] == pixel[count - 1][0]);
					}

					if(same ^ seqSame) {
						if(!seqSame) {
							writeU8(uint8_t(count-2));
							for(i = 0; i < count - 1; i++) {
								writeU8(pixel[i][0]);
							}
							pixel[0][0] = pixel[1][0] = col[0];
							count = 2;
							seqSame = 1;
						} else {
							if(count < 128) {
								writeU8(uint8_t(count+127));
							}
							else {
								writeU8(128);
								writeU16(uint16_t(count));
							}
							writeU8(pixel[0][0]);
							pixel[0][0] = col[0];
							count = 1;
						}
					} else {
						if(!same) {
							pixel[count][0] = col[0];
						}
						count++;
						if((count == 128) && !seqSame) {
							writeU8(127);
							for(i = 0; i < count; i++) {
								writeU8(pixel[i][0]);
							}
							count = 0;
						}
						if((count == 65536) && seqSame) {
							writeU8(128);
							writeU16(uint16_t(count));
							writeU8(pixel[0][0]);
							count = 0;
						}
					}
				}
				if(ferror(m_fd))
					return false;
	}
	if(count) {
		if((count == 1) || (!seqSame)) {
			writeU8(uint8_t(count-1));
			for(i = 0; i < count; i++) {
				writeU8(pixel[i][0]);
			}
		} else {
			if(count < 128) {
				writeU8(uint8_t(count+127));
			}
			else {
				writeU8(128);
				writeU16(uint16_t(count));
			}
			writeU8(pixel[0][0]);
		}
		if(ferror(m_fd))
			return false;
	}

	return true;
}

OIIO_PLUGIN_NAMESPACE_END



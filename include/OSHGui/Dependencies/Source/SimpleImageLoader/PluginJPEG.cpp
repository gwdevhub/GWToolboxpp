#include "SimpleImageLoader.hpp"
#include "PluginJPEG.hpp"
#include "Convert.hpp"

extern "C"
{
#define XMD_H
#undef FAR
#include <setjmp.h>

#include "LibJPEG/jinclude.h"
#include "LibJPEG/jpeglib.h"
#include "LibJPEG/jerror.h"
}

namespace SimpleImageLoader
{
	const uint32_t INPUT_BUF_SIZE = 4096;
	//---------------------------------------------------------------------------
	struct ErrorManager
	{
		jpeg_error_mgr Error;
		jmp_buf SetJmpBuffer;
	};
	//---------------------------------------------------------------------------
	struct SourceManager
	{
		jpeg_source_mgr Source;
		Stream *Data;
		JOCTET *Buffer;
		boolean IsStartOfFile;
	};
	//---------------------------------------------------------------------------
	bool PluginJPEG::IsValidFormat(const RawData &data)
	{
		if (data.size() < 2)
		{
			return false;
		}

		const uint8_t magic[] = { 0xFF, 0xD8 };

		return std::memcmp(data.data(), magic, sizeof(magic)) == 0;
	}
	//---------------------------------------------------------------------------
	void jpeg_error_exit(j_common_ptr cinfo)
	{
		auto error = reinterpret_cast<ErrorManager*>(cinfo->err);

		if (error->Error.msg_code != 70/*JERR_UNKNOWN_MARKER*/)
		{
			jpeg_destroy(cinfo);

			longjmp(error->SetJmpBuffer, 1);
		}
	}
	//---------------------------------------------------------------------------
	void jpeg_output_message(j_common_ptr cinfo)
	{

	}
	//---------------------------------------------------------------------------
	void init_source(j_decompress_ptr cinfo)
	{
		auto src = reinterpret_cast<SourceManager*>(cinfo->src);

		src->IsStartOfFile = TRUE;
	}
	//---------------------------------------------------------------------------
	boolean fill_input_buffer(j_decompress_ptr cinfo)
	{
		auto src = reinterpret_cast<SourceManager*>(cinfo->src);

		auto nbytes = src->Data->Read(src->Buffer, 1, INPUT_BUF_SIZE);

		if (nbytes == 0)
		{
			if (src->IsStartOfFile)
			{
				jpeg_destroy((j_common_ptr)cinfo);

				auto error = (ErrorManager*)cinfo->err;
				error->Error.error_exit((j_common_ptr)cinfo);
			}

			//fake end
			src->Buffer[0] = (JOCTET)0xFF;
			src->Buffer[1] = (JOCTET)JPEG_EOI;

			nbytes = 2;
		}

		src->Source.next_input_byte = src->Buffer;
		src->Source.bytes_in_buffer = nbytes;
		src->IsStartOfFile = FALSE;

		return TRUE;
	}
	//---------------------------------------------------------------------------
	void skip_input_data(j_decompress_ptr cinfo, long bytes)
	{
		auto src = reinterpret_cast<SourceManager*>(cinfo->src);

		if (bytes > 0)
		{
			while (bytes > (long)src->Source.bytes_in_buffer)
			{
				bytes -= (long)src->Source.bytes_in_buffer;

				fill_input_buffer(cinfo);
			}

			src->Source.next_input_byte += (size_t)bytes;
			src->Source.bytes_in_buffer -= (size_t)bytes;
		}
	}
	//---------------------------------------------------------------------------
	void term_source(j_decompress_ptr cinfo)
	{

	}
	//---------------------------------------------------------------------------
	ImageData PluginJPEG::Load(Stream &data)
	{
		jpeg_decompress_struct cinfo;
		ErrorManager errorManager;

		try
		{
			cinfo.err = jpeg_std_error(&errorManager.Error);
			errorManager.Error.error_exit = jpeg_error_exit;
			errorManager.Error.output_message = jpeg_output_message;

			if (setjmp(errorManager.SetJmpBuffer))
			{
				jpeg_destroy_decompress(&cinfo);

				throw;
			}

			jpeg_create_decompress(&cinfo);

			{
				SourceManager *src;
				if (cinfo.src == nullptr)
				{
					cinfo.src = (jpeg_source_mgr*)(*cinfo.mem->alloc_small)((j_common_ptr)&cinfo, JPOOL_PERMANENT, sizeof(SourceManager));

					src = reinterpret_cast<SourceManager*>(cinfo.src);

					src->Buffer = (JOCTET *)(*cinfo.mem->alloc_small)((j_common_ptr)&cinfo, JPOOL_PERMANENT, INPUT_BUF_SIZE * sizeof(JOCTET));
				}

				src = reinterpret_cast<SourceManager*>(cinfo.src);
				src->Source.init_source = init_source;
				src->Source.fill_input_buffer = fill_input_buffer;
				src->Source.skip_input_data = skip_input_data;
				src->Source.resync_to_restart = jpeg_resync_to_restart;
				src->Source.term_source = term_source;
				src->Data = &data;
				src->Source.bytes_in_buffer = 0;
				src->Source.next_input_byte = nullptr;
			}

			jpeg_save_markers(&cinfo, JPEG_COM, 0xFFFF);
			for (int m = 0; m < 16; ++m)
			{
				jpeg_save_markers(&cinfo, JPEG_APP0 + m, 0xFFFF);
			}

			jpeg_read_header(&cinfo, TRUE);

			cinfo.scale_num = 1;
			cinfo.scale_denom = 1;

			jpeg_start_decompress(&cinfo);

			ImageData image;

			if (cinfo.output_components == 4 && cinfo.out_color_space == JCS_CMYK)
			{
				image = ImageData(cinfo.output_width, cinfo.output_height, 24, SIL_RGBA_RED_MASK, SIL_RGBA_GREEN_MASK, SIL_RGBA_BLUE_MASK);
			}
			else
			{
				image = ImageData(cinfo.output_width, cinfo.output_height, 8 * cinfo.output_components, SIL_RGBA_RED_MASK, SIL_RGBA_GREEN_MASK, SIL_RGBA_BLUE_MASK);

				if (cinfo.output_components == 1)
				{
					auto colors = image.GetPalette();

					for (int i = 0; i < 256; ++i)
					{
						colors[i].rgbRed = (uint8_t)i;
						colors[i].rgbGreen = (uint8_t)i;
						colors[i].rgbBlue = (uint8_t)i;
					}
				}
			}

			if (cinfo.out_color_space == JCS_CMYK)
			{
				auto rowStride = cinfo.output_width * cinfo.output_components;
				auto buffer = (*cinfo.mem->alloc_sarray)(reinterpret_cast<j_common_ptr>(&cinfo), JPOOL_IMAGE, rowStride, 1);

				while (cinfo.output_scanline < cinfo.output_height)
				{
					JSAMPROW src = buffer[0];
					JSAMPROW dst = image.GetScanLine(cinfo.output_height - cinfo.output_scanline - 1);

					jpeg_read_scanlines(&cinfo, buffer, 1);

					for (auto x = 0u; x < cinfo.output_width; ++x)
					{
						uint16_t K = (uint16_t)src[3];
						dst[SIL_RGBA_RED] = (uint8_t)((K * src[0]) / 255);		// C -> R
						dst[SIL_RGBA_GREEN] = (uint8_t)((K * src[1]) / 255);	// M -> G
						dst[SIL_RGBA_BLUE] = (uint8_t)((K * src[2]) / 255);		// Y -> B
						src += 4;
						dst += 3;
					}
				}
			}
			else
			{
				while (cinfo.output_scanline < cinfo.output_height)
				{
					auto dst = image.GetScanLine(cinfo.output_height - cinfo.output_scanline - 1);

					jpeg_read_scanlines(&cinfo, &dst, 1);
				}

#if SIMPLEIMAGELOADER_COLORORDER == SIMPLEIMAGELOADER_COLORORDER_BGR
				SwapRedBlue32(image);
#endif
			}

			jpeg_finish_decompress(&cinfo);

			jpeg_destroy_decompress(&cinfo);

			return image;
		}
		catch (...)
		{
			jpeg_destroy_decompress(&cinfo);
		}

		throw;
	}
	//---------------------------------------------------------------------------
}

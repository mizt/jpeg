namespace JPEG {
	
	#import "jpeglib.h"
	
	typedef struct error_mgr {
		struct jpeg_error_mgr pub;
		jmp_buf setjmp_buffer;
	} error_mgr;

	typedef struct {
		struct jpeg_destination_mgr pub;
		JOCTET *buffer;
		int bufsize;
		int datacount;
	} memory_dst_mgr;

	static void init_destination(j_compress_ptr info) {
		memory_dst_mgr *dst = (memory_dst_mgr *)info->dest;
		dst->pub.next_output_byte = dst->buffer;
		dst->pub.free_in_buffer = dst->bufsize;
		dst->datacount=0;
	}

	static boolean empty_output_buffer(j_compress_ptr info) {
		memory_dst_mgr *dst = (memory_dst_mgr *)info->dest;
		dst->pub.next_output_byte = dst->buffer;
		dst->pub.free_in_buffer = dst->bufsize;
		return true;
	}

	static void term_destination(j_compress_ptr info) {
		memory_dst_mgr *dst = (memory_dst_mgr *)info->dest;
		dst->datacount = dst->bufsize - (int)dst->pub.free_in_buffer;
	}

	static void error_exit(j_common_ptr info) {
		error_mgr *err = (error_mgr *)info->err;
		(*info->err->output_message)(info);
		longjmp(err->setjmp_buffer, 1);
	}

	void jpeg_memory_dest(j_compress_ptr info, JOCTET *buffer,int bufsize) {
		if (info->dest==NULL) {
			info->dest = (struct jpeg_destination_mgr *)(*info->mem->alloc_small)((j_common_ptr)info, JPOOL_PERMANENT,sizeof(memory_dst_mgr));
		}
		memory_dst_mgr *dst = (memory_dst_mgr *)info->dest;
		dst->bufsize=bufsize;
		dst->buffer =buffer;
		dst->pub.init_destination = init_destination;
		dst->pub.empty_output_buffer = empty_output_buffer;
		dst->pub.term_destination = term_destination;
	}

	int encode(char *dst, char *src, int width, int height,int quality) {
		struct jpeg_compress_struct info;
		struct jpeg_error_mgr err;
		memset(&info,0,sizeof(info));
		info.err = jpeg_std_error(&err);
		jpeg_create_compress(&info);
		info.image_width  = width;
		info.image_height = height;
		info.input_components = 3;
		info.in_color_space = JCS_RGB;
		jpeg_memory_dest(&info,(JOCTET*)dst,info.image_width*info.image_height*info.input_components);
		jpeg_set_defaults(&info);
		jpeg_set_quality (&info,quality,true);
		jpeg_start_compress(&info,true);
		while(info.next_scanline<info.image_height) {
			JSAMPROW ptr = (JSAMPROW)(src+(info.next_scanline*width*info.input_components));
			jpeg_write_scanlines(&info,&ptr,1);
		}
		jpeg_finish_compress(&info);		
		int len=((memory_dst_mgr *)info.dest)->datacount;
		jpeg_destroy_compress(&info);
		return len;
	}

	static void init_source(j_decompress_ptr info) {}
	static boolean fill_input_buffer(j_decompress_ptr info) { return false; }
		
	static void skip_input_data(j_decompress_ptr info, long bytes) {
		if(bytes>0) {
			info->src->next_input_byte+=(size_t)bytes;
			info->src->bytes_in_buffer-=(size_t)bytes;
		}
	}

	static void term_source(j_decompress_ptr info) {}

	extern void jpeg_memory_src(j_decompress_ptr info, unsigned char *ptr, size_t size) {
		info->src = (struct jpeg_source_mgr *)(*info->mem->alloc_small) ((j_common_ptr)info,JPOOL_PERMANENT,sizeof(jpeg_source_mgr));
		struct jpeg_source_mgr *src = info->src;
		src->init_source = init_source;
		src->fill_input_buffer = fill_input_buffer;
		src->skip_input_data = skip_input_data;
		src->resync_to_restart = jpeg_resync_to_restart;
		src->term_source = term_source;
		src->next_input_byte = ptr;
		src->bytes_in_buffer = size;
	}

	void decode(unsigned char *dst,unsigned char *jpg,int len) {
		
		struct jpeg_decompress_struct info;
		struct error_mgr err;
		
		info.err = jpeg_std_error(&err.pub);
		err.pub.error_exit = error_exit;
		if(setjmp(err.setjmp_buffer)){
			jpeg_destroy_decompress(&info);
			return;
		}
		
		jpeg_create_decompress(&info);
		jpeg_memory_src(&info,jpg,len);
		jpeg_read_header(&info,true);
		jpeg_start_decompress(&info);
		
		int line = info.output_width*info.output_components;
		
		for(int y=0; y<(int)info.output_height; y++) {
			jpeg_read_scanlines(&info,(JSAMPARRAY)&dst,1);
			dst+=line;
		}
		
		jpeg_finish_decompress(&info);
		jpeg_destroy_decompress(&info);
	}
}
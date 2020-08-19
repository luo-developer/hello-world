#include "ui/includes.h"
#include "timer.h"
#include "asm/crc16.h"
#include "ui/lcd_spi/lcd_drive.h"
#include "ascii.h"
/* #include "style.h" */
/* #include "server/video_server.h" */
/* #include "server/video_dec_server.h" */
/* #include "system/includes.h" */
/* #include "asm/jpeg_codec.h" */
/* #include "asm/imb.h" */
#include "font/font_textout.h"

#define UI_DEBUG 0
/* #define UI_BUF_CALC */

#if (UI_DEBUG == 1)

#define UI_PUTS puts
#define UI_PRINTF printf

#else

#define UI_PUTS(...)
#define UI_PRINTF(...)

#endif

struct fb_map_user {
    u16 xoffset;
    u16 yoffset;
    u16 width;
    u16 height;
    u8  *baddr;
    u8  *yaddr;
    u8  *uaddr;
    u8  *vaddr;
    u8 transp;
    u8 format;
};

struct fb_var_screeninfo {
    u16 s_xoffset;            //显示区域x坐标
    u16 s_yoffset;            //显示区域y坐标
    u16 s_xres;               //显示区域宽度
    u16 s_yres;               //显示区域高度
    u16 v_xoffset;      //屏幕的虚拟x坐标
    u16 v_yoffset;      //屏幕的虚拟y坐标
    u16 v_xres;         //屏幕的虚拟宽度
    u16 v_yres;         //屏幕的虚拟高度
    u16 rotate;
};

struct window_head {
    u32 offset;
    u32 len;
    u32 ptr_table_offset;
    u16 ptr_table_len;
    u16 crc_data;
    u16 crc_table;
    u16 crc_head;
};

struct ui_file_head {
    u8  res[16];
    u8 type;
    u8 window_num;
    u16 prop_len;
    u8 rotate;
    u8 rev[3];
};


static u32 ui_rotate = false;
static u32 ui_hori_mirror = false;
static u32 ui_vert_mirror = false;
static int malloc_cnt = 0;
static FILE *ui_file = NULL;

static int open_resource_file();

static const struct ui_platform_api br23_platform_api;
struct ui_priv {
    struct ui_platform_api *api;
    struct lcd_interface *lcd;
    int window_offset;
    struct lcd_info info;
};
static struct ui_priv priv;
#define __this (&priv)

#ifdef UI_BUF_CALC
struct buffer {
	struct list_head list;
	u8* buf;
	int size;
};
struct buffer buffer_used = {0};
#endif

void* br23_malloc(int size)
{
	void* buf;
	malloc_cnt++;
	buf = (void*)malloc(size);

	/* printf("platform_malloc : 0x%x, %d\n", buf, size); */
#ifdef UI_BUF_CALC
	struct buffer* new = (struct buffer*)malloc(sizeof(struct buffer));
	new->buf = buf;
	new->size = size;
	list_add_tail(new, &buffer_used);
	printf("platform_malloc : 0x%x, %d\n", buf, size);

	struct buffer* p;
	int buffer_used_total = 0;
	list_for_each_entry(p, &buffer_used.list, list) {
		buffer_used_total += p->size;
	}
	printf("used buffer size:%d\n\n", buffer_used_total);
#endif

	return buf;
}

void br23_free(void* buf)
{

    /* printf("platform_free : 0x%x\n",buf); */
	free(buf);
	malloc_cnt--;

#ifdef UI_BUF_CALC
	struct buffer* p, *n;
	list_for_each_entry_safe(p, n, &buffer_used.list, list) {
		if (p->buf == buf) {
			printf("platform_free : 0x%x, %d\n", p->buf, p->size);
			__list_del_entry(p);
			free(p);
		}
	}

	int buffer_used_total = 0;
	list_for_each_entry(p, &buffer_used.list, list) {
		buffer_used_total += p->size;
	}
	printf("used buffer size:%d\n\n", buffer_used_total);
#endif
}

int ui_platform_ok()
{
	return (malloc_cnt == 0);
}

static void draw_rect_range_check(struct rect* r, struct fb_map_user* map)
{
	if (r->left < map->xoffset) {
		r->left = map->xoffset;
	}
	if (r->left > (map->xoffset + map->width)) {
		r->left = map->xoffset + map->width;
	}
	if ((r->left + r->width) > (map->xoffset + map->width)) {
		r->width = map->xoffset + map->width - r->left;
	}
	if (r->top < map->yoffset) {
		r->top = map->yoffset;
	}
	if (r->top > (map->yoffset + map->height)) {
		r->top = map->yoffset + map->height;
	}
	if ((r->top + r->height) > (map->yoffset + map->height)) {
		r->height = map->yoffset + map->height - r->top;
	}

	ASSERT(r->left >= map->xoffset);
	ASSERT(r->top  >= map->yoffset);
	ASSERT((r->left + r->width) <= (map->xoffset + map->width));
	ASSERT((r->top + r->height) <= (map->yoffset + map->height));
}


/* 透明色: 16bits 0x55aa      0101 0xxx 1011 01xx 0101 0xxx
 *         24bits 0x50b450    0101 0000 1011 0100 0101 0000 , 80 180 80
 * */
void __font_pix_copy(struct draw_context* dc, int format, struct fb_map_user* map, u8* pix, struct rect* draw, int x, int y,
                     int height, int width, int color)
{

	int i, j, h;
	u16 osd_color;
	u32 size;

	osd_color = (format == DC_DATA_FORMAT_OSD8) || (format == DC_DATA_FORMAT_OSD8A) ? color & 0xff : color & 0xffff ;

	for (j = 0; j < (height + 7) / 8; j++) { /* 纵向8像素为1字节 */
		for (i = 0; i < width; i++) {
			if (((i + x) >= draw->left)
			    && ((i + x) <= (draw->left + draw->width - 1))) { /* x在绘制区域，要绘制 */
				u8 pixel = pix[j * width + i];
				int hh = height - (j * 8);
				if (hh > 8) {
					hh = 8;
				}
				for (h = 0; h < hh; h++) {
					if (((y + j * 8 + h) >= draw->top)
					    && ((y + j * 8 + h) <= (draw->top + draw->height - 1))) { /* y在绘制区域，要绘制 */
						u16 clr = pixel & BIT(h) ? osd_color : 0;
						if (clr) {
							if (platform_api->draw_point) {
								platform_api->draw_point(dc, x + i, y + j * 8 + h, clr);
							}
						}
					}
				} /* endof for h */
			}
		}/* endof for i */
	}/* endof for j */
}


static int image_str_size_check(int page_num, const char* txt, int* width, int* height)
{

	u16 id = ((u8)txt[1] << 8) | (u8)txt[0];
	u16 cnt = 0;
	struct image_file file;
	int w = 0, h = 0;

	while (id != 0x00ff) {
		if (open_image_by_id(&file, id, page_num) != 0) {
			return -EFAULT;
		}
		w += file.width;
		cnt += 2;
		id = ((u8)txt[cnt + 1] << 8) | (u8)txt[cnt];
	}
	h = file.height;
	*width = w;
	*height = h;
	return 0;
}

void platform_putchar(struct font_info* info, u8* pixel, u16 width, u16 height, u16 x, u16 y)
{
	__font_pix_copy(info->dc, info->disp.format,
	                (struct fb_map_user*)info->disp.map,
	                pixel,
	                (struct rect*)info->disp.rect,
	                x,
	                y,
	                height,
	                width,
	                info->disp.color);
}


static void* br23_set_timer(void* priv, void (*callback)(void*), u32 msec)
{
	return (void*)sys_timer_add(priv, callback, msec);
}

static int br23_del_timer(void* fd)
{
    if(fd) {
        sys_timer_del((int)fd);
    }

	return 0;
}

u32 __attribute__((weak)) set_retry_cnt()
{
	return 10;
}

static void *br23_load_widget_info(void *_head, u8 page)
{
	struct ui_file_head head;
	static union ui_control_info info = {0};
	static const int rotate[] = {0, 90, 180, 270};
	static u8 curr_page = 0;

	if (page != (u8) - 1) {
		curr_page = page;
		fseek(ui_file, 0, SEEK_SET);
		fread(ui_file, &head, sizeof(struct ui_file_head));
        ui_rotate = rotate[head.rotate];
        ui_core_set_rotate(ui_rotate);
        switch (head.rotate) {
        case 1: /* 旋转90度 */
            ui_hori_mirror = true;
            ui_vert_mirror = false;
            break;
        case 3:/* 旋转270度 */
            ui_hori_mirror = false;
            ui_vert_mirror = true;
            break;
        default:
            ui_hori_mirror = false;
            ui_vert_mirror = false;
            break;
        }

		fseek(ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head)*curr_page, SEEK_SET);
		fread(ui_file, &__this->window_offset, sizeof(__this->window_offset));
	}
	fseek(ui_file, __this->window_offset + (u32)_head, SEEK_SET);

	if ((u32)_head == 0) {
		fread(ui_file, &info, sizeof(struct window_info));
	} else {
		fread(ui_file, &info.head, sizeof(struct ui_ctrl_info_head));
		fread(ui_file, &((u8*)&info)[sizeof(struct ui_ctrl_info_head)], info.head.len - sizeof(struct ui_ctrl_info_head));
	}

	return &info;
}


static void *br23_load_css(void *_css)
{
	static struct element_css1 css = {0};

	fseek(ui_file, __this->window_offset + (u32)_css, SEEK_SET);
	fread(ui_file, &css, sizeof(struct element_css1));

	return &css;
}


static void *br23_load_image_list(void *_list)
{
	static struct ui_image_list_t list = {0};

	memset(&list, 0x00, sizeof(struct ui_image_list));
	/* ASSERT((u32)_list); */
    if((u32)_list == 0) {
        return NULL;
    }

	fseek(ui_file, __this->window_offset + (u32)_list, SEEK_SET);
	fread(ui_file, &list.num, sizeof(list.num));
    if(list.num == 0) {
        return NULL;
    }
	fread(ui_file, list.image, list.num * sizeof(list.image[0]));

	return &list;
}



static void *br23_load_text_list(void *__list)
{
	static struct ui_text_list_t _list = {0};
	struct ui_text_list_t *list;

	list = &_list;
	memset(list, 0x00, sizeof(struct ui_text_list_t));
	if ((u32)__list == 0) {
		return list;
	}

	fseek(ui_file, __this->window_offset + (u32)__list, SEEK_SET);
	fread(ui_file, &list->num, sizeof(list->num));
	if (list->num == 0) {
		return list;
	}
	ASSERT(list->num <= 4);
	fread(ui_file, list->str, list->num * sizeof(list->str[0]));

	return list;
}



static void* br23_load_window(int id)
{
	u8* ui;
	int i;
	u32* ptr;
	u16* ptr_table;
	struct ui_file_head head;
	struct window_head window;
	int len = sizeof(struct ui_file_head);
	int retry;
	static const int rotate[] = {0, 90, 180, 270};


	if (!ui_file) {
		printf("ui_file : 0x%x\n", ui_file);
		return NULL;
	}
	ui_platform_ok();

	for (retry = 0; retry < set_retry_cnt(); retry++) {
		fseek(ui_file, 0, SEEK_SET);
		fread(ui_file, &head, len);

		if (id >= head.window_num) {
			return NULL;
		}

		fseek(ui_file, sizeof(struct window_head)*id, SEEK_CUR);
		fread(ui_file, &window, sizeof(struct window_head));

		u16 crc = CRC16(&window, (u32) & (((struct window_head*)0)->crc_data));
		if (crc == window.crc_head) {
			ui_rotate = rotate[head.rotate];
			ui_core_set_rotate(ui_rotate);
			switch (head.rotate) {
			case 1: /* 旋转90度 */
				ui_hori_mirror = true;
				ui_vert_mirror = false;
				break;
			case 3:/* 旋转270度 */
				ui_hori_mirror = false;
				ui_vert_mirror = true;
				break;
			default:
				ui_hori_mirror = false;
				ui_vert_mirror = false;
				break;
			}
			goto __read_data;
		}
	}

	return NULL;

__read_data:
	ui = (u8*)__this->api->malloc(window.len);
	if (!ui) {
		return NULL;
	}
	for (retry = 0; retry < set_retry_cnt(); retry++) {
		fseek(ui_file, window.offset, SEEK_SET);
		fread(ui_file, ui, window.len);

		u16 crc = CRC16(ui, window.len);
		if (crc == window.crc_data) {
			goto __read_table;
		}
	}

	__this->api->free(ui);
	return NULL;

__read_table:
	ptr_table = (u16*)__this->api->malloc(window.ptr_table_len);
	if (!ptr_table) {
		__this->api->free(ui);
		return NULL;
	}
	for (retry = 0; retry < set_retry_cnt(); retry++) {
		fseek(ui_file, window.ptr_table_offset, SEEK_SET);
		fread(ui_file, ptr_table, window.ptr_table_len);

		u16 crc = CRC16(ptr_table, window.ptr_table_len);
		if (crc == window.crc_table) {
			u16* offset = ptr_table;
			for (i = 0; i < window.ptr_table_len; i += 2) {
				ptr = (u32*)(ui + *offset++);
				if (*ptr != 0) {
					*ptr += (u32)ui;
				}
			}
			__this->api->free(ptr_table);
			return ui;
		}
	}

	__this->api->free(ui);
	__this->api->free(ptr_table);

	return NULL;
}

static void br23_unload_window(void* ui)
{
    if(ui) {
        __this->api->free(ui);
    }
}


static int br23_load_style(struct ui_style* style)
{
	int err;
	int i, j;
	int len;
	struct vfscan* fs;
	char name[64];
	char style_name[16];
	static char cur_style = 0xff;


	if (!style->file && cur_style == 0) {
		return 0;
	}

	if (ui_file) {
		fclose(ui_file);
	}

	if (style->file == NULL) {
		cur_style = 0;
		open_resource_file();
#if 0
		fs = fscan("mnt/spiflash/res", "-t*.sty");
		if (!fs) {
			printf("open mnt/spiflash/res fail!\n");
			return -EFAULT;
		}
		ui_file = fselect(fs, FSEL_FIRST_FILE, 0);
		if (!ui_file) {
			fscan_release(fs);
			return -ENOENT;
		}
		len = fget_name(ui_file, (u8*)name, 16);
		if (len) {
			style_name[len - 4] = 0;
			memcpy(style_name, name, len - 4);
			ui_core_set_style(style_name);
		}

		fscan_release(fs);
#else
        ui_file = fopen("mnt/sdfile/res/JL.sty","r");
        if(!ui_file) {
            return -ENOENT;
        }
        len = 6;
        strcpy(style_name, "JL.sty");
        if(len) {
            style_name[len - 4] = 0;
            ui_core_set_style(style_name);
        }
#endif
	} else {
		cur_style = 1;
		ui_file = fopen(style->file, "r");
		if (!ui_file) {
			return -EINVAL;
		}
		for (i = 0; style->file[i] != '.'; i++) {
			name[i] = style->file[i];
		}
		name[i++] = '.';
		name[i++] = 'p';
		name[i++] = 'i';
		name[i++] = 'c';
		name[i] = '\0';
		open_resfile(name);

		name[--i] = 'r';
		name[--i] = 't';
		name[--i] = 's';
		open_str_file(name);

		name[i++] = 'a';
		name[i++] = 's';
		name[i++] = 'i';
		font_ascii_init(name);

		for (i = strlen(style->file) - 5; i >= 0; i--) {
			if (style->file[i] == '/') {
				break;
			}
		}

		for (i++, j = 0; style->file[i] != '\0'; i++) {
			if (style->file[i] == '.') {
				name[j] = '\0';
				break;
			}
			name[j++] = style->file[i];
		}
		ASCII_ToUpper(name, j);
		err = ui_core_set_style(name);
		if (err) {
			printf("style_err: %s\n", name);
		}
	}

	return 0;

__err2:
	close_resfile();
__err1:
	fclose(ui_file);

	return err;
}

static int br23_open_draw_context(struct draw_context *dc)
{
	dc->buf_num = 1;
	if (__this->lcd->buffer_malloc) {
		__this->lcd->buffer_malloc(&dc->buf, &dc->len);
	}

	if (__this->lcd->get_screen_info) {
		__this->lcd->get_screen_info(&__this->info);
	}
    switch(__this->info.color_format) {
        case LCD_COLOR_RGB565:
            if(dc->data_format != DC_DATA_FORMAT_OSD16) {
                ASSERT(0,"The color format of layer don't match the lcd driver,page %d please select OSD16!",dc->page);
            }
            break;
        case LCD_COLOR_MONO:
            if(dc->data_format != DC_DATA_FORMAT_MONO) {
                ASSERT(0,"The color format of layer don't match the lcd driver,page %d please select OSD1!",dc->page);
            }
            break;
    }
	if (dc->data_format == DC_DATA_FORMAT_OSD16) {
		dc->fbuf = (u8 *)__this->api->malloc(__this->info.width * 2);
		dc->fbuf_len = __this->info.width * 2;
	} else if (dc->data_format == DC_DATA_FORMAT_MONO) {
		dc->fbuf = (u8 *)__this->api->malloc(__this->info.width * 2);
		dc->fbuf_len = __this->info.width * 2;
	}

	return 0;
}

static int br23_get_draw_context(struct draw_context *dc)
{

	dc->disp.left  = dc->need_draw.left;
	dc->disp.width = dc->need_draw.width;
	if (dc->data_format == DC_DATA_FORMAT_OSD16) {
		int lines = dc->len / dc->need_draw.width / 2;

		if ((dc->disp.top == 0) && (dc->disp.height == 0)) {
			dc->disp.top   = dc->need_draw.top;
			dc->disp.height = lines > dc->need_draw.height ? dc->need_draw.height : lines;
		} else {
			dc->disp.top   = dc->disp.top + dc->disp.height;
			dc->disp.height = lines > (dc->need_draw.top + dc->need_draw.height - dc->disp.top) ?
			                  (dc->need_draw.top + dc->need_draw.height - dc->disp.top) : lines;
		}
	} else if (dc->data_format == DC_DATA_FORMAT_MONO) {
		dc->disp.top = dc->need_draw.top;
		dc->disp.height = dc->need_draw.height;
	}

	return 0;
}

static int br23_put_draw_context(struct draw_context *dc)
{
	if (__this->lcd->draw) {
		if (dc->data_format == DC_DATA_FORMAT_OSD16) {
			__this->lcd->draw(dc->buf, dc->disp.height * dc->disp.width * 2);
		} else if (dc->data_format == DC_DATA_FORMAT_MONO) {
			__this->lcd->draw(dc->buf, __this->info.width * __this->info.height / 8);
		}
	}
	return 0;
}


static int br23_set_draw_context(struct draw_context* dc)
{
    if(__this->lcd->set_draw_area) {
        __this->lcd->set_draw_area(dc->disp.left, dc->disp.left + dc->disp.width - 1, 
                                   dc->disp.top, dc->disp.top + dc->disp.height - 1);
    }
	return 0;
}

static int br23_close_draw_context(struct draw_context* dc)
{
    if(__this->lcd->buffer_free) {
        __this->lcd->buffer_free(dc->buf);
    }
    if(dc->fbuf) {
        __this->api->free(dc->fbuf);
        dc->fbuf = NULL;
        dc->fbuf_len = 0;
    }

	return 0;
}

static int br23_invert_rect(struct draw_context *dc, u32 acolor)
{
	int i;
	int len;
	int w, h;
	int color = acolor & 0xffff;

	if (dc->data_format == DC_DATA_FORMAT_MONO) {
        color |= BIT(31);
        for (h = 0; h < dc->draw.height; h++) {
            for (w = 0; w < dc->draw.width; w++) {
                if (platform_api->draw_point) {
                    platform_api->draw_point(dc, dc->draw.left + w, dc->draw.top + h, color);
                }
            }
        }
    }
	return 0;
}

static int br23_fill_rect(struct draw_context* dc, u32 acolor)
{
	int i;
	int len;
	int w, h;
	int color = acolor & 0xffff;

    if(dc->data_format == DC_DATA_FORMAT_MONO) {
        color = !color;
    }

	for (h = 0; h < dc->draw.height; h++) {
		for (w = 0; w < dc->draw.width; w++) {
			if (platform_api->draw_point) {
				platform_api->draw_point(dc, dc->draw.left + w, dc->draw.top + h, color);
			}
		}
	}

	return 0;
}

static inline void __draw_vertical_line(struct draw_context* dc, int x, int y, int width, int height, int color, int format)
{
	int i, j;
	struct rect r = {0};
	struct rect disp = {0};
   
    disp.left  = x;
    disp.top   = y;
    disp.width = width;
    disp.height= height;
    if (!get_rect_cover(&dc->draw, &disp, &r)) {
        return;
    }

	switch (format) {
	case DC_DATA_FORMAT_OSD16:
		for (i = 0; i < r.width; i++) {
			for (j = 0; j < r.height; j++) {
				if (platform_api->draw_point) {
					platform_api->draw_point(dc, r.left + i, r.top + j, color);
				}
			}
		}
		break;
	case DC_DATA_FORMAT_MONO:
		for (i = 0; i < r.width; i++) {
			for (j = 0; j < r.height; j++) {
				if (platform_api->draw_point) {
					platform_api->draw_point(dc, r.left + i, r.top + j, color);
				}
			}
		}
		break;

	}
}

static inline void __draw_line(struct draw_context* dc, int x, int y, int width, int height, int color, int format)
{
	int i, j;
	struct rect r = {0};
	struct rect disp = {0};
   
    disp.left  = x;
    disp.top   = y;
    disp.width = width;
    disp.height= height;
    if (!get_rect_cover(&dc->draw, &disp, &r)) {
        return;
    }

	switch (format) {
	case DC_DATA_FORMAT_OSD16:
		for (i = 0; i < r.height; i++) {
			for (j = 0; j < r.width; j++) {
				if (platform_api->draw_point) {
					platform_api->draw_point(dc, r.left + j, r.top + i, color);
				}
			}
		}
		break;
	case DC_DATA_FORMAT_MONO:
		for (i = 0; i < r.height; i++) {
			for (j = 0; j < r.width; j++) {
				if (platform_api->draw_point) {
					platform_api->draw_point(dc, r.left + j, r.top + i, color);
				}
			}
		}
		break;
	}
}

static int br23_draw_rect(struct draw_context* dc, struct css_border* border)
{
	int err;
	int offset;
	int color;
    int border_color = border->color & 0xffff;

	/* draw_rect_range_check(&dc->draw, map); */
	/* draw_rect_range_check(&dc->rect, map); */

	if (dc->data_format == DC_DATA_FORMAT_OSD16) {
		color = border->color & 0xffff;
	} else if (dc->data_format == DC_DATA_FORMAT_MONO) {
        if (border_color == 0xad2a) {
            color = 0x55aa;//清显示
        } else {
            color = (border_color != 0x52d5) ? (border_color ? border_color : 0xffff) : 0x55aa;
        }
	}

	if (border->left) {
		if (dc->rect.left >= dc->draw.left &&
		    dc->rect.left <= rect_right(&dc->draw)) {
			__draw_vertical_line(dc, dc->draw.left, dc->draw.top,
			                     border->left, dc->draw.height, color, dc->data_format);
		}
	}
	if (border->right) {
		if (rect_right(&dc->rect) >= dc->draw.left &&
		    rect_right(&dc->rect) <= rect_right(&dc->draw)) {
			__draw_vertical_line(dc, dc->draw.left + dc->draw.width - border->right, dc->draw.top,
			                     border->right, dc->draw.height, color, dc->data_format);
		}
	}
	if (border->top) {
		if (dc->rect.top >= dc->draw.top &&
		    dc->rect.top <= rect_bottom(&dc->draw)) {
			__draw_line(dc, dc->draw.left, dc->draw.top,
			            dc->draw.width, border->top, color, dc->data_format);
		}
	}
	if (border->bottom) {
		if (rect_bottom(&dc->rect) >= dc->draw.top &&
		    rect_bottom(&dc->rect) <= rect_bottom(&dc->draw)) {
			__draw_line(dc, dc->draw.left, dc->draw.top + dc->draw.height - border->bottom,
			            dc->draw.width, border->bottom, color, dc->data_format);
		}
	}

	return 0;
}

static int br23_draw_image(struct draw_context *dc, u32 src, u8 quadrant)
{
	u8* pixbuf;
	struct rect draw_r;
    struct rect r = {0};
    struct rect disp = {0};
	struct image_file file;
	int h, w;

	if (((u16) - 1 == src) || ((u32) - 1 == src)) {
		return -1;
	}

	draw_r.left   = dc->draw.left;
	draw_r.top    = dc->draw.top;
	draw_r.width  = dc->draw.width;
	draw_r.height = dc->draw.height;

	/* UI_PRINTF("image draw %d, %d, %d, %d\n", dc->draw.left, dc->draw.top, dc->draw.width, dc->draw.height); */
	/* UI_PRINTF("image rect %d, %d, %d, %d\n", dc->rect.left, dc->rect.top, dc->rect.width, dc->rect.height); */

	int err = open_image_by_id(&file, src, dc->page);
	if (err) {
		return -EFAULT;
	}

    int x = dc->rect.left;
    int y = dc->rect.top;

    if (dc->align == UI_ALIGN_CENTER) {
        x += (dc->rect.width / 2 - file.width / 2);
        y += (dc->rect.height / 2 - file.height / 2);
    } else if (dc->align == UI_ALIGN_RIGHT) {
        x += dc->rect.width - file.width;
    }

    pixbuf = dc->fbuf;
    if (!pixbuf) {
        return -ENOMEM;
    }

    disp.left   = x;
    disp.top    = y;
    disp.width  = file.width;
    disp.height = file.height;

	if (dc->data_format == DC_DATA_FORMAT_MONO) {
		if (get_rect_cover(&draw_r, &disp, &r)) {
            int _offset = -1;
			for (h = 0; h < r.height; h++) {
				if (file.compress == 0) {
					int offset = (r.top + h - disp.top) / 8 * file.width + (r.left - disp.left);
                    if(_offset != offset) {
                        if (br23_read_image_data(&file, pixbuf, r.width, offset) != r.width) {
                            return -EFAULT;
                        }
                        _offset = offset;
                    }
				} else {
					ASSERT(0, "the compress mode not support!");
				}

				for (w = 0; w < r.width; w++) {
					u8 color = (pixbuf[w] & BIT((r.top + h - disp.top) % 8)) ? 1 : 0;
					if (color) {
						if (platform_api->draw_point) {
							platform_api->draw_point(dc, r.left + w, r.top + h, color);
						}
					}
				}
			}
		}
	} else if (dc->data_format == DC_DATA_FORMAT_OSD16) {
        if(get_rect_cover(&draw_r,&disp,&r)) {
            for(h = 0; h < r.height; h++) {
                if (file.compress == 0) {
                    int offset = (r.top + h - disp.top) * file.width * 2 + (r.left - disp.left) * 2;
                    if (br23_read_image_data(&file, pixbuf, r.width * 2, offset) != r.width * 2) {
                        return -EFAULT;
                    }
                } else {
                    ASSERT(0,"the compress mode not support!");
                }
                for(w = 0; w < r.width; w++) {
                    u16 color = (pixbuf[w * 2 + 1] << 8) | (pixbuf[w * 2]);
                    if (color) {
                        if (platform_api->draw_point) {
                            platform_api->draw_point(dc, r.left + w, r.top + h, color);
                        }
                    }
                }
            }
        }
	}

	return 0;
}

static int br23_show_text(struct draw_context *dc, struct ui_text_attrs *text)
{
	struct rect draw_r;
    struct rect r = {0};
    struct rect disp = {0};
	struct image_file file;

	/* 控件从绝对x,y 转成相对图层的x,y */
	int x = dc->rect.left;
	int y = dc->rect.top;

	/* 绘制区域从绝对x,y 转成相对图层的x,y */
	draw_r.left   = dc->draw.left;
	draw_r.top    = dc->draw.top;
	draw_r.width  = dc->draw.width;
	draw_r.height = dc->draw.height;

	if (text->format && !strcmp(text->format, "text")) {
		static struct font_info* info = NULL;
		static int language = 0;
		if (!info || (language != ui_language_get())) {
			language = ui_language_get();
			info = font_open(NULL, language);
			ASSERT(info, "font_open fail!");
		}

		if (info && (FT_ERROR_NONE == (info->sta & (~FT_ERROR_NOTABFILE)))) {
			info->disp.map    = 0;
			info->disp.rect   = &draw_r;
			info->disp.format = dc->data_format;
			if (dc->data_format == DC_DATA_FORMAT_OSD16) {
				info->disp.color  = text->color;
			} else if (dc->data_format == DC_DATA_FORMAT_MONO) {
				if (text->color == 0xad2a) {
					info->disp.color = 0x55aa;//清显示
				} else {
					info->disp.color = (text->color != 0x52d5) ? (text->color ? text->color : 0xffff) : 0x55aa;
				}
			}
			info->dc = dc;

            info->text_width  = draw_r.width;
            info->text_height = draw_r.height;
			info->flags       = text->flags;

			if (text->encode == FONT_ENCODE_ANSI) {
				int width = font_text_width(info, (u8 *)text->str, text->strlen);
				int height;

				if (info->ascpixel.size) {
					height = info->ascpixel.size;
				} else if (info->pixel.size) {
					height = info->pixel.size;
				} else {
					ASSERT(0, "can't get the height of font.");
				}

				if (width > dc->rect.width) {
					width = dc->rect.width;
				}
				if (height > dc->rect.height) {
					height = dc->rect.height;
				}

				y += (dc->rect.height / 2 - height / 2);
				if (dc->align == UI_ALIGN_CENTER) {
					x += (dc->rect.width / 2 - width / 2);
				} else if (dc->align == UI_ALIGN_RIGHT) {
					x += (dc->rect.width - width);
				}
				info->x = x;
				info->y = y;
				int len = font_textout(info, (u8 *)text->str, text->strlen, x, y);
				ASSERT(len <= 255);
				text->displen = len;
			} else if (text->encode == FONT_ENCODE_UNICODE) {
				if (FT_ERROR_NONE == (info->sta & FT_ERROR_NOTABFILE)) {
					if (text->endian == FONT_ENDIAN_BIG) {
						info->bigendian = true;
					} else {
						info->bigendian = false;
					}
					int width = font_textw_width(info, (u8 *)text->str, text->strlen);
					int height;

					if (info->ascpixel.size) {
						height = info->ascpixel.size;
					} else if (info->pixel.size) {
						height = info->pixel.size;
					} else {
						ASSERT(0, "can't get the height of font.");
					}

					if (width > dc->rect.width) {
						width = dc->rect.width;
					}
					if (height > dc->rect.height) {
						height = dc->rect.height;
					}

					y += (dc->rect.height / 2 - height / 2);
					if (dc->align == UI_ALIGN_CENTER) {
						x += (dc->rect.width / 2 - width / 2);
					} else if (dc->align == UI_ALIGN_RIGHT) {
						x += (dc->rect.width - width);
					}

					info->x = x;
					info->y = y;
					int len = font_textout_unicode(info, (u8 *)text->str, text->strlen, x, y);
					ASSERT(len <= 255);
					text->displen = len;
				}
			} else {
				int width = font_textu_width(info, (u8 *)text->str, text->strlen);
				int height;

				if (info->ascpixel.size) {
					height = info->ascpixel.size;
				} else if (info->pixel.size) {
					height = info->pixel.size;
				} else {
					ASSERT(0, "can't get the height of font.");
				}

				if (width > dc->rect.width) {
					width = dc->rect.width;
				}
				if (height > dc->rect.height) {
					height = dc->rect.height;
				}

				y += (dc->rect.height / 2 - height / 2);
				if (dc->align == UI_ALIGN_CENTER) {
					x += (dc->rect.width / 2 - width / 2);
				} else if (dc->align == UI_ALIGN_RIGHT) {
					x += (dc->rect.width - width);
				}

				info->x = x;
				info->y = y;
				int len = font_textout_utf8(info, (u8 *)text->str, text->strlen, x, y);
				ASSERT(len <= 255);
				text->displen = len;
			}
		}
	} else if (text->format && !strcmp(text->format, "ascii")) {
		u32 w_sum;
		if (!text->str) {
			return 0;
		}
		if ((u8)text->str[0] == 0xff) {
			return 0;
		}

		if (dc->align == UI_ALIGN_CENTER) {
			w_sum = font_ascii_width_check(text->str);
			x += (dc->rect.width / 2 - w_sum / 2);
		} else if (dc->align == UI_ALIGN_RIGHT) {
			w_sum = font_ascii_width_check(text->str);
			x += (dc->rect.width - w_sum);
		}

		while (*text->str) {
			u8* pixbuf = dc->fbuf;
			int width;
			int height;
			int color;
			font_ascii_get_pix(*text->str, pixbuf, dc->fbuf_len, &height, &width);
			if (dc->data_format == DC_DATA_FORMAT_OSD16) {
				color  = text->color;
			} else if (dc->data_format == DC_DATA_FORMAT_MONO) {
				if (text->color == 0xad2a) {
					color = 0x55aa;//清显示
				} else {
					color = (text->color != 0x52d5) ? (text->color ? text->color : 0xffff) : 0x55aa;
				}
			}
			__font_pix_copy(dc, dc->data_format, 0, pixbuf, &draw_r, x, y, height, width, color);
			x += width;
			text->str++;
		}
	} else if (text->format && !strcmp(text->format, "strpic")) {
		u16 id = ((u8)text->str[1] << 8) | (u8)text->str[0];
		u8* pixbuf;
		int w;
		int h;
		if (id == 0xffff) {
			return 0;
		}
		if (open_string_pic(&file, id)) {
			return 0;
		}

		y += (dc->rect.height / 2 - file.height / 2);
		if (dc->align == UI_ALIGN_CENTER) {
			x += (dc->rect.width / 2 - file.width / 2);
		} else if (dc->align == UI_ALIGN_RIGHT) {
			x += (dc->rect.width - file.width);
		}

		pixbuf = dc->fbuf;
		if (!pixbuf) {
			return -ENOMEM;
		}

        disp.left   = x;
        disp.top    = y;
        disp.width  = file.width;
        disp.height = file.height;

        if(get_rect_cover(&draw_r,&disp,&r)) {
			if ((dc->data_format == DC_DATA_FORMAT_MONO) && (text->color == 0xad2a)) {
				if (__this->api->fill_rect) {
					__this->api->fill_rect(dc, 0xffff);
				}
			}
            for (h = 0; h < file.height; h+=8) {
                if (file.compress == 0) {
                    int offset = (h / 8) * file.width;
                    if (br23_read_str_data(&file, pixbuf, file.width, offset) != file.width) {
                        return -EFAULT;
                    }
                } else {
                    ASSERT(0,"the compress mode not support!");
                }
				int color;
				if (dc->data_format == DC_DATA_FORMAT_OSD16) {
					color  = text->color;
				} else if (dc->data_format == DC_DATA_FORMAT_MONO) {
					if (text->color == 0xad2a) {
						color = 0x55aa;//清显示
					} else {
						color = (text->color != 0x52d5) ? (text->color ? text->color : 0xffff) : 0x55aa;
					}
				}
				__font_pix_copy(dc, dc->data_format, 0, pixbuf, &r, x, y + h / 8 * 8, 8, file.width, color);
            }
        }
	} else if (text->format && !strcmp(text->format, "image")) {
		u8* pixbuf;
		u16 cnt = 0;
		u16 id = ((u8)text->str[1] << 8) | (u8)text->str[0];
		u32 w, h;
		int ww, hh;

		if (image_str_size_check(dc->page, text->str, &ww, &hh) != 0) {
			return -EFAULT;
		}
		if (dc->align == UI_ALIGN_CENTER) {
			x += (dc->rect.width / 2 - ww / 2);
		} else if (dc->align == UI_ALIGN_RIGHT) {
			x += (dc->rect.width - ww);
		}
		y += (dc->rect.height / 2 - hh / 2);
		while ((id != 0x00ff) && (id != 0xffff)) {
			if (open_image_by_id(&file, id, dc->page) != 0) {
				return -EFAULT;
			}
			if (dc->data_format == DC_DATA_FORMAT_MONO) {
				pixbuf = dc->fbuf;
				if (!pixbuf) {
					return -ENOMEM;
				}

				disp.left   = x;
				disp.top    = y;
				disp.width  = file.width;
				disp.height = file.height;

				if (get_rect_cover(&draw_r, &disp, &r)) {
                    int _offset = -1;
					for (h = 0; h < r.height; h++) {
						if (file.compress == 0) {
							int offset = (r.top + h - disp.top) / 8 * file.width + (r.left - disp.left);
                            if(_offset != offset) {
                                if (br23_read_image_data(&file, pixbuf, r.width, offset) != r.width) {
                                    return -EFAULT;
                                }
                                _offset = offset;
                            }
						} else {
							ASSERT(0, "the compress mode not support!");
						}
						for (w = 0; w < r.width; w++) {
							u8 color = (pixbuf[w] & BIT((r.top + h - disp.top) % 8)) ? 1 : 0;
							if (color) {
								if (platform_api->draw_point) {
									platform_api->draw_point(dc, r.left + w, r.top + h, color);
								}
							}
						}
					}
				}
			} else if (dc->data_format == DC_DATA_FORMAT_OSD16) {
				pixbuf = dc->fbuf;
				if (!pixbuf) {
					return -ENOMEM;
				}

                disp.left   = x;
                disp.top    = y;
                disp.width  = file.width;
                disp.height = file.height;

                if(get_rect_cover(&draw_r,&disp,&r)) {
                    for(h = 0; h < r.height; h++) {
                        if (file.compress == 0) {
                            int offset = (r.top + h - disp.top) * file.width * 2 + (r.left - disp.left) * 2;
                            if (br23_read_image_data(&file, pixbuf, r.width * 2, offset) != r.width * 2) {
                                return -EFAULT;
                            }
                        } else {
                            ASSERT(0,"the compress mode not support!");
                        }
                        for(w = 0; w < r.width; w++) {
                            u16 color = (pixbuf[w * 2 + 1] << 8) | (pixbuf[w * 2]);
                            if (color) {
                                if (platform_api->draw_point) {
                                    platform_api->draw_point(dc, r.left + w, r.top + h, color);
                                }
                            }
                        }
                    }
                }
			}
			x += file.width;
			cnt += 2;
			id = ((u8)text->str[cnt + 1] << 8) | (u8)text->str[cnt];
		}
	}

	return 0;
}


int br23_draw_point(struct draw_context* dc, u16 x, u16 y, u32 pixel)
{
	if (dc->data_format == DC_DATA_FORMAT_OSD16) {
	int offset = (y - dc->disp.top) * dc->disp.width + (x - dc->disp.left);

    /* ASSERT((offset * 2 + 1) < dc->len, "dc->len:%d", dc->len); */
    if((offset * 2 + 1) >= dc->len) {
        return -1;
    }

	dc->buf[offset * 2    ] = pixel >> 8;
	dc->buf[offset * 2 + 1] = pixel;
	} else if (dc->data_format == DC_DATA_FORMAT_MONO) {
		ASSERT(x < __this->info.width);
		ASSERT(y < __this->info.height);
		if ((x >= __this->info.width) || (y >= __this->info.height)) {
			return -1;
		}

        if (pixel & BIT(31)) {
			dc->buf[y / 8 * __this->info.width + x] ^= BIT(y % 8);
        } else if (pixel == 0x55aa) {
			dc->buf[y / 8 * __this->info.width + x] &=~BIT(y % 8);
        } else if (pixel) {
			dc->buf[y / 8 * __this->info.width + x] |= BIT(y % 8);
		} else {
			dc->buf[y / 8 * __this->info.width + x] &=~BIT(y % 8);
		}
	}

	return 0;
}

int br23_open_device(struct draw_context* dc, const char* device)
{
	return 0;
}

int br23_close_device(int fd)
{
	return 0;
}

static const struct ui_platform_api br23_platform_api = {
	.malloc             = br23_malloc,
	.free               = br23_free,

	.load_style         = br23_load_style,
	.load_window        = br23_load_window,
	.unload_window      = br23_unload_window,

	.open_draw_context  = br23_open_draw_context,
	.get_draw_context   = br23_get_draw_context,
	.put_draw_context   = br23_put_draw_context,
	.set_draw_context   = br23_set_draw_context,
	.close_draw_context = br23_close_draw_context,

    .load_widget_info   = br23_load_widget_info,
    .load_css           = br23_load_css,
    .load_image_list    = br23_load_image_list,
    .load_text_list     = br23_load_text_list,

	.fill_rect          = br23_fill_rect,
	.draw_rect          = br23_draw_rect,
	.draw_image         = br23_draw_image,
	.show_text          = br23_show_text,
	.draw_point         = br23_draw_point,
    .invert_rect        = br23_invert_rect,

	.open_device        = br23_open_device,
	.close_device       = br23_close_device,

	.set_timer          = br23_set_timer,
	.del_timer          = br23_del_timer,

	.file_browser_open  = NULL,
	.get_file_attrs     = NULL,
	.set_file_attrs     = NULL,
	.show_file_preview  = NULL,
	.move_file_preview  = NULL,
	.clear_file_preview = NULL,
	.flush_file_preview = NULL,
	.open_file          = NULL,
	.delete_file        = NULL,
	.file_browser_close = NULL,
};




static int open_resource_file()
{
    /* while (!fdir_exist("mnt/sdfile/res")) { */
        /* printf("%s isn't exist!!!\n", "mnt/sdfile/res"); */
        /* os_time_dly(10); */
    /* } */
    open_resfile("mnt/sdfile/res/menu.res");
    open_str_file("mnt/sdfile/res/str.res");
    font_ascii_init("mnt/sdfile/res/ascii.res");
    return 0;
}

int __attribute__((weak)) lcd_get_scrennifo(struct fb_var_screeninfo *info)
{
    info->s_xoffset = 0;
    info->s_yoffset = 0;
    info->s_xres = 240;
    info->s_yres = 240;

    return 0;
}

int ui_platform_init(void *lcd)
{
    struct rect rect;
    struct lcd_info info = {0};

#ifdef UI_BUF_CALC
	INIT_LIST_HEAD(&buffer_used.list);
#endif

    __this->api = &br23_platform_api;
    ASSERT(__this->api->open_draw_context);
    ASSERT(__this->api->get_draw_context);
    ASSERT(__this->api->put_draw_context);
    ASSERT(__this->api->set_draw_context);
    ASSERT(__this->api->close_draw_context);


    __this->lcd = lcd_get_hdl();
    ASSERT(__this->lcd);
    ASSERT(__this->lcd->init);
    ASSERT(__this->lcd->get_screen_info);
    ASSERT(__this->lcd->buffer_malloc);
    ASSERT(__this->lcd->buffer_free);
    ASSERT(__this->lcd->draw);
    ASSERT(__this->lcd->set_draw_area);

    if(__this->lcd->init) {
        __this->lcd->init(lcd);
    }

    if(__this->lcd->backlight_ctrl) {
        __this->lcd->backlight_ctrl(true);
    }

    if(__this->lcd->get_screen_info) {
        __this->lcd->get_screen_info(&info);
    }
    rect.left   = 0;
    rect.top    = 0;
    rect.width  = info.width;
    rect.height = info.height;

    printf("ui_platform_init :: [%d,%d,%d,%d]\n", rect.left, rect.top, rect.width, rect.height);

    ui_core_init(__this->api, &rect);

    return 0;
}



int ui_style_file_version_compare(int version)
{
    int v;
    int len;
    struct ui_file_head head;
    static u8 checked = 0;

    if (checked == 0) {
        if (!ui_file) {
            puts("ui version_compare ui_file null!\n");
            ASSERT(0);
            return 0;
        }
        fseek(ui_file, 0, SEEK_SET);
        len = sizeof(struct ui_file_head);
        fread(ui_file, &head, len);
        printf("style file version is: 0x%x,UI_VERSION is: 0x%x\n", *(u32 *)(head.res), version);
        if (*(u32 *)head.res != version) {
            puts("style file version is not the same as UI_VERSION !!\n");
            ASSERT(0);
        }
        checked = 1;
    }
    return 0;
}




 /*
    window.c - window handling and drawing

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "window.h"
#include "fonty.h"
#include "keyboard.h"
#include "lcd.h"
#include "timerirq.h"
#include "mpd.h"
#include "screen.h"

/* 
TODO comment no longer valid
These are the window manager functions.
The window manager knows all about the current layout of the screen and handles it. 

The windows are organized in a list (an array).
So we may do some operations for all windows by iterating through this list.
Might as well be a linked list or something.
*/

#define WIN_DEFAULT_TXT_SIZE 80

/* Draw a 1 pixel wide black border at the edge of a window. */
void
win_draw_border(struct Window *win){
	if ( (win->border) && (win->flags & WINFLG_VISIBLE) )
		draw_rect(win->start_col, win->start_row, win->width, win->height, 1, BLACK);	
};
	

/* Initializes a window.  
	Each window is NOT visible by default and is assumed to be a TEXT type window
	This routine just sets some sensible defaults for a window.
*/
void 
win_init(struct Window *win, uint8 row, uint8 col, uint8 height, uint8 width, uint8 border, char *txt){
	win->start_row = row;
	win->start_col = col;
	win->height = height;
	win->width = width;
	win->fg_color = BLACK;
	win->bg_color = WHITE;
	win->border = border;
	win->flags = WINFLG_TEXT;
	win->fits = 1;
	win->scroll = 0;
	win->font = MEDIUMFONT;
	win->buffer_lim = WIN_DEFAULT_TXT_SIZE;
	win->cur_char = 0;
	win->txt = txt;
	txt[0] = '\0';
	win->txt_offs_row = 1;
	win->txt_offs_col = 2;
	win->cur_col = win_txt_col(win);
};


/* Clears visible content of window to background color.
	Normally this means to set the window to the background color.
	But if window type is WINFLG_FRAME and WINFLG_HIDE is set, the foreground color is used to clear the window.
	If clr_border is 1, the border (if any) will also be cleared.
*/
void 
win_clear(struct Window *win, int clr_border){
	int border = clr_border ? 0 : win->border;
	int frame = (win->flags & WINFLG_FRAME) ? 1 : 0;
	int color = ((win->flags & WINFLG_FRAME) && (win->flags & WINFLG_HIDE)) ? win->fg_color : win->bg_color;
	
	if (! (win->flags & WINFLG_VISIBLE)) return;
	
	draw_block(win->start_row+border+frame, win->start_col+border+frame, 
			  win->width-2*border-2*frame, win->height-2*border-2*frame, color);
};


/* Draw the current character at the current text position in a window 
	Increaes cur_char and cur_col if character did fit.
	After end of text draws 4 spaces and then resets cur_char to start of text.
	Returns 1 if the character did fit into the window, else 0.
	Assumes that the font is already set.
*/
static uint8_t 
draw_cur_char(struct Window *win){
	uint8_t res;
	char ch;
	
	/* If we are past the text length, draw space */
	if (win->cur_char >= win->text_len) ch = ' ';
	else ch = win->txt[win->cur_char];
	
	res = draw_char(win_txt_row(win), win->cur_col, ch, win->fg_color, win->bg_color,  txt_col_lim(win) - win->cur_col ); 
	
	// if it does not fit completely, return 0.
	if (res == 0) return 0;
	
	// if it fits completely in remaining window space,
	// increase current character column
	win->cur_col += res;
	
	/* select next character */
	/* After we have drawn 4 spaces beyond the current length, we continue with the beginning of the text */
	if (++win->cur_char >= win->text_len + 4)
		win->cur_char = 0;
	
	return 1;
};

/* We assume the cursor is at the current character column.
	We also use the cur_char.
	The cursor is not advanced after drawing.
*/	
static uint8_t
draw_cursor(struct Window *win, uint8_t reversed){
	char ch;
	
	/* If we are past the text length, draw space */
	if (win->cur_char >= win->text_len) ch = ' ';
	else ch = win->txt[win->cur_char];
	
	if (reversed)
		draw_char(win_txt_row(win), win->cur_col, ch, win->bg_color, win->fg_color, txt_col_lim(win) - win->cur_col ); 
	else	
		draw_char(win_txt_row(win), win->cur_col, ch, win->fg_color, win->bg_color,  txt_col_lim(win) - win->cur_col ); 
	
	return 1;
};

/* 	Clear window (except border) and draw the contents completely (except border) new. Use text in win->txt
	Sets win->fits to 1 if the text fits in window, else sets it to 0.
	Text is centered in window (horizontally as well as vertically)
*/
static void
win_center_text(struct Window *win){
	int txt_height, txt_offs, cur_row;
		

	/* center vertically */
	txt_height = draw_text_space(win->txt, win_txt_row(win), win->start_col + win->border, 
								 win->width - 2*win->border, win->height - 2*win->border, win->fg_color, win->bg_color, 1);

	txt_offs = ( win_txt_height(win) - txt_height) / 2;
	
	if (txt_offs >= 0)
		cur_row  = win_txt_row(win) + txt_offs;
	else 
		cur_row  = win_txt_row(win);
	
	 draw_text_space(win->txt, cur_row, win->start_col + win->border, 
					 win->width - 2*win->border, win->height - 2*win->border, win->fg_color, win->bg_color, 0);

};

/* 	Clear window (except border) and draw the contents completely (except border) new. Use text in win->txt
	Sets win->fits to 1 if the text fits in window, else sets it to 0.
	Honors WINFLG_CENTER, WINFLG_VISIBLE and WINFLG_HIDE.
*/
void 
win_draw_text(struct Window *win){
	if (! (win->flags & WINFLG_VISIBLE)) return;
	if (win->txt == NULL) return; 	
	win_clear(win, 0);
	if (win->flags & WINFLG_HIDE) return;
	
	set_font(win->font);
	
	if (win->flags & WINFLG_CENTER)
		return win_center_text(win);
	
	win->cur_char = 0;
	win->cur_col = win_txt_col(win);
	win->fits = 1;
	while (win->cur_char < win->text_len){
		if (draw_cur_char(win) == 0) {
			win->fits = 0;
			break;
		};
	};
};
		

/* We are given a new text 
	If it is different from current text in window, 
	copy text to window and redraw window
*/
void 
win_new_text(struct Window *win, char *s){
	/* We copy the new text to our internal buffer with clipping if necessary
		We also check if the text currently in buffer is identical
	*/
	if ( strn_cpy_cmp(win->txt, s, win->buffer_lim - 1, &(win->text_len)) != 0)
		return;			// the two strings are identical, do nothing

	win_draw_text(win);
};

void
win_redraw(struct Window *win){
	if (! (win->flags & WINFLG_VISIBLE)) return;
	win_draw_border(win);
	if (win->flags & WINFLG_TEXT)
		win_draw_text(win);
	else if (win->flags & WINFLG_BAR){
		draw_progressbar(win->start_row+1, win->start_col, win->height-2, win->fg_color, win->bg_color, 100, 0);
		draw_progressbar(win->start_row+1, win->start_col, win->height-2, win->fg_color, win->bg_color, 0, win->cur_char);
	} else if (win->flags & WINFLG_RAMP){			
			trianglebar_border(win->start_row+1, win->start_col+1, win->width-2, win->height-2, win->fg_color, win->bg_color);
			draw_trianglebar(win->start_row+1, win->start_col+1, win->width-2, win->height-2, win->fg_color, win->bg_color, 0, win->cur_char);
	};
};

/* ------------------------------ Windows with scrolling text ------------------------------------------------------- */

/* ### Scroll Task ###  
	Some windows might need scrolling to show their full content.
	This task regularily (after a timer has expired) checks each window in turn if it needs scrolling.
	If so, the window is scrolled and the task yields.
	If not, we wait for next timer expiration.
	So we make sure that at most 1 window is scrolled before we are yielding to minimize the time spent in this task.

	TODO If the font of a window has changed, check again if the text fits!

	We do not check all available windows, that would be a logistic nightmare.
	It would also look bad on screen.
	We keep a small list (say 10) of windows to check.
	If you want a window to scroll, add it with win_scroll() and remove it with win_unscroll().

	TODO scrolling currently looks blurred.
	TODO improve that (maybe scrolling single characters at a time ?)
*/
#define SCROLL_LIST_MAX 10
static struct Window *hor_scroll_list[SCROLL_LIST_MAX + 1];

#define SCROLL_OFFSET 8
static void 
win_do_scroll(struct Window *win){ 	
	int res;
	
	set_font(win->font);
	
	scroll(win_txt_row(win), win_txt_col(win), win_txt_width(win), SCROLL_OFFSET);
	
	if (win->cur_col - SCROLL_OFFSET >= 0)
		 win->cur_col -= SCROLL_OFFSET;
	else
		win->cur_col = 0;
	
	do {
		res = draw_cur_char(win);
	} while (res);
};	

/* Insert a window into the scroll list (if a place is free) */
void 
win_scroll(struct Window *win){
	int i;
	for (i=0; i < SCROLL_LIST_MAX; i++){
		if (hor_scroll_list[i] == NULL){
			hor_scroll_list[i] = win;
			return;
		};
	};
};

/* Remove a window from the scroll list */	
void 
win_unscroll(struct Window *win){
	int i;
	for (i=0; i < SCROLL_LIST_MAX; i++){
		if (hor_scroll_list[i] == win){
			hor_scroll_list[i] = NULL;
			return;
		};
	};
};

#define SCROLL_PERIOD (8 * (TICKS_PER_TENTH_SEC))

/* ### Scroll Task ###  */
PT_THREAD (win_scroll_all(struct pt *pt)){
	static int i;
	static struct timer scroll_tmr;
	
	PT_BEGIN(pt);
		
	timer_add(&scroll_tmr, SCROLL_PERIOD, SCROLL_PERIOD);
	while(1){
		for (i=0; i < SCROLL_LIST_MAX; i++){
			PT_WAIT_UNTIL(pt, timer_expired(&scroll_tmr));
			if (hor_scroll_list[i] == NULL) continue;
			if (! (hor_scroll_list[i]->flags & WINFLG_VISIBLE)) continue; 
			if ( ! hor_scroll_list[i]->fits ){
				win_do_scroll(hor_scroll_list[i]);
				PT_YIELD(pt);
			};
		};
		/* We have checked (and potentially scrolled each window). Timer can expire again. */
		scroll_tmr.expired=0;
	};
	PT_END(pt);
};

void
win_scroll_init(){
	int i;
	for (i=0; i < SCROLL_LIST_MAX; i++)
		hor_scroll_list[i] = NULL;
	
	task_add(&win_scroll_all);
};	

/* ---------------------------------------------- Scroll List ---------------------------------------- */
/* 
	An array of windows can form a scroll list, i.e. the user can scroll in this list.
*/

/* The height of a non-selected window in the scroll list */
#define WL_NORMAL_HEIGHT 13

/* The height of a selected window in the scroll list */
#define WL_HIGH_HEIGHT 18

/* The given window has its properties set so that it is selected */
void 
select_win(struct Window *w ){
	w->font = BIGFONT;
	w->height = WL_HIGH_HEIGHT;
	w->border = 1;
};

/* The given window has its properties set so that it is not selected */
void
unselect_win(struct Window *w ){
	w->font = MEDIUMFONT;
	w->height = WL_NORMAL_HEIGHT;
	w->border = 0;
};

/* The selected window is moved one line up */
void
sel_win_up(scroll_list *sl){
	if (sl->sel_win >= 1){
		unselect_win( & (sl->wl[sl->sel_win]) );
		sl->wl[sl->sel_win].start_row = sl->wl[sl->sel_win - 1].start_row + WL_HIGH_HEIGHT;
		win_unscroll(& (sl->wl[sl->sel_win]));
		sl->sel_win--;
		select_win( & (sl->wl[sl->sel_win]) );
		win_scroll(& (sl->wl[sl->sel_win]) );				
		win_redraw(& (sl->wl[sl->sel_win]) );
		win_redraw(& (sl->wl[sl->sel_win + 1]) );
	};
};

/* The selected window is moved one line down */
void
sel_win_down(scroll_list *sl){
	if (sl->sel_win < (sl->num_windows - 1) ){
		unselect_win( & (sl->wl[sl->sel_win]) );
		win_unscroll(& (sl->wl[sl->sel_win]) );
		sl->sel_win++;
		select_win( & (sl->wl[sl->sel_win]) );
		sl->wl[sl->sel_win].start_row = sl->wl[sl->sel_win -1].start_row + WL_NORMAL_HEIGHT;
		win_scroll(& (sl->wl[sl->sel_win]) );
		win_redraw(& (sl->wl[sl->sel_win]) );
		win_redraw(& (sl->wl[sl->sel_win - 1]) );
	};
};

/* Set a specific window as the selected one */
void 
select(scroll_list *sl, int sel){
	while ( sl->sel_win < sel)
		sel_win_down(sl);
		
	while ( sl->sel_win > sel)
		sel_win_up(sl);
};

/* Index of last info text that we can show in our list */
int 
last_info_idx(scroll_list *sl){
	return (sl->first_info_idx + sl->num_windows -1); 
};	


/* Gets the info index corresponding to a given window index */
int
info_idx(scroll_list *sl, int win_idx){
	int no;
	
	no = win_idx + sl->first_info_idx;
	if ( (no < 0) || (no > last_info_idx(sl) ) )
		return -1;
	return no;
};


/* We are called when the current tracklist has potentially changed 
	Updates the info in all windows of the scroll list (sets new text in window).
	NOTE	Here we are updating the whole list. Could be inefficient.
			Maybe better to update one song at a time.
*/
void
scroll_list_changed(scroll_list *sl){
	int i;
	char *info;
	struct Window *pwin;

	for (i=0, pwin = sl->wl; i < sl->num_windows; pwin++, i++){
		info = sl->info_text( info_idx(sl, i) );

		if (NULL == info)
			win_new_text(pwin, "");
		else 
			win_new_text( pwin, info);
	}
};

/*
Parameters:
	sl = pointer to scroll_list structure
	pwl = pointer to the array of windows belonging to the scroll_list
	win_txt = pointer to array with space for window texts
	win_txt_len = size of each win_txt entry
	nw = number of windows in the scroll_list
	(*info_text)() = function which returns the text of a specified info index
	start_row = where shall the scroll_list start on screen
*/
void 
init_scroll_list(scroll_list *sl, struct Window *pwl, char *win_txt, int win_txt_len, int nw, char * (*info_text) (), int start_row ) {
	int i;
	struct Window *pwin;

	sl->wl = pwl;
	sl->num_windows = nw;
	sl->first_info_idx = 0;
	sl->sel_win= 0;
	sl->info_text = info_text;
	
	/* These windows are scroll list lines */
	for (i=0, pwin = sl->wl; i < sl->num_windows; pwin++, i++){

		if (i==sl->sel_win){
			/* The selected window is bigger than usual */ 	
			win_init(pwin, start_row, 0, WL_HIGH_HEIGHT, 128, 1, &(win_txt[i*win_txt_len]));
			select_win(pwin);
		} else {
			win_init(pwin, start_row, 0, WL_NORMAL_HEIGHT, 128, 0, &(win_txt[i*win_txt_len]));
			unselect_win(pwin);
		};
		start_row += pwin->height;
		/* Every second window has a different background color */
		if (0 == (i & 1)) pwin->bg_color = LIGHT_GREY;
	};
	win_scroll( & ((sl->wl)[sl->sel_win]));
};

/* --------------------------------------------- Text Input Window ------------------------------------- */
/*
// got_input(k) user pressed key k 
    show k in window at current cursor position

We need a blink task. Cursor blinks in selected window at 1|2 HZ


*/
#define BLINK_PERIOD (5 * TICKS_PER_TENTH_SEC)

#define WAIT_KEY_TIME (25 * TICKS_PER_TENTH_SEC)

/* We only have one window where the cursor blinks */
static struct Window *pcursor_win = NULL;
/* Maximum length of text for this window */
static int max_txt_len;

static int cursor_new_pos = -1;
static int cursor_pos = 0;				// position of cursor within text string (0 == first character)
static int cursor_col = 0;				// column on screen where cursor has to be drawn

static int last_key = -1;
static int key_cnt = -1;

static struct timer char_tmr;

/* Redraws window and remembers cursor_col, win->cur_char and win->cur_col 
	The variable cursor_pos tells us at which position the cursor currently is.
	cursor_pos >= 0 and <= win->text_len.
	If cursor_pos == win_text_len, the cursor is past the end of string (at NULL character).
	This routine will nevertheless set the correct column for cursor_col.
	
*/
static void
draw_cursor_win(struct Window *win){
	if (NULL == win) return;
	 
	win_clear(win, 0);
	set_font(win->font);
	
	cursor_col = -1;

	win->cur_char = 0;
	win->cur_col = win_txt_col(win);
	win->fits = 1;
	
	while (win->cur_char < win->text_len){
		
		// remember at which column the cursor has to be drawn
		if (win->cur_char == cursor_pos)
			cursor_col = win->cur_col;

		if (draw_cur_char(win) == 0) {
			win->fits = 0;
			break;
		};
	};
	if (cursor_col == -1)			// we were past the end of string
		cursor_col = win->cur_col;
	
	win->cur_char = cursor_pos;
	win->cur_col = cursor_col;
};


/* If pos is at end of string, increase text length (append a space) 
	Returns 0 if text length limit was reached

	Real name: increase text when necessary and possible
*/
static int
inc_txt(int pos){
	if (0 == pcursor_win->txt[pos]){
		if (pos >= max_txt_len )
			return 0;
		pcursor_win->txt[pos] = ' ';
		pcursor_win->txt[pos + 1] = 0;
		pcursor_win->text_len++;
	};
	return 1;
};

/* Returns 1 if cursor could be advanced */
static int
advance_cursor(){
	/* We must not move beyond end of string */
	if (0 == pcursor_win->txt[cursor_pos])
		return 0;
	
	/* Move to next position in text if possible ? */
	if (cursor_pos < (max_txt_len - 1) ){
		cursor_new_pos = cursor_pos + 1;
		return 1;
	} else
		cursor_new_pos = cursor_pos;
	return 0;
};

/* Change the character at pos */
static void
store_char(int pos, char c){
	if (!inc_txt(pos)) return;					// no space for a character
	pcursor_win->txt[pos] = c;
	cursor_new_pos = pos;						// Cursor stays the same, but text has changed
	timer_set(&char_tmr, WAIT_KEY_TIME, 0);		// New character, start auto advance timer
}

/*
	1. Nothing has changed. Wait for time out. Draw cursor on/off. Repeat.
	2. New cursor position or new char at position (new_pos >= 0): Redraw string, remember new cursor column. Goto 2.
	3. Character Timer has expired. New cursor position + 1. Goto 2
*/

/* ### Cursor Blinking Task ###  */
PT_THREAD (win_cursor_blink(struct pt *pt)){
	static struct timer tmr;
	static uint8_t cursor_on=0;
	 
	PT_BEGIN(pt);
		
	timer_add(&tmr, BLINK_PERIOD, BLINK_PERIOD);
	
	while(1){
		tmr.expired = 0;
		cursor_on ^= 1;	
		
		PT_WAIT_UNTIL(pt, ( timer_expired(&tmr) || timer_expired(&char_tmr) || (cursor_new_pos >= 0) ) );
		
		if (pcursor_win == NULL) {
			timer_stop(&char_tmr);
			continue;
		};
		
		if (! (pcursor_win->flags & WINFLG_VISIBLE)) continue;
		if (pcursor_win->txt == NULL) continue; 	
		if (pcursor_win->flags & WINFLG_HIDE) continue;
		
		/* Check if we should advance the cursor because user did not press a key again within time */
		if ( (cursor_new_pos < 0) && timer_expired(&char_tmr)) {	
			advance_cursor();
			timer_stop(&char_tmr);
			key_cnt = -1;
		};
		
		/* Set cursor to a new position ? */
		if (cursor_new_pos >= 0){
			cursor_pos = cursor_new_pos;
			draw_cursor_win(pcursor_win);
			cursor_new_pos = -1;
		};
		
		set_font(pcursor_win->font);
		draw_cursor(pcursor_win, cursor_on);
	};
	PT_END(pt);
};

/* Giving NULL as parameter stops cursor blinking for this window. */
void
win_cursor_set(struct Window *pwin, int size){
	pcursor_win = pwin;
	max_txt_len = size;
	cursor_new_pos = -1;
	cursor_pos = 0;						// TODO better jump to end of string
	draw_cursor_win(pcursor_win);
	last_key = -1;
	key_cnt = -1;
};

void
win_cursor_init(){
	timer_add(&char_tmr, 0, 0);
	timer_stop(&char_tmr);
	task_add(&win_cursor_blink);
};	


static char
key2char(int key, int cnt){
//	const char key_table[10][5] = {" ", ".-@", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz"};
	const char key_table[10][5] = {" ", ".-@", "ABC", "DEF", "GHI", "JKL", "MNO", "PQRS", "TUV", "WXYZ"};
	cnt = cnt % strlen(key_table[key]) ;
	return key_table[key][cnt];
};

/* 
	Our window with cursor has got some input
	
	key_cnt == -1 means no input has been taking place at this cursor position
	key_cnt >= 0 means the same key has been pressed one or more times here
*/
void
win_cursor_input(int new_key){
	char c;
	
	if (new_key == CURSOR_LEFT){
		if (cursor_pos <= 0) return;
		cursor_new_pos = cursor_pos - 1;
		key_cnt = -1;
		last_key = -1;
		timer_stop(&char_tmr);
		return;
	};
	
	if (new_key == CURSOR_RIGHT){
		/* We must not move beyond end of string */
		if (advance_cursor()){
			cursor_new_pos = cursor_pos + 1;
			key_cnt = -1;
			last_key = -1;
			timer_stop(&char_tmr);
		};
		return;
	};	
		
	if (new_key == CURSOR_BACKSPACE){
		if (cursor_pos <= 0) return;
		cursor_new_pos = cursor_pos - 1;
		key_cnt = -1;
		last_key = -1;
		timer_stop(&char_tmr);
		pcursor_win->text_len = str_del(pcursor_win->txt, cursor_new_pos);
		return;	
	};
		
	/* This is the first time a key is pressed here */
	if (key_cnt < 0){
		key_cnt = 0;
		c = key2char(new_key, 0);
		store_char(cursor_pos, c);
		
	} else {
		/* We already entered text at this cursor position */	
		if (new_key == last_key){			// User pressed the same key twice before a time out occured 
			key_cnt++;
			c = key2char(new_key, key_cnt);
			store_char(cursor_pos, c);

		} else {
		// User pressed a different key, advance to the next cursor position, store new char
			key_cnt = 0;
			c = key2char(new_key, 0);
			advance_cursor();
			store_char(cursor_new_pos, c);
		};
	};
	last_key = new_key;		
};


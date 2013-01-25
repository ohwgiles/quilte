#include <string.h>  // memmove
#include <wctype.h>

#include "qvtermwidget.hpp"
#include "vtermcallbacks.hpp"
#include "keyconversion.hpp"

#include <QDebug>
#include <QScrollBar>
#include <QSettings>
#include <QPaintEvent>
#include <QPainter>
#include <QClipboard>
#include <QApplication>
#include <QSocketNotifier>
#include <QFontMetrics>
#include <errno.h>
#include <fcntl.h>
#include <stropts.h>
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <pty.h>
#include <signal.h>
#include <sys/time.h>


/* To accomodate scrollback scrolling, we'll adopt the convention that VTermPos
 * and VTermRect instances always refer to virtual locations within the
 * VTermScreen buffer (or our scrollback buffer if row is negative), and
 * PhyPos and PhyRect instances refer to physical onscreen positions
 */

VTermPos QVTermWidget::vtermPos(PhyPos physical) {
	VTermPos pos;
	pos.row = physical.prow - scrollOffset_;
	pos.col = physical.pcol;
	return pos;
}


PhyRect QVTermWidget::phyRect(VTermRect rect) {
	PhyRect prect;

	prect.start_prow = rect.start_row + scrollOffset_;
	prect.end_prow   = rect.end_row   + scrollOffset_;
	prect.start_pcol = rect.start_col;
	prect.end_pcol   = rect.end_col;
	return prect;
}

void QVTermWidget::flushOutput() {
	size_t bufflen = vterm_output_get_buffer_current(vTerm_);
	if(bufflen) {
		char buffer[bufflen];
		bufflen = vterm_output_bufferread(vTerm_, buffer, bufflen);
		::write(childFd_, buffer, bufflen);
	}
}

void QVTermWidget::pushString(QString str) {
	QVector<uint> unicode = str.toUcs4();
	foreach(uint c, unicode) {
		/* 6 bytes is always enough for any UTF-8 character */
		if(vterm_output_get_buffer_remaining(vTerm_) < 6)
			flushOutput();
		vterm_input_push_char(vTerm_, (VTermModifier)0, c);
	}
	flushOutput();
}

void QVTermWidget::fetchCell(VTermPos pos, VTermScreenCell *cell) {
	if(pos.row < 0) {
		/* pos.row == -1 => sb_buffer[0], -2 => [1], etc... */
		ScrollbackLine *sb_line = scrollbackBuffer_[-pos.row-1];
		if(pos.col < sb_line->cols)
			*cell = sb_line->cells[pos.col];
		else {
			*cell = (VTermScreenCell) { { 0 } };
			cell->width = 1;
			cell->bg = sb_line->cells[sb_line->cols - 1].bg;
		}
	} else {
		vterm_screen_get_cell(vTermScreen_, pos, cell);
	}
}

QString QVTermWidget::getText(VTermPos from, VTermPos to) {
	bool end_blank = false;
	QVector<uint> content;
	VTermPos pos;
	pos.row = from.row;
	pos.col = from.col;
	while(pos.row <= to.row && pos.col <= to.col) {
		VTermScreenCell cell;
		fetchCell(pos, &cell);
		for(int i=0; cell.chars[i]; ++i)
			content.append(cell.chars[i]);

		end_blank = !cell.chars[0];

		pos.col += cell.width;
		if(pos.col > numCols_) {
			++pos.row;
			pos.col = 0;
		}
	}

	if(end_blank)
		content.append('\n');

	return QString::fromUcs4(content.constData());
}


bool QVTermWidget::isWordChar(uint32_t c) {
	if(doubleClickFullword_)
		return c && !iswspace(c);
	else
		return iswalnum(c) || (c == '_');
}

QRect QVTermWidget::displayRect(PhyPos pos) {
	return QRect(pos.pcol * cellHeight_,
					 pos.prow * cellWidth_,
					 cellHeight_,
					 cellWidth_);
}

QRect QVTermWidget::displayRect(VTermPos pos) {
	return QRect(pos.col * cellHeight_,
					 pos.row * cellWidth_,
					 cellHeight_,
					 cellWidth_);

}

QRect QVTermWidget::displayRect(VTermRect rect) {
	if(rect.start_row == rect.end_row) {
		return QRect(rect.start_col * cellHeight_, rect.start_row * cellWidth_, (rect.end_col-rect.start_col) * cellHeight_, cellWidth_);
	} else {
		return QRect(0, rect.start_row * cellWidth_, rect.end_col * cellHeight_, rect.end_row*cellWidth_);
	}
}

QRect QVTermWidget::displayRect(PhyRect rect) {
	return
			QRect(rect.start_pcol * cellHeight_,rect.start_prow * cellWidth_,(rect.end_pcol - rect.start_pcol) * cellHeight_,(rect.end_prow - rect.start_prow) * cellWidth_);
}

void QVTermWidget::paintEvent(QPaintEvent * e) {
	QPainter p(viewport());
	PhyPos ph_pos;

	// clear the background first
	p.fillRect(e->rect(), backgroundColour_);

	int xmin = e->rect().x() / cellHeight_;
	int ymin = e->rect().y() / cellWidth_;
	int xmax = e->rect().width() / cellHeight_ + xmin;
	int ymax = e->rect().height() / cellWidth_ + ymin;

	for(ph_pos.prow = ymin; ph_pos.prow < ymax; ph_pos.prow++) {
		for(ph_pos.pcol = xmin; ph_pos.pcol < xmax; ) {
			VTermPos pos = vtermPos(ph_pos);

			VTermScreenCell cell;
			fetchCell(pos, &cell);

			/* Invert the RV attribute if this cell is selected */
			if(hasHighlight_) {
				VTermPos start = highlightStartPos_, stop = highlightEndPos_;

				int highlighted = (pos.row > start.row || (pos.row == start.row && pos.col >= start.col)) &&
						(pos.row < stop.row  || (pos.row == stop.row  && pos.col <= stop.col));

				if(highlighted)
					cell.attrs.reverse = !cell.attrs.reverse;
			}

			int cursor_here = pos.row == cursorPosition_.row && pos.col == cursorPosition_.col;
			int cursor_visible = cursorBlinkState_ ;//|| !hasFocus();

			QFont fnt(font());
			if(cell.attrs.bold)
				fnt.setWeight(QFont::Bold);
			if(cell.attrs.underline)
				fnt.setUnderline(true);
			if(cell.attrs.italic)
				fnt.setItalic(true);
			if(cell.attrs.strike)
				fnt.setStrikeOut(true);

			QColor fg(cell.fg.red,cell.fg.green,cell.fg.blue);
			QColor bg(cell.bg.red,cell.bg.green,cell.bg.blue);

			if(cell.attrs.reverse || (cursor_visible && cursor_here && cursorShape_ == VTERM_PROP_CURSORSHAPE_BLOCK)) {
				p.setPen(bg);
				p.setBackground(fg);
			} else {
				p.setPen(fg);
				p.setBackground(bg);
			}

			p.setFont(fnt);

			if(cell.chars[0] != 0) { // there is something here
				QString chr = QString::fromUcs4(cell.chars);
				p.fillRect(ph_pos.pcol*cellHeight_, ph_pos.prow*cellWidth_,cellHeight_*cell.width,cellWidth_,p.background());
				p.drawText(ph_pos.pcol*cellHeight_, ph_pos.prow*cellWidth_,cellHeight_*cell.width,cellWidth_,0,chr);
			} else if(cursor_here && cursorShape_ == VTERM_PROP_CURSORSHAPE_BLOCK) {
				p.fillRect(ph_pos.pcol*cellHeight_, ph_pos.prow*cellWidth_,cellHeight_*cell.width,cellWidth_, cursor_visible ? fg : bg);
			}

			if(cursor_visible && cursor_here && cursorShape_ != VTERM_PROP_CURSORSHAPE_BLOCK) {
				QRect cursor_area = displayRect(ph_pos);
				switch(cursorShape_) {
				case VTERM_PROP_CURSORSHAPE_UNDERLINE:
					p.fillRect(cursor_area.x(), cursor_area.y()+cursor_area.height()*0.85, cursor_area.width(), cursor_area.height()*0.15, cursorColour_);
					break;
				case VTERM_PROP_CURSORSHAPE_BAR_LEFT:
					p.fillRect(cursor_area.x(), cursor_area.y(), cursor_area.width()*0.15, cursor_area.height(), cursorColour_);
					break;
				}
			}

			ph_pos.pcol += cell.width;
		}
	}

}

void QVTermWidget::blinkCursor() {
	cursorBlinkState_ = !cursorBlinkState_;
	viewport()->update(displayRect(cursorPosition_));
}

void QVTermWidget::cancelHighlight() {
	if(!hasHighlight_)
		return;

	hasHighlight_ = 0;
	viewport()->update();
}

int QVTermWidget::damage(VTermRect rect) {
	if(hasHighlight_) {
		if((highlightStartPos_.row < rect.end_row - 1 ||
			 (highlightStartPos_.row == rect.end_row - 1 && highlightStartPos_.col < rect.end_col - 1)) &&
				(highlightEndPos_.row > rect.start_row ||
				 (highlightEndPos_.row == rect.start_row && highlightEndPos_.col > rect.start_col))) {
			/* Damage overlaps highlighted region */
			cancelHighlight();
		}
	}
	viewport()->update(displayRect(rect));
	return 1;
}

int QVTermWidget::preScroll(VTermRect rect) {
	if(rect.start_row != 0 || rect.start_col != 0 || rect.end_col != numCols_ || altScreenActive_)
		return 0;

	for(int row = 0; row < rect.end_row; row++) {
		ScrollbackLine *linebuffer = NULL;
		if(numBufferOffscreenLines_ == scrollBufferNumLines_) {
			/* Recycle old row if it's the right size */
			if(scrollbackBuffer_[numBufferOffscreenLines_-1]->cols == numCols_)
				linebuffer = scrollbackBuffer_[numBufferOffscreenLines_-1];
			else
				free(scrollbackBuffer_[numBufferOffscreenLines_-1]);
			memmove(scrollbackBuffer_ + 1, scrollbackBuffer_, sizeof(scrollbackBuffer_[0]) * (numBufferOffscreenLines_ - 1));
		} else if(numBufferOffscreenLines_ > 0) {
			memmove(scrollbackBuffer_ + 1, scrollbackBuffer_, sizeof(scrollbackBuffer_[0]) * numBufferOffscreenLines_);
		}

		if(!linebuffer) {
			linebuffer = (ScrollbackLine*) malloc(sizeof(ScrollbackLine) + numCols_ * sizeof(VTermScreenCell));
			linebuffer->cols = numCols_;
		}

		scrollbackBuffer_[0] = linebuffer;

		for(VTermPos pos = { .row = row, .col = 0 }; pos.col < numCols_; pos.col++) {
			vterm_screen_get_cell(vTermScreen_, pos, linebuffer->cells + pos.col);
		}

		if(numBufferOffscreenLines_ < scrollBufferNumLines_) {
			numBufferOffscreenLines_++;
		}
	}

	int increasedLines = rect.end_row - rect.start_row;
	int old_scroll_offset = scrollOffset_;
	if(increasedLines) {
		verticalScrollBar()->setRange(0, numBufferOffscreenLines_);
		verticalScrollBar()->setValue(verticalScrollBar()->maximum());
	}

	if(!unscrollOnOutput_ && old_scroll_offset != 0)
		verticalScrollBar()->setValue(verticalScrollBar()->maximum()-old_scroll_offset-increasedLines);

	return 1;
}

int QVTermWidget::moveRect(VTermRect dest, VTermRect src) {
	if(hasHighlight_) {
		int start_inside = vterm_rect_contains(src, highlightStartPos_);
		int stop_inside  = vterm_rect_contains(src, highlightEndPos_);

		if(start_inside && stop_inside &&
				(highlightStartPos_.row == highlightEndPos_.row ||
				 (src.start_col == 0 && src.end_col == numCols_))) {
			int delta_row = dest.start_row - src.start_row;
			int delta_col = dest.start_col - src.start_col;

			highlightStartPos_.row += delta_row;
			highlightStartPos_.col += delta_col;
			highlightEndPos_.row  += delta_row;
			highlightEndPos_.col  += delta_col;
		}
		else if(start_inside || stop_inside) {
			cancelHighlight();
		}
	}
	viewport()->update();
	return 1;
}

int QVTermWidget::moveCursor(VTermPos pos, VTermPos oldpos, int visible) {
	cursorPosition_ = pos;
	cursorBlinkState_ = 1;
	return 1;
}

int QVTermWidget::setTerminalProperty(VTermProp prop, VTermValue *val) {
	switch(prop) {
	case VTERM_PROP_CURSORVISIBLE:
		cursor_visible = val->boolean;
		break;
	case VTERM_PROP_CURSORBLINK:
		cursorBlinkState_ = true;
		if(val->boolean)
			cursorBlinkTimer_.start(cursorBlinkInterval_);
		else
			cursorBlinkTimer_.stop();
		break;
	case VTERM_PROP_CURSORSHAPE:
		cursorShape_ = val->number;
		break;
	case VTERM_PROP_ICONNAME:
		currentIcon_ = val->string;
		emit iconChanged(this, currentIcon_);
		break;
	case VTERM_PROP_TITLE:
		currentTitle_ = val->string;
		emit titleChanged(this, currentTitle_);
		break;
	case VTERM_PROP_ALTSCREEN:
		altScreenActive_ = val->boolean;
		if(altScreenActive_) verticalScrollBar()->setRange(0,0);
		else {
			verticalScrollBar()->setRange(0, numBufferOffscreenLines_);
			verticalScrollBar()->setValue(verticalScrollBar()->maximum());
		}
		return 1;
	default:
		return 0;
	}
	return 1;
}

int QVTermWidget::setMouseFunc(VTermMouseFunc func, void *data) {
	mouseFunc = func;
	mouseData = data;
	return 1;
}

int QVTermWidget::bell() {
	QApplication::beep();
	return 1;
}

bool QVTermWidget::event(QEvent *event) {
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *ke = static_cast<QKeyEvent *>(event);
		if (ke->key() == Qt::Key_Tab) {
			keyPressEvent(ke);
			return true;
		}
	}
	return QAbstractScrollArea::event(event);
}

void QVTermWidget::copySelectedText() {
	clipboard->clear();
	if(hasHighlight_)
		clipboard->setText(getText(highlightStartPos_, highlightEndPos_), QClipboard::Clipboard);
}

void QVTermWidget::pasteFromClipboard() {
	pushString(clipboard->text(QClipboard::Clipboard));
}

void QVTermWidget::keyPressEvent(QKeyEvent *e) {
	e->accept();
	if((e->key() == Qt::Key_Insert && (e->modifiers() & Qt::SHIFT)) ||
			(e->key() == Qt::Key_V && (e->modifiers() & Qt::SHIFT) && (e->modifiers() & Qt::CTRL))) {
		pasteFromClipboard();
		return;
	}

	if(e->key() == Qt::Key_C && (e->modifiers() & Qt::SHIFT) && (e->modifiers() & Qt::CTRL)) {
		copySelectedText();
		return;
	}

	if(e->key() == Qt::Key_PageDown && (e->modifiers() & Qt::SHIFT) && (e->modifiers() & Qt::CTRL)) {
		scroll(numRows_/2);
		return;
	}

	if(e->key() == Qt::Key_PageUp && (e->modifiers() & Qt::SHIFT) && (e->modifiers() & Qt::CTRL)) {
		scroll(-numRows_/2);
		return;
	}

	if(e->key() == Qt::Key_Down && (e->modifiers() & Qt::SHIFT) && (e->modifiers() & Qt::CTRL)) {
		scroll(1);
		return;
	}

	if(e->key() == Qt::Key_Up && (e->modifiers() & Qt::SHIFT) && (e->modifiers() & Qt::CTRL)) {
		scroll(-1);
		return ;
	}

	VTermModifier state = convert_qt_modifier(e->modifiers());
	VTermKey keyval = convert_qt_keyval(e->key());

	if(keyval != VTERM_KEY_NONE) //special keys like arrows
		vterm_input_push_key(vTerm_, state, keyval);
	else if(e->text().length()) { //printable
		vterm_input_push_char(vTerm_, VTERM_MOD_NONE, e->text().toUcs4().at(0));
		if(unscrollOnKey_ && scrollOffset_ != 0)
			scroll(scrollOffset_);
	} else
		return;

	flushOutput();
}

void QVTermWidget::inputMethodEvent(QInputMethodEvent *e) {
	QVector<uint> str = e->commitString().toUcs4();
	foreach(uint c, str)
		vterm_input_push_char(vTerm_, VTERM_MOD_NONE,c);

	flushOutput();
	if(unscrollOnKey_ && scrollOffset_)
		scroll(scrollOffset_);
	viewport()->update();
}

void QVTermWidget::mousePressEvent(QMouseEvent * e) {
	PhyPos ph_pos;
	ph_pos.pcol = e->x() / cellHeight_;
	ph_pos.prow = e->y() / cellWidth_;
	VTermPos pos = vtermPos(ph_pos);

	if(mouseFunc && !(e->modifiers() & Qt::SHIFT)) {
		VTermModifier state = convert_qt_modifier(e->modifiers());
		(*mouseFunc)(pos.col, pos.row, e->buttons(), 1, state, mouseData);
	}

	QClipboard* cp = QApplication::clipboard();
	if(e->button() == Qt::MiddleButton) {
		pushString(cp->text(QClipboard::Selection));
	}

	if(e->button() == Qt::LeftButton) {
		cancelHighlight();
		mouseDragState_ = DRAG_PENDING;
		mouseDragStartPos_ = pos;
	}


}
void QVTermWidget::mouseMoveEvent(QMouseEvent *e) {

	if(viewport()->contentsRect().contains(e->pos()) && (mouseDragState_ == DRAG_PENDING || mouseDragState_ == DRAGGING)) {

		PhyPos ph_pos;
		ph_pos.pcol = e->x() / cellHeight_;
		ph_pos.prow = e->y() / cellWidth_;
		VTermPos old_end = mouseDragState_ == DRAGGING ? mouseDragCurrentPos_ : mouseDragStartPos_;
		VTermPos pos = vtermPos(ph_pos);

		if(pos.row == old_end.row && pos.col == old_end.col)
			/* Unchanged; stop here */
			return;

		mouseDragState_ = DRAGGING;
		mouseDragCurrentPos_ = pos;

		VTermPos pos_left1 = mouseDragCurrentPos_;
		if(pos_left1.col > 0) pos_left1.col--;

		bool isEol;
		if(pos_left1.row >= 0)
			isEol = vterm_screen_is_eol(vTermScreen_, pos_left1);
		else {
			ScrollbackLine *sb_line = scrollbackBuffer_[-pos_left1.row-1];
			isEol = true;
			for(int col = pos_left1.col; col < sb_line->cols; ) {
				if(sb_line->cells[col].chars[0]) {
					isEol = false; break;}
				col += sb_line->cells[col].width;
			}
		}

		if(isEol)
			mouseDragCurrentPos_.col = numCols_;

		hasHighlight_ = 1;
		if(vterm_pos_cmp(mouseDragStartPos_, mouseDragCurrentPos_) > 0) {
			highlightStartPos_ = mouseDragCurrentPos_;
			highlightEndPos_  = mouseDragStartPos_;
		}
		else {
			highlightStartPos_ = mouseDragStartPos_;
			highlightEndPos_  = mouseDragCurrentPos_;
			highlightEndPos_.col--; /* exclude partial cell */
		}

		viewport()->update();
	}

}

void QVTermWidget::mouseReleaseEvent(QMouseEvent * e) {
	PhyPos ph_pos;
	ph_pos.pcol = e->x() / cellHeight_;
	ph_pos.prow = e->y() / cellWidth_;
	VTermPos pos = vtermPos(ph_pos);

	if(mouseFunc && !(e->modifiers() & Qt::SHIFT)) {
		VTermModifier state = convert_qt_modifier(e->modifiers());
		(*mouseFunc)(pos.col, pos.row, e->buttons(), 0, state, mouseData);
	}

	if(e->button() == Qt::LeftButton && mouseDragState_ == DRAGGING) {
		mouseDragState_ = NO_DRAG;
		if(vterm_pos_cmp(pos, mouseDragStartPos_) != 0) {
			clipboard->setText(getText(highlightStartPos_, highlightEndPos_), QClipboard::Selection);
		}
	}

	mouseDragState_ = NO_DRAG;
}

void QVTermWidget::mouseDoubleClickEvent(QMouseEvent * e) {
	PhyPos ph_pos;
	ph_pos.pcol = e->x() / cellHeight_;
	ph_pos.prow = e->y() / cellWidth_;
	VTermPos pos = vtermPos(ph_pos);

	VTermPos start_pos = pos;
	while(start_pos.col > 0 || start_pos.row > 0) {
		VTermPos cellpos = start_pos;
		VTermScreenCell cell;

		cellpos.col--;
		if(cellpos.col < 0) {
			cellpos.row--;
			cellpos.col = numCols_ - 1;
		}
		fetchCell(cellpos, &cell);
		if(!isWordChar(cell.chars[0]))
			break;

		start_pos = cellpos;
	}

	VTermPos stop_pos = pos;
	while(stop_pos.col < numCols_ - 1 || stop_pos.row < numRows_ - 1) {
		VTermPos cellpos = stop_pos;
		VTermScreenCell cell;

		cellpos.col++;
		if(cellpos.col >= numCols_) {
			cellpos.row++;
			cellpos.col = 0;
		}
		fetchCell(cellpos, &cell);
		if(!isWordChar(cell.chars[0]))
			break;

		stop_pos = cellpos;
	}

	hasHighlight_ = 1;
	highlightStartPos_ = start_pos;
	highlightEndPos_  = stop_pos;

	viewport()->update();
	clipboard->setText(getText(highlightStartPos_, highlightEndPos_), QClipboard::Selection);
}


void QVTermWidget::scrollContentsBy(int dx, int delta) {
	if(altScreenActive_)
		return;

	if(delta > 0) {
		if(scrollOffset_ + delta > numBufferOffscreenLines_)
			delta = numBufferOffscreenLines_ - scrollOffset_;
	}
	else if(delta < 0) {
		if(delta < -scrollOffset_)
			delta = -scrollOffset_;
	}

	if(!delta)
		return;

	scrollOffset_ += delta;
	viewport()->update();
}

void QVTermWidget::focusInEvent(QFocusEvent *) {
	viewport()->update(displayRect(cursorPosition_));
}

void QVTermWidget::focusOutEvent(QFocusEvent *) {

	viewport()->update(displayRect(cursorPosition_));
}

void QVTermWidget::resizeEvent(QResizeEvent * e) {
	numCols_ = e->size().width() / cellHeight_;
	numRows_ = e->size().height() / cellWidth_;
	vterm_set_size(vTerm_, numRows_, numCols_);
	vterm_screen_flush_damage(vTermScreen_);
	struct winsize size = { numRows_, numCols_, 0, 0 };
	ioctl(childFd_, TIOCSWINSZ, &size);
}

QVTermWidget::~QVTermWidget() {
	vterm_free(vTerm_);
	for(int i=0; i<numBufferOffscreenLines_; ++i) {
		free(scrollbackBuffer_[i]);
	}
	delete[] scrollbackBuffer_;
}

void QVTermWidget::setEnableAltScreen(bool v) {
	enableAltScreen_ = v;
	vterm_screen_reset(vTermScreen_, 1);
	vterm_screen_enable_altscreen(vTermScreen_, enableAltScreen_?1:0);
}

void QVTermWidget::setBoldHighBright(bool v) {
	boldHighbright_ = v;
	VTermState *state = vterm_obtain_state(vTerm_);
	vterm_state_set_bold_highbright(state, boldHighbright_);
}

void QVTermWidget::setFont(const QFont &fnt) {
	QAbstractScrollArea::setFont(fnt);
	QFontMetrics qfm(font());
	cellHeight_ = qfm.maxWidth();
	cellWidth_ = qfm.height();
	setTermSize(numRows_, numCols_);
}

void QVTermWidget::setScrollBufferSize(int lines) {
	scrollBufferNumLines_ = lines;
	if(scrollbackBuffer_) {
		for(int i=0; i<numBufferOffscreenLines_; ++i) {
			free(scrollbackBuffer_[i]);
		}
		delete[] scrollbackBuffer_;
	}
	scrollbackBuffer_ = new ScrollbackLine*[scrollBufferNumLines_];
}

void QVTermWidget::setTermSize(int rows, int cols) {
	numRows_ = rows;
	numCols_ = cols;
	vterm_set_size(vTerm_, rows, cols);
}

QVTermWidget::QVTermWidget(QWidget *parent)
	: QAbstractScrollArea(parent)
{
	numRows_ = 20;
	numCols_ = 100;

	mouseDragState_ = NO_DRAG;
	clipboard = QApplication::clipboard();
	mouseFunc = 0;
	mouseData = 0;
	altScreenActive_ = false;
	numBufferOffscreenLines_ = 0;
	scrollOffset_ = 0;
	scrollBufferNumLines_ = 0;
	scrollbackBuffer_ = 0;
	cursorBlinkInterval_ = 0;

	cursor_visible = true;
	cursorBlinkState_ = false;
	mouseDragState_ = NO_DRAG;
	hasHighlight_ = false;

	vTerm_ = vterm_new(numRows_, numCols_);
	vterm_parser_set_utf8(vTerm_, 1);
	vTermScreen_ = vterm_obtain_screen(vTerm_);
	registerVtermCallbacks(vTermScreen_, this);
	vterm_screen_set_damage_merge(vTermScreen_, VTERM_DAMAGE_SCROLL);
	vterm_screen_reset(vTermScreen_, 1);

	// set up defaults
	setTermSize(numRows_, numCols_);
	setDoubleClickFullWord(true);
	setCursorBlinkInterval(500);
	setUnscrollOnKey(true);
	setCursorColour(Qt::white);
	setDefaultColours(Qt::white, Qt::black);
	setBoldHighBright(true);
	setFont(QFont("monospace"));
	setScrollBufferSize(1000);
	// BUG: Once the alt screen is enabled it cannot be disabled
	// enabled is the common behaviour, go with it for now
	setEnableAltScreen(true);
	setCursorShape(VTERM_PROP_CURSORSHAPE_BLOCK);

	setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	setAttribute( Qt::WA_InputMethodEnabled);

	setCursor(Qt::IBeamCursor);
	verticalScrollBar()->setCursor(Qt::ArrowCursor);

	connect(&cursorBlinkTimer_, SIGNAL(timeout()), this, SLOT(blinkCursor()));
	cursorBlinkTimer_.start(cursorBlinkInterval_);
}

void QVTermWidget::setCursorBlinkInterval(int ms) {
	cursorBlinkInterval_ = ms;
	if(cursorBlinkInterval_ == 0)
		cursorBlinkTimer_.stop();
	cursorBlinkState_ = true;
	cursorBlinkTimer_.setInterval(cursorBlinkInterval_);
	viewport()->update(displayRect(cursorPosition_));
}

void QVTermWidget::setDefaultColours(QColor fg_col, QColor bg_col) {
	backgroundColour_ = bg_col;
	foregroundColour_ = fg_col;

	VTermColor fg;
	fg.red   = fg_col.red();
	fg.green = fg_col.green();
	fg.blue  = fg_col.blue();

	VTermColor bg;
	bg.red   = bg_col.red();
	bg.green = bg_col.green();
	bg.blue  = bg_col.blue();
	vterm_state_set_default_colors(vterm_obtain_state(vTerm_), &fg, &bg);

	viewport()->update();
}

void QVTermWidget::setCursorShape(int shape) {
	VTermState *state = vterm_obtain_state(vTerm_);
	VTermValue vtv;
	vtv.number = shape;
	vterm_state_set_termprop(state, VTERM_PROP_CURSORSHAPE, &vtv);
}

void QVTermWidget::start(QString term, QString shellstr, QString command) {
	// direct copy from pangoterm

	/* None of the docs about termios explain how to construct a new one of
	 * these, so this is largely a guess */
	struct termios termios;
	termios.c_iflag = ICRNL|IXON|IUTF8;
	termios.c_oflag = OPOST|ONLCR|NL0|CR0|TAB0|BS0|VT0|FF0;
	termios.c_cflag = CS8|CREAD;
	termios.c_lflag = ISIG|ICANON|IEXTEN|ECHO|ECHOE|ECHOK;

#ifdef ECHOCTL
	termios.c_lflag |= ECHOCTL;
#endif
#ifdef ECHOKE
	termios.c_lflag |= ECHOKE;
#endif

	cfsetspeed(&termios, 38400);

	termios.c_cc[VINTR]    = 0x1f & 'C';
	termios.c_cc[VQUIT]    = 0x1f & '\\';
	termios.c_cc[VERASE]   = 0x7f;
	termios.c_cc[VKILL]    = 0x1f & 'U';
	termios.c_cc[VEOF]     = 0x1f & 'D';
	termios.c_cc[VEOL]     = _POSIX_VDISABLE;
	termios.c_cc[VEOL2]    = _POSIX_VDISABLE;
	termios.c_cc[VSTART]   = 0x1f & 'Q';
	termios.c_cc[VSTOP]    = 0x1f & 'S';
	termios.c_cc[VSUSP]    = 0x1f & 'Z';
	termios.c_cc[VREPRINT] = 0x1f & 'R';
	termios.c_cc[VWERASE]  = 0x1f & 'W';
	termios.c_cc[VLNEXT]   = 0x1f & 'V';
	termios.c_cc[VMIN]     = 1;
	termios.c_cc[VTIME]    = 0;

	struct winsize size;
	size.ws_row = numRows_;
	size.ws_col = numCols_;
	size.ws_xpixel = 0;
	size.ws_ypixel = 0;

	pid_t kid = forkpty(&childFd_, NULL, &termios, &size);
	if(kid == 0) {
		/* Restore the ISIG signals back to defaults */
		signal(SIGINT,  SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGSTOP, SIG_DFL);
		signal(SIGCONT, SIG_DFL);

		char* termEnv = strdup(("TERM="+term).toLocal8Bit().constData());
		putenv(termEnv);

		char* shell = strdup(shellstr.toLocal8Bit().constData());

		char** args;

		if(!command.isEmpty()) {
			args = (char**) malloc(4 * sizeof(char*));
			args[0] = shell;
			args[1] = strdup("-c");
			args[2] = strdup(command.toLocal8Bit().constData());
			args[3] = NULL;
		} else {
			args = (char**) malloc(2 * sizeof(char*));
			args[0] = shell;
			args[1] = NULL;
		}

		execvp(shell, args);
		emit fatalError(this, QString("Cannot exec ") + shell + ": " + strerror(errno));
		_exit(1);
	}

	fcntl(childFd_, F_SETFL, fcntl(childFd_, F_GETFL) | O_NONBLOCK);

	socketNotifier_ = new QSocketNotifier(childFd_, QSocketNotifier::Read);
	connect(socketNotifier_, SIGNAL(activated(int)), this, SLOT(fdReadable(int)));
}

static uint64_t unixTimeMicro() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

void QVTermWidget::fdReadable(int) {
	// Make sure we don't take longer than 20msec doing this
	uint64_t deadline_time = unixTimeMicro();
	while(1) {
		// Linux kernel's PTY buffer is a fixed 4096 bytes (1 page) so there's
		// never any point read()ing more than that
		char buffer[4096];

		ssize_t bytes = read(childFd_, buffer, sizeof buffer);

		if(bytes == -1 && errno == EAGAIN)
			break;

		if(bytes == 0 || (bytes == -1 && errno == EIO)) {
			socketNotifier_->disconnect();
			emit finished(this);
			return;
		}
		if(bytes < 0) {
			qDebug() << "read failed";
			emit fatalError(this, QString("read(master) failed - %s\n") + strerror(errno));
			return;
		}
		vterm_push_bytes(vTerm_, buffer, bytes);

		if(unixTimeMicro() >= deadline_time)
			break;
	}
	vterm_screen_flush_damage(vTermScreen_);
	viewport()->update();
}

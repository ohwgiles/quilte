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

static VTermPos vtermPos(const TermPosition& p) {
	VTermPos vp;
	vp.row = p.row;
	vp.col = p.col;
	return vp;
}

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

VTermScreenCell QVTermWidget::fetchCell(const TermPosition pos) const {
	VTermScreenCell c;
	VTermScreenCell* cell = &c;
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
VTermPos vt = ::vtermPos(pos);
		vterm_screen_get_cell(vTermScreen_, vt, cell);
	}
		return c;
}

QString QVTermWidget::getText(VTermPos from, VTermPos to) {
	bool end_blank = false;
	QVector<uint> content;
	TermPosition pos(from.row, from.col, numRows_, numCols_);
//	pos.row = from.row;
//	pos.col = from.col;
	while(pos.row <= to.row && pos.col <= to.col) {
		VTermScreenCell cell = fetchCell(pos);
		//fetchCell(pos, &cell);
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

QString QVTermWidget::getEntireBuffer() const {
	QVector<uint> content;
	TermPosition pos( -numBufferOffscreenLines_, 0, numRows_, numCols_);
	while(pos.row <= numRows_ && pos.col <= numCols_) {
		VTermScreenCell cell = fetchCell(pos);
		for(int i=0; cell.chars[i]; ++i)
			content.append(cell.chars[i]);

		pos.col += cell.width;
		if(pos.col > numCols_) {
			++pos.row;
			pos.col = 0;
			content.append('\n');
		}
	}
	return QString::fromUcs4(content.constData());
}


bool QVTermWidget::isWordChar(uint32_t c) {
	if(doubleClickFullword_)
		return c && !iswspace(c);
	else
		return iswalnum(c) || (c == '_');
}

QRect QVTermWidget::displayRect(PhyPos pos) {
	return QRect(pos.pcol * cellWidth_,
					 pos.prow * cellHeight_,
					 cellWidth_,
					 cellHeight_);
}

QRect QVTermWidget::displayRect(VTermPos pos) {
	return QRect(pos.col * cellWidth_,
					 pos.row * cellHeight_,
					 cellWidth_,
					 cellHeight_);

}

QRect QVTermWidget::displayRect(VTermRect rect) {
	if(rect.start_row == rect.end_row) {
		return QRect(rect.start_col * cellWidth_, rect.start_row * cellHeight_, (rect.end_col-rect.start_col) * cellWidth_, cellHeight_);
	} else {
		return QRect(0, rect.start_row * cellHeight_, rect.end_col * cellWidth_, rect.end_row*cellHeight_);
	}
}

QRect QVTermWidget::displayRect(PhyRect rect) {
	return
			QRect(rect.start_pcol * cellWidth_,rect.start_prow * cellHeight_,(rect.end_pcol - rect.start_pcol) * cellWidth_,(rect.end_prow - rect.start_prow) * cellHeight_);
}

void QVTermWidget::paintEvent(QPaintEvent * e) {
	QPainter p(viewport());
	//PhyPos ph_pos;

	// clear the background first
	p.fillRect(e->rect(), backgroundColour_);

	int xmin = e->rect().x() / cellWidth_;
	int ymin = e->rect().y() / cellHeight_;
	int xmax = e->rect().width() / cellWidth_ + xmin;
	int ymax = e->rect().height() / cellHeight_ + ymin;

	enum { RTL = -1, LTR = 1 } textDirection = LTR;
	//QChar::Direction currentDirection = QChar::DirL;
	//VTermPos beginningOfEmbeddedRtl;
//	int colOffset = 0;
//	int rowOffset = 0;
	TermPosition pos(ymin-scrollOffset_,xmin,numRows_,numCols_);
	TermPosition atThisPoint, returnTo;
	for(int j = ymin; j < ymax; j++) {
		for(int i = xmin; i < xmax; ) {
			TermPosition ph_pos(j,i,numRows_,numCols_);
			//PhyPos ph_pos = { .prow = j + rowOffset, .pcol = i + colOffset };
			//VTermPos pos = { .row = j - scrollOffset_, .col = i };
			//VTermPos pos = vtermPos(ph_pos);

			VTermScreenCell cell = fetchCell(pos);

			if(textDirection == LTR && QChar(cell.chars[0]).direction() == QChar::DirR) {
				textDirection = RTL;
				atThisPoint = pos;
				//TermPosition scan = pos;
				// find the next strong LTR, then backtrack to the first weak char
				while(cell.chars[0] && QChar(cell.chars[0]).direction() != QChar::DirL) {
					++pos;
					cell = fetchCell(pos);
				}
				--pos;
				cell = fetchCell(pos);
				while(QChar(cell.chars[0]).direction() != QChar::DirR) {
					--pos;
					cell = fetchCell(pos);
				}
				returnTo = pos;
				cell = fetchCell(pos);
			}



			/* Invert the RV attribute if this cell is selected */
			if(hasHighlight_) {
				VTermPos start = highlightStartPos_, stop = highlightEndPos_;

				int highlighted = (pos.row > start.row || (pos.row == start.row && pos.col >= start.col)) &&
						(pos.row < stop.row  || (pos.row == stop.row  && pos.col <= stop.col));

				if(highlighted)
					cell.attrs.reverse = !cell.attrs.reverse;
			}

			int cursor_here = pos.row == cursorPosition_.row && pos.col == cursorPosition_.col;
			int cursor_visible = cursorBlinkState_ || !hasFocus();

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

			if(cell.chars[0] == 0) { // empty cell
				p.fillRect(ph_pos.col*cellWidth_, ph_pos.row*cellHeight_,cellWidth_*cell.width,cellHeight_, p.background());

			} else if(cell.chars[0] != 0) { // there is something here
				QString chr = QString::fromUcs4(cell.chars);
				p.fillRect(ph_pos.col*cellWidth_, ph_pos.row*cellHeight_,cellWidth_*cell.width,cellHeight_,p.background());
				p.drawText(ph_pos.col*cellWidth_, ph_pos.row*cellHeight_,cellWidth_*cell.width,cellHeight_,0,chr);
			}

			if(cursor_visible && cursor_here && cursorShape_ != VTERM_PROP_CURSORSHAPE_BLOCK) {
				QRect cursor_area = ph_pos.rect(cellWidth_, cellHeight_);//displayRect(ph_pos);
				switch(cursorShape_) {
				case VTERM_PROP_CURSORSHAPE_UNDERLINE:
					p.fillRect(cursor_area.x(), cursor_area.y()+cursor_area.height()*0.85, cursor_area.width(), cursor_area.height()*0.15, cursorColour_);
					break;
				case VTERM_PROP_CURSORSHAPE_BAR_LEFT:
					p.fillRect(cursor_area.x(), cursor_area.y(), cursor_area.width()*0.15, cursor_area.height(), cursorColour_);
					break;
				}
			}

			//ph_pos.pcol += cell.width * textDirection;
			i += cell.width;// * textDirection;

			if(pos == atThisPoint) {
				textDirection = LTR;
				pos = returnTo;
			}
			if(textDirection == LTR) ++pos; else --pos;

		}
	}
	QPen pen;
	pen.setStyle(Qt::DotLine);
	pen.setColor(foregroundColour_);
	foreach(VTermRect r, searchResults_) {
		r.start_row += scrollOffset_;
		r.end_row +=scrollOffset_;
		if((r.start_row >= ymin && r.start_row <= ymax) || (r.end_row >= ymin && r.end_row <= ymax)) {
			p.setPen(pen);
			p.drawRect(r.start_col*cellWidth_, r.start_row*cellHeight_, cellWidth_*(r.end_col-r.start_col +1),cellHeight_);
		}
	}


}

void QVTermWidget::findText(QString txt) {
	QVector<uint32_t> ucs = txt.toUcs4();
	const uint32_t* searchTermEnd = &ucs.constData()[ucs.count()-1];
	const uint32_t* searchCursor = searchTermEnd;
	searchResults_.clear();
	VTermRect matchRange;
	for(int j = numRows_-1; j>=-numBufferOffscreenLines_; --j) {
		for(int i = numCols_-1; i>=0; --i) {
			//VTermScreenCell cell;
			TermPosition pos(j,i,numRows_,numCols_);
			//VTermPos pos = { .row = j, .col = i };
			VTermScreenCell cell = fetchCell(pos);
			// match backwards
			if(cell.chars[0] == *searchCursor) {
				if(searchCursor == searchTermEnd) { //this is a new match
					matchRange.end_col = i;
					matchRange.end_row = j;
					searchCursor--;
				} else if(searchCursor == ucs.constData()) {
					// if we made it to the end of the string, a match is found
					matchRange.start_col = i;
					matchRange.start_row = j;
					searchResults_.append(matchRange);
					// reset the cursor
					searchCursor =  searchTermEnd;
				} else searchCursor--;
			} else // a mismatch has occurred, reset the cursor
				searchCursor = searchTermEnd;
		}
	}
	// if we've matched anything
	if(searchResults_.count() > 0) {
		// highlight the first matching one
		hasHighlight_ = true;
		highlightEndPos_.row = searchResults_.at(0).end_row;
		highlightEndPos_.col = searchResults_.at(0).end_col;
		highlightStartPos_.row = searchResults_.at(0).start_row;
		highlightStartPos_.col = searchResults_.at(0).start_col;
		// if it's not visible in the current scroll buffer, scroll there
		int min = 0 - scrollOffset_;
		int max = numRows_ - scrollOffset_;
		if(!(highlightEndPos_.row > min && highlightEndPos_.row < max && highlightStartPos_.row > min && highlightStartPos_.row << max)) {
			scroll(highlightStartPos_.row);
		}
	}
	viewport()->update();
}

void QVTermWidget::findNext() {
	int index = -1;
	// first find the current selected one by comparing the highlight with
	// the search results
	if(hasHighlight_) {
		for(int i = 0; i < searchResults_.count(); ++i) {
			const VTermRect& p = searchResults_[i];
			qDebug() << highlightEndPos_.row << p.end_row << highlightEndPos_.col << p.end_col << highlightStartPos_.row << p.start_row << highlightStartPos_.col << p.start_col;
			if(highlightEndPos_.row == p.end_row && highlightEndPos_.col == p.end_col && highlightStartPos_.row == p.start_row && highlightStartPos_.col == p.start_col) {
				index = i;
				break;
			}
		}
	}

	// unless we came up with nothing, the one we're after is the next one
	if(index < searchResults_.count()-1) index++;
	const VTermRect& p = searchResults_[index];

	// move the highlight
	hasHighlight_ = true;
	highlightEndPos_.row = p.end_row;
	highlightEndPos_.col = p.end_col;
	highlightStartPos_.row = p.start_row;
	highlightStartPos_.col = p.start_col;

	// now scroll there if it's not visible
	int min = 0 - scrollOffset_;
	int max = numRows_ - scrollOffset_;

	if(!(p.start_row > min && p.start_row < max && p.end_row > min && p.end_row < max)) {
		scroll(p.start_row);
	} else
	viewport()->update();
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
	for(int i=0; i<searchResults_.count(); ++i) {
		searchResults_[i].start_row--;
		searchResults_[i].end_row--;
	}
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

		VTermPos pos;
		for(pos.row = row, pos.col = 0; pos.col < numCols_; pos.col++) {
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
	ph_pos.pcol = e->x() / cellWidth_;
	ph_pos.prow = e->y() / cellHeight_;
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
		ph_pos.pcol = e->x() / cellWidth_;
		ph_pos.prow = e->y() / cellHeight_;
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
	ph_pos.pcol = e->x() / cellWidth_;
	ph_pos.prow = e->y() / cellHeight_;
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
//	PhyPos ph_pos;
//	ph_pos.pcol = e->x() / cellWidth_;
//	ph_pos.prow = e->y() / cellHeight_;
//	VTermPos pos = vtermPos(ph_pos);
	TermPosition pos(e->y() / cellHeight_ - scrollOffset_, e->x() / cellWidth_, numRows_, numCols_);
	TermPosition start_pos = pos;
	while(start_pos.col > 0 || start_pos.row > 0) {
		TermPosition cellpos = start_pos;
		//VTermPos cellpos = start_pos;
		//VTermScreenCell cell;

		cellpos.col--;
		if(cellpos.col < 0) {
			cellpos.row--;
			cellpos.col = numCols_ - 1;
		}
		VTermScreenCell cell = fetchCell(cellpos);
		if(!isWordChar(cell.chars[0]))
			break;

		start_pos = cellpos;
	}

	TermPosition stop_pos = pos;
	while(stop_pos.col < numCols_ - 1 || stop_pos.row < numRows_ - 1) {
		TermPosition cellpos = stop_pos;
//		VTermPos cellpos = stop_pos;
//		VTermScreenCell cell;

		cellpos.col++;
		if(cellpos.col >= numCols_) {
			cellpos.row++;
			cellpos.col = 0;
		}
		VTermScreenCell cell = fetchCell(cellpos);
		if(!isWordChar(cell.chars[0]))
			break;

		stop_pos = cellpos;
	}

	hasHighlight_ = 1;
	highlightStartPos_.row = start_pos.row;
	highlightStartPos_.col = start_pos.col;
	highlightEndPos_.row  = stop_pos.row;
	highlightEndPos_.col  = stop_pos.col;

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
	numCols_ = e->size().width() / cellWidth_;
	numRows_ = e->size().height() / cellHeight_;
	// some crazy window managers allow arbitrary shrinking of windows.
	// make sure we don't call vterm_set_size with 0,0
	if(numCols_ < 1) numCols_ = 1;
	if(numRows_ < 1) numRows_ = 1;
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
	// TODO why is this necessary?
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
	cellWidth_ = qfm.maxWidth();
	cellHeight_ = qfm.height();
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
	numBufferOffscreenLines_ = 0;
	verticalScrollBar()->setRange(0,0);
}

void QVTermWidget::setTermSize(int rows, int cols) {
	numRows_ = rows;
	numCols_ = cols;
	vterm_set_size(vTerm_, rows, cols);
}

QVTermWidget::QVTermWidget(bool withAltScreen, QWidget *parent)
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

	// set up defaults to be overridden by client config
	setTermSize(numRows_, numCols_);
//	setDoubleClickFullWord(true);
//	setCursorBlinkInterval(500);
//	setUnscrollOnKey(true);
//	setCursorColour(Qt::white);
//	//setDefaultColours(Qt::white, Qt::black);
//	setBoldHighBright(true);
//	setFont(QFont("monospace"));
//	setScrollBufferSize(1000);
//	if(withAltScreen) setEnableAltScreen(withAltScreen);

	//vterm_screen_reset(vTermScreen_, 1);
	//setCursorShape(VTERM_PROP_CURSORSHAPE_BLOCK);

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
	cursorShape_ = shape;
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

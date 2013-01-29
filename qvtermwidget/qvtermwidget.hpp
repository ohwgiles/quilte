#ifndef QVTERMWIDGET_HPP
#define QVTERMWIDGET_HPP

#include <QAbstractScrollArea>
#include <QTimer>
#include <QScrollBar>
extern "C" {
#include <vterm.h>
}
class QSocketNotifier;
struct PangoTerm;
struct VTermCallbacks;
struct PhyPos{
  int prow, pcol;
} ;
struct PhyRect {
  int start_prow, end_prow, start_pcol, end_pcol;
};
struct ScrollbackLine{
  int cols;
  VTermScreenCell cells[];
} ;


class QVTermWidget : public QAbstractScrollArea {
	Q_OBJECT
public:
	QVTermWidget(QWidget* parent = 0);
	virtual ~QVTermWidget();
	void pushString(QString str);

	QString getText(VTermPos from, VTermPos to);
	QString getEntireBuffer() const;
	void copySelectedText();
	void pasteFromClipboard();
	void findText(QString txt);
	void findNext();
	void clearSearchResults() { searchResults_.clear(); }
	QSize minimumSize() const { return QSize(10*cellWidth_,cellHeight_); }
private:
	// todo range not rect
QVector<VTermRect> searchResults_;

public:
	void setDefaultColours(QColor fg, QColor bg);

	void setDoubleClickFullWord(bool v) { doubleClickFullword_ = v; }
	bool doubleClickFullWord() const { return doubleClickFullword_; }

	void setCursorBlinkInterval(int ms);
	int cursorBlinkInterval() const { return cursorBlinkInterval_; }

	void setUnscrollOnKey(bool v) { unscrollOnKey_ = v; }
	bool unscrollOnKey() const { return unscrollOnKey_; }

	void setUnscrollOnOutput(bool v) { unscrollOnOutput_ = v; }
	bool unscrollOnOutput() const { return unscrollOnOutput_; }

	void setCursorColour(QColor c) { cursorColour_ = c; }
	QColor cursorColour() const { return cursorColour_; }

	void setBoldHighBright(bool v);
	bool boldHighBright() const { return boldHighbright_; }

	void setEnableAltScreen(bool v);
	bool enableAltScreen() const { return enableAltScreen_; }

	void setScrollBufferSize(int lines);
	int scrollBufferSize() const { return scrollBufferNumLines_; }

	void setCursorShape(int shape);
	int cursorShape() const { return cursorShape_; }

	void setFont(const QFont &);

	void setTermSize(int rows, int cols);
	QSize termSize() const { return QSize(numRows_, numCols_); }

	QString title() const { return currentTitle_; }

protected:
	bool isWordChar(uint32_t c);
	//void showEvent(QShowEvent *);
	void paintEvent(QPaintEvent *);
	void keyPressEvent(QKeyEvent *);
	bool event(QEvent *event);
void inputMethodEvent(QInputMethodEvent *);
	void mousePressEvent(QMouseEvent *);
	void mouseMoveEvent(QMouseEvent *);
	void mouseReleaseEvent(QMouseEvent *);
	void mouseDoubleClickEvent(QMouseEvent *);
	void scrollContentsBy(int dx, int dy);
	void resizeEvent(QResizeEvent *);
	void focusInEvent(QFocusEvent *);
	void focusOutEvent(QFocusEvent *);
public:
	PangoTerm* pt_;

public:
	VTermPos vtermPos(PhyPos physical);
	QRect displayRect(PhyPos pos);
	QRect displayRect(PhyRect pos);

	QRect displayRect(VTermPos pos);
	QRect displayRect(VTermRect rect);
	PhyRect phyRect(VTermRect rect);

	friend struct VTermCallbacks;
private slots:
	void fdReadable(int);
	void blinkCursor();
signals:
	void titleChanged(QVTermWidget* self,QString);
	void iconChanged(QVTermWidget* self,QString);
	void finished(QVTermWidget* self);
	void fatalError(QVTermWidget* self, QString message);
private:
	int damage(VTermRect rect);
	int preScroll(VTermRect rect);
	int moveRect(VTermRect dest, VTermRect src);
	int moveCursor(VTermPos pos, VTermPos oldpos, int visible);
	int setTerminalProperty(VTermProp prop, VTermValue *val);
	int setMouseFunc(VTermMouseFunc func, void *data);
	int bell();

	void fetchCell(VTermPos pos, VTermScreenCell *cell) const;

	void startCursorBlinking();
	void stopCursorBlinking();
	void storeClipboard();
	void cancelHighlight();
	void flushOutput();
	QSocketNotifier* socketNotifier_;
QTimer cursorBlinkTimer_;
public:
void makeNew(int numRows_, int numCols_);
	void forkChild();
	void start(QString term, QString shell, QString command = "");
	int childFd_;

	void scroll(int v) {
		int o = verticalScrollBar()->value();
		verticalScrollBar()->setValue(o+v); }
QString currentIcon_;
QString currentTitle_;

	bool doubleClickFullword_;
	int cursorBlinkInterval_;
	bool unscrollOnKey_;
	bool unscrollOnOutput_;
	QColor cursorColour_;
	bool boldHighbright_;
	bool enableAltScreen_;
	int cursorShape_;




	VTerm *vTerm_;
	VTermScreen *vTermScreen_;


	VTermMouseFunc mouseFunc;
	void *mouseData;


	short unsigned int numRows_;
	short unsigned int numCols_;

	int altScreenActive_;
	int scrollOffset_;

	int scrollBufferNumLines_;
	int numBufferOffscreenLines_;
	ScrollbackLine **scrollbackBuffer_;

	int cellHeight_;
	int cellWidth_;

	QColor foregroundColour_;
	QColor backgroundColour_;
//	QColor cursorColour_;

	int cursor_visible;    /* VTERM_PROP_CURSORVISIBLE */
	bool cursorBlinkState_; /* during high state of blink */
	VTermPos cursorPosition_;
	//int cursorShape_;



	//guint cursor_timer_id;

	/* These four positions relate to the click/drag highlight state */

	enum { NO_DRAG, DRAG_PENDING, DRAGGING } mouseDragState_;
	/* Initial mouse position of selection drag */
	VTermPos mouseDragStartPos_;
	/* Current mouse position of selection drag */
	VTermPos mouseDragCurrentPos_;

	/* Start and stop bounds of the selection */
	bool hasHighlight_;
	VTermPos highlightStartPos_;
	VTermPos highlightEndPos_;

	QClipboard *clipboard;




};

#endif // QVTERMWIDGET_HPP

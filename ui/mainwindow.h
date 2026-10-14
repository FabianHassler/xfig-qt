/*
 * mainwindow.h - the xfig main window.
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "xaw.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	MainWindow();
	static MainWindow *instance() { return s_instance; }

	/* hooks from the Xt layer, after the peer was made */
	void	widgetCreated(Widget w, const char *name, const char *wclass);
	void	widgetSetValues(Widget w, ArgList args, Cardinal n);

	/* File menu, after the file dialog (also used by test scripts) */
	bool	openPath(const QString &path);
	bool	mergePath(const QString &path);
	bool	saveAsPath(const QString &path);
	bool	exportPath(const QString &path, int lang);

protected:
	void	closeEvent(QCloseEvent *e) override;
	void	resizeEvent(QResizeEvent *e) override;

private:
	void	createMenus();
	void	fillMenu(QMenu *qm, int index);
	void	adopt(XawPeer *p, QWidget *area);
	void	fitModePanel();
	void	setupRulers();
	void	openFile();
	void	mergeFile();
	void	exportFile();
	bool	saveFileAs();
	bool	maybeSave();
	QString	fileDialogDir() const;

	static MainWindow *s_instance;
	QGridLayout	*m_grid;
	QVBoxLayout	*m_left, *m_right;
	QHBoxLayout	*m_bottom, *m_top, *m_middle;
	QHBoxLayout	*m_rulerRow = nullptr;	/* top ruler and unit box */
	bool		m_rulerRight = true;	/* side ruler right of the canvas */
	bool		m_rulersSetUp = false;
	Widget		m_mouseWidget = nullptr;
	XawPeer		*m_modePanel = nullptr;
	QLabel		*m_message;
	Widget		m_msgWidget = nullptr;
	QList<QMenu *>	m_menus;
};

#endif

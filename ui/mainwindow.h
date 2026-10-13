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

protected:
	void	closeEvent(QCloseEvent *e) override;
	void	resizeEvent(QResizeEvent *e) override;

private:
	void	createMenus();
	void	fillMenu(QMenu *qm, int index);
	void	adopt(XawPeer *p, QWidget *area);
	void	fitModePanel();

	static MainWindow *s_instance;
	QGridLayout	*m_grid;
	QVBoxLayout	*m_left, *m_right;
	QHBoxLayout	*m_bottom, *m_top;
	Widget		m_mouseWidget = nullptr;
	XawPeer		*m_modePanel = nullptr;
	QLabel		*m_message;
	Widget		m_msgWidget = nullptr;
	QList<QMenu *>	m_menus;
};

#endif

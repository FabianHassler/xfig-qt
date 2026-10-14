/*
 * print.h - print through the system's print dialog.
 */
#ifndef PRINT_H
#define PRINT_H

#include "xq.h"

class QPrinter;

/* File > Print...: the print dialog, then print; false if cancelled or
   failed (failures are reported) */
bool	print_figure(QWidget *parent);

/* print to the printer or PDF file the dialog chose in printer */
bool	print_figure_with(QPrinter &printer, QWidget *parent);

/* Ghostscript (gs); empty if not found */
QString	find_ghostscript();

#endif

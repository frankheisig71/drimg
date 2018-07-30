#ifndef DRIMGWIDGETBASE_H
#define DRIMGWIDGETBASE_H

#include <QWidget>
#include <QModelIndex>
#include "gemddlg.h"

typedef unsigned char  byte;
typedef unsigned int   UINT;
typedef unsigned long  ULONG;

namespace Ui {
class drimgwidgetbase;
}

class drimgwidgetbase : public QWidget
{
    Q_OBJECT

public:
    explicit drimgwidgetbase(QWidget *parent = 0);
    ~drimgwidgetbase();

    void detSDloop();
    void getCHS();
    void getForm();
    void detSD();
    void setImageProgress(int *prog, int *prnxt, int *status, ULONG *copied, ULONG m, ULONG SecCnt);

private slots:
    void on_refrButton_clicked();
    void on_quitButton_clicked();
    void on_listBox1_clicked(const QModelIndex &index);

    void on_FileTrButton_clicked();

    void on_readButton_clicked();

    void on_openIfButton_clicked();

    void on_writeButton_clicked();

    void on_ov2roCB_clicked();

    void on_swapCB_clicked();

    void on_abortButton_clicked();

    void on_creimfButton_clicked();

private:
    Ui::drimgwidgetbase *ui;
    GemdDlg* gem;
};

#endif // DRIMGWIDGETBASE_H

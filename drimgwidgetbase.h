#ifndef DRIMGWIDGETBASE_H
#define DRIMGWIDGETBASE_H

#include <QWidget>
#include <QModelIndex>
#include "gemddlg.h"
#include <stdio.h>
#ifdef WINDOWS
#include <windows.h>
#include <winioctl.h>
#endif

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
    int  detSD(char* devStr, int* extVolumese);
    #ifdef WINDOWS
    bool OpenSavingFile(HANDLE* f, bool hdf);
    bool OpenReadingFile(HANDLE* f, bool hdf);
    bool OpenInputFile(HANDLE* f, char* Name, bool IsDevice);
    bool OpenOutputFile(HANDLE* f, char* Name, bool IsDevice);
    long long CopyImage(HANDLE f_in, HANDLE f_out, ULONG SecCnt, bool swap, bool h256rb, bool fromOrig, ULONG* writtenSec);
    bool IsPhysDevice(HANDLE fh);
    #else
    bool OpenSavingFile(FILE** f, bool hdf);
    bool OpenReadingFile(FILE** f, bool hdf);
    bool OpenInputFile(FILE** f, char* Name, bool IsDevice);
    bool OpenOutputFile(FILE** f, char* Name, bool IsDevice);
    long long CopyImage(FILE* f_in, FILE* f_out, ULONG SecCnt, bool swap, bool h256rb, bool fromOrig, ULONG* writtenSec);
    bool IsPhysDevice(FILE* f);
    #endif


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
    void disableAll();
    void enableAll();

};

#endif // DRIMGWIDGETBASE_H

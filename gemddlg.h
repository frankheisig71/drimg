#ifndef GEMDDLG_H
#define GEMDDLG_H

#include <QDialog>
#include <QModelIndex>

#define FRANKS_DEBUG
//#define WINDOWS

namespace Ui {
class GemdDlg;
}

class GemdDlg : public QDialog
{
    Q_OBJECT

public:
    explicit GemdDlg(QWidget *parent = 0);
    ~GemdDlg();
    void closeEvent (QCloseEvent *event);

    void loadroot(bool doList);
    void ShowPartitionUsage();
    //void opensubd(int index, bool doList);
    void OpenFATSubDir(unsigned char* DirBuffer, int EntryPos, char* NameBuffer, bool doList);
    void subdirh(bool doList);
    void FATDirUp(unsigned char* DirBuffer, char* NameBuffer, bool doList);
    void LoadDirBuf(unsigned int *StartCluster, unsigned char* DirBuffer);
    void LoadSubDir(bool IsRoot, bool doList);
    int  ExtractFile(unsigned char* DirBuffer, int pos);
    void EraseFile(unsigned char* DirBuffer, int EntryPos);
    void GetFATFileName(unsigned char* DirBuffer, int DirPos, char* NameBuffer);
    void WriteCurrentDirBuf(void);
    void EnterSubDir(unsigned char* DirBuffer, int EntryPos, char* NameBuffer, bool MakeLocalDir, bool EnterLocalDir);
    int  MakeSubF(unsigned int Clun);
    bool EnterUpDir(unsigned char* DirBuffer, char* NameBuffer, unsigned int* EntryPos, bool ChangeLocalDir);
    int  AddSingleDirToCurrentDir(QString FilePathName, unsigned char* DirBuffer);
    bool AddFileToCurrentDir(QString FilePathName, unsigned char* DirBuffer);
    bool AddDirTreeToCurrentDir(QString PathName, unsigned char* DirBuffer);
    unsigned long WriteSectors( int StartSector, int Count, unsigned char *Buffer);


private slots:
    void on_partLB_clicked(const QModelIndex &index);
    void on_filesLB_clicked(const QModelIndex &index);
    void on_filesLB_doubleClicked(const QModelIndex &index);
    void on_opdirP_clicked();
    void on_DeleteFiles_clicked();
    void on_setddP_clicked();
    void on_ExtractFiles_clicked();
    void on_timeCB_clicked();
    void on_AddFiles_clicked();

    void on_newfP_clicked();

    void on_desallP_clicked();

    void on_quitP_clicked();

private:
    Ui::GemdDlg *ui;

    void OpenDialog();
};

int fileopen(const char* fname, const unsigned long flags);
int fileclose(int fd);

int  LoadEntryName(unsigned char* DirBuffer, int EntryPos, bool IsRoot, QString* Name);
void FATlookUp(unsigned int StartCluster, unsigned int* NextCluster, unsigned int* FileSector);
void FATfreeCluster(unsigned int Cluster);
unsigned int GetFATFileLength(unsigned char* DirBuffer, int EntryPos);
void SetFATFileLength(unsigned char* DirBuffer, int EntryPos, unsigned int Length);
void SetFATFileDateTime(unsigned char* DirBuffer, int EntryPos, struct stat* FileParams);
void GetFATFileDateTime(unsigned char* DirBuffer, int EntryPos, tm* DateTime);
void SortFATNames(unsigned char* DirBuffer, unsigned int* UnSortList, unsigned int* SortList, unsigned int* SortIndex);

#endif // GEMDDLG_H

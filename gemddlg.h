#ifndef GEMDDLG_H
#define GEMDDLG_H

//#define FRANKS_DEBUG
#define WINDOWS

#ifdef WINDOWS
// #undef WINVER
// #undef _WIN32_WINNT
// #define WINVER _WIN32_WINNT_VISTA //minimum requirement
// #define _WIN32_WINNT _WIN32_WINNT_VISTA //minimum requirement
 #include <time.h>
 #include <windows.h>
 #include <sys/stat.h>
 #include <winioctl.h>
 #include <winbase.h>
#else
 #include <sys/time.h>
 #include <utime.h>
 #include <pwd.h>
 #include <grp.h>
#endif
#include <sys/stat.h>
#include <QDialog>
#include <QModelIndex>

#define DRIVE_NAME_LENGHT 32
#define MAX_PHYS_DEVICES  16
#define MIN_SECTOR_SIZE   512
#define NO_PHYS_DRIVE -1
#define ERROR_CHECKING_PHYS_DRIVE -2

#ifdef WINDOWS
#define MAX_DRIVE_COUNT 24
#define MAX_DRIVES_PER_VOLUME 128
typedef struct _M_VOLUME_DISK_EXTENTS {
   DWORD       NumberOfDiskExtents;
   DISK_EXTENT Extents[MAX_DRIVES_PER_VOLUME];
}M_VOLUME_DISK_EXTENTS;
#else
#define MAX_DRIVE_COUNT 26
#define INVALID_SET_FILE_POINTER -1
#endif

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
    void OpenFATSubDir(unsigned char* DirBuffer, int EntryPos, bool doList);
    void subdirh(bool doList);
    void FATDirUp(unsigned char* DirBuffer, char* NameBuffer, bool doList);
    void LoadDirBuf(unsigned int *StartCluster, unsigned char* DirBuffer);
    void LoadSubDir(bool IsRoot, bool doList);
    int  ExtractFile(unsigned char* DirBuffer, int pos);
    void EraseFile(unsigned char* DirBuffer, int EntryPos);
    void SetFATFileDateTime(unsigned char* DirBuffer, int EntryPos, struct tm* DateTime);
    void GetFATFileName(unsigned char* DirBuffer, int DirPos, char* NameBuffer);
    #ifdef WINDOWS
    void SetLocalFileDateTime(tm DateTime, HANDLE fHandle);
    #else
    void SetLocalFileDateTime(tm DateTime, char* FileName);
    void SetLocalFileOwner(uid_t _uid, uid_t _gid, char* FileName);
    #endif
    bool WriteCurrentDirBuf(void);
    bool WriteFAT(void);
    void EnterSubDir(unsigned char* DirBuffer, int EntryPos, bool MakeLocalDir, bool EnterLocalDir, bool ReadOnly);
    int  MakeSubF(unsigned int Clun);
    bool EnterUpDir(unsigned char* DirBuffer, char* NameBuffer, unsigned int* EntryPos, bool ChangeLocalDir, bool ReadOnly);
    int  AddSingleDirToCurrentDir(QString FilePathName, QString FullPath, unsigned char* DirBuffer, bool SetLocalDateTime);
    bool AddFileToCurrentDir(QString FilePathName, unsigned char* DirBuffer);
    bool AddDirTreeToCurrentDir(QString PathName, unsigned char* DirBuffer);
    unsigned long WriteSectors( int StartSector, int Count, unsigned char *Buffer, bool sync);


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
    void on_saveFAT_clicked();

private:
    Ui::GemdDlg *ui;

    void OpenDialog();
};

int  LoadEntryName(unsigned char* DirBuffer, int EntryPos, bool IsRoot, QString* Name);
void FATlookUp(unsigned int StartCluster, unsigned int* NextCluster, unsigned int* FileSector);
void FATfreeCluster(unsigned int Cluster);
unsigned int GetFATFileLength(unsigned char* DirBuffer, int EntryPos);
void SetFATFileLength(unsigned char* DirBuffer, int EntryPos, unsigned int Length);
void GetFATFileDateTime(unsigned char* DirBuffer, int EntryPos, tm* DateTime);
void SortFATNames(unsigned char* DirBuffer, unsigned int* UnSortList, unsigned int* SortList, unsigned int* SortIndex);
void ShowErrorDialog(QWidget* parent, const char* message, int number);
#endif // GEMDDLG_H

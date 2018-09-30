/***************************************************************************
 *   Copyright (C) 2006 by P. Putnik                                       *
 *   pp@ppest.org                                                          *
 *   new QT bindings by F. Heisig 2018                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "gemddlg.h"
#include "ui_gemddlg.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDirIterator>
#include <QCloseEvent>
#include <QTreeView>
#include <QtGui>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef WINDOWS
#include <direct.h>
#endif

typedef unsigned long  ULONG;
typedef unsigned int   UINT;

extern int Filesys; // 0-unknown, 1-DOS, 2-PlusIDE, 3-PlusIDE 256 // 4-7 Charea ;  11-GemDos
extern char Swf ; // Sub filesystem 1-Swapped High/Low
extern int Fsub ;  // Swap request flag - by read or write drive
extern QString fileName;
extern QString op, qhm;
extern ULONG filelen, u ;
extern ULONG ChaSecCnt ;

extern int selected;
extern char detDev[MAX_DRIVE_COUNT][DRIVE_NAME_LENGHT];
extern char physd[DRIVE_NAME_LENGHT];
extern char loadedF[256] ;
extern long LastIOError;
extern int form;
extern int clust ;
extern int status ;

extern char segflag ;
extern ULONG SecCnt ;
extern ULONG OffFS;
extern QString op, qhm;
extern UINT sectr ;
extern ULONG cylc ;
extern UINT heads ;
extern bool ov2ro;
/*
extern unsigned char bufr[640];
extern unsigned char  buf2[524];
extern unsigned char  bufsw[524];
extern QString litet;
*/
#ifdef WINDOWS
extern HANDLE OpenFileX(const char* name, const char * mode);
extern HANDLE OpenDevice(const char* name, const char * mode);
extern void CloseFileX(HANDLE* f);
extern int ReadFromFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, HANDLE fh);
extern int WriteToFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, HANDLE fh);
extern int PutCharFile(const int data, HANDLE fh);
extern long long GetDeviceLengthHandle(HANDLE fh);
extern long long GetDeviceLength(const char* devName);
extern long long GetFileLength64(HANDLE fh);
extern long GetFileLength(HANDLE fh);
extern long SeekFileX(HANDLE fh, int offset, int origin);
extern long long SeekFileX64(HANDLE fh, long long offset, int origin);
extern int  IsPhysDevice(HANDLE f);
extern unsigned char *alignBuffer(const unsigned char *buffer, unsigned long blockSize);
#else
extern FILE* OpenDevice(const char* name, const char * mode);
extern FILE* OpenFileX(const char* name, const char * mode);
extern void CloseFileX(FILE** f);
extern int ReadFromFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, FILE* fh);
extern int WriteToFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, FILE* fh);
extern int PutCharFile(const int data, FILE* fh);
extern long long GetDeviceLengthHandle(FILE* fh);
extern long long GetDeviceLength(const char* devName);
extern long long GetFileLength64(FILE* fh);
extern long GetFileLength(FILE* fh);
extern long SeekFileX(FILE* fh, int offset, int origin);
extern long long SeekFileX64(FILE* fh, long long offset, int origin);
extern int  GetLastError(void);
extern int  IsPhysDevice(FILE* f);
#endif

#ifdef WINDOWS
static HANDLE fhdl, fhout;
#define FILE_OPEN_FAILED INVALID_HANDLE_VALUE
#else
static FILE *fhdl = nullptr, *fhout = nullptr;
static struct stat fileStatus;
static bool fileStatusValid = false;
#define FILE_OPEN_FAILED NULL
#endif

ULONG epos, eposIn, stfilelen, copied ;
ULONG Gpartp[24] ; // GemDos part postions
ULONG Gparsiz[24] ; // GemDos part sizes
ULONG  dirflen ;
int reculev, selcnt, selP, j, d, e ;
unsigned int   oe, zz;
unsigned int   fatTime, fatDate;
long tosave;

int           PartSubDirLevel;
unsigned int  PartSectorSize;
unsigned int  PartReservedSectors ; // Used clusters, free clusters of partition
unsigned int  PartSectorsPerFAT;
unsigned int  PartRootDirSectors;
unsigned int  PartSectorsPerCluster;
unsigned int  PartClusterByteLength;
unsigned int  PartHeadSectors;
unsigned int  PartFirstRootDirSector;
unsigned long PartTotalSectors;
unsigned long PartTotalDataClusters;
unsigned long PartStartPosition;

const unsigned long DIR_ENTRIES_MAX_CNT  = 512;
unsigned int  DirCurrentStartCluster;
unsigned int  DirCurrentClustes[98] ; // For storing DIR cluster -> Points To Sector
unsigned int  DirCurrentLenght;
unsigned int  DirCurrentClusterCnt;
unsigned int  DirParentStartCluster;
unsigned int  DirEntryPos[DIR_ENTRIES_MAX_CNT] ;
unsigned int  DirEntrySortPos[DIR_ENTRIES_MAX_CNT];
unsigned int  DirEntrySortIndex[DIR_ENTRIES_MAX_CNT];
unsigned int  DirEntryCnt;
QStringList   DirRawFilelist;

#define MAX_SUBDIR_DEPTH 16
#define MAX_FAT_BUFFER_SIZE 132000
unsigned int  pDirEntryPos[MAX_SUBDIR_DEPTH] ; // Position in parent dir, by recursive extraction
unsigned int  credirClu[16] ; // Created DIR cluster spread store
unsigned int  credirCnt;
#ifdef WINDOWS
unsigned char _dirbuf[MIN_SECTOR_SIZE * 100 + MIN_SECTOR_SIZE];
unsigned char _PartFATbuffer[MAX_FAT_BUFFER_SIZE + MIN_SECTOR_SIZE];
unsigned char _trasb[MIN_SECTOR_SIZE *128 + MIN_SECTOR_SIZE] ;  // 128 sector
unsigned char *dirbuf, *PartFATbuffer, *trasb;
#else
unsigned char dirbuf[MIN_SECTOR_SIZE * 100];
unsigned char PartFATbuffer[MAX_FAT_BUFFER_SIZE];
unsigned char trasb[MIN_SECTOR_SIZE * 128] ;  // 128 sector
#endif
char subnam[12][MAX_SUBDIR_DEPTH] ;
char subdnam[13];
char subpath[256];
static char DestDir[256];
char res ;
bool timestCur=0;
const unsigned long INVALID_VALUE = 0xFFFFFFFF;
const unsigned long LAST_CLUSTER  = 0xFFFF;

int DirLevel;

#ifdef FRANKS_DEBUG
unsigned char* debug_dirbuf;
#endif

GemdDlg::GemdDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GemdDlg)
{
    ui->setupUi(this);
    ui->AddFiles->setIcon(QIcon(":/Icons/AddFiles.png"));
    ui->desallP->setIcon(QIcon(":/Icons/DeselectAll.png"));
    ui->DeleteFiles->setIcon(QIcon(":/Icons/DeleteFiles.png"));
    ui->ExtractFiles->setIcon(QIcon(":/Icons/Extract.png"));
    ui->opdirP->setIcon(QIcon(":/Icons/AddDir.png"));
    ui->quitP->setIcon(QIcon(":/Icons/Quit.png"));
    ui->setddP->setIcon(QIcon(":/Icons/DestDir.png"));
    ui->newfP->setIcon(QIcon(":/Icons/OpenFolder.png"));
    OpenDialog();
}

GemdDlg::~GemdDlg()
{
    delete ui;
}

void GenerateFileName(unsigned char* buf, int offs, QString *name)
{
   char nstr[60];

   name->clear();
   for(int k=0; k<8; k++) {
      nstr[k] = buf[offs+k] ;
   }
   nstr[8] = '.' ;
   nstr[9] = buf[offs+8];
   nstr[10] = buf[offs+9];
   nstr[11] = buf[offs+10];
   nstr[12] = ' ';
   nstr[13] = ' ';
   nstr[14] = 0 ;
   int o = buf[offs+11] ;
   if ( (o & 16) == 16 ) {
      name->append("DIR");
   }
   else if ((o & 8) == 8) {
      name->append("VOL");
   }
   else {
      // File lenght in bytes from DIR fields:
      unsigned int dirflen = 16777216*buf[offs+31]+65536*buf[offs+30]+256*buf[offs+29]+buf[offs+28] ;
      name->setNum(dirflen);
   }
   name->insert(0, nstr);
}

void GemdDlg::OpenDialog()
{
   int p;
   unsigned long m, n;
   char buffer[64];
   #ifdef WINDOWS
   unsigned char _bufr[MIN_SECTOR_SIZE + MIN_SECTOR_SIZE];
   unsigned char *bufr;
   unsigned char _buf2[MIN_SECTOR_SIZE + MIN_SECTOR_SIZE];
   unsigned char *buf2;
   #else
   unsigned char bufr[MIN_SECTOR_SIZE];
   unsigned char buf2[MIN_SECTOR_SIZE];
   #endif
   ui->saveFAT->setVisible(false);
   ui->AddFiles->setEnabled(false);
   ui->opdirP->setEnabled(false);
   ui->newfP->setEnabled(false);
   ui->ExtractFiles->setEnabled(false);
   ui->DeleteFiles->setEnabled(false);

   #ifdef WINDOWS
   //align main buffers for faster access
   dirbuf = alignBuffer(_dirbuf, MIN_SECTOR_SIZE);
   PartFATbuffer = alignBuffer(_PartFATbuffer, MIN_SECTOR_SIZE);
   trasb = alignBuffer(_trasb, MIN_SECTOR_SIZE);
   bufr = alignBuffer(_bufr, MIN_SECTOR_SIZE);
   buf2 = alignBuffer(_buf2, MIN_SECTOR_SIZE);
   #endif

   DestDir[0] = '\0';
   // Open file or physical drive:
   if (selected<16){
      strcpy(physd, detDev[selected]);
      if ((ov2ro)  && ( SecCnt>0x800000)) {
         fhdl = OpenDevice(physd, "rb" );
      }
      else{
         fhdl = OpenDevice(physd, "r+b" );
      }
   }
   else{
      fhdl = OpenFileX(loadedF, "r+b" );
   }
   if (fhdl == FILE_OPEN_FAILED) {
      QMessageBox::critical(this, "Error", "Drive/file open error ", QMessageBox::Cancel, QMessageBox::Cancel);
      return; //gemdpex
   }
   // Load bootsector
   ReadFromFile(bufr, 1, 512, fhdl);
   if ( Filesys == 13 ){
      Gpartp[0] = 0 ;
      m = bufr[19]+256*bufr[20] ;
      if ( m == 0 ) {
         m = bufr[32]+bufr[33]*256 + bufr[34]*65536 +bufr[35]*16777216 ;
         qhm = "BIGDOS part. ";
      }
      else{
         qhm = "DOS16 part. ";
      }
      n = (bufr[11]+256*bufr[12])>>9 ;   // Sector size
      Gparsiz[0] = m ;
      QString num;
      num.setNum(Gparsiz[0]/2*n);
      qhm.append(num);
      qhm.append(" KB");
      ui->partLB->addItem(qhm);
   }
   // Detect partition system - if DOS (PC) then look only DOS16 and BIGDOS partitions
   // And in extend. partitions
   if ( Filesys == 1 ) {
      p = 0 ; // Partition counter (index)
      for (int i=0x1BE;i<0x1EF;i=i+16) { // Four primary slots
         if (( bufr[i+4] == 6 ) || ( bufr[i+4] == 4 )) {
            // Position, absolute, in sectors
            Gpartp[p] = bufr[i+8]+bufr[i+9]*256 + bufr[i+10]*65536 +bufr[i+11]*16777216 ;
            // Get size:
            Gparsiz[p] = bufr[i+12]+bufr[i+13]*256 + bufr[i+14]*65536 +bufr[i+15]*16777216 ;
            // Not looking CHS entries!
            // Size in KB:
            qhm.setNum(Gparsiz[p]/2);
            if ( bufr[i+4] == 6 ) {  // BigDOS code
               qhm.insert(0, "BigDOS prim.: ");
            }
            if ( bufr[i+4] == 4 ) {  // DOS16 code
               qhm.insert(0, "DOS16 prim.: ");
            }
            qhm.append(" KB");
            ui->partLB->addItem(qhm);
            p++ ;
            if ( p> 24 ){ break; }
         } //dlookext
         if ( bufr[i+4] == 5 ) {  // Extended
            // Position, absolute, in sectors
            epos = bufr[i+8]+bufr[i+9]*256 + bufr[i+10]*65536 +bufr[i+11]*16777216 ;
            do{
               strcpy(buffer,"Ext. ");
               if (SeekFileX64(fhdl, epos*512, SEEK_SET) == INVALID_SET_FILE_POINTER){
                  ShowErrorDialog(this, "Seek error.!", INVALID_SET_FILE_POINTER);
               }
               ReadFromFile(buf2, 1, 512, fhdl);
               // First entry must be regular partition
               if (( buf2[0x1C2] == 6 ) || ( buf2[0x1C2] == 4 )){
                  //Get pos, absolute:
                  Gpartp[p] =  epos + buf2[0x1C6]+buf2[0x1C7]*256 + buf2[0x1C8]*65536 +buf2[0x1C9]*16777216 ;
                  // Get size:
                  Gparsiz[p] = buf2[0x1CA]+buf2[0x1CB]*256 + buf2[0x1CC]*65536 +buf2[0x1CD]*16777216 ;
                  // Size in KB:
                  qhm.setNum(Gparsiz[p]/2);
                  if ( buf2[0x1C2] == 6 ) {  // BigDOS code
                     qhm.insert(0, "BigDOS: ");
                  }
                  if ( buf2[0x1C2] == 4 ) {  // DOS16 code
                     qhm.insert(0, "DOS16: ");
                  }
                  qhm.append(" KB");
                  ui->partLB->addItem(qhm) ;
                  //SendMessage(GetDlgItem(hDlgWnd, GemP), LB_ADDSTRING, 0, (LPARAM) dstr );
                  p++ ;
                  if ( p> 24 ) break ;
                  // Look for further Extended slots:
               } //dlookext2:
               if ( buf2[0x1D2] == 5 ) {
                  epos = epos+buf2[0x1D6]+buf2[0x1D7]*256 + buf2[0x1D8]*65536 +buf2[0x1D9]*16777216 ;
               }
            }while( buf2[0x1D2] == 5 );
            if ( buf2[0x1D2] == 0 ) break ; // End of chain
         }
      } // i loop end
   } //if end
   // See is swapped High/Low... TODO
   // Detecting GemDos partitions
   else {
      if (Swf)
      {   // Swap low/high bytes:
         unsigned char k;
         for (n=0;n<512;n=n+2)
         { k=bufr[n] ; bufr[n]=bufr[n+1];bufr[n+1]=k; }
      }
      p = 0 ; // Partition counter (index)
      for (int i=0x1C6;i<0x1F7;i=i+12) { // Four primary slots
         if (  ((bufr[i+1] == 'G') & (bufr[i+2] == 'E') & (bufr[i+3] == 'M')) | ((bufr[i+1] == 'B') & (bufr[i+2] == 'G') & (bufr[i+3] == 'M'))) {
            // Primary GemDos part
            // Position, absolute, in sectors
            Gpartp[p] = bufr[i+7]+bufr[i+6]*256 + bufr[i+5]*65536 +bufr[i+4]*16777216 ;
            // Get size:
            Gparsiz[p] = bufr[i+11]+bufr[i+10]*256 + bufr[i+9]*65536 +bufr[i+8]*16777216 ;
            // Size in KB:
            qhm.setNum(Gparsiz[p]/2);
            qhm.insert(0, "Primary: ");
            qhm.append(" KB");
            ui->partLB->addItem(qhm);
            p++;
            if (p > 24) { break; }
         }
         if ((bufr[i+1] == 'X') && (bufr[i+2] == 'G') && (bufr[i+3] == 'M')) {
            // Extended GemDos slot
            // Position, absolute, in sectors
            eposIn = bufr[i+7]+bufr[i+6]*256 + bufr[i+5]*65536 +bufr[i+4]*16777216 ;
            epos = eposIn ;
            bool reloop;
            do{
               reloop = false;
               if(SeekFileX64(fhdl, epos*512, SEEK_SET) == INVALID_SET_FILE_POINTER){
                  ShowErrorDialog(this, "Seek error.!", INVALID_SET_FILE_POINTER);
               }
               ReadFromFile(buf2, 1, 512, fhdl);
               if (Swf) {
                  // Swap low/high bytes:
                  unsigned char k;
                  for (n=0;n<512;n=n+2){
                     k=buf2[n];
                     buf2[n]=buf2[n+1];
                     buf2[n+1]=k;
                  }
               }
               // First entry must be regular partition
               if ( ((buf2[0x1C7] == 'G') & (buf2[0x1C8] == 'E')  &  (buf2[0x1C9] == 'M'))| ((buf2[0x1C7] == 'B') & (buf2[0x1C8] == 'G')  &  (buf2[0x1C9] == 'M')) ) {
                  //Get pos, absolute:
                  Gpartp[p] = epos + buf2[0x1CD]+buf2[0x1CC]*256 + buf2[0x1CB]*65536 +buf2[0x1CA]*16777216 ;
                  // Get size:
                  Gparsiz[p] = buf2[0x1D1]+buf2[0x1D0]*256 + buf2[0x1CF]*65536 +buf2[0x1CE]*16777216 ;
                  // Size in KB:
                  qhm.setNum(Gparsiz[p]/2);
                  qhm.insert(0, "Extended: ");
                  qhm.append(" KB");
                  ui->partLB->addItem(qhm);
                  //SendMessage(GetDlgItem(hDlgWnd, GemP), LB_ADDSTRING, 0, (LPARAM) dstr );
                  p++;
                  if ( p > 24 ) { break; }
                  // Look for further Extended slots:
                  if ((buf2[0x1D3] == 'X') && (buf2[0x1D4] == 'G') && (buf2[0x1D5] == 'M')) {
                     epos = eposIn+buf2[0x1D9]+buf2[0x1D8]*256 + buf2[0x1D7]*65536 +buf2[0x1D6]*16777216 ;
                     reloop = true;
                  }
               }
            }while(reloop);
            if ( p > 24 ) { break; }
         }
      } // i loop end
   } // else end
}

void GemdDlg::on_partLB_clicked(const QModelIndex &index)
{
   unsigned int  n, m;
   unsigned char k;
   #ifdef WINDOWS
   unsigned char _buf2[MIN_SECTOR_SIZE + MIN_SECTOR_SIZE];
   unsigned char *buf2;
   #else
   unsigned char buf2[MIN_SECTOR_SIZE];
   #endif

   #ifdef WINDOWS
   //align buffers for faster access
   buf2 = alignBuffer(_buf2, MIN_SECTOR_SIZE);
   #endif


   selP = index.row();
   //Load bootsector of partition:
   PartStartPosition =  Gpartp[selP]*512 ;
   if(SeekFileX64(fhdl, PartStartPosition, SEEK_SET) == INVALID_SET_FILE_POINTER){
      ShowErrorDialog(this, "Seek error.!", INVALID_SET_FILE_POINTER);
   }
   ReadFromFile(buf2, 1, MIN_SECTOR_SIZE, fhdl);
   //lseek(drih, PartStartPosition, 0 );
   //read(drih, buf2, 512);
   if (Swf)
   {   // Swap low/high bytes:
       for (unsigned int n=0;n<MIN_SECTOR_SIZE;n=n+2)
       { k=buf2[n] ; buf2[n]=buf2[n+1];buf2[n+1]=k; }
   }
   // Get BPB parameters:
   PartSectorSize         = buf2[11]+256*buf2[12] ;
   PartSectorsPerFAT      = buf2[22] ;
   n = PartSectorSize>>9 ;
   PartRootDirSectors     = (buf2[17]/16 + 16*buf2[18])/n ;
   PartSectorsPerCluster  = buf2[13] ;
   PartClusterByteLength  = PartSectorsPerCluster * PartSectorSize;
   PartReservedSectors    = buf2[14]+256*buf2[15] ; //usually this value is 1, but it seems to contain trash sometimes
   if (PartReservedSectors > 1) { PartReservedSectors = 1; } //not nice, but needed.
   PartHeadSectors        = PartReservedSectors+PartSectorsPerFAT*2+PartRootDirSectors ;
   PartFirstRootDirSector = PartReservedSectors+PartSectorsPerFAT*2 ;
   PartTotalSectors= buf2[19]+256*buf2[20] ;
   if ( PartTotalSectors == 0 ){
       PartTotalSectors = buf2[32]+buf2[33]*256 + buf2[34]*65536 +buf2[35]*16777216 ;
   }
   PartTotalDataClusters = (PartTotalSectors-PartHeadSectors)/PartSectorsPerCluster;
   if ((PartTotalDataClusters / 2 + 4) > MAX_FAT_BUFFER_SIZE){
      ui->filesLB->clear();
      ShowErrorDialog(this, "Partition table seems to be damaged.", 0);
      return;
   }
   // Display begin sector and count of sectors of selected part. :
   #ifdef FRANKS_DEBUG
   qhm.setNum( Gpartp[selP] );
   qhm.insert(0, "start sec.: ");
   qhm.append(", sec. cnt: ");
   QString num;
   if ( Filesys == 13) {
      num.setNum( PartTotalSectors );
   }
   else {
       num.setNum( Gparsiz[selP] );
   }
   qhm.append(num);
   ui->partinfLab->setText(qhm);
   #endif
   // Load FAT #1
   if(SeekFileX64(fhdl, PartStartPosition+PartSectorSize*PartReservedSectors, SEEK_SET) == INVALID_SET_FILE_POINTER){
      ShowErrorDialog(this, "Seek error.!", INVALID_SET_FILE_POINTER);
   }
   ReadFromFile(PartFATbuffer, 1, PartSectorsPerFAT*PartSectorSize, fhdl);
   if (Swf)
   {   // Swap low/high bytes:
      for (m=0;m<PartSectorsPerFAT*PartSectorSize;m=m+2){
         k=PartFATbuffer[m] ; PartFATbuffer[m]=PartFATbuffer[m+1];PartFATbuffer[m+1]=k;
      }
   }
   loadroot(true);
   ui->AddFiles->setEnabled(true);
   ui->opdirP->setEnabled(true);
   ui->newfP->setEnabled(true);
   ui->ExtractFiles->setEnabled(true);
   ui->DeleteFiles->setEnabled(true);
}
void GemdDlg::on_filesLB_clicked(const QModelIndex &index)
{
    tm  DateTime;
    unsigned int dpos;
    int i, l, listOffset;
    QChar* buf;

    listOffset = (PartSubDirLevel > 0) ? 1 : 0;
    dpos = DirEntrySortPos[index.row()-listOffset];
    QString qhm;
    GenerateFileName(dirbuf, dpos, &qhm);
    l = qhm.length();
    buf = qhm.data();
    for(i=l-1;i>=0;i--){
       if (i>12){ qhm.remove(i,1); }
       else{ if(buf[i] == ' '){ qhm.remove(i,1); } }
    }
    l = qhm.length();
    if(buf[l-1] == '.') {qhm.remove(l-1,1); }
    GetFATFileDateTime(dirbuf, dpos, &DateTime);
    qhm.append(" ");
    QString num;
    num.setNum(DateTime.tm_year + 1900);
    qhm.append(num);
    if (DateTime.tm_mon < 10){ qhm.append("/0"); } else { qhm.append("/"); }
    num.setNum(DateTime.tm_mon + 1);
    qhm.append(num);
    if (DateTime.tm_mday < 10){ qhm.append("/0"); } else { qhm.append("/"); }
    num.setNum(DateTime.tm_mday);
    qhm.append(num);
    if (DateTime.tm_hour < 10){ qhm.append(" 0"); } else { qhm.append(" "); }
    num.setNum(DateTime.tm_hour);
    qhm.append(num);
    if (DateTime.tm_min < 10){ qhm.append(":0"); } else { qhm.append(":"); }
    num.setNum(DateTime.tm_min);
    qhm.append(num);
    if (DateTime.tm_sec < 10){ qhm.append(":0"); } else { qhm.append(":"); }
    num.setNum(DateTime.tm_sec);
    qhm.append(num);
    ui->errti->setText(qhm);
}

unsigned int GetFreeClusters(void)
{
   unsigned int i = 0 ;
   unsigned short fx;

   for(unsigned int n = 4; n < (2*PartTotalDataClusters+4);  n = n+2 ) {
      if (n >= MAX_FAT_BUFFER_SIZE)
      { break; }
      fx = PartFATbuffer[n]+256*PartFATbuffer[n+1] ;
      if ( fx == 0 ) i++  ;
   }
   return(i);
}
void ShowErrorDialog(QWidget* parent, const char* message, int number){
    QString qhm;
    if (number > 0) {
       qhm.setNum(number);
       qhm.insert(0, " (");
       qhm.insert(0, message);
       qhm.append(")");
    } else {
       qhm = message;
    }
    QMessageBox::critical(parent, "Error", qhm, QMessageBox::Cancel, QMessageBox::Cancel);
}


void GemdDlg::ShowPartitionUsage()
{
   unsigned int  SecsFact;
   unsigned long UsedClusters, FreeClusters;

   SecsFact = PartSectorSize/512 ;
   FreeClusters = GetFreeClusters();
   UsedClusters = PartTotalDataClusters-FreeClusters;
   qhm.setNum( (UsedClusters*PartSectorsPerCluster)/2*SecsFact);
   qhm.insert(0,"Used: ");
   qhm.append(" KB  Free: ");
   QString num;
   num.setNum( (FreeClusters*PartSectorsPerCluster)/2*SecsFact );
   qhm.append(num);
   qhm.append(" KB");
   ui->usfrLabel->setText(qhm);
}

void FATlookUp(unsigned int StartCluster, unsigned int* NextCluster, unsigned int* FileSector) // For FAT 16 MS DOS
{
  int fp = 2*StartCluster  ; // 2 times because it' a byte-array holding words
  *NextCluster = PartFATbuffer[fp]+256*PartFATbuffer[fp+1] ;
  *FileSector = PartHeadSectors+(StartCluster-2)*PartSectorsPerCluster ; //first sector of cluster
}
void FATfreeCluster(unsigned int Cluster)
{
  int fp = 2*Cluster  ; // 2 times because it' a byte-array holding words
  PartFATbuffer[fp] = 0;
  PartFATbuffer[fp+1] = 0;
}

int ReadSectors( int StartSector, int Count, unsigned char *Buffer)
{
   int d,e,j;
   unsigned char buf[MIN_SECTOR_SIZE];

   if(SeekFileX64(fhdl, PartStartPosition+StartSector*PartSectorSize, SEEK_SET) == INVALID_SET_FILE_POINTER){
      LastIOError = INVALID_SET_FILE_POINTER;
      return 0;
   }
   //lseek(drih, PartStartPosition+StartSector*PartSectorSize, 0 );
   if (Swf) { j = Count*PartSectorSize/512 ;
      for (e=0;e<j;e++)
      {
         ReadFromFile(buf, 1, MIN_SECTOR_SIZE, fhdl);
         //read(drih, buf, 512);
         for (d=0;d<512;d=d+2) { Buffer[d+e*MIN_SECTOR_SIZE]=buf[d+1] ; Buffer[d+1+e*MIN_SECTOR_SIZE]=buf[d]; }
      }
   }
   else {
      ReadFromFile(Buffer, 1, Count*PartSectorSize, fhdl);
      //read(drih, Buffer, Count*PartSectorSize);
   }
   return 1;
}
unsigned long GemdDlg::WriteSectors( int StartSector, int Count, unsigned char *Buffer, bool sync)
{
    unsigned char buf[MIN_SECTOR_SIZE];
    unsigned int u, written = 0;
    bool failed = false;
    if (Count > 0){
       if(SeekFileX64(fhdl, PartStartPosition+StartSector*PartSectorSize, SEEK_SET) == INVALID_SET_FILE_POINTER){
          ShowErrorDialog(this, "Seek error.!", INVALID_SET_FILE_POINTER);
       }
       if (Swf) {
          j = Count*PartSectorSize/MIN_SECTOR_SIZE;
          u = 0;
          for (e=0;e<j;e++) {
             for (d=0;d<MIN_SECTOR_SIZE;d=d+2) {
                 buf[d+1]=Buffer[d+e*MIN_SECTOR_SIZE] ;
                 buf[d]=Buffer[d+1+e*MIN_SECTOR_SIZE];
             }
             u = WriteToFile(buf, 1, MIN_SECTOR_SIZE, fhdl);
             if ((failed = (u < MIN_SECTOR_SIZE)) == true) break;
             written = written + u;
          }
       }
       else {
          written = WriteToFile(Buffer, 1, Count*PartSectorSize, fhdl);
          failed = (written < Count*PartSectorSize);
       }
       if (failed) {
          ShowErrorDialog(this, "Drive/file write error.", LastIOError);
       }
    }
    if(sync) {
    #ifdef WINDOWS
    FlushFileBuffers(fhdl);
    #else
    fdatasync(fileno(fhdl));
    #endif
    }
    return written;
}
void GemdDlg::LoadDirBuf(unsigned int *StartCluster, unsigned char* DirBuffer)
{
   int n = 0;
   unsigned int NextCluster, FileSector;
   unsigned long CurrentBufOffs = 0;

   do{ //fatop2:
      FATlookUp(*StartCluster, &NextCluster, &FileSector) ;
      //load cluster
      // Store loaded cluster start sector numbers for further writes!!
      DirCurrentClustes[n] = FileSector ;
      res = ReadSectors(FileSector, PartSectorsPerCluster, dirbuf+CurrentBufOffs);
      CurrentBufOffs = CurrentBufOffs+PartClusterByteLength ;
      n++;
      if ( NextCluster>0xFFF0 ){ break; }// For FAT 16
      *StartCluster = NextCluster ;
   }while(CurrentBufOffs<512*98);
   //subdirf:
   DirCurrentLenght = n*PartSectorsPerCluster ; // Current DIR length in sectors
   DirCurrentClusterCnt = n;
   //Clear following sector for sure
   for (int o=0; o<512; o++){ DirBuffer[CurrentBufOffs+o] = 0; }
}
bool GemdDlg::WriteCurrentDirBuf(void)
{
   unsigned long CurrentBufOffs = 0;

   if ( PartSubDirLevel == 0 ) WriteSectors( PartFirstRootDirSector, PartRootDirSectors, dirbuf, false);
   else  {
      for (unsigned int i=0; i<DirCurrentClusterCnt; i++){
         if (WriteSectors( DirCurrentClustes[i], PartSectorsPerCluster, dirbuf+CurrentBufOffs, false) == 0)
         { return false; }
         CurrentBufOffs = CurrentBufOffs+PartClusterByteLength;
      }
      //WriteSectors( 0, 0, NULL, true); //just sync
   }
   return true;
}
bool GemdDlg::WriteFAT(void){
   //Write FAT back:
   if (WriteSectors( PartReservedSectors, PartSectorsPerFAT, PartFATbuffer, false) == 0)
   { return false; }
   //Second FAT
   if (WriteSectors( PartReservedSectors+PartSectorsPerFAT, PartSectorsPerFAT, PartFATbuffer, true))
   { return false; }
   ShowPartitionUsage();
   { return true; }
}

int LoadEntryName(unsigned char* DirBuffer, int EntryPos, bool IsRoot, QString* Name)
{
   if (DirBuffer[EntryPos] == 0) return 1 ;  // Skip empty
   if (DirBuffer[EntryPos] == 0xE5) return 2 ; // Skip deleted
   if ((!IsRoot) && (DirBuffer[EntryPos] == 0x2E)) return 2 ; // Skip subdir marks
   GenerateFileName(DirBuffer, EntryPos, Name);
   return 0 ;
}

int PadtoFATFileName(char *Instr, char* Outstr)
{
   int i, n, k, c;
   // Look where is .
   k = strlen(Instr);
   for (n=0; n<k; n++) {
      if ( Instr[n] == '.' ) break;
   }
   if (k>8) {
      if (n>8) return 0 ;
   }// Invalid filename
   strncpy(Outstr,Instr,n+1) ;
   // Padd name with  spaces by need
   for (c=n;c<8;c++){ Outstr[c]=' '; }
   // Extension if is
   for (i=8;n<k-1;i++) {Outstr[i]=Instr[n+1]; n++; }
   for (c=i;c<11;c++) Outstr[c]=' ';
   Outstr[c]= 0;
   return 1;
}

void PadTOStoLocal(char* TOSName, char*DOSName){
    int p = 0;

    DOSName[0] = '\0';
    for(int i=0; i<12; i++){
       if ((unsigned char)TOSName[i] == 0){ break; }
       if (((unsigned char)TOSName[i] > 126) ||  ((unsigned char)TOSName[i] < 32)){
          //pad TOS specials
          sprintf(DOSName, "%s%s%i%s", DOSName, "[#", (unsigned char)TOSName[i], "]");
          p = strlen(DOSName);
       }
       else{
          DOSName[p++] = TOSName[i];
          DOSName[p] = 0;
       }
    }
}
void PadLocalToTOS(QString DOSName, char* TOSName){
   int p = 0;
   int i = 0;
   int l1 = DOSName.length();
   const char* _DOSName;

   QByteArray ba = DOSName.toLatin1();
   _DOSName = ba.data();

   while(i<l1){
     if ((DOSName[i] == '[') && (i<l1-3)) {
        if (DOSName[i+1] == '#'){
           int e = i+2;
           while ((e<l1) && (DOSName[e] != ']')) { e++;}
           if (e < l1) {
               bool okay = false;
               QString num = DOSName.mid(i+2, e-i-2);
               int o = num.toInt(&okay);
               if(okay){
                  TOSName[p++] = (unsigned char)o;
                  TOSName[p] = 0;
                  i = e+1;
                  continue;
               }
           }
        }
     }
     TOSName[p++] = _DOSName[i++];
     TOSName[p] = 0;
   }
}


int MakeFATFileName(QString SrcFileName, char* DestFileName)
{
   int  i, l1, l2, l3;
   bool stopCount;
   QString qhm, fname, fext;
   char TOSfname[256];

   QFileInfo FileInfo(SrcFileName);
   fname = FileInfo.fileName();
   fext  = FileInfo.suffix();
   l2 = fext.length();
   l1 = fname.length();
   if (l2 > 0) { l1 -= (l2 + 1); }
   //count usable length considering TOS special chars: [#xyz]
   l2 = 0; l3 = 0;
   stopCount = false;
   for(i=0;i<l1;i++){
      l3++;
      if ((fname[i] == '[') && (i < l1-3)) {
         if (fname[i+1] == '#'){
            stopCount = true;
         }
      }
      if ((fname[i] == ']') && (stopCount)){
         stopCount = false;
      }
      if (!stopCount) {l2++;}
      if (l2 == 8) {break;}
   }
   qhm = fname.left(l3);
   fname = qhm.toUpper();
   if (!fext.isEmpty()){
      l1 = fext.length();
      l2 = 0; l3 = 0;
      stopCount = false;
      for(i=0;i<l1;i++){
         l3++;
         if ((fext[i] == '[') && (i < l1-3)) {
            if (fext[i+1] == '#'){
               stopCount = true;
            }
         }
         if ((fext[i] == ']') && (stopCount)){
            stopCount = false;
         }
         if (!stopCount) {l2++;}
         if (l2 == 3) {break;}
      }
      qhm = fext.left(l3);
      fname = fname + '.' + qhm.toUpper();
   }
   PadLocalToTOS(fname, TOSfname);
   return(PadtoFATFileName(TOSfname, DestFileName));
}


void GemdDlg::LoadSubDir(bool IsRoot, bool doList)
{
   unsigned int n, i;
   QString Name;

   DirEntryCnt = 0 ;
   DirRawFilelist.clear();
   for (n=0;n<DirCurrentLenght*PartSectorSize; n=n+32) {
      int rc = LoadEntryName(dirbuf, n, IsRoot, &Name);
      if (rc == 1) break;
      if (rc == 2) continue;
      DirRawFilelist.append(Name);
      DirEntryPos[DirEntryCnt] = n ; // Store pos (bytewise) in DIR
      DirEntryCnt++;
      if (DirEntryCnt >= DIR_ENTRIES_MAX_CNT) { break; }
   }
   for(i=DirEntryCnt;i<DIR_ENTRIES_MAX_CNT;i++){ DirEntryPos[i] = 0; }

   if (doList){
      SortFATNames(dirbuf, DirEntryPos, DirEntrySortPos, DirEntrySortIndex);
      ui->filesLB->clear();
      if (!IsRoot) ui->filesLB->addItem("..");
      for(i=0; i<DirEntryCnt; i++){
          ui->filesLB->addItem(DirRawFilelist.value(DirEntrySortIndex[i]));
      }
   }
   QCoreApplication::processEvents();
}
void GemdDlg::loadroot(bool doList)
{
   if(SeekFileX64(fhdl, PartStartPosition+PartFirstRootDirSector*PartSectorSize, SEEK_SET) == INVALID_SET_FILE_POINTER){
      ShowErrorDialog(this, "Seek error.!", INVALID_SET_FILE_POINTER);
   }
   ReadFromFile(dirbuf, 1, PartRootDirSectors*PartSectorSize, fhdl);
   if (Swf)
   {
      // Swap low/high bytes:
      unsigned char k;
      for (unsigned long m=0;m<PartRootDirSectors*PartSectorSize;m=m+2){
         k=dirbuf[m] ; dirbuf[m]=dirbuf[m+1];dirbuf[m+1]=k;
      }
   }
   // List files from ROOT dir:
   if(doList) { ui->dirLabel->setText("ROOT"); }
   DirCurrentLenght = PartRootDirSectors ;
   PartSubDirLevel = 0;
   DirParentStartCluster = 0 ;
   DirCurrentLenght = PartRootDirSectors; // Current DIR length in sectors
   LoadSubDir(true, doList);
   ShowPartitionUsage() ;
}
void GemdDlg::subdirh(bool doList)
{
   int k;

   DirParentStartCluster = DirCurrentStartCluster ; // Keep this value for SubDIR creation
   if ( PartSubDirLevel ) {
      int spafl, c = 0;
      subpath[0] = '/';
      for(int o=0;o<PartSubDirLevel;o++){
         spafl = 0;
         for(int i=0;i<11;i++) {
            k = subnam[i][o+1] ;
            if(k){
               if (k == ' ' ){
                  spafl++;
               }
               else {
                  if ( spafl>0 ) { subpath[c++] = '.' ; spafl = -111 ; }
                  subpath[c++] = k ;
               }
            }
         }
         subpath[c++]='/';
      }
      subpath[c]= 0;
      if (doList) {ui->dirLabel->setText(subpath);}
   }
   else { if (doList) {ui->dirLabel->setText("root");} }
   // Now load it to dirbuf
   LoadDirBuf(&DirCurrentStartCluster, dirbuf);
   LoadSubDir(false, doList);
}
void GemdDlg::OpenFATSubDir(unsigned char* DirBuffer, int EntryPos, bool doList)
{
   if (EntryPos >= 0) {
      int o = DirBuffer[EntryPos+11] ;
      if ((o & 16) == 16 ) {     // If not SubDir break
         //SetCursor( jc) ;
         PartSubDirLevel++ ;
         DirCurrentStartCluster = DirBuffer[EntryPos+26]+256*DirBuffer[EntryPos+27] ;
         //Get dir name:
         for (int o=0; o<11; o++) {
            // store SUBdir name
            subnam[o][PartSubDirLevel] = DirBuffer[EntryPos+o];
         }
         subdirh(doList);
      }
   }
}

void GemdDlg::FATDirUp(unsigned char* DirBuffer, char* NameBuffer, bool doList)
{
   //are we in subdir? :
   if (dirbuf[0] == 0x2E) {
      //SetCursor( jc) ;
      DirCurrentStartCluster = DirBuffer[32+26]+256*dirbuf[32+27] ;
      if (DirCurrentStartCluster == 0 ) { loadroot(doList); }     //it is root then
      else {
         PartSubDirLevel--;
         if (NameBuffer != NULL){
            for (int o=0;o<11;o++) { NameBuffer[o] = subnam[o][PartSubDirLevel]; }
         }
         subdirh(doList) ;
      }
   }
}
void GemdDlg::EnterSubDir(unsigned char* DirBuffer, int EntryPos, bool MakeLocalDir, bool EnterLocalDir, bool ReadOnly)
{
   char DirName[13];
   char localName[256];
   tm DateTime;

   pDirEntryPos[PartSubDirLevel] = EntryPos; //we need the position if we come back up
   GetFATFileName(DirBuffer, EntryPos, DirName);
   GetFATFileDateTime(DirBuffer, EntryPos, &DateTime);
   if (!ReadOnly) { WriteCurrentDirBuf(); }
   OpenFATSubDir(DirBuffer, EntryPos, false);
   PadTOStoLocal(DirName, localName);
   if(MakeLocalDir){
      #ifdef WINDOWS
      if (_mkdir(localName) != 0){
         ShowErrorDialog(this, "Making local directory failed.", GetLastError());
         return;
      }
      #else
      if (mkdir(localName,0777) != 0){
         ShowErrorDialog(this, "Making local directory failed.", GetLastError());
         return;
      }
      QString DirPath = QDir::currentPath();
      DirPath.append("/");
      DirPath.append(localName);
      if (fileStatusValid){ SetLocalFileOwner(fileStatus.st_uid, fileStatus.st_gid, (char*)DirPath.toLatin1().data()); }
      #endif
   }
   if (EnterLocalDir) {
      if (chdir(localName) != 0){
         ShowErrorDialog(this, "Open local directory failed.", GetLastError());
         return;
      }
   }
   ui->dirLabel->setText(DirName);
   QCoreApplication::processEvents();
}

bool GemdDlg::EnterUpDir(unsigned char* DirBuffer, char* NameBuffer, unsigned int* UpEntryPos, bool ChangeLocalDir, bool ReadOnly) //returns IsRoot
{
   if (PartSubDirLevel == 0) { //we're already on root level
      return true;
   }
   if (!ReadOnly) { WriteCurrentDirBuf(); }
   FATDirUp(DirBuffer, NameBuffer, false);
   if (NameBuffer != NULL) {
      ui->dirLabel->setText(NameBuffer);
      QCoreApplication::processEvents();
   }
   if(ChangeLocalDir){
      if(chdir("..") != 0){
         ShowErrorDialog(this, "Open local directory failed.", GetLastError());
         return false;
      }
   }
   if (UpEntryPos != NULL) {
      *UpEntryPos = pDirEntryPos[PartSubDirLevel];
   }
   return (PartSubDirLevel == 0);
}


void GemdDlg::on_setddP_clicked()
{
   #ifndef FRANKS_DEBUG
   fileName = QFileDialog::getExistingDirectory(this, tr("Select dest. dir:"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
   if( !fileName.isEmpty() )
   #endif
   {
      #ifndef FRANKS_DEBUG
      strcpy(DestDir, (char*)fileName.toLatin1().data());
      chdir(DestDir);
      #endif
      #ifndef WINDOWS
      if (stat(DestDir, &fileStatus) != 0){
          ShowErrorDialog(this, "Getting dir status failed.", errno);
          fileStatusValid = false;
      } else { fileStatusValid = true; }
      //debug
      /*
      if (fileStatusValid){
         QString qhm;
         struct passwd *pw = getpwuid(fileStatus.st_uid);
         struct group *grp = getgrgid(fileStatus.st_gid);
         qhm.clear();
         qhm.append("User: ");
         qhm.append(pw->pw_name);
         qhm.append(", Group: ");
         qhm.append(grp->gr_name);
         QMessageBox::information(this, "Owner Information", qhm, QMessageBox::Ok, QMessageBox::Ok);
      }
      */
      #endif
   }
}

void GemdDlg::on_filesLB_doubleClicked(const QModelIndex &index)
{
   if (PartSubDirLevel > 0) {
       if (index.row() == 0) { //updir
           FATDirUp(dirbuf, NULL, true);
       } else {
           OpenFATSubDir(dirbuf, DirEntrySortPos[index.row()-1], true);
       }

   } else { OpenFATSubDir(dirbuf, DirEntrySortPos[index.row()], true); }
}

void GemdDlg::on_timeCB_clicked()
{
   if( ui->timeCB->isChecked() ) timestCur=1;
   else timestCur=0;
}
#ifdef WINDOWS
//file time will locally be stored as UTC time and shown as Local time on local filesystem
//
void GemdDlg::SetLocalFileDateTime(tm DateTime, HANDLE fHandle)
#else
void GemdDlg::SetLocalFileDateTime(tm DateTime, char* FileName)
#endif
{
    #ifdef WINDOWS
    FILETIME  fTime;
    SYSTEMTIME sysdt;
    #else
    struct utimbuf fildt;
    time_t tim ;
    #endif

    #ifdef WINDOWS
    sysdt.wMilliseconds = 0;
    sysdt.wSecond  = DateTime.tm_sec;
    sysdt.wMinute  = DateTime.tm_min;
    sysdt.wHour    = DateTime.tm_hour;

    sysdt.wDay     = DateTime.tm_mday;
    sysdt.wMonth   = DateTime.tm_mon + 1; //tm: 0..11, SYSTEMTIME: 1..12
    sysdt.wYear    = DateTime.tm_year + 1900;

    SystemTimeToFileTime(&sysdt, &fTime);
    if (SetFileTime(fHandle, &fTime, &fTime, &fTime) == 0){
       ShowErrorDialog(this, "Setting file/dir date/time failed.", GetLastError());
    }
    #else
    //dirLabel->setText(asctime( &tout));
    tim = mktime( &DateTime) ;
    fildt.actime = tim;
    fildt.modtime = tim;
    if (utime(FileName, &fildt) != 0){
       ShowErrorDialog(this, "File set time error.", errno);
    }
    #endif
}
#ifndef WINDOWS
void GemdDlg::SetLocalFileOwner(uid_t _uid, uid_t _gid, char* FileName){

   struct stat fstat;

   if (stat(FileName, &fstat) == 0){
      //debug
      /*
      {
         QString qhm;
         struct passwd *pw = getpwuid(fstat.st_uid);
         struct group *grp = getgrgid(fstat.st_gid);
         qhm = FileName;
         qhm.append(": User: ");
         qhm.append(pw->pw_name);
         qhm.append(", Group: ");
         qhm.append(grp->gr_name);
         QMessageBox::information(this, "Current owner Information", qhm, QMessageBox::Ok, QMessageBox::Ok);
      }
      */
      if ((fstat.st_uid != _uid) || (fstat.st_gid != _gid)){
         //debug
         /*
         {
            QString qhm;
            struct passwd *pw = getpwuid(fstat.st_uid);
            struct group *grp = getgrgid(fstat.st_gid);
            qhm = "Change owner/group from:\n";
            qhm.append("User: ");
            qhm.append(pw->pw_name);
            qhm.append(", Group: ");
            qhm.append(grp->gr_name);
            qhm.append("\nTo:\n");
            pw = getpwuid(_uid);
            grp = getgrgid(_gid);
            qhm.append("User: ");
            qhm.append(pw->pw_name);
            qhm.append(", Group: ");
            qhm.append(grp->gr_name);
            QMessageBox::information(this, "Current Owner Information", qhm, QMessageBox::Ok, QMessageBox::Ok);
         }
         */
         if (chown(FileName, _uid, _gid) != 0){
            ShowErrorDialog(this, "Setting file/dir owner/group failed.", errno);
         }
      }
   } else { ShowErrorDialog(this, "Getting file/dir status failed.", errno); }
}
#endif

int GemdDlg::ExtractFile(unsigned char* DirBuffer, int EntryPos)
{
   bool doloop = false;
   int rc = 0;
   unsigned int StartCluster, NextCluster, FileSector;
   unsigned int FileLength, msapos = 0;
   #ifdef WINDOWS
   unsigned char _DataBuffer[MIN_SECTOR_SIZE * 128 + MIN_SECTOR_SIZE];
   unsigned char *DataBuffer;
   #else
   unsigned char DataBuffer[MIN_SECTOR_SIZE * 128];  // 128 sector
   #endif
   int RemainingBytes;
   char name[13], localName[256];
   tm DateTime;

   #ifdef WINDOWS
   //align buffers for faster access
   DataBuffer = alignBuffer(_DataBuffer, MIN_SECTOR_SIZE);
   #endif
   //load file from drive/image:
   StartCluster = DirBuffer[EntryPos+26]+256*DirBuffer[EntryPos+27] ;
   FileLength = 16777216*DirBuffer[EntryPos+31]+65536*DirBuffer[EntryPos+30]+256*DirBuffer[EntryPos+29]+DirBuffer[EntryPos+28] ;
   RemainingBytes = FileLength;
   GetFATFileDateTime(DirBuffer, EntryPos, &DateTime);
   GetFATFileName(DirBuffer, EntryPos, name);
   // open save file:
   PadTOStoLocal(name, localName);
   fhout = OpenFileX(localName, "wb");
   if (fhout == FILE_OPEN_FAILED) {
      ShowErrorDialog(this, "File open error.", LastIOError);
      ui->errti->setText("Error!");
	  return 1;
   }
   long tosave = PartClusterByteLength ;
   // get sector(s) to load and next cluster
   do {
      FATlookUp(StartCluster, &NextCluster, &FileSector);
      ReadSectors(FileSector, PartSectorsPerCluster, DataBuffer);
      if ( RemainingBytes<(int)(PartClusterByteLength) ) { tosave = RemainingBytes; }
      ULONG written;
      if ((written = WriteToFile(DataBuffer, 1, tosave, fhout)) > 0) {
         msapos = msapos + written;
      }
      RemainingBytes = RemainingBytes-PartClusterByteLength;
      doloop = ( RemainingBytes >= 0 ) && ( NextCluster <= 0xfff0 );
      if (doloop) {StartCluster = NextCluster;}
   } while(doloop);
   if (msapos != FileLength ) {
      ui->errti->setText("error!");
      rc = 1;
   }
   else if (!timestCur) {
      #ifdef WINDOWS
      SetLocalFileDateTime(DateTime, fhout);
      CloseFileX(&fhout);
      #else
      CloseFileX(&fhout);
      SetLocalFileDateTime(DateTime, localName);
      if (fileStatusValid){ SetLocalFileOwner(fileStatus.st_uid, fileStatus.st_gid, localName); }
      #endif
   } else { CloseFileX(&fhout); }
   return rc;
}


void GemdDlg::GetFATFileName(unsigned char* DirBuffer, int EntryPos, char* NameBuffer)
{
   unsigned int i, k;
   
   for(i=0;i<8;i++) {
      if (DirBuffer[EntryPos+i] == 32) break ;
      NameBuffer[i] = DirBuffer[EntryPos+i] ;
   }
   if (DirBuffer[EntryPos+8] != 32){
      NameBuffer[i] = '.' ;
	  i++ ;
	  for(k=8;k<11;k++) {
         if (DirBuffer[EntryPos+k] == 32) { break; }
         NameBuffer[i] = DirBuffer[EntryPos+k];
		 i++;
	  }
   }
   NameBuffer[i] = 0 ; // String term.
   ui->exwht->setText(NameBuffer);
   //ui->errti->setText(" ");
   QCoreApplication::processEvents();
}
bool IsEmpty(unsigned char* DirBuffer, int EntryPos)
{
   unsigned char o;
   o = DirBuffer[EntryPos] ;
   if ( o == 0 ) { return true; }
   return false;
}
bool IsDeleted(unsigned char* DirBuffer, int EntryPos)
{
   unsigned char o;
   o = DirBuffer[EntryPos] ;
   if ( o == 0xE5 ) { return true; }
   return false;
}
bool IsSubDir(unsigned char* DirBuffer, int EntryPos)
{
   unsigned char o;
   o = DirBuffer[EntryPos+11] ;
   if ( (o & 0x10) == 16 ) { return true; }
   return false;
}
bool IsFile(unsigned char* DirBuffer, int EntryPos)
{
   unsigned char o;
   o = DirBuffer[EntryPos+11] ;
   if ((o & 0x18) == 0) { return true; }
   return false;
}
bool IsVolume(unsigned char* DirBuffer, int EntryPos)
{
   unsigned char o;
   o = DirBuffer[EntryPos+11] ;
   if ((o & 0x08) == 8 ) { return true; }
   return false;
}
unsigned int GetFATFileLength(unsigned char* DirBuffer, int EntryPos)
{
   unsigned int m;
   m =  DirBuffer[EntryPos+28];
   m += DirBuffer[EntryPos+29] << 8;
   m += DirBuffer[EntryPos+30] << 16;
   m += DirBuffer[EntryPos+31] << 24;
   return m;
}

void SetFATFileLength(unsigned char* DirBuffer, int EntryPos, unsigned int Length)
{
   DirBuffer[EntryPos+28] = Length;
   DirBuffer[EntryPos+29] = Length>>8;
   DirBuffer[EntryPos+30] = Length>>16;
   DirBuffer[EntryPos+31] = Length>>24;
}

tm* CurrentDateTime(void)
{

    static tm DateTime;
    time_t tim ;
    #ifdef WINDOWS
    tm * trc;
    #endif

    // current timestamp:
    time(&tim);
    #ifdef WINDOWS
    trc = gmtime(&tim);
    if (trc != NULL) {
       memcpy(&DateTime, trc, sizeof(tm));
    } else {
       DateTime.tm_hour = 0;
       DateTime.tm_min  = 0;
       DateTime.tm_sec  = 0;
       DateTime.tm_mday = 1;
       DateTime.tm_mon  = 0;
       DateTime.tm_year = 80;
    }
    #else
    localtime_r(&tim, &DateTime);
    #endif

    return &DateTime;
}

void GemdDlg::SetFATFileDateTime(unsigned char* DirBuffer, int EntryPos, struct tm* DateTime)
{
   unsigned int n;

   n = (DateTime->tm_sec/2)|(DateTime->tm_min<<5)|(DateTime->tm_hour<<11) ;
   fatTime = n ;
   DirBuffer[EntryPos+22] = n;
   DirBuffer[EntryPos+23] = n>>8;
   n = (DateTime->tm_mday)|((DateTime->tm_mon+1)<<5)|( (DateTime->tm_year-80)<<9) ;
   fatDate = n;
   DirBuffer[EntryPos+24] = n;
   DirBuffer[EntryPos+25] = n>>8;
}

void GetFATFileDateTime(unsigned char* DirBuffer, int EntryPos, tm* DateTime)
{
   unsigned int n;

   n = DirBuffer[EntryPos+22];
   n = n + (DirBuffer[EntryPos+23] << 8);
   fatTime = n ;

   DateTime->tm_hour =  n >> 11;
   DateTime->tm_min  = (n >> 5) & 0x003F;
   DateTime->tm_sec  = (n & 0x1F) << 1;
   //n = (tout.tm_sec/2)|(tout.tm_min<<5)|(tout.tm_hour<<11) ;

   n = DirBuffer[EntryPos+24];
   n = n + (DirBuffer[EntryPos+25] << 8);
   fatDate = n;
   DateTime->tm_year = (n >> 9) + 80; //GEMDOS: from 1980 UNIX/DOS/WINDOWS: from 1900
   DateTime->tm_mon  = ((n >> 5) & 0x000F) - 1; //GEMDOS: 1..12 UNIX/DOS/WINDOWS: 0..11 !!
   if (DateTime->tm_mon < 0) { DateTime->tm_mon = 0; }
   DateTime->tm_mday =  n & 0x001F;
   //n = (tout.tm_mday)|((tout.tm_mon+1)<<5)|( (tout.tm_year-80)<<9) ;

}

void SortFATNames(unsigned char* DirBuffer, unsigned int* UnSortList, unsigned int* SortList, unsigned int* SortIndex)
{
   QStringList VolNameList;
   QStringList DirNameList;
   QStringList FileNameList;
   unsigned int i, k;

   i = 0;
   for (QStringList::iterator sl = DirRawFilelist.begin(); sl != DirRawFilelist.end(); ++sl) {
       QString CurrentItem = *sl;
       if( !IsEmpty(DirBuffer, UnSortList[i]) && !IsDeleted(DirBuffer, UnSortList[i])){
          if (IsVolume(DirBuffer, UnSortList[i])){
              VolNameList.append(CurrentItem);
          }
          if (IsSubDir(DirBuffer, UnSortList[i])){
              DirNameList.append(CurrentItem);
          }
          if (IsFile(DirBuffer, UnSortList[i])){
              FileNameList.append(CurrentItem);
          }
      }
      i++;
   }
   VolNameList.sort();
   DirNameList.sort();
   FileNameList.sort();
   k = 0;
   for (QStringList::iterator sl = VolNameList.begin(); sl != VolNameList.end(); ++sl) {
      QString SortItem = *sl;
      i = 0;
      for (QStringList::iterator pl = DirRawFilelist.begin(); pl != DirRawFilelist.end(); ++pl) {
         QString PrimItem = *pl;
         if (SortItem.compare(PrimItem, Qt::CaseInsensitive) == 0){
            SortList[k] = UnSortList[i];
            SortIndex[k] = i;
            k++;
         }
         i++;
      }
   }
   for (QStringList::iterator sl = DirNameList.begin(); sl != DirNameList.end(); ++sl) {
      QString SortItem = *sl;
      i = 0;
      for (QStringList::iterator pl = DirRawFilelist.begin(); pl != DirRawFilelist.end(); ++pl) {
         QString PrimItem = *pl;
         if (SortItem.compare(PrimItem, Qt::CaseInsensitive) == 0){
            SortList[k] = UnSortList[i];
            SortIndex[k] = i;
            k++;
         }
         i++;
      }
   }
   for (QStringList::iterator sl = FileNameList.begin(); sl != FileNameList.end(); ++sl) {
      QString SortItem = *sl;
      i = 0;
      for (QStringList::iterator pl = DirRawFilelist.begin(); pl != DirRawFilelist.end(); ++pl) {
         QString PrimItem = *pl;
         if (SortItem.compare(PrimItem, Qt::CaseInsensitive) == 0){
            SortList[k] = UnSortList[i];
            SortIndex[k] = i;
            k++;
         }
         i++;
      }
   }
   for(i=k; i<DIR_ENTRIES_MAX_CNT;i++){ SortList[i] = 0; }

}

void GemdDlg::EraseFile(unsigned char* DirBuffer, int EntryPos)
{
   unsigned int StartCluster, NextCluster, FileSector, FileLength, l;
   bool CheckFileLenght;
   char EntryName[64];

   GetFATFileName(DirBuffer, EntryPos, EntryName);
   if (DirBuffer[EntryPos] == 0) return;  // Skip empty
   if (DirBuffer[EntryPos] == 0xE5) return; // Skip deleted
   StartCluster = DirBuffer[EntryPos+26]+256*DirBuffer[EntryPos+27] ;
   // free clusters
   CheckFileLenght = IsFile(DirBuffer, EntryPos);
   if (CheckFileLenght) { FileLength = GetFATFileLength(DirBuffer, EntryPos); }
   else { FileLength = 1; }
   l = 0;
   while((StartCluster <= 0xfff0) && (l < FileLength)) {
      FATlookUp(StartCluster, &NextCluster, &FileSector);
      FATfreeCluster(StartCluster);
      StartCluster = NextCluster;
      if (StartCluster == 0){
          QMessageBox::critical(this, EntryName, "FAT error! Cluster chain ends with zero.", QMessageBox::Cancel, QMessageBox::Cancel);
          break;
      }
   }
   DirBuffer[EntryPos] = 0xE5; // erase Dir entry
}


bool GemdDlg::AddDirTreeToCurrentDir(QString PathName, unsigned char* DirBuffer){

    int  EntryPos, CurLocalDirLevel;
    int  NewLocalDirLevel, LocalDirLevel;
    bool failed = false;

    //PartSubDirLevel
    LocalDirLevel = PathName.count(QLatin1Char('/'));
    #ifdef WINDOWS
    //QT under Windows quotes dir separators sometimes as '/' sometimes as '\'
    //we have to take care for both, fortunately they they exclude each other
    //under Windows
    LocalDirLevel += PathName.count(QLatin1Char('\\'));
    #endif
    QFileInfo fi(PathName);
    QString fn = fi.fileName();
    if (chdir((char*)PathName.toLatin1().data()) != 0){
        ShowErrorDialog(this, "Open local directory failed.", GetLastError());
        return false;
    }
    //EntryPos = AddSingleDirToCurrentDir(fn, PathName, DirBuffer, true);
    //if (EntryPos == -1) {return false;}
    //EnterSubDir(DirBuffer, EntryPos, false, false, false);

    CurLocalDirLevel = LocalDirLevel + 1;
    QDirIterator it(PathName, QDir::NoFilter, QDirIterator::Subdirectories);
    while (it.hasNext()) {
       QString qhm = it.next();
       QFileInfo fi = it.fileInfo();
       if (fi.fileName().compare(".") == 0) {continue;}
       if (fi.fileName().compare("..") == 0) {continue;}
       NewLocalDirLevel = qhm.count(QLatin1Char('/'));
       #ifdef WINDOWS
       NewLocalDirLevel += qhm.count(QLatin1Char('\\'));
       #endif
       while (NewLocalDirLevel < CurLocalDirLevel){
          EnterUpDir(DirBuffer, NULL, NULL, false, false);
          CurLocalDirLevel--;
       }
       if (fi.isDir()){
          EntryPos = AddSingleDirToCurrentDir(fi.fileName(), qhm, DirBuffer, true);
          if (EntryPos == -1) {break;}
          EnterSubDir(DirBuffer, EntryPos, false, false, false);
          CurLocalDirLevel++;
       } else if (fi.isFile()){
          failed = !AddFileToCurrentDir(qhm, DirBuffer);
          if (failed) { break; }
       }
    }
    while (CurLocalDirLevel > (LocalDirLevel + 1)){
       EnterUpDir(DirBuffer, NULL, NULL, false, false);
       CurLocalDirLevel--;
    }
    return !failed;
}




bool GetFirstFreeDirSlot(unsigned char* DirBuffer, unsigned int* NewEntryPos)
{
   unsigned short fx;

   if (PartSubDirLevel==0) fx = 0;
   else fx=64; // By subdir is 2 first slot occupied
   for ( *NewEntryPos = fx; *NewEntryPos<(DirCurrentLenght*PartSectorSize);*NewEntryPos=*NewEntryPos+32) {
      if (DirBuffer[*NewEntryPos]==0) return true;
   }
   //If not found 0 seek E5 for deleted slot:
   for ( *NewEntryPos = fx; *NewEntryPos<(DirCurrentLenght*PartSectorSize);*NewEntryPos=*NewEntryPos+32) {
      if (DirBuffer[*NewEntryPos]==0xE5) return true;
   }
   return false ;
}
int CheckNameExistsInDir(char* Name, unsigned char* DirBuffer)
{
   unsigned int n, p, fx;

   if (PartSubDirLevel==0) p = 0;
   else p = 64; // By subdir is 2 first slot occupied
   for ( fx = p; fx<(DirCurrentLenght*PartSectorSize);fx=fx+32) {
      for (n=0;n<11;n++) {
         if ( Name[n] != DirBuffer[fx+n]) break;
      }
      if (n==11) return 0; // Same name found
   }
   return 1; // Not found same name
}
unsigned long SeekFirstFreeCluster(void)
{
   unsigned int m, fx;

   for( m = 4; m < (2*PartTotalDataClusters+4);  m = m+2 ) {
      fx = PartFATbuffer[m]+256*PartFATbuffer[m+1] ;
      if ( fx == 0 ) break  ; // free cluster
   }
   if ( m ==(2*PartTotalDataClusters+4) ) return INVALID_VALUE ; // Not found
   return m/2 ; // This is first free cluster #
}

unsigned long SeekNextFreeCluster(unsigned long StartCluster)
{
   unsigned long m, fx, ne;

   ne = 2 * StartCluster + 2 ; // Twice cluster # and go ahead to next
   for( m = ne; m < (2*PartTotalDataClusters+4);  m = m+2 )
   {
      fx = PartFATbuffer[m]+256*PartFATbuffer[m+1] ;
      if ( fx == 0 ) break  ; // free cluster
   }
   if ( m==(2*PartTotalDataClusters+4) ) { m=m/2; return INVALID_VALUE; } // Not found
   return m/2 ; // This is first free cluster #
}

void MarkCluster(unsigned long Cluster, unsigned long PointsTo)
{
   Cluster = Cluster*2 ;
   PartFATbuffer[Cluster]= PointsTo;
   PartFATbuffer[Cluster+1]= PointsTo >> 8;
}

int GemdDlg::MakeSubF(unsigned int Clun) // Create start cluster of subdirectory
{
   int n;
   unsigned int FileSector;

   // Calculate logical sector by cluster #
   FileSector = PartHeadSectors+(Clun-2)*PartSectorsPerCluster ;
   // Create content:
   // First clear:
   for (UINT i=0; i<(PartClusterByteLength); i++) trasb[i]=0 ;
   trasb[0] = 0x2E ;
   for (UINT i =1; i<11;i++ ) trasb[i] = ' ' ;
   trasb[11] = 16 ; // SubDir flag
   n = fatTime ;
   trasb[22] = n;
   trasb[23] = n>>8;
   n = fatDate ;
   trasb[24] = n;
   trasb[25] = n>>8;
   n = Clun ;
   trasb[26] = n;
   trasb[27] = n>>8; // Start cluster of itself
   trasb[32] = 0x2E ;
   trasb[33] = 0x2E ;
   for (UINT n =34;n<32+11;n++ ) trasb[n] = ' ' ;
   trasb[43] = 16 ; // SubDir flag
   n = DirParentStartCluster; //!!!!!!! zero if parent is ROOT
   trasb[26+32] = n;
   trasb[27+32] = n>>8; // Start cluster of parent
   // Write it to drive or image:
   UINT result = WriteSectors( FileSector, PartSectorsPerCluster, trasb, false);
   /*
   if ( res != (PartClusterByteLength) )   {
      MessageBox(hDlgWnd, "Write error!", "Problem",
      MB_OK | MB_ICONSTOP);
   }
   */
   if ( result != (PartClusterByteLength)) { return 0;}
   return 1;
}
int GemdDlg::AddSingleDirToCurrentDir(QString FilePathName, QString FullPath, unsigned char* DirBuffer, bool SetLocalDateTime){

    unsigned long m,n;
    char subdnam[256];
    char name[60];
    unsigned int FileSector, EntryPos;
    tm dDateTime;
    #ifdef WINDOWS
    HANDLE DirH;
    FILETIME tc, tlm, tla;
    #else
    struct stat fparms ;
    #endif

    strcpy(subdnam,(char*)FilePathName.toLatin1().data());
    int res = MakeFATFileName(FilePathName, name);
    //char res = PadtoFATFileName(subdnam, name) ;
    if (!res) {
       QMessageBox::critical(this, tr("Invalid filename!"), tr("Must have dot before ext."), QMessageBox::Cancel, QMessageBox::Cancel);
       return -1;
    }
    // Testing
    ui->exwht->setText(name);
    // On free cluster at least needed
    if ( GetFreeClusters() < 1 ) {
      QMessageBox::critical(this, tr("Partition full!"), tr("No place"), QMessageBox::Cancel, QMessageBox::Cancel);
      return -1;
    }
    res = CheckNameExistsInDir(name, DirBuffer) ;
    if (!res) {
       QMessageBox::critical(this, tr("Name exists!"), tr("Name already in Dir."), QMessageBox::Cancel, QMessageBox::Cancel);
       return -1;
    }
    // Now seek first free slot in DIR:
    if (!GetFirstFreeDirSlot(DirBuffer, &EntryPos)){
       QMessageBox::critical(this, tr("DIR full!"), tr("Error"), QMessageBox::Cancel, QMessageBox::Cancel);
       return -1;
    }
    // Copy name at EntryPos, add timestamp and directory flag
    for (n=0;n<11;n++) DirBuffer[EntryPos+n]=name[n];
    DirBuffer[EntryPos+11]=16; // DIR flag
    //Set start cluster #
    // Seek first free cluster
    m = SeekFirstFreeCluster(); //we have already checked - there are definitely enough free ones
    n = m;
    DirBuffer[EntryPos+26] = n;
    DirBuffer[EntryPos+27] = n>>8;
    credirClu[0] = m ; // store spread of created
    credirCnt = 1;
    // seek for next 15 free cluster:
    ULONG oldCluster = m;
    n = PartSectorSize>>9 ;
    while ( credirCnt<(32/(PartSectorsPerCluster*n)) ) {
       if ((m = SeekNextFreeCluster(m)) == INVALID_VALUE) break ;
       credirClu[credirCnt] = m ;
       // write into FAT
       MarkCluster(oldCluster, m);
       credirCnt++ ;
       oldCluster = m;
    }
    // mark as last:
    MarkCluster(oldCluster, LAST_CLUSTER) ;
    // Create and write empty SubDir:
    // first cluster of:
    MakeSubF(credirClu[0]);
    // First clear:
    for (UINT n=0;n<(PartClusterByteLength);n++){ trasb[n]=0; }
    // clear following clusters of DIR:
    for (UINT i=1;i<credirCnt;i++) {
       // Calculate logical sector by cluster #
       FileSector = PartHeadSectors+(credirClu[i]-2)*PartSectorsPerCluster ;
       res = WriteSectors( FileSector, PartSectorsPerCluster, trasb, false);
    }
    if (SetLocalDateTime && (!timestCur)) {
       #ifdef WINDOWS
       DirH = CreateFileA((char*)FullPath.toLatin1().data(), GENERIC_READ,  FILE_SHARE_READ, NULL,
                                 OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
       if (DirH != INVALID_HANDLE_VALUE) {
          if (GetFileTime(DirH, &tc, &tlm, &tla)){
             SYSTEMTIME sysdt;
             FileTimeToSystemTime(&tlm, &sysdt);
             dDateTime.tm_sec  = sysdt.wSecond;
             dDateTime.tm_min  = sysdt.wMinute;
             dDateTime.tm_hour = sysdt.wHour;
             dDateTime.tm_mday = sysdt.wDay;
             dDateTime.tm_mon  = sysdt.wMonth - 1;
             dDateTime.tm_year = sysdt.wYear - 1900;

          } else memcpy(&dDateTime, CurrentDateTime(), sizeof(tm));
          CloseHandle(DirH);

       } else {
           ShowErrorDialog(this, "Error open dir for getting modification time. ", GetLastError());
           memcpy(&dDateTime, CurrentDateTime(), sizeof(tm));
       }

       #else
       stat((char*)FullPath.toLatin1().data(), &fparms );
       localtime_r( &fparms.st_mtime, &dDateTime);
       #endif
    } else {
       memcpy(&dDateTime, CurrentDateTime(), sizeof(tm));
    }
    SetFATFileDateTime(DirBuffer, EntryPos, &dDateTime);
    return EntryPos;
}



bool GemdDlg::AddFileToCurrentDir(QString FilePathName, unsigned char* DirBuffer)
{
   char   FATFileName[256];
   char   LocalFileName[256];
   struct stat fparms ;
   unsigned int k, FileByteLength;
   //unsigned int FileStartCluster;
   unsigned int n, FileClusterCount, EntryPos, FileSector, CurrentFileCluster, PreviousFileCluster;
   bool failed = false;
   FILE *inFile;
   tm   fDateTime;

   strcpy(LocalFileName, (char*)FilePathName.toLatin1().data());   // Filename with path?
   MakeFATFileName(FilePathName, FATFileName);
   ui->exwht->setText(FATFileName);   // show corrected filename
   QCoreApplication::processEvents();
   //sleep(1); //testing
   // Open file and get it's length:
   // Ignore directories:
   stat( LocalFileName , &fparms );
   if (S_IFDIR & fparms.st_mode ) {
      QMessageBox::critical(this, LocalFileName, tr("Entered AddFile with directory entry!"), QMessageBox::Cancel, QMessageBox::Cancel);
      return(true);
   }
   inFile = fopen(LocalFileName, "rb");
   if (inFile == NULL) {
      QMessageBox::critical(this, tr("File open error"), FilePathName, QMessageBox::Cancel, QMessageBox::Cancel);
      return(false);
   }
   //get filelength :
   fseek(inFile, 0, SEEK_END) ;
   FileByteLength = ftell(inFile); //Filelength got
   fseek(inFile,0,SEEK_SET); // Back to filestart dude!
   // Get needed cluster count:
   n = PartClusterByteLength;
   FileClusterCount = FileByteLength/n ;
   if ( FileByteLength % n) FileClusterCount++ ;
   // Check is there enough space:
   if ( GetFreeClusters() < FileClusterCount ) {
      QMessageBox::critical(this, tr("No place for file"), FilePathName, QMessageBox::Cancel, QMessageBox::Cancel);
      fclose(inFile);
      return(false);
   }
   // Is name already in cur DIR?
   res = CheckNameExistsInDir(FATFileName, dirbuf) ;
   if (!res) {
      QMessageBox::critical(this, tr("Name exists!"), "Name already in Dir.", QMessageBox::Cancel, QMessageBox::Cancel);
      fclose(inFile);
      return(false);
   }
   // Now seek first free slot in DIR:
   if (!GetFirstFreeDirSlot(DirBuffer, &EntryPos)) {
      QMessageBox::critical(this, tr("Directory full!"), "Error", QMessageBox::Cancel, QMessageBox::Cancel);
      fclose(inFile);
      return(false);
   }
   // Copy name at EntryPos, add timestamp and directory flag
   for (n=0;n<11;n++)
       DirBuffer[EntryPos+n]=FATFileName[n];
   //Enter filelength in filerecord:
   SetFATFileLength(DirBuffer, EntryPos, FileByteLength);
   #ifdef WINDOWS
   {
     tm* ptime = gmtime(&fparms.st_mtime);
     memcpy(&fDateTime, (ptime != NULL) ? ptime : CurrentDateTime(), sizeof(tm));
   }
   #else
   localtime_r( &fparms.st_mtime, &fDateTime);
   #endif
   SetFATFileDateTime(DirBuffer, EntryPos, (timestCur) ? CurrentDateTime() : &fDateTime);
   //special case empty files:
   if (FileClusterCount == 0){
       DirBuffer[EntryPos+26] = (unsigned char)LAST_CLUSTER;    //write LAST_CLUSTER to DirEntry
       DirBuffer[EntryPos+27] = (unsigned char)(LAST_CLUSTER >> 8);
       //FileStartCluster = LAST_CLUSTER;
   }
   else {
      // Set start cluster #
      // Seek first free cluster
      CurrentFileCluster = SeekFirstFreeCluster(); //we have already checked - there are definitely enough free ones
      //FileStartCluster = CurrentFileCluster;
      n = CurrentFileCluster;
      DirBuffer[EntryPos+26] = n;    //write 1st FileCluster to DirEntry
      DirBuffer[EntryPos+27] = n>>8;
      //Copy FileSectors in order:
      if ( (FileByteLength)<(PartClusterByteLength) )  {
         k = FileByteLength ;
         // First clear:
         for ( UINT n=0;n<(PartClusterByteLength);n++) trasb[n]=0 ; // clear trasb for have zeros after file end
      }
      else { k =  PartClusterByteLength; }
      res=fread(trasb,1,k,inFile);
      FileSector = PartHeadSectors+(CurrentFileCluster-2) * PartSectorsPerCluster;
      failed = (WriteSectors(FileSector, PartSectorsPerCluster, trasb, false) == 0);
      if (!failed){
         // if only 1 cluster mark it as last:
         if (FileClusterCount==1) {
            MarkCluster(CurrentFileCluster, LAST_CLUSTER);
         }
         else {
            unsigned int WrittenFileByteLength = PartClusterByteLength;
            for (UINT n=1; n<FileClusterCount; n++)
            {
               if ( (FileByteLength-WrittenFileByteLength)<(PartClusterByteLength) ) {
                  k = FileByteLength-WrittenFileByteLength ;
                  for (UINT i=0;i<(PartClusterByteLength);i++) { trasb[i]=0; } // clear trasb for have zeros after file end
               }
               else { k = PartClusterByteLength; }
               res=fread(trasb,1,k,inFile);
               PreviousFileCluster = CurrentFileCluster;
               if ((CurrentFileCluster = SeekNextFreeCluster(CurrentFileCluster)) == INVALID_VALUE)
               {
                  MarkCluster(PreviousFileCluster, LAST_CLUSTER) ; break ;
                  QMessageBox::critical(this, tr("Directory full!"), tr("File not written completely! (Should not happen!)"), QMessageBox::Cancel, QMessageBox::Cancel);
               }
               // write into FAT
               MarkCluster(PreviousFileCluster, CurrentFileCluster);
               MarkCluster(CurrentFileCluster, LAST_CLUSTER) ;
               //PreviousFileCluster = m;
               FileSector = PartHeadSectors+(CurrentFileCluster - 2)*PartSectorsPerCluster ;
               failed = (WriteSectors( FileSector, PartSectorsPerCluster, trasb, false) == 0);
               if (failed)
               { break; }
               WrittenFileByteLength = WrittenFileByteLength + PartClusterByteLength;
            }
         }
      }
   }
   fclose(inFile);
   //debug
   /*
   {
      unsigned int sx, nx;
      unsigned int flen = 0;
      sx = FileStartCluster;
      while (sx < 0xfff0){
         flen++;
         FATlookUp(sx, &nx, &FileSector);
         sx = nx;
      }
      if(flen != FileClusterCount){
          QString qhm = "File length differs from Cluster count. MaxCount: ";
          QString num;
          num.setNum(PartTotalDataClusters);
          qhm.append(num);
          qhm.append("\n Cluster length: ");
          num.setNum(PartClusterByteLength);
          qhm.append(num);
          qhm.append("; file byte length: ");
          num.setNum(FileByteLength);
          qhm.append(num);
          qhm.append("\nfile cluster count by file byte length: ");
          num.setNum(FileClusterCount);
          qhm.append(num);
          qhm.append("\nfile cluster count on FAT:  ");
          num.setNum(flen);
          qhm.append(num);
          QMessageBox::critical(this, LocalFileName, qhm, QMessageBox::Ok, QMessageBox::Ok);
      }
   }
   */
   if (failed){
      loadroot(true);
   }
   return(!failed);
}

void GemdDlg::on_opdirP_clicked()
{

   QString dir = QFileDialog::getExistingDirectory(this, tr("Select directory whose content shall be written to device's current directory"), "",
                                                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
   if(!dir.isEmpty()){
      bool failed = true;
      if (AddDirTreeToCurrentDir(dir, dirbuf)){
         if ((failed = !WriteCurrentDirBuf()) == false){
            failed = !WriteFAT();
         }
      }
      if (failed){
         loadroot(true);
      }else{
         // Need to refresh listbox content
         LoadSubDir((PartSubDirLevel == 0), true) ;
         setCursor(QCursor(Qt::ArrowCursor));
      }
   } // if Seldfiles end
}

void GemdDlg::on_DeleteFiles_clicked()
{
   unsigned int CurrentDirEntryPos, MainEntryDirPos, button;
   int  DirLevel, listOffset, FileRemainsLevel;
   char message[256];
   char name[32];
   bool Abort = false;
   bool YesToAll = false;
   bool failed = true;

   if (ui->filesLB->currentRow() < 0) { return; }
   if ( QMessageBox::warning(this, "Deleting files!",
                             "You are about to delete files and / or directories.\nThey can not be restored!\n\nAre you sure?",
                             QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Cancel) { return; }

   //QMessageBox::critical(this, "Deleting!", "Deleting", QMessageBox::Cancel);

   listOffset = (PartSubDirLevel > 0) ? 1 : 0;
   setCursor(QCursor(Qt::WaitCursor));
   DirLevel   = 0;
   FileRemainsLevel = 0;
   pDirEntryPos[0] = 0;
   // Get count of items in listbox
   int litc = ui->filesLB->count()-listOffset;
   for (int p=0;p<litc;p++) { //step through listed files
      // see is item selected:
      if ( ui->filesLB->item(p+listOffset)->isSelected()) { // care for selected only
         //ui->filesLB->item(p+listOffset)->setSelected(false);
         MainEntryDirPos = DirEntrySortPos[p];// Position in DIR
         //DirEntrySortPos will not be updated as long as the widget-list is not updated - but maybe a copy of it would be more save
         if (IsVolume(dirbuf, MainEntryDirPos)) { continue; }    //avoid VOL extraction
         GetFATFileName(dirbuf, MainEntryDirPos, name);
         // Recursive subdir extraction Open SubDir, show files, and extract
         // Need to store level, parent & pos in parent
         if (IsSubDir(dirbuf, MainEntryDirPos)) {
            if (!YesToAll) {
               sprintf(message, "Delete subdirectory %s including all files below?", name);
               button = QMessageBox::warning(this, name, message, QMessageBox::Yes | QMessageBox::No | QMessageBox::YesToAll | QMessageBox::Abort);
               if (button ==  QMessageBox::No){ continue; }//next entry
               if ((Abort = (button ==  QMessageBox::Abort)) == true){ break; }
               YesToAll = (button ==  QMessageBox::YesToAll);
            }
            // Store pos in parent, here by selection number
            EnterSubDir(dirbuf, MainEntryDirPos, false, false, false);
            DirLevel++;
            CurrentDirEntryPos = 0;
            do{
               bool intoNextSubDir = false;
               for (UINT n=CurrentDirEntryPos; n<PartSectorSize*DirCurrentLenght; n=n+32) {
                  if (IsEmpty(dirbuf, n)) { break; }
                  if (IsDeleted(dirbuf, n)) { continue; }
                  GetFATFileName(dirbuf, n, name);
                  if((name[0] == '.') && (name[1] == '\0')) continue;
                  if((name[0] == '.') && (name[1] == '.') && (name[2] == '\0')) continue;
                  if (IsFile(dirbuf, n)) { //no subdir, it's a file
                     if (!YesToAll) {
                        sprintf(message, "Delete file %s?", name);
                        button = QMessageBox::warning(this, name, message, QMessageBox::Yes | QMessageBox::No | QMessageBox::YesToAll | QMessageBox::Abort);
                        if (button ==  QMessageBox::No){ FileRemainsLevel = DirLevel; continue; }//next entry
                        if ((Abort = (button ==  QMessageBox::Abort)) == true){ break; }
                        YesToAll = (button ==  QMessageBox::YesToAll);
                     }
                     EraseFile(dirbuf, n);
                  }else if (IsSubDir(dirbuf, n)) {
                     // its a subdir -> store pos in dir, leave n loop
                     if (!YesToAll) {
                        sprintf(message, "Delete subdirectory %s including all files below?", name);
                        button = QMessageBox::warning(this, name, message, QMessageBox::Yes | QMessageBox::No | QMessageBox::YesToAll | QMessageBox::Abort);
                        if (button ==  QMessageBox::No){ FileRemainsLevel = DirLevel; continue; }//next entry
                        if ((Abort = (button ==  QMessageBox::Abort)) == true){ break; }
                        YesToAll = (button ==  QMessageBox::YesToAll);
                     }
                     EnterSubDir(dirbuf, n, false, false, false);
                     CurrentDirEntryPos = 0;
                     DirLevel++;
                     intoNextSubDir = true;
                     break;
                  }
               }  //n loop end
               if(intoNextSubDir){ continue; }
               //we are though
               if (FileRemainsLevel == DirLevel){ //there wss a file left in SubDir
                  WriteCurrentDirBuf(); //rewrite Dir
               }
               //dir up
               EnterUpDir(dirbuf, name, &CurrentDirEntryPos, true, true);
               DirLevel--;
               //do not delete dirs not empty
               if (FileRemainsLevel <= DirLevel) {
                  EraseFile(dirbuf, CurrentDirEntryPos); //just mark Clusters as free
               }
               if (FileRemainsLevel > DirLevel) {FileRemainsLevel = DirLevel; }
               if (DirLevel == 0) { //we're on topmost level
                  break;
               }
               CurrentDirEntryPos += 32; // point to next entry
               //are we in subdir? :
               if (dirbuf[0] != 0x2E) { QMessageBox::critical(this, "Curious problem!", "Bounced to bottom of root directory.\n This should never happen!", QMessageBox::Cancel); return; } //If it's not 'dot' entry then we're through with root DIR. If, it's the end of a SUBDIR
            }while( PartSubDirLevel > 0);
            if (Abort) { break; }
            continue; //next main dir entry
         }
         if (Abort) { break; }
         if (!YesToAll) {
            sprintf(message, "Delete file %s?", name);
            button = QMessageBox::warning(this, name, message, QMessageBox::Yes | QMessageBox::No | QMessageBox::YesToAll | QMessageBox::Abort);
            if (button ==  QMessageBox::No){ FileRemainsLevel = DirLevel; continue; }//next entry
            if ((Abort = (button ==  QMessageBox::Abort)) == true){ break; }
            YesToAll = (button ==  QMessageBox::YesToAll);
         }
         EraseFile(dirbuf, MainEntryDirPos);
      }
      if (Abort) { break; }
   }
   if (!Abort) {
      if ((failed = !WriteCurrentDirBuf()) == false){
         failed = !WriteFAT();
      }
   }
   // Need to refresh listbox content
   if (failed){
      loadroot(true);
   } else {
      LoadSubDir((PartSubDirLevel == 0), true) ;
      setCursor(QCursor(Qt::ArrowCursor));
   }
}

void GemdDlg::on_ExtractFiles_clicked()
{
   unsigned int CurrentDirEntryPos, MainEntryDirPos;
   //int DirLevel;
   int listOffset;
   char name[64];

   #ifdef FRANKS_DEBUG
   #ifdef WINDOWS
   strcpy(DestDir, "D:\\Projekte\\tools\\build-DrImg\\testoutput");
   #else
   strcpy(DestDir, "/home/frank/Projekte/ATARI/DrImg/testoutput");
   #endif
   if (chdir(DestDir)) {}
   on_setddP_clicked();
   #endif
   if(DestDir[0] == '\0'){
      on_setddP_clicked();
   }

   if (ui->filesLB->currentRow() < 0) { return; }
   listOffset = (PartSubDirLevel > 0) ? 1 : 0;
   setCursor(QCursor(Qt::WaitCursor));
   DirLevel   = 0 ;
   pDirEntryPos[0] = 0;
   // Get count of items in listbox
   int litc = ui->filesLB->count()-listOffset;
   for (int p=0;p<litc;p++) { //step through listed files
      // see is item selected:
      if ( ui->filesLB->item(p+listOffset)->isSelected()) { // care for selected only
         //ui->filesLB->item(p+listOffset)->setSelected(false);
         MainEntryDirPos = DirEntrySortPos[p];// Position in DIR
         //DirEntrySortPos will not be updated as long as the widget-list is not updated - but maybe a copy of it would be more save
         if (IsVolume(dirbuf, MainEntryDirPos)) { continue; }  //avoid VOL extraction
         // Recursive subdir extraction Open SubDir, show files, and extract
         if (IsSubDir(dirbuf, MainEntryDirPos)) {
            EnterSubDir(dirbuf, MainEntryDirPos, true, true, true);
            DirLevel++;
            CurrentDirEntryPos = 0; // 1st 2 entries of sub dir are '.' and '..'
            do{
               bool intoNextSubDir = false;
               for (UINT n=CurrentDirEntryPos; n<PartSectorSize*DirCurrentLenght; n=n+32) {
                  if (IsEmpty(dirbuf, n)) break; //empty entry
                  if (IsDeleted(dirbuf, n)) continue; //deleted entry
                  GetFATFileName(dirbuf, n, name);
                  if((name[0] == '.') && (name[1] == '\0')) continue;
                  if((name[0] == '.') && (name[1] == '.') && (name[2] == '\0')) continue;
                  if (IsFile(dirbuf, n)) { //no subdir, it's a file
                     ExtractFile(dirbuf, n);
                  }else if (IsSubDir(dirbuf, n)) {
                     // its a subdir -> store pos in dir, leave n loop
                     EnterSubDir(dirbuf, n, true, true, true);
                     CurrentDirEntryPos = 0;
                     DirLevel++;
                     intoNextSubDir = true;
                     break;
                  }
               }  //n loop end
               if(intoNextSubDir){ intoNextSubDir = false; continue; }
               //we are though, dir up
               QString DirPath = QDir::currentPath();
               EnterUpDir(dirbuf, name, &CurrentDirEntryPos, true, false);
               if (!timestCur){
                   tm DateTime;
                   GetFATFileDateTime(dirbuf, CurrentDirEntryPos, &DateTime);
                  #ifdef WINDOWS
                   HANDLE DirH = CreateFileA((char*)DirPath.toLatin1().data(), GENERIC_READ | GENERIC_WRITE,  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                             NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
                   if (DirH != INVALID_HANDLE_VALUE) {
                      SetLocalFileDateTime(DateTime, DirH);
                      CloseHandle(DirH);
                   } else {
                      ShowErrorDialog(this, "Error open dir for time modification. ", GetLastError());
                   }
                   #else
                   SetLocalFileDateTime(DateTime, (char*)DirPath.toLatin1().data());
                   #endif
               }
               DirLevel--;
               if (DirLevel == 0) { //we're on topmost level
                  break;
               }
               CurrentDirEntryPos += 32; // point to next entry
               //are we in subdir? :
               if (dirbuf[0] != 0x2E) { QMessageBox::critical(this, "Curious problem!", "Bounced to bottom of root directory.\n This should never happen!", QMessageBox::Cancel); return; } //If it's not 'dot' entry then we're through with root DIR. If, it's the end of a SUBDIR
            }while( PartSubDirLevel > 0);
            continue; //next main dir entry
         }
         ExtractFile(dirbuf, MainEntryDirPos);
      } // if selected end
   }  // p loop end
   setCursor(QCursor(Qt::ArrowCursor));
}

void GemdDlg::on_AddFiles_clicked()
{
   bool failed = false;
   // Multiple file open dialQFileDialogog:
   QFileDialog FileSelector;
   FileSelector.setWindowIcon(QIcon("://Icons/atarifolder.png"));
   QStringList SeldFiles = FileSelector.getOpenFileNames(this, tr("Select files to write to device"), NULL, "All files(*.*);;ST PRG, TOS, TTP, APP files (*.prg *.tos *.ttp *.app);;Binary files(*.bin)");
   if(!SeldFiles.isEmpty())  {
      for (QStringList::Iterator iq = SeldFiles.begin(); iq != SeldFiles.end(); ++iq)
      {
         failed = !AddFileToCurrentDir(*iq, dirbuf);
         if (failed)
         { break; }
      } // iq for loop end
      //Write Directory:
      if (!failed){
         if ((failed = !WriteCurrentDirBuf()) == false){
            failed = !WriteFAT();
         }
      }
      if (failed){
         loadroot(true);
      } else {
         // Need to refresh listbox content
         LoadSubDir((PartSubDirLevel == 0), true);
      }
   } // if Seldfiles end
   setCursor(QCursor(Qt::ArrowCursor))    ;
}

void GemdDlg::on_newfP_clicked()
{

   bool failed = true;
   QString litet = ui->dirED->text();
   //GetDlgItemText(hDlgWnd, GemCreD , szText , (LPARAM) 64);
   if ( litet.length() == 0 ){
      QMessageBox::critical(this, tr("Invalid filename!"), tr("Enter something"), QMessageBox::Cancel, QMessageBox::Cancel);
      return;
   }
   // Look is name base 8 space:
   int c = 0;
   for (int n=0;n<8;n++) {
      if (litet[n]==' ') c++ ;
   }
   if (c==8) {
      QMessageBox::critical(this, tr("Invalid filename!"), tr("All the spaces..."), QMessageBox::Cancel, QMessageBox::Cancel);
      return;
   }
   int EntryPos = AddSingleDirToCurrentDir(litet, litet, dirbuf, false);
   SetFATFileDateTime(dirbuf, EntryPos, CurrentDateTime());
   //Write Directory:
   if ((failed = !WriteCurrentDirBuf()) == false){
      failed = !WriteFAT();
   }
   if (failed){
      loadroot(true);
   } else {
   // Need to refresh listbox content
     LoadSubDir((PartSubDirLevel == 0), true);
   }
}


void GemdDlg::on_desallP_clicked()
{
   ui->filesLB->clearSelection();
}

void GemdDlg::on_quitP_clicked()
{
   GemdDlg::close() ;
}

void GemdDlg::closeEvent (QCloseEvent *event)
{
   if(event){}
   if (fhdl != FILE_OPEN_FAILED){
      CloseFileX(&fhdl);
   }
}
//debug
void GemdDlg::on_saveFAT_clicked()
{
   #ifdef FRANKS_DEBUG
   #ifdef WINDOWS
   HANDLE fo;
   strcpy(DestDir, "D:\\Projekte\\tools\\build-DrImg\\testoutput");
   #else
   FILE *fout;
   strcpy(DestDir, "/home/frank/Projekte/ATARI/DrImg/testoutput");
   #endif
   if (chdir(DestDir)) {}
   fo = OpenFileX("FAT.bin", "wb");
   WriteToFile(PartFATbuffer, 1, PartSectorsPerFAT*PartSectorSize, fo);
   CloseFileX(&fo);
   #endif
}
